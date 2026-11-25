#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include "beat.h"

static Pattern *pat;
static Image *paper, *ink, *rest, *hit, *cursor;
static Rectangle grid, playbox, savebox, slower, faster;
static int fd = -1, col = -1, rem, dirty, playerpid;
static char msg[ERRMAX];
static vlong next;

static Image*
colour(ulong c)
{
	Image *i;

	i = allocimage(display, Rect(0,0,1,1), RGB24, 1, c);
	if(i == nil)
		sysfatal("allocimage: %r");
	return i;
}

static Rectangle
button(Point *p, char *s)
{
	Rectangle r;

	r = Rpt(*p, addpt(*p, Pt(stringwidth(font, s)+12, font->height+8)));
	draw(screen, r, rest, nil, ZP);
	string(screen, addpt(*p, Pt(6,4)), ink, ZP, font, s);
	p->x = r.max.x+4;
	return r;
}

static Rectangle
cell(int i, int j)
{
	return Rect(grid.min.x+i*Dx(grid)/Nstep, grid.min.y+j*Dy(grid)/Nvoice,
		grid.min.x+(i+1)*Dx(grid)/Nstep, grid.min.y+(j+1)*Dy(grid)/Nvoice);
}

static void
playhead(void)
{
	Rectangle r;

	if(Dx(grid) == 0)
		return;
	r = Rect(grid.min.x, grid.max.y+1, grid.max.x, grid.max.y+4);
	draw(screen, r, paper, nil, ZP);
	if(col >= 0){
		r.min.x = cell(col,0).min.x+1;
		r.max.x = cell(col,0).max.x;
		draw(screen, r, cursor, nil, ZP);
	}
}

void
eresized(int new)
{
	Rectangle r, c;
	Point p;
	char s[32];
	int i, j, h;

	if(new && getwindow(display, Refnone) < 0)
		sysfatal("resize: %r");
	draw(screen, screen->r, paper, nil, ZP);
	r = insetrect(screen->r, 8);
	p = r.min;
	playbox = button(&p, fd < 0 ? "Play" : "Stop");
	savebox = button(&p, dirty ? "Save*" : "Save ");
	slower = button(&p, "-");
	faster = button(&p, "+");
	snprint(s, sizeof s, "%d bpm", pat->bpm);
	string(screen, addpt(p, Pt(4,4)), ink, ZP, font, s);
	h = font->height;
	grid = Rect(r.min.x+stringwidth(font, "snare")+8, playbox.max.y+h+8,
		r.max.x, r.max.y-h-8);
	if(Dy(grid) > 4*(h+16))
		grid.max.y = grid.min.y+4*(h+16);
	if(Dx(grid) < 3*Nstep || Dy(grid) < Nvoice*4)
		grid.max = grid.min;
	else{
		grid.max.x -= Dx(grid)%Nstep;
		grid.max.y -= Dy(grid)%Nvoice;
		for(j = 0; j < Nvoice; j++){
			c = cell(0,j);
			string(screen, Pt(r.min.x, (c.min.y+c.max.y-h)/2), ink, ZP, font, voice[j]);
			for(i = 0; i < Nstep; i++){
				c = cell(i,j);
				c.min.x += i%4 == 0 && Dx(c) > 6 ? 3 : 1;
				c.min.y++;
				draw(screen, c, pat->row[j] & 1<<i ? hit : rest, nil, ZP);
			}
		}
		for(i = 0; i < Nstep; i += 4){
			c = cell(i,0);
			snprint(s, sizeof s, "%d", i/4+1);
			string(screen, Pt(c.min.x+2, grid.min.y-h-2), ink, ZP, font, s);
		}
		playhead();
	}
	string(screen, Pt(r.min.x, grid.max.y+7), ink, ZP, font, msg);
	flushimage(display, 1);
}

static int
player(void)
{
	int audio, p[2];

	if(pipe(p) < 0)
		return -1;
	audio = open("/dev/audio", OWRITE);
	if(audio < 0)
		goto error;
	playerpid = fork();
	if(playerpid == 0){
		close(p[1]);
		if(dup(p[0], 0) < 0 || dup(audio, 1) < 0)
			sysfatal("dup: %r");
		if(p[0] > 1)
			close(p[0]);
		close(audio);
		execl("/bin/games/opl3", "opl3", nil);
		sysfatal("exec games/opl3: %r");
	}
	close(audio);
	if(playerpid < 0)
		goto error;
	close(p[0]);
	return p[1];
error:
	close(p[0]);
	close(p[1]);
	return -1;
}

static void
stop(void)
{
	Waitmsg *w;

	close(fd);
	fd = -1;
	col = -1;
	if(playerpid <= 0)
		return;
	while((w = wait()) != nil){
		if(w->pid == playerpid){
			if(w->msg[0])
				snprint(msg, sizeof msg, "opl3: %s", w->msg);
			free(w);
			break;
		}
		free(w);
	}
	playerpid = 0;
}

static int
pipenote(void*, char *s)
{
	return strcmp(s, "sys: write on closed pipe") == 0;
}

static void
save(char *name)
{
	int f;

	f = open(name, OWRITE);
	if(f < 0 || putpattern(f, pat) < 0)
		snprint(msg, sizeof msg, "save: %r");
	else{
		dirty = 0;
		msg[0] = 0;
	}
	if(f >= 0)
		close(f);
}

static void
tick(void)
{
	int n;
	vlong now, dt;

	now = uptime();
	col = (col+1)%Nstep;
	n = step(fd, pat, col, &rem);
	if(n < 0){
		snprint(msg, sizeof msg, "opl3: %r");
		stop();
		eresized(0);
		return;
	}
	dt = (vlong)n*1000000000/Oplrate;
	if(now-next >= dt)
		next = now;
	next += dt;
	playhead();
	flushimage(display, 1);
}

void
edit(char *name, Pattern *p, int piped)
{
	Event e;
	Point xy;
	ulong eventid;
	int buttons, key, i, j, n, ms;

	pat = p;
	if(initdraw(nil, nil, name) < 0)
		sysfatal("initdraw: %r");
	paper = colour(0xf4f0e6ff);
	ink = colour(0x182e31ff);
	rest = colour(0xd4dcd3ff);
	hit = colour(0x24726bff);
	cursor = colour(0xbf5228ff);
	einit(Emouse|Ekeyboard);
	atnotify(pipenote, 1);
	buttons = 0;
	eresized(0);
	for(;;){
		if(fd >= 0){
			if(uptime() >= next)
				tick();
			if(fd >= 0 && !ecanread(Emouse|Ekeyboard)){
				ms = (next-uptime()+999999)/1000000;
				if(ms > 0)
					sleep(ms < 10 ? ms : 10);
				continue;
			}
		}
		eventid = event(&e);
		key = 0;
		if(eventid == Ekeyboard)
			key = e.kbdc;
		else if(eventid == Emouse){
			xy = e.mouse.xy;
			n = e.mouse.buttons & ~buttons;
			buttons = e.mouse.buttons;
			if(!(n & 1))
				continue;
			if(ptinrect(xy, playbox)) key = ' ';
			else if(ptinrect(xy, savebox)) key = 's';
			else if(ptinrect(xy, slower)) key = '-';
			else if(ptinrect(xy, faster)) key = '+';
			else if(ptinrect(xy, grid)){
				i = (xy.x-grid.min.x)/(Dx(grid)/Nstep);
				j = (xy.y-grid.min.y)/(Dy(grid)/Nvoice);
				pat->row[j] ^= 1<<i;
				dirty = 1;
				msg[0] = 0;
			}else
				continue;
		}
		switch(key){
		case 'q':
			if(dirty){
				strcpy(msg, "s to save, Q to discard");
				break;
			}
			/* fall through */
		case 'Q':
			if(fd >= 0)
				stop();
			return;
		case 's':
			save(name);
			break;
		case '+':
		case '-':
			n = pat->bpm + (key == '+' ? 1 : -1);
			if(n >= Minbpm && n <= Maxbpm){
				pat->bpm = n;
				rem = 0;
				dirty = 1;
				msg[0] = 0;
			}
			break;
		case ' ':
			msg[0] = 0;
			if(fd >= 0)
				stop();
			else if((fd = piped ? dup(1, -1) : player()) < 0)
				snprint(msg, sizeof msg, "audio: %r");
			else if(kit(fd) < 0){
				snprint(msg, sizeof msg, "opl3: %r");
				stop();
			}else{
				next = uptime();
				rem = 0;
			}
			break;
		}
		eresized(0);
	}
}
