#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "fns.h"
#include "virtio.h"

/* VirtIO 1.2, section 5.14. One output stream in audio(3)'s native format. */
enum {
	Jackinfo = 1,
	Pcminfo = 0x100,
	Setparams,
	Prepare,
	Release,
	Start,
	Stop,
	Chmapinfo = 0x200,

	Ok = 0x8000,
	Badmsg,
	Notsupp,
	Ioerr,

	S16 = 5,
	Hz44100 = 6,
	Maxperiod = 64*1024,
	Maxbuffer = 1024*1024,

	Idle = 0,
	Configured,
	Prepared,
	Running,
	Stopped,
};

typedef struct Vsnd Vsnd;
struct Vsnd {
	QLock;
	Channel *kick;
	char *path;
	int fd, state;
	u32int period;
};

static void
sndclose(Vsnd *s)
{
	if(s->fd >= 0) close(s->fd);
	s->fd = -1;
}

static void
sndstatus(VIOBuf *b, u32int status, u32int latency)
{
	uchar r[8];

	PUT32(r,0,status);
	PUT32(r,4,latency);
	vioqwrite(b,r,sizeof r);
}

static int
snddrain(VIODev *d)
{
	VIOBuf *b;
	int irq, i;

	irq = 0;
	for(i = 0; i < d->qu[2].size; i++){
		b = viogetbuf(d->qu+2,0);
		if(b == nil) break;
		sndstatus(b,Ok,0);
		irq |= viopublish(b);
	}
	return irq;
}

static int
sndcontrol(VIODev *d, VIOBuf *b)
{
	Vsnd *s;
	uchar h[24], r[36];
	u32int cmd, status, start, count, size, buffer, period;
	int n, nr, irq;

	s = d->aux;
	irq = 0;
	nr = 4;
	status = Badmsg;
	memset(r,0,sizeof r);
	n = vioqread(b,h,sizeof h);
	if(n < 4 || vioqrem(b,1) < 4) goto out;
	cmd = GET32(h,0);
	switch(cmd){
	case Jackinfo:
	case Pcminfo:
	case Chmapinfo:
		if(n != 16) break;
		start = GET32(h,4);
		count = GET32(h,8);
		size = cmd == Pcminfo ? 32 : 24;
		if(cmd == Jackinfo){
			if(start == 0 && count == 0) status = Ok;
			break;
		}
		if(start > 1 || count > 1-start || GET32(h,12) != size
		|| vioqrem(b,1) < 4+count*size) break;
		status = Ok;
		if(count == 0) break;
		nr += size;
		if(cmd == Pcminfo){
			PUT64(r,12,1ULL<<S16);
			PUT64(r,20,1ULL<<Hz44100);
			r[29] = r[30] = 2;
		}else{
			r[9] = 2;
			r[10] = 3;	/* front left */
			r[11] = 4;	/* front right */
		}
		break;
	case Setparams:
		if(n != 24 || GET32(h,4) != 0 || s->state == Running
		|| s->state == Stopped) break;
		buffer = GET32(h,8);
		period = GET32(h,12);
		if(period == 0 || period > Maxperiod || (period & 3) != 0
		|| buffer < period || buffer > Maxbuffer || buffer % period != 0) break;
		if(GET32(h,16) != 0 || h[20] != 2 || h[21] != S16 || h[22] != Hz44100){
			status = Notsupp;
			break;
		}
		sndclose(s);
		s->period = period;
		s->state = Configured;
		status = Ok;
		break;
	case Prepare:
		if(n != 8 || GET32(h,4) != 0
		|| (s->state != Configured && s->state != Prepared)) break;
		s->state = Prepared;
		status = Ok;
		break;
	case Start:
		if(n != 8 || GET32(h,4) != 0
		|| (s->state != Prepared && s->state != Stopped)) break;
		if(s->fd < 0 && (s->fd = open(s->path,OWRITE)) < 0){
			vmerror("virtio sound: open %s: %r",s->path);
			status = Ioerr;
			break;
		}
		s->state = Running;
		status = Ok;
		break;
	case Stop:
		if(n != 8 || GET32(h,4) != 0 || s->state != Running) break;
		s->state = Stopped;
		status = Ok;
		break;
	case Release:
		if(n != 8 || GET32(h,4) != 0
		|| (s->state != Prepared && s->state != Stopped)) break;
		/* Return queued periods before the release response, with one IRQ. */
		irq = snddrain(d);
		sndclose(s);
		s->state = Configured;
		status = Ok;
		break;
	default:
		status = Notsupp;
		break;
	}
out:
	PUT32(r,0,status);
	vioqwrite(b,r,nr);
	return irq;
}

static void
sndplay(VIODev *d, VIOBuf *b)
{
	Vsnd *s;
	Dir *dir;
	uchar buf[4096];
	u32int n, latency;
	int k, m, off;

	s = d->aux;
	if(vioqrem(b,1) < 8 || vioqread(b,buf,4) != 4 || GET32(buf,0) != 0)
		goto bad;
	n = vioqrem(b,0);
	if(n == 0 || n > s->period || (n & 3) != 0 || s->fd < 0)
		goto bad;
	while(n != 0){
		if((d->devstat & DRIVEROK) == 0) goto bad;
		m = n < sizeof buf ? n : sizeof buf;
		if(vioqread(b,buf,m) != m) goto bad;
		for(off = 0; off < m; off += k){
			k = write(s->fd,buf+off,m-off);
			if(k <= 0){
				vmerror("virtio sound: write %s: %r",s->path);
				sndclose(s);
				goto bad;
			}
		}
		n -= m;
	}
	latency = 0;
	dir = dirfstat(s->fd);
	if(dir != nil){
		if(dir->length > 0 && dir->length <= Maxbuffer)
			latency = dir->length;
		free(dir);
	}
	sndstatus(b,Ok,latency);
	return;
bad:
	sndstatus(b,Ioerr,0);
}

static void
sndproc(void *arg)
{
	VIODev *d;
	Vsnd *s;
	VIOBuf *b;
	int irq;

	d = arg;
	s = d->aux;
	threadsetname("virtio sound");
	for(;;){
		qlock(s);
		irq = 0;
		b = viogetbuf(d->qu,0);
		if(b != nil)
			irq = sndcontrol(d,b);
		else if((b = viogetbuf(d->qu+3,0)) != nil)
			sndstatus(b,Ioerr,0);
		else if(s->state == Running && (b = viogetbuf(d->qu+2,0)) != nil)
			sndplay(d,b);
		qunlock(s);
		if(b == nil){
			recvul(s->kick);
			continue;
		}
		irq |= viopublish(b);
		/* No locks or live buffers may be held while notifying the monitor. */
		if(irq) vionotify(d);
	}
}

static void
sndkick(VIOQueue *q)
{
	Vsnd *s;

	s = q->d->aux;
	nbsendul(s->kick,1);
}

static void
sndreset(VIODev *d)
{
	Vsnd *s;

	s = d->aux;
	qlock(s);
	sndclose(s);
	s->state = Idle;
	s->period = 0;
	qunlock(s);
}

static u32int
sndconfig(int isin, u16int port, u32int, int sz, VIODev*)
{
	uchar c[12];
	u32int v;
	int i;

	memset(c,0,sizeof c);
	PUT32(c,4,1);	/* one stream */
	PUT32(c,8,1);	/* one channel map */
	v = 0;
	if(isin)
		for(i = 0; i < sz && port+i < sizeof c; i++)
			v |= (u32int)c[port+i] << (8*i);
	return v;
}

int
mkviosnd(char *path)
{
	VIODev *d;
	Vsnd *s;
	Dir *dir;
	int i;

	dir = dirstat(path);
	if(dir == nil) return -1;
	free(dir);
	s = emalloc(sizeof *s);
	s->path = strdup(path);
	s->fd = -1;
	s->kick = chancreate(sizeof(ulong),1);
	d = mkviodev(0x1059,0x040100,25,4);
	d->aux = s;
	d->io = sndconfig;
	d->reset = sndreset;
	viomodern(d);
	for(i = 0; i < 4; i++)
		mkvioqueue(d,128,sndkick);
	proccreate(sndproc,d,16384);
	return 0;
}
