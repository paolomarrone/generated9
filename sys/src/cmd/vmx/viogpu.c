/* VirtIO GPU and VirGL file transport. See UPSTREAM.gpu and LICENSE.gpu. */
#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"
#include "virtio.h"

enum {
	Maxcmd = 256*1024,
	Maxmem = 256*1024*1024,
	Maxbacking = 64*1024*1024,
	Maxres = 4096,
	Ok = 0x1100,
	Err = 0x1200,
	Nomem,
	Badscanout,
	Badresource,
	Badcontext,
	Badarg,
};

typedef struct Gctx Gctx;
typedef struct Gres Gres;
typedef struct Glink Glink;
typedef struct Gmem Gmem;
typedef struct Vgpu Vgpu;

struct Gctx {
	u32int id, host;
	int ctl, cmd, slot, nres;
	ulong bytes;
	Gctx *next;
};
struct Glink {
	Gctx *ctx;
	Glink *next;
};
struct Gmem {
	uchar *p;
	ulong size;
};
struct Gres {
	u32int id, host, desc[10];
	int fd, nmem, query;
	uchar qstate[16];
	ulong size, backing;
	Gmem *mem;
	Gctx *owner;
	Glink *link;
	Gres *next;
};
struct Vgpu {
	QLock;
	char *root;
	uchar *caps, *pixels;
	int ncaps, width, height, nres;
	Gctx *ctx, *pool;
	Gres *res, *scanout;
	u32int x, y, w, h;
};

static u32int
ctxinfo(Gctx *c)
{
	char buf[80], *f[3];
	int n;

	n = pread(c->ctl, buf, sizeof(buf)-1, 0);
	if(n <= 0) return 0;
	buf[n] = 0;
	if(tokenize(buf, f, 3) != 3) return 0;
	c->slot = atoi(f[0]);
	c->host = strtoul(f[1], nil, 10);
	return strtoul(f[2], nil, 10);
}

static Gctx*
newctx(Vgpu *g, u32int id)
{
	Gctx *c;
	char *path;

	c = emalloc(sizeof(*c));
	c->id = id;
	path = smprint("%s/clone", g->root);
	c->ctl = open(path, ORDWR);
	free(path);
	if(c->ctl < 0){ free(c); return nil; }
	ctxinfo(c);
	path = smprint("%s/%d/cmd", g->root, c->slot);
	c->cmd = open(path, OWRITE);
	free(path);
	if(c->host == 0 || c->cmd < 0){
		close(c->ctl);
		if(c->cmd >= 0) close(c->cmd);
		free(c);
		return nil;
	}
	return c;
}

static void
freectx(Gctx *c)
{
	close(c->cmd);
	close(c->ctl);
	free(c);
}

static Gctx*
findctx(Vgpu *g, u32int id)
{
	Gctx *c;

	for(c=g->ctx; c != nil; c=c->next)
		if(c->id == id) return c;
	return nil;
}

static Gres*
findres(Vgpu *g, u32int id)
{
	Gres *r;

	for(r=g->res; r != nil; r=r->next)
		if(r->id == id) return r;
	return nil;
}

static int
attached(Gres *r, Gctx *c)
{
	Glink *l;

	for(l=r->link; l != nil; l=l->next)
		if(l->ctx == c) return 1;
	return 0;
}

static void
detach(Gres *r, Gctx *c)
{
	Glink **p, *l;

	for(p=&r->link; (l=*p) != nil; p=&l->next)
		if(l->ctx == c){
			if(r->host != 0) fprint(c->ctl, "unref %ud", r->host);
			*p = l->next;
			free(l);
			return;
		}
}

static void
freehost(Vgpu *g, Gres *r)
{
	Gctx *c, **cp;
	Glink *l;

	if(r->host != 0){
		for(l=r->link; l != nil; l=l->next)
			fprint(l->ctx->ctl, "unref %ud", r->host);
		c = r->owner;
		fprint(c->ctl, "unref %ud", r->host);
		if(r->fd >= 0) close(r->fd);
		c->bytes -= r->size;
		if(--c->nres == 0){
			for(cp=&g->pool; *cp != c; cp=&(*cp)->next) {}
			*cp = c->next;
			freectx(c);
		}
	}
	r->host = r->size = 0;
	r->owner = nil;
	r->fd = -1;
}

static void
blank(Vgpu *g)
{
	g->scanout = nil;
	vgacursor(0,0,1,nil,0,0);
	vgapresent(nil,g->width,g->height);
}

static void
freeres(Vgpu *g, Gres *r)
{
	Gres **p;

	if(g->scanout == r) blank(g);
	while(r->link != nil) detach(r, r->link->ctx);
	freehost(g,r);
	for(p=&g->res; *p != r; p=&(*p)->next) {}
	*p = r->next;
	g->nres--;
	free(r->mem);
	free(r);
}

/* Resources have no guest context until CTX_ATTACH_RESOURCE. */
static int
allocres(Vgpu *g, Gres *r, ulong size)
{
	Gctx *c;
	Glink *l;
	u32int *d;
	char *path;
	char err[ERRMAX];

	if(size == 0 || size > Maxbacking) return -1;
	for(c=g->pool; c != nil; c=c->next)
		if(c->nres < Maxres && size <= Maxmem-c->bytes) break;
	if(c == nil){
		c = newctx(g, 0);
		if(c == nil) return -1;
		c->next = g->pool;
		g->pool = c;
	}
	d = r->desc;
	if(fprint(c->ctl, "resource %ud %ud %ud %ud %ud %ud %ud %ud %ud %ud %lud",
		d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],size) < 0){
		if(c->nres == 0){
			/* A new allocation context is still at the head of the pool. */
			rerrstr(err,sizeof(err));
			g->pool = c->next;
			freectx(c);
			werrstr("%s",err);
		}
		return -1;
	}
	r->host = ctxinfo(c);
	r->owner = c;
	r->size = size;
	c->bytes += size;
	c->nres++;
	path = smprint("%s/%d/%ud", g->root, c->slot, r->host);
	r->fd = open(path, ORDWR);
	free(path);
	if(r->fd < 0) goto fail;
	for(l=r->link; l != nil; l=l->next)
		if(fprint(l->ctx->ctl, "attach %ud %ud", c->host, r->host) < 0)
			goto fail;
	return 0;
fail:
	rerrstr(err,sizeof(err));
	freehost(g,r);
	werrstr("%s",err);
	return -1;
}

/* Preserve bytes outside a transferred box without interpreting its format. */
static int
backing(Gres *r, int read, ulong off, ulong len)
{
	ulong pos, skip, n;
	int i;

	if(r->host == 0 || off > r->backing || len > r->backing-off) return -1;
	pos = 0;
	for(i=0; i<r->nmem && len != 0; i++){
		if(off >= pos+r->mem[i].size){ pos += r->mem[i].size; continue; }
		skip = off-pos;
		n = MIN(len,r->mem[i].size-skip);
		if((read ? pread(r->fd, r->mem[i].p+skip, n, off) : pwrite(r->fd, r->mem[i].p+skip, n, off)) != n)
			return -1;
		off += n;
		len -= n;
		pos += r->mem[i].size;
	}
	return len == 0 ? 0 : -1;
}

/* VirGL 1 queries write a 16-byte state at the start of their backing. */
static int
query(Gres *r, int read)
{
	uchar state[16];

	if(!read){
		if(backing(r,0,0,sizeof(state)) < 0) return -1;
		return pread(r->fd,r->qstate,sizeof(state),0) == sizeof(state) ? 0 : -1;
	}
	if(pread(r->fd,state,sizeof(state),0) != sizeof(state)) return -1;
	/* Do not overwrite a guest reset unless the host actually wrote a result. */
	if(memcmp(state,r->qstate,sizeof(state)) == 0) return 0;
	if(backing(r,1,4,12) < 0) return -1;
	/* Publish the result before its ready flag to a polling guest CPU. */
	coherence();
	return backing(r,1,0,4);
}

static int
transfer(Gres *r, Gctx *c, u32int *box, uvlong off, u32int level, u32int stride, u32int layer, int read)
{
	if(off >= r->backing || backing(r,0,0,r->backing) < 0) return -1;
	if(c == nil) c = r->owner;
	if(fprint(c->ctl, "%s %ud %ud %ud %ud %ud %ud %ud %ud %llud %ud %ud",
		read ? "download" : "upload", r->host, level,
		box[0],box[1],box[2],box[3],box[4],box[5],off,stride,layer) < 0)
		return -1;
	return read ? backing(r,1,0,r->backing) : 0;
}

static int
mapres(Vgpu *g, Gctx *c, u32int *p, int query)
{
	Gres *r;

	if(*p == 0) return 0;
	r = findres(g, *p);
	if(r == nil || r->host == 0 || !attached(r, c)) return -1;
	if(query) r->query = 1;
	*p = r->host;
	return 0;
}

/* Only resource handles change. Object names and shader programs pass through. */
static int
submit(Vgpu *g, Gctx *c, u32int *p, int words)
{
	u32int *end, *start;
	Gres *r;
	int op, obj, n, i, a, b, step, isquery;

	start = p;
	end = p+words;
	while(p < end){
		op = p[0]&255;
		obj = p[0]>>8 & 255;
		n = p[0]>>16;
		if(n >= end-p) return -1;
		a = b = step = isquery = 0;
		switch(op){
		case 1:	/* CREATE_OBJECT */
			if(obj == 6 || obj == 8 || obj == 10) a = 2;
			else if(obj == 9){ a = 4; isquery = 1; }
			else if(obj == 0 || obj > 10) return -1;
			break;
		case 6: a = 3; step = 3; break;	/* vertex buffers */
		case 8:
			if(n == 20){ a = 15; b = 20; }
			break;
		case 9: case 11: a = 1; break;	/* inline write, index buffer */
		case 16: a = 4; b = 13; break;	/* blit */
		case 17: a = 1; b = 6; break;	/* resource copy */
		case 27: a = 5; break;	/* uniform buffer */
		case 34: a = 5; step = 3; break;
		case 35: a = 7; step = 5; break;
		case 37: a = 7; break;	/* indirect grid */
		case 40: a = 4; step = 3; break;
		case 42: a = 2; break;	/* query result buffer */
		default:
			/* capset 1 does not advertise newer resource or transfer commands. */
			if(op > 42) return -1;
			break;
		}
		if(a != 0){
			if(step != 0){
				if(n < a-step || (n-a)%step != 0) return -1;
				for(i=a; i<=n; i+=step)
					if(mapres(g,c,p+i,isquery) < 0) return -1;
			}else if(a > n || mapres(g,c,p+a,isquery) < 0) return -1;
		}
		if(b != 0 && (b > n || mapres(g,c,p+b,0) < 0)) return -1;
		p += n+1;
	}
	/* Mesa resets query state through the guest mapping before reusing it. */
	for(r=g->res; r != nil; r=r->next)
		if(r->query && r->mem != nil && attached(r,c) && query(r,0) < 0) return -1;
	/* Each write is fenced, including writes to query backing memory. */
	if(write(c->cmd, start, words*4) != words*4) return -1;
	for(r=g->res; r != nil; r=r->next)
		if(r->query && r->mem != nil && attached(r,c) && query(r,1) < 0) return -1;
	return 0;
}

static int
pixels(Vgpu *g, Gres *r, int x, int y, int w, int h)
{
	int i;
	uchar *p, t;

	if(r->host == 0 || w < 1 || h < 1 || w > g->width || h > g->height
	|| x < 0 || y < 0 || x+w > r->desc[3] || y+h > r->desc[4]
	|| w*h*4 > r->size)
		return -1;
	if(r->desc[1] != 1 && r->desc[1] != 2 && r->desc[1] != 67 && r->desc[1] != 134)
		return -1;
	if(fprint(r->owner->ctl, "download %ud 0 %d %d 0 %d %d 1 0 %d 0", r->host,x,y,w,h,w*4) < 0
	|| pread(r->fd, g->pixels, w*h*4, 0) != w*h*4)
		return -1;
	if(r->desc[1] == 67 || r->desc[1] == 134)
		for(i=0; i<w*h; i++){
			p = g->pixels+4*i;
			t = p[0]; p[0] = p[2]; p[2] = t;
		}
	return 0;
}

static int
control(Vgpu *g, uchar *buf, int n, uchar *reply, int *nr)
{
	u32int *p, op, id, box[6];
	Gres *r;
	Gctx *c, **cp;
	Glink *l;
	Gmem *mem;
	uvlong addr, size;
	int i, count;
	ulong len;

	p = (u32int*)buf;
	op = p[0];
	c = findctx(g, p[4]);
	id = n >= 28 ? p[6] : 0;
	r = findres(g, id);
	switch(op){
	case 0x100:	/* GET_DISPLAY_INFO */
		*nr = 24+16*24;
		PUT32(reply,32,g->width);
		PUT32(reply,36,g->height);
		PUT32(reply,40,1);
		return Ok+1;
	case 0x108:	/* GET_CAPSET_INFO */
		if(n != 32 || id != 0) return Badarg;
		*nr = 40;
		PUT32(reply,24,1);
		PUT32(reply,28,1);
		PUT32(reply,32,g->ncaps);
		return Ok+2;
	case 0x109:	/* GET_CAPSET */
		if(n != 32 || id != 1 || p[7] > 1) return Badarg;
		*nr = 24+g->ncaps;
		memmove(reply+24,g->caps,g->ncaps);
		return Ok+3;
	case 0x200:	/* CTX_CREATE */
		if(n != 96 || p[4] == 0 || c != nil || p[6] > 64 || p[7] != 0) return Badarg;
		c = newctx(g,p[4]);
		if(c == nil) return Nomem;
		c->next = g->ctx;
		g->ctx = c;
		return Ok;
	case 0x201:	/* CTX_DESTROY */
		if(n != 24 || c == nil) return Badcontext;
		for(r=g->res; r != nil; r=r->next) detach(r,c);
		for(cp=&g->ctx; *cp != c; cp=&(*cp)->next) {}
		*cp = c->next;
		freectx(c);
		return Ok;
	case 0x101: case 0x204:	/* RESOURCE_CREATE_2D / 3D */
		if(id == 0 || r != nil || n != (op == 0x101 ? 40 : 72)) return Badarg;
		if(g->nres == Maxres) return Nomem;
		r = emalloc(sizeof(*r));
		r->id = id;
		r->fd = -1;
		if(op == 0x204) memmove(r->desc,p+7,sizeof(r->desc));
		else{
			r->desc[0] = 2;
			r->desc[1] = p[7];
			r->desc[2] = 2|8|1<<18;
			r->desc[3] = p[8];
			r->desc[4] = p[9];
			r->desc[5] = r->desc[6] = r->desc[9] = 1;
		}
		r->next = g->res;
		g->res = r;
		g->nres++;
		return Ok;
	case 0x102:	/* RESOURCE_UNREF */
		if(n != 32 || r == nil) return Badresource;
		freeres(g,r);
		return Ok;
	case 0x106:	/* ATTACH_BACKING */
		if(n < 32 || r == nil || r->mem != nil) return Badarg;
		count = p[7];
		if(count < 1 || count > (n-32)/16 || 32+16*count != n) return Badarg;
		mem = emalloc(count*sizeof(*mem));
		size = 0;
		for(i=0; i<count; i++){
			addr = GET64(buf,32+16*i);
			len = GET32(buf,40+16*i);
			mem[i].p = gptr(addr,len);
			mem[i].size = len;
			size += len;
			if(mem[i].p == nil || len == 0 || size > Maxbacking){ free(mem); return Badarg; }
		}
		if((r->host == 0 && allocres(g,r,size) < 0) || size > r->size){ free(mem); return Nomem; }
		r->mem = mem;
		r->nmem = count;
		r->backing = size;
		/* Collapse physically adjacent entries before copying across gpu(3). */
		for(i=1, count=0; i<r->nmem; i++){
			if(mem[count].p+mem[count].size == mem[i].p) mem[count].size += mem[i].size;
			else mem[++count] = mem[i];
		}
		r->nmem = count+1;
		return Ok;
	case 0x107:
		if(n != 32 || r == nil) return Badresource;
		free(r->mem);
		r->mem = nil;
		r->nmem = r->backing = 0;
		return Ok;
	case 0x202: case 0x203:	/* CTX_ATTACH / DETACH_RESOURCE */
		if(n != 32 || r == nil) return Badresource;
		if(c == nil) return Badcontext;
		if(op == 0x203) detach(r,c);
		else if(!attached(r,c)){
			if(r->host != 0 && fprint(c->ctl,"attach %ud %ud",r->owner->host,r->host) < 0) return Nomem;
			l = emalloc(sizeof(*l));
			l->ctx = c;
			l->next = r->link;
			r->link = l;
		}
		return Ok;
	case 0x105:	/* TRANSFER_TO_HOST_2D */
		if(n != 56) return Badarg;
		r = findres(g,p[12]);
		if(r == nil) return Badresource;
		box[0]=p[6]; box[1]=p[7]; box[2]=0; box[3]=p[8]; box[4]=p[9]; box[5]=1;
		return transfer(r,nil,box,GET64(buf,40),0,r->desc[3]*4,0,0) < 0 ? Err : Ok;
	case 0x205: case 0x206:	/* TRANSFER_TO / FROM_HOST_3D */
		if(n != 72) return Badarg;
		r = findres(g,p[14]);
		if(r == nil) return Badresource;
		if(p[4] != 0 && (c == nil || !attached(r,c))) return Badcontext;
		return transfer(r,c,p+6,GET64(buf,48),p[15],p[16],p[17],op == 0x206) < 0 ? Err : Ok;
	case 0x207:	/* SUBMIT_3D */
		if(n < 32 || p[6] != n-32 || p[6] == 0 || (p[6]&3)) return Badarg;
		if(c == nil) return Badcontext;
		return submit(g,c,p+8,(n-32)/4) < 0 ? Err : Ok;
	case 0x103:	/* SET_SCANOUT */
		if(n != 48 || p[10] != 0) return Badscanout;
		if(p[11] == 0){ blank(g); return Ok; }
		r = findres(g,p[11]);
		if(r == nil || p[8] == 0 || p[9] == 0 || p[8] > g->width || p[9] > g->height
		|| (uvlong)p[6]+p[8] > r->desc[3] || (uvlong)p[7]+p[9] > r->desc[4]) return Badarg;
		g->scanout = r;
		g->x=p[6]; g->y=p[7]; g->w=p[8]; g->h=p[9];
		return Ok;
	case 0x104:	/* RESOURCE_FLUSH */
		if(n != 48) return Badarg;
		r = findres(g,p[10]);
		if(r == nil) return Badresource;
		if(r == g->scanout){
			if(pixels(g,r,g->x,g->y,g->w,g->h) < 0) return Err;
			vgapresent(g->pixels,g->w,g->h);
		}
		return Ok;
	case 0x300: case 0x301:	/* UPDATE / MOVE_CURSOR */
		if(n != 56 || p[6] != 0) return Badscanout;
		if(op == 0x300 && p[10] != 0){
			r = findres(g,p[10]);
			if(r == nil || r->desc[3] != 64 || r->desc[4] != 64 || p[11] >= 64 || p[12] >= 64) return Badarg;
			if(pixels(g,r,0,0,64,64) < 0) return Err;
		}
		vgacursor(p[7],p[8],op == 0x300,p[10] ? g->pixels : nil,p[11],p[12]);
		return Ok;
	}
	return Badarg;
}

static void
gpuproc(void *arg)
{
	VIOQueue *q;
	VIOBuf *b;
	Vgpu *g;
	uchar *buf, reply[4096];
	int n, nr, result;

	q = arg;
	g = q->d->aux;
	buf = emalloc(Maxcmd+32);
	threadsetname("virtio gpu");
	for(;;){
		b = viogetbuf(q,1);
		n = vioqrem(b,0);
		memset(reply,0,sizeof(reply));
		nr = 24;
		if(n < 24 || n > Maxcmd+32 || (n&3) || vioqread(b,buf,n) != n)
			result = Badarg;
		else{
			qlock(g);
			werrstr("invalid GPU request");
			result = control(g,buf,n,reply,&nr);
			qunlock(g);
			if(result >= Err) vmerror("virtio gpu command %#ux: response %#ux: %r",GET32(buf,0),result);
			/* Completion of host gpu(3) operations also completes the guest fence. */
			if(GET32(buf,4)&1){
				PUT32(reply,4,1);
				PUT64(reply,8,GET64(buf,8));
				PUT32(reply,16,GET32(buf,16));
			}
		}
		PUT32(reply,0,result);
		vioqwrite(b,reply,nr);
		vioputbuf(b);
	}
}

static u32int
gpuconfig(int isin, u16int port, u32int, int, VIODev*)
{
	if(!isin) return 0;
	if(port == 8 || port == 12) return 1;	/* one scanout, capset 1 */
	return 0;
}

static void
gpureset(VIODev *d)
{
	Vgpu *g;
	Gctx *c;

	g = d->aux;
	qlock(g);
	while(g->res != nil) freeres(g,g->res);
	while((c=g->ctx) != nil){ g->ctx=c->next; freectx(c); }
	while((c=g->pool) != nil){ g->pool=c->next; freectx(c); }
	blank(g);
	qunlock(g);
}

int
mkviogpu(char *root)
{
	VIODev *d;
	VIOQueue *q;
	Vgpu *g;
	char *path;
	int fd, i;
	extern int maxw, maxh;
	extern uintptr fbsz;

	if(fbsz == 0 || maxw < 64 || maxh < 64 || maxw > 4096 || maxh > 4096){
		werrstr("VirtIO GPU needs a framebuffer of 64x64 through 4096x4096");
		return -1;
	}
	path = smprint("%s/caps",root);
	fd = open(path,OREAD);
	free(path);
	if(fd < 0) return -1;
	g = emalloc(sizeof(*g));
	g->caps = emalloc(3072);
	g->ncaps = read(fd,g->caps,3072);
	close(fd);
	if(g->ncaps < 4 || GET32(g->caps,0) != 1){
		free(g->caps); free(g);
		werrstr("GPU requires VirGL capability set 1");
		return -1;
	}
	g->root = strdup(root);
	g->width = maxw;
	g->height = maxh;
	g->pixels = emalloc(maxw*maxh*4);
	d = mkviodev(0x1050,0x030000,16,2);
	d->aux = g;
	d->devfeat = 1;
	d->io = gpuconfig;
	d->reset = gpureset;
	viomodern(d);
	for(i=0; i<2; i++){
		q = mkvioqueue(d,256,viowakeup);
		proccreate(gpuproc,q,16384);
	}
	return 0;
}
