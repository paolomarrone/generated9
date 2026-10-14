/* Use the monitor's actual memory map and register cache, without starting VMs. */
#define threadmain vmxmain
#include "../vmx.c"
#undef threadmain
#include "../virtio.h"

static uvlong now;
static Region *code, *lapic;

static void
registers(void)
{
	Vcpu *saved, *c;
	char path[64], *s, *absent = "absent", *expected = "ax=0x123456789abcde55;";
	int fd;
	static char first[] = "ax 0x123456789abcdef0\nbx 0x1111111111111111\ncx 017\ndx 42\nsi 0x50\n";
	static char second[] = "ax 0xfedcba9876543210\nbx 0x2222222222222222\ncx 020\ndx 43\nsi 0x60\n";

	saved = cpu;
	c = emalloc(2 * sizeof(Vcpu));
	snprint(path, sizeof(path), "/tmp/vmx-regs.%d", getpid());
	fd = create(path, ORDWR|ORCLOSE, 0600);
	assert(fd >= 0);
	c[0].regsfd = c[1].regsfd = fd;
	assert(pwrite(fd, first, sizeof(first)-1, 0) == sizeof(first)-1);
	cpu = c;
	assert(rget(RAX) == 0x123456789abcdef0ULL && rhave(RDX));
	rsetsz(RAX, 0x55, 1);
	/* Reading another value in the snapshot must preserve pending writes. */
	assert(rget(RBX) == 0x1111111111111111ULL);
	assert(rget(RAX) == 0x123456789abcde55ULL);
	rpoke(RDX, 99, 1);
	assert(rget(RDX) == 99 && rget(RCX) == 15);
	s = rcflush(1);
	assert(s != nil && strcmp(s, expected) == 0);
	assert(pwrite(fd, second, sizeof(second)-1, 0) == sizeof(second)-1);
	/* Resuming invalidates decoded and still-unread values alike. */
	assert(rget(RBX) == 0x2222222222222222ULL);
	assert(rget(RSI) == 0x60);
	assert(rget(RAX) == 0xfedcba9876543210ULL && rget(RDX) == 43);
	assert(pwrite(fd, first, sizeof(first)-1, 0) == sizeof(first)-1);
	cpu = c + 1;
	assert(rget(RBX) == 0x1111111111111111ULL);
	cpu = c;
	assert(rget(RBX) == 0x2222222222222222ULL);
	/* An uncached lookup writes dirty registers before taking a new snapshot. */
	rcflush(1);
	close(fd);
	fd = create(path, ORDWR|ORCLOSE, 0600);
	assert(fd >= 0);
	cpu->regsfd = fd;
	rset(RAX, 0x42);
	rset(RBX, 0x73);
	assert(!rhave(absent));
	assert(rget(RAX) == 0x42 && rget(RBX) == 0x73);
	assert(rcflush(1) == nil);
	close(fd);
	free(c);
	cpu = saved;
	print("PASS: register snapshots, partial writes, exit values and CPU isolation\n");
}

uvlong
nanosec(void)
{
	return now;
}

static u32int
readreg(int off)
{
	u32int v;

	assert(lapic->mmio(Lapicbase + off, &v, 4, 0) == 0);
	return v;
}

static void
writereg(int off, u32int v)
{
	assert(lapic->mmio(Lapicbase + off, &v, 4, 1) == 0);
}

static void
mode(int n)
{
	int i;
	char *s;
	static char *seg[] = {"cs", "ds", "es", "fs", "gs", "ss"};

	for(i = 0; i < nelem(x86reg); i++)
		rset(x86reg[i], 0);
	for(i = 0; i < nelem(seg); i++){
		rset(seg[i], 0);
		s = smprint("%sbase", seg[i]); rset(s, 0);
		s = smprint("%slimit", seg[i]); rset(s, 0xffffffff);
		s = smprint("%sperm", seg[i]); rset(s, 0x93);
	}
	rset("csperm", n == 8 ? 0x209b : n == 4 ? 0xc09b : 0x9b);
	rset("cr0real", 0);
	rset("flags", 2);
	rset(RPC, code->start);
	if(n == 2){
		rset("csbase", code->start);
		rset(RPC, 0);
	}
}

static void
instruction(uchar *p, int n)
{
	memmove(code->v, p, n);
	assert(x86step() == 1);
}

static void
decoder(void)
{
	/* mov 0xfee00020,%eax: the high displacement bit is data, not an error. */
	uchar absolute[] = {0x8b, 0x05, 0x20, 0x00, 0xe0, 0xfe};
	uchar rexbase[] = {0x41, 0x8b, 0x40, 0x20};
	uchar rexreg[] = {0x44, 0x8b, 0x08};
	uchar rexindex[] = {0x42, 0x8b, 0x04, 0xa0};
	uchar rexlast[] = {0x67, 0x44, 0x8b, 0x08};
	uchar rexignored[] = {0x44, 0x67, 0x8b, 0x08};
	uchar addr32[] = {0x67, 0x8b, 0x04, 0x25, 0x20, 0x00, 0xe0, 0xfe};
	uchar relative[] = {0x8b, 0x05, 0, 0, 0, 0};
	uchar gs[] = {0x65, 0x8b, 0x04, 0x25, 0x20, 0, 0, 0};
	uchar real[] = {0x66, 0x8b, 0x00};
	uchar bpsi[] = {0x66, 0x8b, 0x02};
	uchar bpdi[] = {0x66, 0x8b, 0x03};
	uchar exchange[] = {0x87, 0x01};
	u32int id;

	id = cpu->id << 24;
	mode(4);
	instruction(absolute, sizeof(absolute));
	assert(rget(RAX) == id);
	mode(2);
	rset("dsbase", Lapicbase);
	rset(RBX, 16); rset(RSI, 16);
	instruction(real, sizeof(real));
	assert(rget(RAX) == id);
	/* BP-based 16-bit addressing defaults to SS, not DS. */
	rset("dsbase", code->start); rset("ssbase", Lapicbase);
	rset(RBX, 0); rset(RBP, 16); rset(RDI, 16);
	instruction(bpsi, sizeof(bpsi));
	assert(rget(RAX) == id);
	instruction(bpdi, sizeof(bpdi));
	assert(rget(RAX) == id);
	if(sizeof(uintptr) == 8){
		mode(8);
		rset(R8, Lapicbase);
		instruction(rexbase, sizeof(rexbase));
		assert(rget(RAX) == id);
		rset(RAX, Lapicbase+32); rset(R9, -1);
		instruction(rexreg, sizeof(rexreg));
		assert(rget(R9) == id);
		/* Only a REX immediately before the opcode extends registers. */
		rset(RCX, -1); rset(R9, -1);
		instruction(rexlast, sizeof(rexlast));
		assert(rget(R9) == id && rget(RCX) == ~0ULL);
		rset(R9, -1);
		instruction(rexignored, sizeof(rexignored));
		assert(rget(RCX) == id && rget(R9) == ~0ULL);
		rset(RAX, Lapicbase); rset(R12, 8);
		instruction(rexindex, sizeof(rexindex));
		assert(rget(RAX) == id);
		instruction(addr32, sizeof(addr32));
		assert(rget(RAX) == id);
		PUT32(relative, 2, Lapicbase+32 - (code->start + sizeof(relative)));
		instruction(relative, sizeof(relative));
		assert(rget(RAX) == id);
		rset("gsbase", Lapicbase);
		instruction(gs, sizeof(gs));
		assert(rget(RAX) == id);
	}
	mode(4);
	rset(RAX, 0x60); rset(RCX, Lapicbase+0x80);
	instruction(exchange, sizeof(exchange));
	assert(rget(RAX) == 0 && readreg(0x80) == 0x60);
}

static void
halted(void)
{
	Vcpu *saved, *c;
	char *s, *expected = "pc=0x101;";
	int i;

	saved = cpu;
	c = emalloc(sizeof(Vcpu));
	c->regsfd = -1;
	cpu = c;
	for(i = 0; i < 2; i++){
		c->state = VMRUNNING;
		c->irqactive = 0x51;
		processexit(i ? ".hlt 0 pc 0x100 ilen 1 flags 0x202" :
			".hlt 0 pc 0x100 ilen 1 flags 2");
		assert(c->state == (i ? VMRUNNING : VMHALT));
		assert(rget(RFLAGS) == (i ? 0x202 : 2));
		s = rcflush(1);
		assert(s != nil && strcmp(s, expected) == 0);
	}
	free(c);
	cpu = saved;
	print("PASS: HLT interrupt state without a register snapshot\n");
}

static void
msrs(void)
{
	uvlong pat;

	/* WRMSR consumes EDX:EAX and ECX, even in 64-bit mode. */
	rset(RPC, 0);
	rset(RCX, 0x1234567800000277ULL);
	rset(RAX, 0xdeadbeef00070406ULL);
	rset(RDX, 0xcafebabe00070406ULL);
	processexit(".wrmsr 0 ilen 2");
	pat = rget("pat");
	assert(pat == 0x0007040600070406ULL && rget(RPC) == 2);
	processexit(".rdmsr 0 ilen 2");
	assert(rget(RAX) == 0x00070406 && rget(RDX) == 0x00070406 && rget(RPC) == 4);
}

static void
interrupts(void)
{
	writereg(0xf0, 0x1ff);
	/* TPR masks priority classes; accepting an interrupt moves IRR to ISR. */
	writereg(0x300, 1<<18 | 0x51);
	assert(cpu->irqactive == -1 && readreg(0x220) == 1<<17);
	writereg(0x80, 0);
	assert(cpu->irqactive == 0x51);
	apicack(cpu, 0x51);
	assert(readreg(0x220) == 0 && readreg(0x120) == 1<<17);
	assert(readreg(0xa0) == 0x50);
	writereg(0xb0, 0);
	assert(readreg(0x120) == 0 && readreg(0xa0) == 0);
	/* One-shot expiry leaves the initial count readable. */
	writereg(0x3e0, 11);
	writereg(0x320, 0x52);
	writereg(0x380, 100);
	assert(readreg(0x390) == 100);
	now += 1000;
	apicadvance();
	assert(cpu->irqactive == 0x52);
	assert(readreg(0x380) == 100 && readreg(0x390) == 0);
	apicack(cpu, 0x52); writereg(0xb0, 0);
	/* A divider change preserves the remaining count; periodic mode reloads. */
	writereg(0x320, 1<<17 | 0x52);
	writereg(0x380, 100);
	now += 400;
	writereg(0x3e0, 0);
	assert(readreg(0x390) == 60);
	now += 1200;
	apicadvance();
	assert(readreg(0x390) == 100 && cpu->irqactive == 0x52);
	apicack(cpu, 0x52); writereg(0xb0, 0);
	apicreset(cpu);
	assert(readreg(0x390) == 0 && readreg(0x380) == 0);
	assert(readreg(0x320) == 1<<16 && readreg(0xf0) == 255);
}

static uvlong
getmsr(int reg)
{
	uvlong v;

	assert(apicmsr(1, reg, &v) == 1);
	return v;
}

static void
putmsr(int reg, uvlong v)
{
	assert(apicmsr(0, reg, &v) == 1);
}

static void
x2apic(void)
{
	uvlong v;
	int i;
	static int readonly[] = {0x802, 0x803, 0x80a, 0x80d, 0x810, 0x827, 0x839};
	static int reserved[] = {0x800, 0x804, 0x809, 0x80e, 0x829, 0x831, 0x840, 0xbff};

	assert(apicmsr(1, 0x802, &v) == -1);
	for(i = 0; i < ncpu; i++){
		cpu = cpus + i;
		apicreset(cpu);
		cpu->irqactive = -1;
		putmsr(0x1b, (uvlong)Lapicbase | 0xc00);
		assert(getmsr(0x802) == i && getmsr(0x80d) == 1U<<i);
		putmsr(0x80f, 0x1ff);
	}
	cpu = cpus;
	/* A real MSR exit supplies all its operands, without a register read. */
	rcflush(1);
	processexit(".wrmsr 0 pc 0x100 ilen 2 cx 0x830 dx 1 ax 0x61");
	assert(rget(RPC) == 0x102 && cpus[1].irqactive == 0x61);
	assert(getmsr(0x830) == (1ULL<<32 | 0x61));
	cpu++;
	apicack(cpu, 0x61); putmsr(0x80b, 0);
	/* Neither physical nor logical destinations may lose their high bits. */
	cpu = cpus;
	putmsr(0x830, 0x100000100000061ULL);
	assert(cpus[1].irqactive == -1);
	cpus[1].id = 17;
	putmsr(0x830, 0x1000200000861ULL);
	assert(cpus[1].irqactive == 0x61);
	cpu++;
	assert(getmsr(0x802) == 17 && getmsr(0x80d) == 0x10002);
	apicack(cpu, 0x61); putmsr(0x80b, 0);
	putmsr(0x83f, 0x62);
	assert(cpu->irqactive == 0x62);
	apicack(cpu, 0x62); putmsr(0x80b, 0);
	putmsr(0x830, 0xffffffff00000063ULL);
	assert(cpus[0].irqactive == 0x63 && cpus[1].irqactive == 0x63);
	for(i = 0; i < ncpu; i++){
		cpu = cpus + i;
		apicack(cpu, 0x63); putmsr(0x80b, 0);
	}
	assert(readreg(0x30) == ~0U);
	writereg(0x80, 0xff);
	assert(getmsr(0x808) == 0);
	for(i = 0; i < nelem(readonly); i++){
		v = 0;
		assert(apicmsr(0, readonly[i], &v) == -1);
	}
	for(i = 0; i < nelem(reserved); i++){
		v = 0;
		assert(apicmsr(0, reserved[i], &v) == -1);
		assert(apicmsr(1, reserved[i], &v) == -1);
	}
	assert(apicmsr(1, 0x80b, &v) == -1 && apicmsr(1, 0x83f, &v) == -1);
	v = 1; assert(apicmsr(0, 0x80b, &v) == -1);
	v = 0x100; assert(apicmsr(0, 0x808, &v) == -1);
	v = 1ULL<<32; assert(apicmsr(0, 0x838, &v) == -1);
	v = 1<<20; assert(apicmsr(0, 0x830, &v) == -1);
	putmsr(0x832, 0x1000 | 0x52);
	assert(getmsr(0x832) == 0x52);
	putmsr(0x835, 0x5000 | 0x53);
	assert(getmsr(0x835) == 0x53);
	putmsr(0x83f, 15);
	assert(getmsr(0x828) == 1<<5 && cpu->irqactive == -1);
	putmsr(0x828, 0);
	assert(getmsr(0x828) == 0);
	/* INIT keeps x2APIC mode and the derived IDs, while resetting the LVT. */
	apicreset(cpu);
	assert(getmsr(0x802) == 17 && getmsr(0x80d) == 0x10002);
	assert(getmsr(0x80f) == 0xff && getmsr(0x832) == 1<<16);
	for(i = 0; i < ncpu; i++){
		cpu = cpus + i;
		v = (uvlong)Lapicbase | 0x800;
		assert(apicmsr(0, 0x1b, &v) == -1);
		putmsr(0x1b, Lapicbase);
		assert(apicmsr(1, 0x802, &v) == -1);
		v = (uvlong)Lapicbase | 0xc00;
		assert(apicmsr(0, 0x1b, &v) == -1);
		cpu->id = i;
		putmsr(0x1b, (uvlong)Lapicbase | 0x800);
		assert(readreg(0x20) == i<<24 && readreg(0xd0) == 0);
	}
	print("PASS: x2APIC MSRs, destinations, broadcast, SELF IPI, faults and INIT\n");
}

static u32int
ioreg(int index, u32int value, int wr)
{
	Region *r;
	u32int sel;

	r = regptr(Ioapicbase);
	sel = index;
	assert(r->mmio(Ioapicbase, &sel, 4, 1) == 0);
	assert(r->mmio(Ioapicbase+16, &value, 4, wr) == 0);
	return value;
}

static void
iointerrupts(void)
{
	writereg(0xf0, 0x1ff);
	/* Active-low PCI input: high is idle; EOI retriggers an asserted line. */
	ioapicline(5, 1);
	ioreg(0x1b, cpu->id<<24, 1);
	ioreg(0x1a, 1<<15 | 1<<13 | 0x60, 1);
	assert(cpu->irqactive == -1);
	ioapicline(5, 0);
	assert(cpu->irqactive == 0x60 && (ioreg(0x1a, 0, 0) & 1<<14) != 0);
	apicack(cpu, 0x60); writereg(0xb0, 0);
	assert(cpu->irqactive == 0x60);
	ioapicline(5, 1);
	apicack(cpu, 0x60); writereg(0xb0, 0);
	assert(cpu->irqactive == -1 && (ioreg(0x1a, 0, 0) & 1<<14) == 0);
	/* Edge inputs do not retrigger while held high. */
	ioreg(0x1d, cpu->id<<24, 1);
	ioreg(0x1c, 0x61, 1);
	ioapicline(6, 1);
	assert(cpu->irqactive == 0x61);
	apicack(cpu, 0x61); writereg(0xb0, 0);
	ioapicline(6, 1);
	assert(cpu->irqactive == -1);
	ioapicline(6, 0);
	ioapicline(6, 1);
	assert(cpu->irqactive == 0x61);
	apicack(cpu, 0x61); writereg(0xb0, 0);
	/* Logical destination matching uses the programmed local destination. */
	writereg(0xd0, 2<<24);
	writereg(0x310, 2<<24);
	writereg(0x300, 1<<11 | 0x62);
	assert(cpu->irqactive == 0x62 && cpus[0].irqactive == -1);
	apicack(cpu, 0x62); writereg(0xb0, 0);
}

static u32int
vioreg(VIODev *v, int read, int off, u32int value, int size)
{
	return v->pci->bar[0].io(read,off,value,size,v);
}

static void
virtiotransport(void)
{
	VIODev *v;
	VIOQueue *q;
	VIOBuf *b;
	uchar *p, buf[8];
	char request[] = "request", response[] = "response";
	uvlong base;

	mainid = threadid();
	v = mkviodev(0x1050,0x030000,16,1);
	v->pci->irqno = 255;
	v->devfeat = 1;
	viomodern(v);
	q = mkvioqueue(v,8,viowakeup);
	assert(vioreg(v,1,4,0,4) == 1);
	vioreg(v,0,0,1,4);
	assert(vioreg(v,1,4,0,4) == 1);
	vioreg(v,0,12,2,4);
	vioreg(v,0,20,11,1);
	assert((vioreg(v,1,20,0,1)&8) == 0);
	vioreg(v,0,12,1,4);
	vioreg(v,0,8,1,4);
	vioreg(v,0,12,1,4);
	vioreg(v,0,20,11,1);
	assert(vioreg(v,1,20,0,1) == 11);
	p = code->v;
	base = code->start;
	memset(p,0,BY2PG);
	vioreg(v,0,32,base,4);
	vioreg(v,0,40,base+256,4);
	vioreg(v,0,48,base+320,4);
	vioreg(v,0,36,1,4);
	vioreg(v,0,28,1,2);
	assert(vioreg(v,1,28,0,2) == 0);
	vioreg(v,0,36,0,4);
	vioreg(v,0,32,base+1,4);
	vioreg(v,0,28,1,2);
	assert(vioreg(v,1,28,0,2) == 0);
	vioreg(v,0,32,base,4);
	vioreg(v,0,24,3,2);
	vioreg(v,0,28,1,2);
	assert(vioreg(v,1,28,0,2) == 0);
	vioreg(v,0,24,8,2);
	vioreg(v,0,28,1,2);
	assert(vioreg(v,1,28,0,2) == 1);
	/* An enabled queue's configuration is immutable until reset. */
	vioreg(v,0,32,base+16,4);
	vioreg(v,0,24,4,2);
	assert(vioreg(v,1,32,0,4) == base && vioreg(v,1,24,0,2) == 8);
	vioreg(v,0,20,15,1);
	PUT64(p,0,base+512); PUT32(p,8,8); PUT16(p,12,BUFCHAIN); PUT16(p,14,1);
	PUT64(p,16,base+528); PUT32(p,24,8); PUT16(p,28,BUFWR);
	memmove(p+512,request,8);
	PUT16(p,256,USEDNOIRQ);
	PUT16(p,258,1);
	b = viogetbuf(q,0);
	assert(b != nil && vioqread(b,buf,8) == 8 && memcmp(buf,request,8) == 0);
	assert(vioqwrite(b,response,8) == 8);
	vioputbuf(b);
	assert(q->livebuf == 0 && GET16(p,322) == 1 && GET32(p,328) == 8);
	assert(memcmp(p+528,response,8) == 0);
	/* Bad heads and cycles must not dereference nil or build infinite chains. */
	PUT16(p,262,99); PUT16(p,258,2);
	assert(viogetbuf(q,0) == nil);
	PUT16(p,14,0); PUT16(p,258,3);
	assert(viogetbuf(q,0) == nil);
	PUT16(p,14,1); PUT16(p,258,4);
	b = viogetbuf(q,0);
	assert(b != nil && q->livebuf == 1);
	v->devstat = 0;
	vioputbuf(b);
	assert(q->livebuf == 0 && GET16(p,322) == 1);
	vioreg(v,0,20,0,1);
	assert(q->desc == nil && q->enabled == 0 && q->size == 8);
	assert(v->guestfeat == 0 && v->dfsel == 0 && v->gfsel == 0 && v->qsel == 0);
	print("PASS: modern VirtIO negotiation, split queues, malformed chains and reset completion\n");
}

void
threadmain(int, char **)
{
	int i;

	ncpu = 2;
	cpus = emalloc(ncpu * sizeof(Vcpu));
	for(i = 0; i < ncpu; i++){
		cpus[i].id = i;
		cpus[i].state = VMRUNNING;
		cpus[i].irqactive = -1;
		cpus[i].ctlfd = open("/dev/null", OWRITE);
		assert(cpus[i].ctlfd >= 0);
	}
	cpu = cpus + 1;
	registers();
	halted();
	apicinit();
	lapic = regptr(Lapicbase);
	code = mkregion(0x80001000, 0x80002000, REGRWX);
	code->v = emalloc(BY2PG);
	code->ve = (uchar*)code->v + BY2PG;
	decoder();
	msrs();
	interrupts();
	iointerrupts();
	x2apic();
	virtiotransport();
	print("PASS: MMIO decoding, MSRs, APIC priority, timers, IPI and I/O APIC\n");
	threadexitsall(nil);
}
