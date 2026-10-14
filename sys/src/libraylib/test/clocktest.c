#include <u.h>
#include <libc.h>
#include <raylib.h>

static int fd;

static void
setclock(uvlong wall, uvlong ticks)
{
	uchar b[32];
	int i;

	memset(b,0,sizeof b);
	for(i=0; i<8; i++){
		b[7-i] = wall>>(8*i);
		b[31-i] = ticks>>(8*i);
	}
	if(pwrite(fd,b,sizeof b,0) != sizeof b) sysfatal("write clock: %r");
}

void
main(void)
{
	char path[64];
	double t;

	if(rfork(RFNAMEG) < 0) sysfatal("clock namespace: %r");
	snprint(path,sizeof path,"/tmp/rayclock.%d",getpid());
	fd = create(path,ORDWR|ORCLOSE,0600);
	if(fd < 0) sysfatal("create clock: %r");
	setclock(10000000000000ULL,1000000000ULL);
	if(bind(path,"/dev/bintime",MREPL) < 0) sysfatal("bind clock: %r");
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(160,120,"raylib clock test");
	t = GetTime();
	if(fabs(t) > .001) sysfatal("initial time %g",t);
	setclock(13600000000000ULL,2000000000ULL);
	t = GetTime();
	if(fabs(t-1) > .001) sysfatal("forward wall-clock change: %g",t);
	setclock(6400000000000ULL,3000000000ULL);
	t = GetTime();
	if(fabs(t-2) > .001) sysfatal("backward wall-clock change: %g",t);
	CloseWindow();
	print("PASS: elapsed time ignores forward/backward wall-clock changes\n");
	exits(nil);
}
