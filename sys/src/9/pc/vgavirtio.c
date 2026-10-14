#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"

#define Image IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"

extern VGAdev vgavirtiodev;

static QLock displaylock;	/* serialize presentation with enable/disable */

static struct {
	Lock;
	Rendez wait;
	Rectangle dirty;
	int pending, enabled, started;
} fb;

static int
pending(void*)
{
	return fb.pending;
}

static void
virtioproc(void*)
{
	VGAscr *scr;
	Rectangle r;
	int enabled;
	char err[ERRMAX];

	scr = &vgascreen[0];
	for(;;){
		sleep(&fb.wait, pending, nil);
		qlock(&displaylock);
		ilock(&fb);
		r = fb.dirty;
		fb.dirty = ZR;
		fb.pending = 0;
		enabled = fb.enabled;
		iunlock(&fb);
		if(waserror()){
			kstrcpy(err, up->errstr, sizeof(err));
			ilock(&fb);
			fb.enabled = 0;
			iunlock(&fb);
			qunlock(&displaylock);
			print("virtio display: %s\n", err);
			continue;
		}
		if(enabled && scr->dev == &vgavirtiodev)
			gpudisplay(scr, r);
		else
			gpudisplay(nil, ZR);
		qunlock(&displaylock);
		poperror();
	}
}

/* Screen flushes also occur in interrupt and kernel-console paths. */
static void
virtioflush(VGAscr*, Rectangle r)
{
	if(badrect(r) || Dx(r) == 0 || Dy(r) == 0)
		return;
	ilock(&fb);
	if(fb.enabled){
		if(fb.pending)
			combinerect(&fb.dirty, r);
		else
			fb.dirty = r;
		fb.pending = 1;
	}
	iunlock(&fb);
	wakeup(&fb.wait);
}

static void
virtiodrawinit(VGAscr *scr)
{
	scr->useflush = 1;
	if(scr->gscreen != nil)
		virtioflush(scr, scr->gscreen->r);
}

static void
virtioenable(VGAscr *scr)
{
	int start;

	qlock(&displaylock);
	qlock(&drawlock);
	scr->softscreen = 1;
	ilock(&fb);
	fb.enabled = 1;
	start = !fb.started;
	fb.started = 1;
	iunlock(&fb);
	virtiodrawinit(scr);
	qunlock(&drawlock);
	qunlock(&displaylock);
	if(start)
		kproc("virtiodisplay", virtioproc, nil);
}

static void
virtiodisable(VGAscr*)
{
	qlock(&displaylock);
	qlock(&drawlock);
	ilock(&fb);
	fb.enabled = 0;
	fb.pending = 1;
	iunlock(&fb);
	qunlock(&drawlock);
	qunlock(&displaylock);
	wakeup(&fb.wait);
}

VGAdev vgavirtiodev = {
	"virtio",
	virtioenable,
	virtiodisable,
	0,
	0,
	virtiodrawinit,
	0,
	0,
	0,
	virtioflush,
};
