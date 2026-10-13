/* Linux PID 1 for the vmx integration test; built on the Linux host. */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <cpuid.h>
#include <fcntl.h>
#include <sched.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

enum { Rounds = 8, Iterations = 25000, Block = 4096 };
static pthread_barrier_t barrier;
static atomic_ulong total;
static unsigned long ncpu;
static volatile unsigned long *page;

static void fail(const char *what)
{
	printf("VMX-SMP-FAIL: %s: %s\n", what, strerror(errno));
	for(;;) pause();
}

static void rendezvous(void)
{
	int rc = pthread_barrier_wait(&barrier);
	if(rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD)
		fail("barrier");
}

static void *worker(void *arg)
{
	unsigned long id = (unsigned long)arg;
	unsigned ax, bx, cx, dx;
	struct timespec before, after, delay = {0, 2000000};
	cpu_set_t set;
	void *writebuf, *readbuf;
	int fd;
	double sum = id + 0.5;

	CPU_ZERO(&set);
	CPU_SET(id, &set);
	if(sched_setaffinity(0, sizeof(set), &set) != 0)
		fail("affinity");
	__cpuid(1, ax, bx, cx, dx);
	if(ncpu > 1 && ((bx >> 24) != id || ((bx >> 16) & 255) != ncpu))
		fail("CPUID topology");
	if(posix_memalign(&writebuf, Block, Block) || posix_memalign(&readbuf, Block, Block))
		fail("I/O buffers");
	fd = open("/dev/vda", O_RDWR | O_DIRECT);
	if(fd < 0)
		fail("open virtio disk");
	rendezvous();
	for(unsigned long round = 0; round < Rounds; round++){
		/* Changing a shared mapping forces remote TLB invalidations. */
		if(id == 0){
			if(mprotect((void *)page, Block, PROT_READ | PROT_WRITE) != 0)
				fail("mprotect RW");
			*page = round;
			if(mprotect((void *)page, Block, PROT_READ) != 0)
				fail("mprotect R");
		}
		rendezvous();
		if(*page != round)
			fail("shared mapping");
		for(int i = 0; i < Iterations; i++){
			atomic_fetch_add_explicit(&total, 1, memory_order_relaxed);
			sum += 0.25;
		}
		if(sched_getcpu() != (int)id || sum != id + 0.5 + (round+1)*Iterations*0.25)
			fail("CPU or floating point state");
		if((round & 3) == 0){
			/* Distinct CR3s on each CPU also exercise VPID reuse by the host. */
			pid_t child = fork();
			if(child < 0)
				fail("fork");
			if(child == 0){
				if(mprotect((void *)page, Block, PROT_READ | PROT_WRITE) != 0)
					_exit(1);
				*page = 0x1000 + id;
				for(int i = 0; i < 100; i++){
					sched_yield();
					if(*page != 0x1000 + id)
						_exit(1);
				}
				_exit(0);
			}
			int status;
			if(waitpid(child, &status, 0) != child || status != 0 || *page != round)
				fail("address space isolation");
		}
		/* Concurrent DMA and level interrupts, with uncached disk I/O. */
		memset(writebuf, (id + round*ncpu) & 255, Block);
		off_t offset = (id*Rounds + round)*Block;
		if(pwrite(fd, writebuf, Block, offset) != Block ||
		   pread(fd, readbuf, Block, offset) != Block ||
		   memcmp(writebuf, readbuf, Block) != 0)
			fail("virtio disk I/O");
		if(clock_gettime(CLOCK_MONOTONIC, &before) != 0 ||
		   nanosleep(&delay, NULL) != 0 ||
		   clock_gettime(CLOCK_MONOTONIC, &after) != 0)
			fail("local timer");
		long long elapsed = (after.tv_sec-before.tv_sec)*1000000000LL + after.tv_nsec-before.tv_nsec;
		if(elapsed < delay.tv_nsec)
			fail("clock moved backwards or early timer");
		rendezvous();
	}
	if(fdatasync(fd) != 0)
		fail("disk flush");
	close(fd);
	free(readbuf);
	free(writebuf);
	printf("CPU %lu: atomics, floating point, mappings, disk and timers passed\n", id);
	return NULL;
}

static void workload(void)
{
	pthread_t threads[64];
	pthread_attr_t attr;
	if(sysconf(_SC_NPROCESSORS_ONLN) != (long)ncpu)
		fail("online CPU count");
	atomic_store(&total, 0);
	if(pthread_barrier_init(&barrier, NULL, ncpu) != 0)
		fail("barrier init");
	if(pthread_attr_init(&attr) != 0 || pthread_attr_setstacksize(&attr, 256*1024) != 0)
		fail("thread stack size");
	for(unsigned long i = 0; i < ncpu; i++)
		if(pthread_create(&threads[i], &attr, worker, (void *)i) != 0)
			fail("pthread_create");
	pthread_attr_destroy(&attr);
	for(unsigned long i = 0; i < ncpu; i++)
		if(pthread_join(threads[i], NULL) != 0)
			fail("pthread_join");
	pthread_barrier_destroy(&barrier);
	if(atomic_load(&total) != ncpu * Rounds * Iterations)
		fail("atomic total");
}

static void modules(void)
{
	char name[256];
	FILE *f = fopen("/modules", "r");
	if(f == NULL)
		fail("module list");
	while(fgets(name, sizeof(name), f)){
		name[strcspn(name, "\n")] = 0;
		int fd = open(name, O_RDONLY);
		if(fd < 0 || syscall(SYS_finit_module, fd, "", 0) != 0)
			fail(name);
		close(fd);
	}
	fclose(f);
}

static void online(unsigned long cpu, char value)
{
	char path[128];
	snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%lu/online", cpu);
	int fd = open(path, O_WRONLY);
	if(fd < 0 || write(fd, &value, 1) != 1)
		fail(path);
	close(fd);
}

static void interrupts(void)
{
	char line[8192], *p, *end;
	int local = 0, nmis = 0;
	FILE *f = fopen("/proc/interrupts", "r");
	if(f == NULL)
		fail("interrupt counters");
	while(fgets(line, sizeof(line), f)){
		if(ncpu > 1 && strstr(line, "virtio") && strstr(line, "-edge"))
			fail("PCI interrupt must be level triggered");
		if(strstr(line, "NMI:") || strstr(line, "LOC:") || strstr(line, "CAL:") ||
		   strstr(line, "TLB:") || strstr(line, "virtio"))
			fputs(line, stdout);
		if((p = strstr(line, "NMI:")) != NULL){
			p += 4;
			for(unsigned long i = 0; i < ncpu; i++){
				if(strtoul(p, &end, 10) != 0)
					nmis++;
				p = end;
			}
		}
		if((p = strstr(line, "LOC:")) == NULL)
			continue;
		local = 1;
		p += 4;
		for(unsigned long i = 0; i < ncpu; i++){
			unsigned long n = strtoul(p, &end, 10);
			if(ncpu > 1 && (end == p || n == 0))
				fail("CPU without local timer interrupts");
			p = end;
		}
	}
	fclose(f);
	if(ncpu > 1 && !local)
		fail("no local APIC timer");
	if(ncpu > 1 && nmis < (int)ncpu - 1)
		fail("NMI broadcast");
}

int main(int argc, char **argv)
{
	char *end;
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc != 2)
		fail("expected CPU count argument");
	ncpu = strtoul(argv[1], &end, 10);
	if(*end || ncpu < 1 || ncpu > 64)
		fail("invalid CPU count");
	mkdir("/proc", 0755);
	mkdir("/sys", 0755);
	if(mount("proc", "/proc", "proc", 0, NULL) != 0 ||
	   mount("sysfs", "/sys", "sysfs", 0, NULL) != 0 ||
	   mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0)
		fail("mount");
	printf("VMX-TEST: expected=%lu online=%ld\n", ncpu, sysconf(_SC_NPROCESSORS_ONLN));
	modules();
	page = mmap(NULL, Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(page == MAP_FAILED)
		fail("mmap");
	workload();
	for(int round = 0; round < 2 && ncpu > 1; round++){
		for(unsigned long i = 1; i < ncpu; i++){
			online(i, '0');
			online(i, '1');
		}
		printf("VMX-TEST: CPU restart round %d\n", round+1);
		workload();
	}
	if(ncpu > 1){
		/* Linux requests NMI backtraces from the other CPUs. */
		int fd = open("/proc/sysrq-trigger", O_WRONLY);
		if(fd < 0 || write(fd, "l", 1) != 1)
			fail("NMI backtrace request");
		close(fd);
	}
	interrupts();
	printf("VMX-SMP-PASS cpus=%lu total=%lu\n", ncpu, atomic_load(&total));
	for(;;) pause();
}
