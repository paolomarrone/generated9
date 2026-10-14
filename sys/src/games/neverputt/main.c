/*
 * Copyright (C) 2003 Robert Kooima
 *
 * NEVERPUTT is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

/* Native event loop adapted from putt/main.c. */
#include "port.h"
#include <draw.h>
#include <event.h>
#include "audio.h"
#include "config.h"
#include "fs.h"
#include "game.h"
#include "hole.h"
#include "mtrl.h"
#include "state.h"
#include "st_all.h"
#include "st_conf.h"
#include "video.h"

static Rune held[64];
static int nheld;

int
key_shift(void)
{
	int i;

	for(i=0; i<nheld; i++)
		if(held[i] == Kshift)
			return 1;
	return 0;
}

static int
keydown(int key, int down)
{
	if(key == '\n')
		return st_buttn(config_get_d(CONFIG_JOYSTICK_BUTTON_A),down);
	if(key == Kesc)
		return st_buttn(config_get_d(video_get_grab() ? CONFIG_JOYSTICK_BUTTON_START : CONFIG_JOYSTICK_BUTTON_B),down);
	if(key == (KF|9) && down){
		config_tgl_d(CONFIG_FPS);
		return 1;
	}
	if(key == (KF|12) && down){
		char path[64];
		snprint(path,sizeof(path),"Screenshots/screen%05d.png",config_screenshot());
		video_snap(path);
		return 1;
	}
	if(config_tst_d(CONFIG_KEY_FORWARD,key))
		st_stick(config_get_d(CONFIG_JOYSTICK_AXIS_Y0),down ? -1 : 0);
	else if(config_tst_d(CONFIG_KEY_BACKWARD,key))
		st_stick(config_get_d(CONFIG_JOYSTICK_AXIS_Y0),down ? 1 : 0);
	else if(config_tst_d(CONFIG_KEY_LEFT,key))
		st_stick(config_get_d(CONFIG_JOYSTICK_AXIS_X0),down ? -1 : 0);
	else if(config_tst_d(CONFIG_KEY_RIGHT,key))
		st_stick(config_get_d(CONFIG_JOYSTICK_AXIS_X0),down ? 1 : 0);
	else
		return st_keybd(key,down);
	return 1;
}

static int
keyboard(char *s)
{
	Rune now[64];
	int n, i, j, alive;

	if(*s != 'k' && *s != 'K')
		return 1;
	n = 0;
	for(s++; *s && n<nelem(now); n++)
		s += chartorune(&now[n],s);
	alive = 1;
	for(i=0; i<nheld; i++){
		for(j=0; j<n && now[j]!=held[i]; j++) {}
		if(j == n) alive &= keydown(held[i],0);
	}
	for(i=0; i<n; i++){
		for(j=0; j<nheld && held[j]!=now[i]; j++) {}
		if(j == nheld) alive &= keydown(now[i],1);
	}
	memmove(held,now,n*sizeof(Rune));
	nheld = n;
	return alive;
}

static void
usage(void)
{
	fprint(2,"usage: games/neverputt [-d data-directory] [-h hole.sol] [-n frames]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	Event e;
	char *data, *hole, *p, *end;
	const char *path;
	ulong kind, keys, timer;
	vlong last, now, start;
	Point prev, center;
	int fd, alive, buttons, i, x, y, dx, dy, grab, frames, limit;

	data = hole = nil;
	limit = 0;
	ARGBEGIN{
	case 'd': data = EARGF(usage()); break;
	case 'h': hole = EARGF(usage()); break;
	case 'n': limit = atoi(EARGF(usage())); if(limit < 1) usage(); break;
	default: usage();
	}ARGEND
	if(argc) usage();
	fs_init(argv0);
	config_paths(data);
	config_init();
	config_load();
	/* rio owns the window; this backend provides a single classic GL target. */
	config_set_d(CONFIG_FULLSCREEN,0);
	config_set_d(CONFIG_REFLECTION,0);
	config_set_d(CONFIG_MULTISAMPLE,0);
	config_set_d(CONFIG_HMD,0);
	log_init("Neverputt","neverputt.log");
	fs_mkdir("Screenshots");
	lang_init();
	srand(time(nil));
	if(!video_init()) sysfatal("video initialization failed");
	einit(Emouse);
	fd = open("/dev/kbd",OREAD);
	if(fd < 0) sysfatal("open keyboard: %r");
	keys = estart(4,fd,256);
	timer = etimer(8,16);
	audio_init();
	mtrl_init();
	init_state(&st_null);
	path = hole == nil ? nil : fs_resolve(hole);
	if(path != nil){
		hole_init(nil);
		if(!hole_load(0,path) || !hole_load(1,path) || !hole_goto(1,1))
			sysfatal("load hole %s",path);
		goto_state(&st_next);
	}else if(hole != nil)
		sysfatal("hole not found: %s",hole);
	else
		goto_state(&st_title);
	buttons = grab = frames = 0;
	prev = Pt(-1,-1);
	alive = 1;
	last = start = uptime();
	while(alive){
		kind = event(&e);
		if(kind == keys){
			p = (char*)e.data;
			end = p+e.n;
			/* A keyboard read may contain several NUL-terminated records. */
			while(p<end){
				char *z = memchr(p,0,end-p);
				if(z == nil) sysfatal("incomplete keyboard record");
				alive &= keyboard(p);
				p = z+1;
			}
		}else if(kind == Emouse){
			if(prev.x == -1) prev = e.mouse.xy;
			dx = e.mouse.xy.x-prev.x;
			dy = e.mouse.xy.y-prev.y;
			if(video_get_grab() && !grab)
				dx = dy = 0;
			prev = e.mouse.xy;
			x = e.mouse.xy.x-screen->r.min.x;
			y = e.mouse.xy.y-screen->r.min.y;
			video_point(&x,&y);
			st_point(x,y,dx,-dy);
			for(i=0; i<3; i++)
				if((buttons^e.mouse.buttons)&(1<<i))
					alive &= st_click(i+1,(e.mouse.buttons>>i)&1);
			buttons = e.mouse.buttons;
			if(video_get_grab() && (!grab || !ptinrect(prev,insetrect(screen->r,32)))){
				center = divpt(addpt(screen->r.min,screen->r.max),2);
				emoveto(center);
				prev = center;
			}
			grab = video_get_grab();
		}
		if(kind != timer || !alive)
			continue;
		video_prepare();
		audio_check();
		now = uptime();
		st_timer(now-last > 250000000 ? .25 : (now-last)*1e-9);
		last = now;
		st_paint((now-start)*1e-9);
		video_swap();
		if(++frames == limit)
			break;
	}
	fprint(2,"Neverputt: %d frames, %.1f fps\n",frames,frames*1e9/(uptime()-start));
	audio_free();
	goto_state(&st_null);
	mtrl_quit();
	config_save();
	log_quit();
	fs_quit();
	video_quit();
	exits(nil);
}
