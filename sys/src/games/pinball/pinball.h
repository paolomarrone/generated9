enum {
	Hz = 480,
	Railrad = 4, Ballrad = 10, Bumperrad = 29, Fliplen = 90, Fliprad = 10,
	Nbumper = 3,
	Ntarget = 3,
	Nlane = 3,
	Ready = 0, Play, Ramp, Lost, Over,
	Left = 1, Right = 2, Launch = 4, Nudge = 8,
	Wall = 0, Sling, Target, Gate,
	Flip = 1, Pop = 2, Hit = 4, Shot = 8, Drain = 16, Lift = 32, Win = 64,
};

typedef struct V V;
typedef struct Rail Rail;
typedef struct Game Game;

struct V { double x, y; };
struct Rail {
	V a, b;
	int kind, id;
};
struct Game {
	V p, v;
	double angle[2], omega[2];
	double charge, clock, save, tilt, nudge, ramp;
	double flash[Nbumper], target[Ntarget], sling[2];
	int state, ball, keys, tilted, served, events;
	int targets, lanes, stage, hits, missions, multiplier;
	vlong score, bonus;
};

extern Rail rails[];
extern int nrail;
extern V bumpers[Nbumper], lanes[Nlane], pivots[2], slings[2][3];

void tableinit(void);
void newgame(Game*);
void step(Game*, int);
V fliptip(Game*, int);
V ramppos(double);
int demokeys(Game*);
