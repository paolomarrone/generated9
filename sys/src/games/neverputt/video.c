/* Native rio window and TinyGL binding. */
#include "port.h"
#include <draw.h>
#include <memdraw.h>
#include <event.h>
#include <cursor.h>
#include <tinygl.h>
#include "video.h"
#include "config.h"
#include "glext.h"
#include "gui.h"
#include "image.h"

struct video video;
static Image *picture;
static uchar *pixels;
static int initialized, grabbed, resizepending, fps;
static Rectangle content;
static Cursor invisible;
static char snapshot[256];

void
eresized(int new)
{
	if(new && getwindow(display, Refnone) < 0)
		sysfatal("get window: %r");
	resizepending = 1;
}

static void
resize(void)
{
	int w, h, rw, rh;

	w = Dx(screen->r);
	h = Dy(screen->r);
	if(w < 1 || h < 1)
		return;
	if(w > 2048) w = 2048;
	if(h > 2048) h = 2048;
	video.window_w = w;
	video.window_h = h;
	tglresize(w,h);
	freeimage(picture);
	picture = allocimage(display, Rect(0,0,w,h), XRGB32, 0, DNofill);
	free(pixels);
	pixels = malloc(w*h*4);
	if(picture == nil || pixels == nil)
		sysfatal("frame allocation: %r");
	/* Preserve the game's logical canvas across rio window changes. */
	rw = w;
	rh = (vlong)w*video.device_h/video.device_w;
	if(rh > h){
		rh = h;
		rw = (vlong)h*video.device_w/video.device_h;
	}
	if(rw < 1) rw = 1;
	if(rh < 1) rh = 1;
	content = Rect((w-rw)/2,(h-rh)/2,(w+rw)/2,(h+rh)/2);
	glViewport(content.min.x,h-content.max.y,Dx(content),Dy(content));
	resizepending = 0;
}

int
video_init(void)
{
	if(initdraw(nil,nil,"Neverputt") < 0)
		sysfatal("initdraw: %r");
	memimageinit();
	return video_mode(0,config_get_d(CONFIG_WIDTH),config_get_d(CONFIG_HEIGHT));
}

int
video_mode(int fullscreen, int w, int h)
{
	USED(fullscreen);
	if(w < 1 || h < 1 || w > 2048 || h > 2048)
		return 0;
	video.device_w = w;
	video.device_h = h;
	video.device_scale = 1;
	config_set_d(CONFIG_WIDTH,w);
	config_set_d(CONFIG_HEIGHT,h);
	config_set_d(CONFIG_FULLSCREEN,0);
	if(!initialized){
		tglinit(nil, w, h);
		initialized = 1;
		glext_init();
		glClearColor(0,0,0,1);
		glEnable(GL_NORMALIZE);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_LIGHTING);
		glEnable(GL_BLEND);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glDepthFunc(GL_LEQUAL);
	}
	resize();
	return 1;
}

void
video_point(int *x, int *y)
{
	*x = (*x-content.min.x)*video.device_w/Dx(content);
	*y = (content.max.y-*y)*video.device_h/Dy(content);
}

void
video_swap(void)
{
	static vlong last;
	static int frames;
	vlong now;
	int bytes;

	bytes = video.window_w*video.window_h*4;
	tglread(pixels,bytes);
	if(loadimage(picture,picture->r,pixels,bytes) < 0)
		sysfatal("load frame: %r");
	drawop(screen,rectaddpt(picture->r,screen->r.min),picture,nil,ZP,S);
	if(flushimage(display,1) < 0)
		sysfatal("present frame: %r");
	if(snapshot[0]){
		image_snap(snapshot);
		snapshot[0] = 0;
	}
	frames++;
	now = nsec();
	if(last == 0) last = now;
	if(now-last >= 1000000000LL){
		fps = frames*1e9/(now-last)+.5;
		frames = 0;
		last = now;
	}
}

void
video_prepare(void)
{
	if(resizepending) resize();
}

void
video_quit(void)
{
	tglclose();
	freeimage(picture);
	free(pixels);
}

int video_perf(void) { return fps; }
int video_display(void) { return 0; }
void video_snap(const char *path) { snprint(snapshot,sizeof(snapshot),"%s",path); }
void video_show_cursor(void) { gui_set_cursor(0); esetcursor(nil); }
void video_hide_cursor(void) { gui_set_cursor(0); esetcursor(&invisible); }
void video_set_grab(int warp) { USED(warp); grabbed=1; video_hide_cursor(); }
void video_clr_grab(void) { grabbed=0; video_show_cursor(); }
int video_get_grab(void) { return grabbed; }
