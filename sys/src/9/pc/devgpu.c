/* VirtIO 1.2 GPU display and VirGL command transport. See gpu(3).
 * Wire layouts follow virtio_gpu.h; its notice follows.
 */
/*
 * Virtio GPU Device
 *
 * Copyright Red Hat, Inc. 2013-2014
 *
 * Authors:
 *     Dave Airlie <airlied@redhat.com>
 *     Gerd Hoffmann <kraxel@redhat.com>
 *
 * This header is BSD licensed so anyone can use the definitions
 * to implement compatible drivers/servers:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/pci.h"
#include "../port/error.h"
#include "../port/virtio10.h"

#define Image IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"

enum {
	Create2d = 0x101,
	Unref = 0x102,
	Scanout,
	Flush,
	Transfer,
	Attach = 0x106,
	Capinfo = 0x108,
	Getcap = 0x109,
	Ctxcreate = 0x200,
	Ctxdestroy,
	Ctxattach,
	Ctxdetach,
	Rescreate,
	Upload,
	Download,
	Submit,
	Ok = 0x1100,
	Maxctx = 32,
	Maxres = 256,
	Maxmem = 64*1024*1024,
	Maxcmd = 256*1024,
	Qdir = 0,
	Qclone,
	Qcaps,
	Qctl,
	Qcmd,
	Qres,
};

typedef struct Resource Resource;
typedef struct Context Context;

struct Resource {
	u32int id;
	uchar *data;
	ulong size;
};

struct Context {
	int ref;
	u32int id;
	u32int last;
	ulong bytes;
	Resource res[Maxres];
};

static QLock gpulock;		/* table, contexts, and one outstanding command */
static struct {
	Pcidev *pci;
	Vio cfg, isr, notify;
	Vdesc *desc;
	Vring *avail, *used;
	u16int *availent;
	Vused *usedent;
	Rendez wait;
	u16int usedidx;
	uvlong fence;
	int ready, failed;
	u32int nextid;
	uchar *caps;
	ulong ncaps;
	u32int *command;
	Resource fb;
	int width, height;
	Context ctx[Maxctx];
} gpu;

#define SLOT(q) ((int)((q).path >> 16)-1)
#define FILE(q) ((int)((q).path & 0xffff))
#define PATH(s, f) (((uvlong)((s)+1)<<16)|(f))

static int
done(void*)
{
	coherence();
	return gpu.used->idx != gpu.usedidx;
}

static void
gpuinterrupt(Ureg*, void*)
{
	if(vin8(&gpu.isr, 0) & 1)
		wakeup(&gpu.wait);
}

static void
gpufail(void)
{
	/* Stop DMA before callers release command and resource memory. */
	gpu.failed = 1;
	pciclrbme(gpu.pci);
	vout8(&gpu.cfg, Vconf_status, 0);
	while(vin8(&gpu.cfg, Vconf_status) != 0)
		delay(1);
}

/* All wire words are little endian; this driver is PC-only. */
static void
request(u32int *req, int nreq, u32int *resp, int nresp, u32int expected)
{
	ulong len, start;
	char err[ERRMAX];

	if(gpu.failed)
		error("gpu unavailable after device failure");
	memset(resp, 0, nresp);
	req[1] = 1;		/* fence: completion includes GPU execution */
	req[2] = ++gpu.fence;
	req[3] = gpu.fence >> 32;
	gpu.desc[0] = (Vdesc){PADDR(req), nreq, 1, 1};
	gpu.desc[1] = (Vdesc){PADDR(resp), nresp, 2, 0};
	gpu.availent[gpu.avail->idx & 1] = 0;
	coherence();
	gpu.avail->idx++;
	coherence();
	vout16(&gpu.notify, 0, 0);
	/* A note cannot release memory still owned by the device. */
	start = MACHP(0)->ticks;
	while(!done(nil) && TK2MS(MACHP(0)->ticks-start) < 10000){
		if(!waserror()){
			tsleep(&gpu.wait, done, nil, 100);
			poperror();
		}
	}
	if(!done(nil)){
		gpufail();
		error("gpu command timed out");
	}
	coherence();
	len = gpu.usedent[gpu.usedidx & 1].len;
	if(gpu.usedent[gpu.usedidx++ & 1].id != 0 || len < 24 || len > nresp){
		gpufail();
		error("gpu invalid queue response");
	}
	if((resp[1]&1) == 0 || resp[2] != req[2] || resp[3] != req[3]){
		gpufail();
		error("gpu invalid fence response");
	}
	if(resp[0] >= 0x1200 && resp[0] <= 0x1205){
		snprint(err, sizeof(err), "gpu command %#ux: response %#ux", req[0], resp[0]);
		error(err);
	}
	if(resp[0] != expected || len != nresp){
		gpufail();
		error("gpu unexpected response");
	}
}

static void
command(u32int *req, int n)
{
	u32int resp[6];

	request(req, n, resp, sizeof(resp), Ok);
}

static int
matchcap(Pcidev *p, int cap, int off, int typ)
{
	int bar;

	if(cap != 9 || pcicfgr8(p, off+3) != typ)
		return 1;
	bar = pcicfgr8(p, off+4);
	return bar >= nelem(p->mem) || p->mem[bar].size == 0;
}

static int
mapcap(int typ, int size, Vio *v)
{
	int cap;

	cap = pcienumcaps(gpu.pci, matchcap, typ);
	if(virtiomapregs(gpu.pci, cap, size, v) == nil)
		error("gpu missing PCI capability");
	return cap;
}

static void
gpuprobe(void)
{
	Vio dev;
	uchar *p;
	u32int lo, hi, mult, off, req[8], resp[10], *caps;
	int cap, n, i;

	if(gpu.ready)
		return;
	if(gpu.failed)
		error(Enodev);
	gpu.pci = pcimatch(nil, 0x1af4, 0x1050);
	if(gpu.pci == nil)
		error(Enodev);
	/* Probe once; failed hardware must not be initialized over live DMA. */
	gpu.failed = 1;
	pcienable(gpu.pci);
	mapcap(1, Vconf_sz, &gpu.cfg);
	if(waserror()){
		gpufail();
		nexterror();
	}
	vout8(&gpu.cfg, Vconf_status, 0);
	while(vin8(&gpu.cfg, Vconf_status) != 0)
		delay(1);
	vout8(&gpu.cfg, Vconf_status, Sacknowledge|Sdriver);
	vout32(&gpu.cfg, Vconf_devfeatsel, 0);
	lo = vin32(&gpu.cfg, Vconf_devfeat);
	vout32(&gpu.cfg, Vconf_devfeatsel, 1);
	hi = vin32(&gpu.cfg, Vconf_devfeat);
	if((hi&Fversion1) == 0)
		error("gpu requires VirtIO 1.0");
	vout32(&gpu.cfg, Vconf_drvfeatsel, 0);
	vout32(&gpu.cfg, Vconf_drvfeat, lo&1);
	vout32(&gpu.cfg, Vconf_drvfeatsel, 1);
	vout32(&gpu.cfg, Vconf_drvfeat, Fversion1);
	vout8(&gpu.cfg, Vconf_status, Sacknowledge|Sdriver|Sfeaturesok);
	if((vin8(&gpu.cfg, Vconf_status)&Sfeaturesok) == 0)
		error("gpu rejected features");
	mapcap(3, 1, &gpu.isr);
	mapcap(4, 16, &dev);
	n = vin32(&dev, 12);
	virtiounmap(&dev, 16);
	cap = mapcap(2, 0, &gpu.notify);
	mult = pcicfgr32(gpu.pci, cap+16);
	vout16(&gpu.cfg, Vconf_queuesel, 0);
	if(vin16(&gpu.cfg, Vconf_queuesize) < 2)
		error("gpu control queue too small");
	off = vin16(&gpu.cfg, Vconf_queuenotifyoff);
	if((uvlong)off*mult+2 > pcicfgr32(gpu.pci, cap+12))
		error("gpu invalid notification offset");
	if(gpu.notify.type == Vio_port)
		gpu.notify.port += off*mult;
	else
		gpu.notify.mem += off*mult;
	p = mallocalign(BY2PG, BY2PG, 0, 0);
	if(p == nil)
		error(Enomem);
	memset(p, 0, BY2PG);
	gpu.desc = (void*)p;
	gpu.avail = (void*)(p+32);
	gpu.availent = (void*)(p+36);
	gpu.used = (void*)(p+48);
	gpu.usedent = (void*)(p+52);
	vout16(&gpu.cfg, Vconf_queuesize, 2);
	vout64(&gpu.cfg, Vconf_queuedesc, PADDR(gpu.desc));
	vout64(&gpu.cfg, Vconf_queueavail, PADDR(gpu.avail));
	vout64(&gpu.cfg, Vconf_queueused, PADDR(gpu.used));
	vout16(&gpu.cfg, Vconf_queueenable, 1);
	intrenable(gpu.pci->intl, gpuinterrupt, nil, gpu.pci->tbdf, "gpu");
	pcisetbme(gpu.pci);
	coherence();
	vout8(&gpu.cfg, Vconf_status, Sacknowledge|Sdriver|Sfeaturesok|Sdriverok);
	gpu.failed = 0;
	for(i=0; (lo&1) != 0 && i<n; i++){
		memset(req, 0, sizeof(req));
		req[0] = Capinfo;
		req[6] = i;
		request(req, sizeof(req), resp, sizeof(resp), Ok+2);
		if(resp[6] != 1 || resp[7] < 1)
			continue;
		if(resp[8] == 0 || resp[8] > Maxcmd)
			error("gpu invalid capability size");
		gpu.ncaps = resp[8];
		caps = malloc(24+gpu.ncaps);
		if(caps == nil)
			error(Enomem);
		if(waserror()){
			free(caps);
			nexterror();
		}
		req[0] = Getcap;
		req[6] = req[7] = 1;
		request(req, sizeof(req), caps, 24+gpu.ncaps, Ok+3);
		memmove(caps, caps+6, gpu.ncaps);
		gpu.caps = (uchar*)caps;
		poperror();
		break;
	}
	if((lo&1) != 0 && gpu.caps == nil)
		error("gpu has no VirGL 1 capabilities");
	gpu.ready = 1;
	poperror();
}

static u32int
newid(void)
{
	if(gpu.nextid == ~0U)
		error("gpu identifiers exhausted");
	return ++gpu.nextid;
}

static void
rescmd(int op, u32int ctx, u32int id)
{
	u32int req[8];

	memset(req, 0, sizeof(req));
	req[0] = op;
	req[4] = ctx;
	req[6] = id;
	command(req, sizeof(req));
}

static void
attachbacking(Resource *r)
{
	u32int req[12];

	memset(req, 0, sizeof(req));
	req[0] = Attach;
	req[6] = r->id;
	req[7] = 1;
	req[8] = PADDR(r->data);
	req[9] = (uvlong)PADDR(r->data) >> 32;
	req[10] = r->size;
	command(req, sizeof(req));
}

static void
freefb(void)
{
	u32int req[12];

	if(waserror()){
		/* Teardown must stop DMA even if the device rejects the unref. */
		gpufail();
		free(gpu.fb.data);
		memset(&gpu.fb, 0, sizeof(gpu.fb));
		nexterror();
	}
	if(gpu.fb.id != 0 && !gpu.failed){
		memset(req, 0, sizeof(req));
		req[0] = Scanout;
		command(req, sizeof(req));
		rescmd(Unref, 0, gpu.fb.id);
	}
	poperror();
	free(gpu.fb.data);
	memset(&gpu.fb, 0, sizeof(gpu.fb));
}

/* Called by the display process with drawlock held; no interrupt-time DMA. */
void
gpudisplay(Memimage *m, Rectangle r)
{
	u32int req[14], id;
	uchar *src, *dst;
	int w, h, y, n;

	qlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	gpuprobe();
	if(m == nil){
		freefb();
		goto Out;
	}
	w = Dx(m->r);
	h = Dy(m->r);
	if(m->chan != XRGB32 || !eqpt(m->r.min, ZP)
	|| w <= 0 || h <= 0 || w > 4096 || h > 4096)
		error("virtio display requires x8r8g8b8, at most 4096x4096");
	if(gpu.fb.id == 0 || gpu.width != w || gpu.height != h){
		freefb();
		gpu.width = w;
		gpu.height = h;
		id = newid();
		gpu.fb.size = w*h*4;
		gpu.fb.data = malloc(gpu.fb.size);
		if(gpu.fb.data == nil)
			error(Enomem);
		if(waserror()){
			if(!waserror()){
				freefb();
				poperror();
			}
			nexterror();
		}
		memset(req, 0, sizeof(req));
		req[0] = Create2d;
		req[6] = id;
		req[7] = 2;	/* B8G8R8X8 */
		req[8] = w;
		req[9] = h;
		command(req, 40);
		gpu.fb.id = id;
		attachbacking(&gpu.fb);
		memset(req, 0, sizeof(req));
		req[0] = Scanout;
		req[8] = w;
		req[9] = h;
		req[11] = gpu.fb.id;
		command(req, 48);
		r = m->r;
		poperror();
	}
	if(!rectclip(&r, m->r))
		goto Out;
	n = (r.min.y*w+r.min.x)*4;
	src = byteaddr(m, r.min);
	dst = gpu.fb.data+n;
	for(y=r.min.y; y<r.max.y; y++){
		memmove(dst, src, Dx(r)*4);
		src += m->width*sizeof(ulong);
		dst += w*4;
	}
	memset(req, 0, sizeof(req));
	req[0] = Transfer;
	req[6] = r.min.x;
	req[7] = r.min.y;
	req[8] = Dx(r);
	req[9] = Dy(r);
	req[10] = n;
	req[12] = gpu.fb.id;
	command(req, sizeof(req));
	req[0] = Flush;
	req[10] = gpu.fb.id;
	command(req, 48);
Out:
	qunlock(&gpulock);
	poperror();
}

static void
freeres(Context *ctx, Resource *r)
{
	if(!gpu.failed){
		rescmd(Ctxdetach, ctx->id, r->id);
		rescmd(Unref, 0, r->id);
	}
	ctx->bytes -= r->size;
	free(r->data);
	memset(r, 0, sizeof(*r));
}

static Context*
context(Qid q)
{
	Context *ctx;
	int slot, f;

	slot = SLOT(q);
	f = FILE(q);
	if(slot < 0 || slot >= Maxctx)
		error(Enonexist);
	ctx = &gpu.ctx[slot];
	if(ctx->ref == 0)
		error(Enonexist);
	if(f < Qres){
		if(q.vers != ctx->id)
			error(Enonexist);
	}else if(f-Qres >= Maxres || ctx->res[f-Qres].id != q.vers)
		error(Enonexist);
	return ctx;
}

static Resource*
resource(Context *ctx, u32int id)
{
	int i;

	for(i=0; i<Maxres; i++)
		if(ctx->res[i].id == id && id != 0)
			return &ctx->res[i];
	error("gpu unknown resource");
}

static Chan*
gpuattach(char *spec)
{
	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	gpuprobe();
	qunlock(&gpulock);
	poperror();
	return devattach(L'∇', spec);
}

static int
gpugen(Chan *c, char*, Dirtab*, int, int s, Dir *dp)
{
	Context *ctx;
	Resource *r;
	Qid q;
	int slot;

	slot = SLOT(c->qid);
	if(s == DEVDOTDOT){
		devdir(c, (Qid){0, 0, QTDIR}, "#∇", 0, eve, 0555, dp);
		return 1;
	}
	if(slot < 0){
		if(s == 0){
			devdir(c, (Qid){Qclone, 0, 0}, "clone", 0, eve, 0660, dp);
			return 1;
		}
		if(s == 1){
			devdir(c, (Qid){Qcaps, 0, 0}, "caps", gpu.ncaps, eve, 0444, dp);
			return 1;
		}
		slot = s-2;
		if(slot >= Maxctx)
			return -1;
		ctx = &gpu.ctx[slot];
		if(ctx->ref == 0)
			return 0;
		snprint(up->genbuf, sizeof(up->genbuf), "%d", slot);
		devdir(c, (Qid){PATH(slot, Qdir), ctx->id, QTDIR}, up->genbuf, 0, eve, 0555, dp);
		return 1;
	}
	ctx = context(c->qid);
	q = (Qid){PATH(slot, Qctl+s), ctx->id, 0};
	if(s < 2){
		devdir(c, q, s == 0 ? "ctl" : "cmd", 0, eve, s == 0 ? 0660 : 0220, dp);
		return 1;
	}
	if(s-2 >= Maxres)
		return -1;
	r = &ctx->res[s-2];
	if(r->id == 0)
		return 0;
	q.vers = r->id;
	snprint(up->genbuf, sizeof(up->genbuf), "%ud", r->id);
	devdir(c, q, up->genbuf, r->size, eve, 0660, dp);
	return 1;
}

static Walkqid*
gpuwalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *w;

	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	w = devwalk(c, nc, name, nname, nil, 0, gpugen);
	qunlock(&gpulock);
	poperror();
	return w;
}

static int
gpustat(Chan *c, uchar *p, int n)
{
	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	n = devstat(c, p, n, nil, 0, gpugen);
	qunlock(&gpulock);
	poperror();
	return n;
}

static Chan*
gpuopen(Chan *c, int mode)
{
	Context *ctx;
	u32int req[24];
	int i;

	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	if(!(c->qid.type&QTDIR) && FILE(c->qid) != Qcaps && !iseve())
		error(Eperm);
	if(c->qid.path == Qclone){
		if(gpu.caps == nil)
			error("gpu requires VirGL for rendering contexts");
		if(openmode(mode) != ORDWR)
			error(Eperm);
		for(i=0; i<Maxctx && gpu.ctx[i].ref != 0; i++)
			;
		if(i == Maxctx)
			error("gpu no free contexts");
		ctx = &gpu.ctx[i];
		memset(ctx, 0, sizeof(*ctx));
		ctx->id = newid();
		memset(req, 0, sizeof(req));
		req[0] = Ctxcreate;
		req[4] = ctx->id;
		req[6] = 10;
		memmove(req+8, "generated9", 10);
		command(req, sizeof(req));
		c->qid = (Qid){PATH(i, Qctl), ctx->id, 0};
		ctx->ref = 1;
		c->mode = ORDWR;
		c->flag |= COPEN;
		c->offset = 0;
		c->aux = ctx;
	}else{
		devopen(c, mode, nil, 0, gpugen);
		if(SLOT(c->qid) >= 0){
			ctx = context(c->qid);
			ctx->ref++;
			c->aux = ctx;
		}
	}
	qunlock(&gpulock);
	poperror();
	return c;
}

static void
gpuclose(Chan *c)
{
	Context *ctx;
	u32int req[6];
	int i;

	if((c->flag&COPEN) == 0 || c->aux == nil)
		return;
	qlock(&gpulock);
	ctx = c->aux;
	c->aux = nil;
	if(--ctx->ref == 0){
		for(i=0; i<Maxres; i++){
			if(ctx->res[i].id == 0)
				continue;
			if(waserror()){
				gpufail();
				freeres(ctx, &ctx->res[i]);
				continue;
			}
			freeres(ctx, &ctx->res[i]);
			poperror();
		}
		if(!gpu.failed){
			if(waserror())
				gpufail();
			else{
				memset(req, 0, sizeof(req));
				req[0] = Ctxdestroy;
				req[4] = ctx->id;
				command(req, sizeof(req));
				poperror();
			}
		}
	}
	qunlock(&gpulock);
}

static long
readmem(vlong off, void *a, long n, void *data, ulong size)
{
	if(off < 0)
		error(Ebadarg);
	if(off >= size)
		return 0;
	if(n > size-off)
		n = size-off;
	memmove(a, (uchar*)data+off, n);
	return n;
}

static long
gpuread(Chan *c, void *a, long n, vlong off)
{
	Context *ctx;
	Resource *r;
	char buf[64];

	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	if(c->qid.type&QTDIR)
		n = devdirread(c, a, n, nil, 0, gpugen);
	else if(c->qid.path == Qcaps)
		n = readmem(off, a, n, gpu.caps, gpu.ncaps);
	else{
		ctx = context(c->qid);
		if(FILE(c->qid) == Qctl){
			snprint(buf, sizeof(buf), "%d %ud %ud\n", SLOT(c->qid), ctx->id, ctx->last);
			n = readstr(off, a, n, buf);
		}else if(FILE(c->qid) >= Qres){
			r = &ctx->res[FILE(c->qid)-Qres];
			n = readmem(off, a, n, r->data, r->size);
		}else
			error(Eperm);
	}
	qunlock(&gpulock);
	poperror();
	return n;
}

static ulong
number(char *s)
{
	uvlong v;
	char *e;

	v = strtoull(s, &e, 0);
	if(*s == 0 || *s == '-' || *e != 0 || v > 0xffffffffULL)
		error(Ebadarg);
	return v;
}

static void
control(Context *ctx, Cmdbuf *cb)
{
	Resource *r;
	u32int req[18], v[12];
	int i, op;

	if(cb->nf < 2 || cb->nf > nelem(v)+1)
		error(Ebadarg);
	for(i=1; i<cb->nf; i++)
		v[i-1] = number(cb->f[i]);
	memset(req, 0, sizeof(req));
	if(strcmp(cb->f[0], "resource") == 0){
		/* target format bind width height depth array levels samples flags bytes */
		if(cb->nf != 12 || v[10] == 0 || v[10] > Maxmem-ctx->bytes)
			error(Ebadarg);
		if(v[0] > 8 || v[3] == 0 || v[3] > (v[0] == 0 ? Maxmem : 16384)
		|| v[4] == 0 || v[4] > 16384 || v[5] == 0 || v[5] > 16384
		|| v[6] == 0 || v[6] > 2048 || v[7] > 14 || v[8] > 64 || (v[9]&~1) != 0)
			error(Ebadarg);
		for(i=0; i<Maxres && ctx->res[i].id != 0; i++)
			;
		if(i == Maxres)
			error("gpu no free resources");
		r = &ctx->res[i];
		r->id = newid();
		r->size = v[10];
		r->data = nil;
		if(waserror()){
			free(r->data);
			memset(r, 0, sizeof(*r));
			nexterror();
		}
		r->data = malloc(r->size);
		if(r->data == nil)
			error(Enomem);
		req[0] = Rescreate;
		req[6] = r->id;
		memmove(req+7, v, 10*4);
		command(req, sizeof(req));
		if(waserror()){
			if(!gpu.failed){
				if(waserror())
					gpufail();
				else{
					rescmd(Unref, 0, r->id);
					poperror();
				}
			}
			nexterror();
		}
		attachbacking(r);
		rescmd(Ctxattach, ctx->id, r->id);
		ctx->bytes += r->size;
		ctx->last = r->id;
		poperror();
		poperror();
		return;
	}
	r = resource(ctx, v[0]);
	if(strcmp(cb->f[0], "unref") == 0 && cb->nf == 2){
		freeres(ctx, r);
		return;
	}
	if(strcmp(cb->f[0], "upload") == 0)
		op = Upload;
	else if(strcmp(cb->f[0], "download") == 0)
		op = Download;
	else
		error(Ebadarg);
	/* id level x y z w h d offset stride layerstride */
	if(cb->nf != 12 || v[8] >= r->size)
		error(Ebadarg);
	req[0] = op;
	req[4] = ctx->id;
	memmove(req+6, v+2, 6*4);
	req[12] = v[8];
	req[14] = r->id;
	req[15] = v[1];
	req[16] = v[9];
	req[17] = v[10];
	command(req, sizeof(req));
}

static long
gpuwrite(Chan *c, void *a, long n, vlong off)
{
	Context *ctx;
	Resource *r;
	Cmdbuf *cb;
	u32int *req;
	int f;

	eqlock(&gpulock);
	if(waserror()){
		qunlock(&gpulock);
		nexterror();
	}
	ctx = context(c->qid);
	f = FILE(c->qid);
	if(f == Qctl){
		if(n > 512)
			error(Etoobig);
		cb = parsecmd(a, n);
		if(waserror()){
			free(cb);
			nexterror();
		}
		control(ctx, cb);
		free(cb);
		poperror();
	}else if(f == Qcmd){
		if(n <= 0 || (n&3) != 0 || n > Maxcmd)
			error(Ebadarg);
		/* gpulock and the fence keep this single staging buffer private. */
		if(gpu.command == nil){
			gpu.command = mallocz(32+Maxcmd, 0);
			if(gpu.command == nil)
				error(Enomem);
		}
		req = gpu.command;
		memset(req, 0, 32);
		req[0] = Submit;
		req[4] = ctx->id;
		req[6] = n;
		memmove(req+8, a, n);
		command(req, 32+n);
	}else if(f >= Qres){
		r = &ctx->res[f-Qres];
		if(off < 0 || off > r->size || n > r->size-off)
			error(Ebadarg);
		memmove(r->data+off, a, n);
	}else
		error(Eperm);
	qunlock(&gpulock);
	poperror();
	return n;
}

Dev gpudevtab = {
	L'∇',
	"gpu",
	devreset,
	devinit,
	devshutdown,
	gpuattach,
	gpuwalk,
	gpustat,
	gpuopen,
	devcreate,
	gpuclose,
	gpuread,
	devbread,
	gpuwrite,
	devbwrite,
	devremove,
	devwstat,
};
