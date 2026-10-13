#include <u.h>
#include <libc.h>

enum { Memsize = 64*1024*1024 };

static int
clone(char *path, int len)
{
	char buf[32];
	int fd, n;

	fd = open("#X/clone", ORDWR|ORCLOSE);
	if(fd < 0 || (n = read(fd, buf, sizeof(buf)-1)) <= 0)
		sysfatal("clone: %r");
	buf[n] = 0;
	snprint(path, len, "#X/%s", buf);
	return fd;
}

static int
vmopen(char *path, char *file, int mode)
{
	char name[128];
	int fd;

	snprint(name, sizeof(name), "%s/%s", path, file);
	fd = open(name, mode);
	if(fd < 0)
		sysfatal("open %s: %r", name);
	return fd;
}

static ulong
used(void)
{
	char buf[512], *p;
	int fd, n;

	fd = open("/dev/swap", OREAD);
	if(fd < 0 || (n = read(fd, buf, sizeof(buf)-1)) <= 0)
		sysfatal("swap: %r");
	close(fd);
	buf[n] = 0;
	p = strstr(buf, " user\n");
	if(p == nil)
		sysfatal("no user page count");
	while(p > buf && p[-1] != '\n')
		p--;
	return strtoul(p, nil, 10);
}

static uvlong
regread(int fd, char *name)
{
	char buf[4096], key[64], *p;
	int n;

	buf[0] = '\n';
	n = pread(fd, buf+1, sizeof(buf)-2, 0);
	if(n <= 0)
		sysfatal("clock read: %r");
	buf[n+1] = 0;
	snprint(key, sizeof(key), "\n%s ", name);
	p = strstr(buf, key);
	if(p == nil)
		sysfatal("missing %s register", name);
	return strtoull(p+strlen(key), nil, 0);
}

static uvlong
clockread(int fd, uvlong *adjust)
{
	*adjust = regread(fd, "tscadjust");
	return regread(fd, "tsc");
}

static void
rdtsctest(int ctl, int regs, int waitfd)
{
	char buf[128];
	uvlong a, b, c;
	int n;

	a = regread(regs, "tsc");
	if(fprint(ctl, "go pc=0;") < 0)
		sysfatal("go: %r");
	n = read(waitfd, buf, sizeof(buf)-1);
	if(n <= 0)
		sysfatal("wait: %r");
	buf[n] = 0;
	if(strncmp(buf, ".vmcall ", 8) != 0)
		sysfatal("unexpected exit: %s", buf);
	b = (u32int)regread(regs, "ax") | regread(regs, "dx")<<32;
	c = regread(regs, "tsc");
	assert(a <= b && b <= c);
}

static void
clocktest(void)
{
	char path[2][64], name[64];
	int ctl[2], regs[2], waitfd, seg, fd, i;
	uvlong a, b, c, adjust, delta;
	uchar *p;

	snprint(name, sizeof(name), "#g/vmx-clock-%d", getpid());
	seg = create(name, OREAD|ORCLOSE, DMDIR|0700);
	if(seg < 0)
		sysfatal("clock segment: %r");
	fd = vmopen(name, "ctl", OWRITE);
	if(fprint(fd, "va %#ux 4096 sticky", 0x10000000) < 0)
		sysfatal("clock segment size: %r");
	close(fd);
	p = segattach(0, name+3, (void*)0x10000000, 4096);
	if(p == (void*)-1)
		sysfatal("clock segment attach: %r");
	memmove(p, "\x0f\x31\x0f\x01\xc1", 5); /* RDTSC; VMCALL */
	for(i = 0; i < 2; i++){
		ctl[i] = clone(path[i], sizeof(path[i]));
		regs[i] = vmopen(path[i], "regs", ORDWR);
		clockread(regs[i], &adjust);
		assert(adjust == 0);
	}
	waitfd = vmopen(path[0], "wait", OREAD);
	fd = vmopen(path[0], "map", OWRITE);
	if(fprint(fd, "rwx wb 0 4096 %s 0\n", name+3) < 0)
		sysfatal("clock map: %r");
	close(fd);
	/* Large positive and negative adjustments must not reach another CPU. */
	for(i = 0; i < 2; i++){
		delta = i == 0 ? (1ULL<<60) + 0x12345678 : 0ULL - (1ULL<<60);
		a = clockread(regs[1], &adjust);
		if(fprint(regs[0], "tscadjust %#ullx\n", delta) < 0)
			sysfatal("adjust: %r");
		b = clockread(regs[0], &adjust) - delta;
		assert(adjust == delta);
		c = clockread(regs[1], &adjust);
		assert(adjust == 0 && a <= b && b <= c);
		rdtsctest(ctl[0], regs[0], waitfd);
	}
	/* Setting TSC must also update ADJUST; clearing it restores the clock. */
	if(fprint(regs[0], "tsc 0\n") < 0)
		sysfatal("set tsc: %r");
	a = clockread(regs[0], &adjust);
	assert(a < 1ULL<<40);
	rdtsctest(ctl[0], regs[0], waitfd);
	if(write(ctl[0], "init", 4) != 4)
		sysfatal("init: %r");
	delta = adjust;
	b = clockread(regs[0], &adjust);
	assert(b >= a && adjust == delta);
	a = clockread(regs[1], &adjust);
	if(fprint(regs[0], "tscadjust 0\n") < 0)
		sysfatal("clear adjust: %r");
	b = clockread(regs[0], &adjust);
	assert(adjust == 0);
	c = clockread(regs[1], &adjust);
	assert(adjust == 0 && a <= b && b <= c);
	for(i = 0; i < 2; i++){
		close(regs[i]);
		close(ctl[i]);
	}
	close(waitfd);
	segdetach(p);
	close(seg);
	print("VMX-CLOCK-PASS: TSC writes, adjustments, isolation and INIT\n");
}

static void
trial(char *mount, int round)
{
	char path[64], next[64], name[128], buf[4096];
	int ctl, oldctl, regs, waitfd, seg, fd, pid, i;
	Waitmsg *w;
	Dir *d;
	ulong before;

	before = used();
	if(open("#X/clone", OEXEC) >= 0)
		sysfatal("execute permission on ctl");
	ctl = clone(path, sizeof(path));
	regs = vmopen(path, "regs", OREAD);
	waitfd = vmopen(path, "wait", OREAD);
	if(bind(path, mount, MREPL) < 0)
		sysfatal("bind: %r");
	snprint(name, sizeof(name), "#g/vmx-test-%d", getpid());
	seg = create(name, OREAD|ORCLOSE, DMDIR|0700);
	if(seg < 0)
		sysfatal("segment: %r");
	fd = vmopen(name, "ctl", OWRITE);
	if(fprint(fd, "va %#ux %#ux sticky", 0x10000000, Memsize) < 0)
		sysfatal("segment size: %r");
	close(fd);
	fd = vmopen(path, "map", OWRITE);
	if(fprint(fd, "rwx wb 0 %#ux %s 0\n", Memsize, name+3) < 0)
		sysfatal("map: %r");
	close(fd);
	close(seg);
	if(used() < before + Memsize/4096)
		sysfatal("segment was not allocated");

	/* Share the fd group: the blocked wait must not keep the ctl open. */
	pid = rfork(RFPROC);
	if(pid < 0)
		sysfatal("rfork: %r");
	if(pid == 0){
		if(read(waitfd, buf, sizeof(buf)) >= 0)
			exits("wait succeeded after quit");
		exits(nil);
	}
	pid = rfork(RFPROC);
	if(pid < 0)
		sysfatal("rfork: %r");
	if(pid == 0){
		while(pread(regs, buf, sizeof(buf), 0) >= 0)
			;
		exits(nil);
	}
	sleep(20);
	if(round % 3 == 1){
		if(write(ctl, "quit", 4) != 4)
			sysfatal("quit: %r");
	}else if(round % 3 == 2){
		snprint(name, sizeof(name), "%s/ctl", path);
		if(remove(name) < 0)
			sysfatal("remove: %r");
	}
	oldctl = ctl;
	if(round % 3 == 0){
		close(ctl);
		oldctl = -1;
	}
	d = dirstat(path);
	if(d != nil)
		sysfatal("removed CPU still exists: %s", path);
	ctl = clone(next, sizeof(next));
	if(strcmp(path, next) != 0)
		sysfatal("slot was not reused");
	if(oldctl >= 0)
		close(oldctl);
	fd = vmopen(next, "regs", OREAD);
	if(read(fd, buf, sizeof(buf)) <= 0)
		sysfatal("new CPU disappeared");
	close(fd);
	if(pread(regs, buf, sizeof(buf), 0) >= 0)
		sysfatal("old regs handle reached new CPU");
	snprint(name, sizeof(name), "%s/status", mount);
	if(open(name, OREAD) >= 0)
		sysfatal("old directory reached new CPU");
	if(used() > before + Memsize/4096/2)
		sysfatal("removed CPU retained guest memory");
	for(i = 0; i < 2; i++){
		w = wait();
		if(w == nil || w->msg[0] != 0)
			sysfatal("reader failed");
		free(w);
	}
	close(waitfd);
	close(regs);
	close(ctl);
	if(unmount(nil, mount) < 0)
		sysfatal("unmount: %r");
}

void
main(void)
{
	char mount[64];
	int fd, i;

	rfork(RFNAMEG);
	clocktest();
	snprint(mount, sizeof(mount), "/tmp/vmx-test-%d", getpid());
	fd = create(mount, OREAD, DMDIR|0700);
	if(fd < 0)
		sysfatal("mount directory: %r");
	close(fd);
	for(i = 0; i < 32; i++)
		trial(mount, i);
	remove(mount);
	print("VMX-DEVICE-PASS: 32 lifetimes, memory, waits and stale handles\n");
	exits(nil);
}
