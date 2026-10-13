#include <u.h>
#include <libc.h>

enum { Iterations = 200000 };
static long total, gate;

static void
worker(int cpu)
{
	char path[64];
	int fd, i;
	double sum;
	vlong start;

	snprint(path, sizeof(path), "/proc/%d/ctl", getpid());
	fd = open(path, OWRITE);
	if(fd < 0 || fprint(fd, "wired %d", cpu) < 0)
		sysfatal("wire cpu%d: %r", cpu);
	close(fd);
	if(semacquire(&gate, 1) < 0)
		sysfatal("start: %r");
	sleep(0);
	start = uptime();
	sum = cpu + 0.5;
	for(i = 0; i < Iterations; i++){
		ainc(&total);
		sum += 0.25;
		if((i & 1023) == 0)
			sleep(0);
	}
	if(sum != cpu + 0.5 + Iterations*0.25 || uptime() < start)
		sysfatal("cpu%d: floating point or clock", cpu);
	exits(nil);
}

void
main(int argc, char **argv)
{
	char buf[1024], *end;
	int fd, i, n, ncpu, online, started, failed;
	Waitmsg *w;

	if(argc != 2)
		sysfatal("usage: %s cpus", argv[0]);
	ncpu = strtol(argv[1], &end, 10);
	if(*end != 0 || ncpu < 1 || ncpu > 64)
		sysfatal("invalid CPU count");
	fd = open("/dev/sysstat", OREAD);
	if(fd < 0)
		sysfatal("sysstat: %r");
	online = 0;
	while((n = read(fd, buf, sizeof(buf))) > 0)
		for(i = 0; i < n; i++)
			if(buf[i] == '\n') online++;
	close(fd);
	if(n < 0 || online != ncpu)
		sysfatal("expected %d CPUs, found %d", ncpu, online);
	for(started = 0; started < ncpu; started++){
		n = rfork(RFPROC|RFMEM);
		if(n < 0){
			fprint(2, "rfork: %r\n");
			break;
		}
		if(n == 0)
			worker(started);
	}
	semrelease(&gate, started);
	failed = started != ncpu;
	for(i = 0; i < started; i++){
		w = wait();
		if(w == nil)
			sysfatal("wait: %r");
		if(w->msg[0] != 0){
			fprint(2, "%d: %s\n", w->pid, w->msg);
			failed = 1;
		}
		free(w);
	}
	if(failed || total != ncpu*Iterations)
		sysfatal("native SMP test failed: total %ld", total);
	print("VMX-PLAN9-PASS cpus=%d total=%ld\n", ncpu, total);
	exits(nil);
}
