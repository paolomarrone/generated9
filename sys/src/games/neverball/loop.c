/* Native event loop shared by Neverball and Neverputt. */
#include "port.h"
#include <draw.h>
#include <event.h>
#include "audio.h"
#include "config.h"
#include "state.h"
#include "text.h"
#include "video.h"

static int running;

void game_quit(void) { running = 0; }

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
	if(key == Kbs || key == Kdel) return 1;
	if(key == '\n')
		return st_buttn(config_get_d(CONFIG_JOYSTICK_BUTTON_A),down);
	if(key == Kesc)
		return st_keybd(key,down);
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

	if(*s == 'c'){
		Rune r;
		chartorune(&r, s+1);
		if(r == Kbs || r == Kdel) return st_keybd(r,1);
		else if(r >= ' ' && r != Kdel && (r < KF || r >= Spec+0x100)) text_input_char(r);
		return 1;
	}
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

void
game_loop(int limit)
{
	Event e;
	char *p, *end;
	ulong kind, keys, timer;
	vlong last, now, start;
	Point prev, center;
	int fd, alive, buttons, i, x, y, dx, dy, grab, frames;

	fd = open("/dev/kbd", OREAD);
	if(fd < 0) sysfatal("open keyboard: %r");
	keys = estart(4, fd, 256);
	timer = etimer(8, 16);
	running = 1;
	buttons = grab = frames = 0;
	prev = Pt(-1,-1);
	alive = 1;
	last = start = uptime();
	while(alive && running){
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
			st_point(x,y,dx,config_get_d(CONFIG_MOUSE_INVERT) ? dy : -dy);
			if((e.mouse.buttons & 8) && !(buttons & 8)) st_wheel(0,1);
			if((e.mouse.buttons & 16) && !(buttons & 16)) st_wheel(0,-1);
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
	fprint(2,"%s: %d frames, %.1f fps\n", argv0,frames,frames*1e9/(uptime()-start));
	close(fd);
}
