enum {
	Nstep = 16,
	Nvoice = 4,
	Minbpm = 30,
	Maxbpm = 300,
	Oplrate = 49716,
};

typedef struct Pattern Pattern;
struct Pattern {
	int bpm;
	ushort row[Nvoice];
};

extern char *voice[Nvoice];

int	putpattern(int, Pattern*);
int	kit(int);
int	step(int, Pattern*, int, int*);
void	edit(char*, Pattern*, int);
