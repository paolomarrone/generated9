#include <u.h>
#include <libc.h>
#include "../pinball.h"

static void
check(int ok, char *what)
{
	if(!ok)
		sysfatal("%s",what);
}

static void
ticks(Game *g, int n, int keys)
{
	while(n-- > 0)
		step(g,keys);
}

static void
place(Game *g, double x, double y, double vx, double vy)
{
	g->state = Play;
	g->p = (V){x,y};
	g->v = (V){vx,vy};
}

void
main(void)
{
	Game g;
	V a;
	uint seed;
	vlong score;
	double speed, left;
	int i, j, keys, games, pops, ramps, missions, cleared;

	tableinit();
	newgame(&g);
	check(g.state == Ready && g.ball == 1,"new game");
	ticks(&g,Hz,Launch);
	check(g.state == Ready && g.charge > .99,"charge spring");
	step(&g,0);
	check(g.state == Play && g.v.y < -1300 && g.save > 0,"launch on release");
	ticks(&g,Hz*2,0);
	check(g.p.x < 526,"launch enters playfield");
	newgame(&g);
	place(&g,537,155,-500,0);
	ticks(&g,16,0);
	check(g.p.x < 526 && g.v.x < 0,"launch gate opens inward");
	place(&g,513,155,500,0);
	step(&g,0);
	check(g.v.x < 0,"launch gate blocks reentry");
	newgame(&g);
	place(&g,55,400,-700,0);
	step(&g,0);
	check(g.v.x > 0 && g.p.x >= 54,"rail reflection");
	newgame(&g);
	place(&g,230,200,0,600);
	step(&g,0);
	check(g.v.y < -500 && g.score == 100,"bumper kick and score");
	score = g.score;
	place(&g,230,200,0,600);
	step(&g,0);
	check(g.score == score,"bumper contact debounce");
	newgame(&g);
	for(i = 0; i < Ntarget; i++){
		place(&g,91,313+52*i,-400,0);
		ticks(&g,6,0);
	}
	check(g.targets == 7 && g.stage == 1,"relay mission stage");
	for(i = 0; i < 5; i++){
		g.flash[0] = 0;
		place(&g,230,200,0,600);
		step(&g,0);
	}
	check(g.hits == 5 && g.stage == 2,"reactor mission stage");
	place(&g,458,425,0,-500);
	step(&g,0);
	check(g.state == Ramp && g.missions == 1 && g.stage == 0,"orbit completes mission");
	check(g.score == 28250,"mission award");
	ticks(&g,2*Hz+2,0);
	check(g.state == Play && g.p.x > 150 && g.p.x < 180,"ramp returns to inlane");
	newgame(&g);
	for(i = 0; i < Nlane; i++){
		place(&g,lanes[i].x,lanes[i].y,0,0);
		step(&g,0);
	}
	check(g.multiplier == 2 && g.score == 1500,"rollover multiplier");
	newgame(&g);
	/* Strike a descending ball with the moving left flipper. */
	a = pivots[0];
	place(&g,a.x+58,a.y+8,0,300);
	ticks(&g,8,Left);
	check(g.v.y < -300,"moving flipper transfers momentum");
	/* The return lanes must clear the sling corners at low speed. */
	for(i = 0; i < 2; i++){
		newgame(&g);
		place(&g,i == 0 ? 122 : 458,635,0,0);
		cleared = 0;
		for(j = 0; j < 3*Hz; j++){
			step(&g,0);
			if(g.p.y > 710) cleared = 1;
		}
		check(cleared,"inlane clears the slingshot corner");
		newgame(&g);
		place(&g,i == 0 ? 166 : 414,680,0,0);
		ticks(&g,2*Hz,0);
		check(g.p.y > 740 || g.state != Play,"ball rolls past the flipper pivot");
	}
	newgame(&g);
	step(&g,Launch);
	step(&g,0);
	ticks(&g,Hz,0);
	left = g.save;
	place(&g,290,870,0,300);
	step(&g,0);
	check(g.state == Ready && g.ball == 1 && g.save < left,"ball saver");
	left = g.save;
	ticks(&g,Hz,Launch);
	step(&g,0);
	check(g.state == Play && g.save == left,"saved ball resumes its timer");
	for(i = 1; i <= 3; i++){
		g.save = 0;
		g.bonus = 100;
		place(&g,290,870,0,300);
		step(&g,0);
		check(g.state == Lost && g.ball == i,"drain");
		ticks(&g,Hz*2,0);
		check(g.state == (i == 3 ? Over : Ready),"three-ball game");
	}
	check(g.score == 300,"drain bonus counted once");
	newgame(&g);
	for(i = 0; i < 3; i++){
		place(&g,290,440,0,0);
		g.nudge = 0;
		step(&g,0);
		step(&g,Nudge);
	}
	check(g.tilted,"three rapid nudges tilt");
	score = g.score;
	place(&g,230,200,0,600);
	step(&g,Left|Right);
	check(g.score == score && g.angle[0] == .40,"tilt disables points and flippers");
	g.save = 5;
	g.bonus = 1000;
	place(&g,290,870,0,300);
	step(&g,0);
	check(g.state == Lost && g.score == score,"tilt disables saver and bonus");
	print("PASS: spring, rails, bumpers, flippers, missions, ramp, multiplier, drains and tilt\n");

	/* Take turns playing and letting the ball drain, using only the controls. */
	newgame(&g);
	games = pops = ramps = missions = 0;
	for(i = 0; i < Hz*600; i++){
		if(g.state == Over){
			games++;
			newgame(&g);
		}
		keys = demokeys(&g);
		if((i/(30*Hz))%2)
			keys &= Launch;
		j = g.missions;
		step(&g,keys);
		missions += g.missions-j;
		if(g.events & Pop) pops++;
		if(g.events & Lift) ramps++;
		g.events = 0;
		check(!isNaN(g.p.x) && !isNaN(g.p.y),"finite position");
		check(g.p.x > 0 && g.p.x < 600 && g.p.y > 0 && g.p.y < 880,"ball stays inside table or drain");
		speed = g.v.x*g.v.x+g.v.y*g.v.y;
		check(speed < 5000.0*5000,"bounded collision velocity");
	}
	check(games > 0 && missions > 0,"complete games and missions from launch");
	print("PASS: 600 simulated seconds, %d games, %d pops, %d ramps, %d missions\n",games,pops,ramps,missions);
	seed = 17;
	for(j = 0; j < 12; j++){
		newgame(&g);
		for(i = 0; i < Hz*15; i++){
			seed = seed*1664525+1013904223;
			keys = g.state == Ready ? demokeys(&g) : (seed>>24)&15;
			step(&g,keys);
			check(!isNaN(g.v.x) && !isNaN(g.v.y),"finite velocity under varied inputs");
			check(g.p.x > 0 && g.p.x < 600 && g.p.y > 0 && g.p.y < 880,"varied-input containment");
		}
	}
	print("PASS: varied-input simulation\n");
	exits(nil);
}
