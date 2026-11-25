#include <u.h>
#include <libc.h>
#include "pinball.h"

Rail rails[80];
int nrail;
V bumpers[Nbumper] = {{230,240}, {355,245}, {290,340}};
V lanes[Nlane] = {{180,135}, {290,120}, {400,135}};
V pivots[2] = {{185,710}, {395,710}};
V slings[2][3] = {
	{{135,540}, {135,632}, {205,662}},
	{{445,540}, {445,632}, {375,662}},
};

static const double dt = 1.0/Hz;

static double
fmin(double a, double b)
{
	return a < b ? a : b;
}

static double
fmax(double a, double b)
{
	return a > b ? a : b;
}

static V
add(V a, V b)
{
	return (V){a.x+b.x, a.y+b.y};
}

static V
sub(V a, V b)
{
	return (V){a.x-b.x, a.y-b.y};
}

static V
mul(V a, double s)
{
	return (V){a.x*s, a.y*s};
}

static double
dot(V a, V b)
{
	return a.x*b.x+a.y*b.y;
}

static double
clamp(double x, double lo, double hi)
{
	return x < lo ? lo : x > hi ? hi : x;
}

static void
rail(double x, double y, double u, double v, int kind, int id)
{
	Rail *r;

	if(nrail == nelem(rails))
		sysfatal("too many rails");
	r = &rails[nrail++];
	r->a = (V){x,y};
	r->b = (V){u,v};
	r->kind = kind;
	r->id = id;
}

void
tableinit(void)
{
	V a, b;
	double t;
	int i, j;

	nrail = 0;
	a = (V){40,180};
	for(i = 1; i <= 24; i++){
		t = PI+i*PI/24;
		b = (V){305+265*cos(t), 180+125*sin(t)};
		rail(a.x,a.y,b.x,b.y,Wall,0);
		a = b;
	}
	rail(40,180,40,665,Wall,0);
	rail(40,665,100,800,Wall,0);
	rail(100,800,180,840,Wall,0);
	rail(570,180,570,810,Wall,0);
	rail(570,810,530,810,Wall,0);
	rail(526,111,526,200,Gate,0);
	rail(526,200,526,810,Wall,0);
	rail(526,650,475,800,Wall,0);
	rail(475,800,400,840,Wall,0);
	/* Return lanes and the two slingshots feed the flippers. */
	rail(95,500,95,658,Wall,0);
	rail(95,658,185,702,Wall,0);
	rail(485,500,485,658,Wall,0);
	rail(485,658,395,702,Wall,0);
	for(i = 0; i < 2; i++){
		for(j = 0; j < 3; j++){
			a = slings[i][j];
			b = slings[i][(j+1)%3];
			rail(a.x,a.y,b.x,b.y,j == 2 ? Sling : Wall,i);
		}
	}
	for(i = 0; i < Ntarget; i++)
		rail(70,300+i*52,82,326+i*52,Target,i);
}

static void
serve(Game *g)
{
	g->state = Ready;
	g->p = (V){548,760};
	g->v = (V){0,0};
	g->charge = 0;
	g->clock = 0;
}

void
newgame(Game *g)
{
	memset(g,0,sizeof *g);
	g->ball = 1;
	g->multiplier = 1;
	g->angle[0] = .40;
	g->angle[1] = PI-.40;
	serve(g);
}

V
fliptip(Game *g, int i)
{
	return add(pivots[i],(V){Fliplen*cos(g->angle[i]),Fliplen*sin(g->angle[i])});
}

/* A raised return track; its ball is outside the playfield collision plane. */
V
ramppos(double t)
{
	static V path[] = {
		{458,420}, {480,330}, {475,190}, {425,110},
		{300,85}, {160,115}, {115,230}, {110,440}, {110,625}, {165,672},
	};
	V a, b, c, d;
	int i;
	double f, f2, f3;

	t = clamp(t,0,1)*(nelem(path)-1);
	i = t;
	if(i == nelem(path)-1)
		return path[i];
	f = t-i;
	f2 = f*f;
	f3 = f2*f;
	a = path[i == 0 ? 0 : i-1];
	b = path[i];
	c = path[i+1];
	d = path[i+2 < nelem(path) ? i+2 : i+1];
	/* Catmull-Rom interpolation joins the track with continuous tangents. */
	return add(add(mul(a,(-f+2*f2-f3)/2),mul(b,(2-5*f2+3*f3)/2)),
		add(mul(c,(f+4*f2-3*f3)/2),mul(d,(-f2+f3)/2)));
}

static void
award(Game *g, int points)
{
	if(!g->tilted){
		g->score += points*g->multiplier;
		g->bonus += points/10;
	}
}

/* Capsule contact, including the surface velocity of a rotating flipper. */
static int
contact(Game *g, V a, V b, double radius, double bounce, double omega, double kick)
{
	V ab, q, n, surface, relative;
	double u, d, vn;

	ab = sub(b,a);
	d = dot(ab,ab);
	u = d == 0 ? 0 : clamp(dot(sub(g->p,a),ab)/d,0,1);
	q = add(a,mul(ab,u));
	n = sub(g->p,q);
	radius += Ballrad;
	d = dot(n,n);
	if(d >= radius*radius)
		return 0;
	d = sqrt(d);
	if(d < .000001){
		/* An exact centre hit has no geometric normal: oppose the velocity. */
		n = mul(g->v,-1);
		d = sqrt(dot(n,n));
		if(d < .000001)
			n = (V){0,-1};
		else
			n = mul(n,1/d);
		d = 0;
	}else
		n = mul(n,1/d);
	g->p = add(g->p,mul(n,radius-d+.01));
	q = sub(q,a);
	surface = (V){-omega*q.y,omega*q.x};
	relative = sub(g->v,surface);
	vn = dot(relative,n);
	if(vn >= 0)
		return 0;
	g->v = add(g->v,mul(n,-(1+bounce)*vn+kick));
	return 1;
}

static void
lost(Game *g)
{
	g->events |= Drain;
	if(g->save > 0 && !g->tilted){
		serve(g);
		return;
	}
	if(!g->tilted)
		g->score += g->bonus*g->multiplier;
	g->state = Lost;
	g->clock = 1.4;
}

static void
moveball(Game *g)
{
	Rail *r;
	V tip[2];
	double speed, kick;
	int i, pass;

	/* Max travel is 3.34 pixels per tick, below the ball and flipper radii. */
	g->v.y += 620*dt;
	speed = sqrt(dot(g->v,g->v));
	if(speed > 1600)
		g->v = mul(g->v,1600/speed);
	g->p = add(g->p,mul(g->v,dt));
	for(i = 0; i < 2; i++)
		tip[i] = fliptip(g,i);
	for(pass = 0; pass < 2; pass++){
		for(i = 0; i < nrail; i++){
			r = &rails[i];
			/* The vertical launch gate opens toward the playfield only. */
			if(r->kind == Gate && g->p.x > r->a.x)
				continue;
			kick = r->kind == Sling && !g->tilted && g->sling[r->id] == 0 ? 240 : 0;
			if(!contact(g,r->a,r->b,Railrad,.78,0,kick))
				continue;
			if(r->kind == Sling && kick > 0){
				g->sling[r->id] = .15;
				award(g,50);
				g->events |= Pop;
			}
			if(r->kind == Target && g->target[r->id] == 0 && !g->tilted){
				g->target[r->id] = .2;
				g->targets |= 1<<r->id;
				award(g,250);
				g->events |= Hit;
				if(g->stage == 0 && g->targets == 7){
					g->stage = 1;
					g->events |= Win;
				}
			}
		}
		for(i = 0; i < Nbumper; i++){
			kick = !g->tilted && g->flash[i] == 0 ? 420 : 0;
			if(contact(g,bumpers[i],bumpers[i],Bumperrad,.9,0,kick) && kick > 0){
				g->flash[i] = .12;
				award(g,100);
				g->events |= Pop;
				if(g->stage == 1 && ++g->hits == 5){
					g->stage = 2;
					g->events |= Win;
				}
			}
		}
		for(i = 0; i < 2; i++)
			contact(g,pivots[i],tip[i],Fliprad,.75,g->omega[i],0);
	}
}

void
step(Game *g, int keys)
{
	V a;
	double angle, old;
	int i, pressed;

	if(g->state == Over)
		return;
	pressed = keys & ~g->keys;
	g->keys = keys;
	for(i = 0; i < Nbumper; i++)
		g->flash[i] = fmax(0,g->flash[i]-dt);
	for(i = 0; i < Ntarget; i++)
		g->target[i] = fmax(0,g->target[i]-dt);
	for(i = 0; i < 2; i++){
		g->sling[i] = fmax(0,g->sling[i]-dt);
		old = g->angle[i];
		angle = (keys & (1<<i)) && !g->tilted ? -.48 : .40;
		if(i == 1)
			angle = PI-angle;
		g->angle[i] += clamp(angle-old,-22*dt,22*dt);
		g->omega[i] = (g->angle[i]-old)/dt;
	}
	if((pressed & (Left|Right)) && !g->tilted)
		g->events |= Flip;
	if(g->state == Lost){
		g->clock -= dt;
		if(g->clock <= 0){
			if(g->ball == 3){
				g->state = Over;
			}else{
				g->ball++;
				g->tilted = g->served = 0;
				g->tilt = g->save = 0;
				g->bonus = 0;
				g->lanes = 0;
				g->multiplier = 1;
				serve(g);
			}
		}
		return;
	}
	if(g->state == Ready){
		if(keys & Launch)
			g->charge = fmin(1,g->charge+dt);
		else if(g->charge > 0){
			g->v.y = -(800+550*g->charge);
			g->charge = 0;
			g->state = Play;
			g->events |= Shot;
			if(!g->served){
				g->save = 8;
				g->served = 1;
			}
		}
		return;
	}
	g->save = fmax(0,g->save-dt);
	g->tilt = fmax(0,g->tilt-.20*dt);
	g->nudge = fmax(0,g->nudge-dt);
	if((pressed & Nudge) && g->nudge == 0 && !g->tilted){
		g->nudge = .35;
		g->tilt += 1;
		if(g->tilt >= 2.5)
			g->tilted = 1;
		else{
			g->v.y -= 180;
			g->v.x += g->p.x < 290 ? 65 : -65;
		}
	}
	if(g->state == Ramp){
		g->ramp += dt/2;
		g->p = ramppos(g->ramp);
		if(g->ramp >= 1){
			g->state = Play;
			g->v = (V){110,240};
		}
		return;
	}
	moveball(g);
	for(i = 0; i < Nlane; i++){
		a = sub(g->p,lanes[i]);
		if(dot(a,a) < 19*19 && !(g->lanes & (1<<i)) && !g->tilted){
			g->lanes |= 1<<i;
			award(g,500);
			g->events |= Hit;
			if(g->lanes == 7){
				g->multiplier = g->multiplier < 5 ? g->multiplier+1 : 5;
				g->events |= Win;
			}
		}
	}
	if(!g->tilted && g->p.x > 436 && g->p.x < 479 &&
	   g->p.y > 395 && g->p.y < 430 && g->v.y < -180){
		g->state = Ramp;
		g->ramp = 0;
		g->p = ramppos(0);
		award(g,2000);
		g->events |= Lift;
		if(g->stage == 2){
			award(g,25000);
			g->missions++;
			g->stage = g->targets = g->hits = g->lanes = 0;
			g->events |= Win;
		}
	}
	/* A weak launch falls back onto the spring instead of losing a ball. */
	if(g->p.x > 537 && g->p.y > 760 && g->v.y > 0)
		serve(g);
	if(g->p.y > 865)
		lost(g);
}

int
demokeys(Game *g)
{
	if(g->state == Ready)
		return g->charge < .62 ? Launch : 0;
	if(g->state == Play && g->p.y > 687 && g->p.y < 755)
		return g->p.x < 290 ? Left : Right;
	return 0;
}
