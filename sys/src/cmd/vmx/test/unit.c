/* Use the monitor's actual memory map and register cache, without starting VMs. */
#define threadmain vmxmain
#include "../vmx.c"
#undef threadmain
#include "../virtio.h"

static uvlong now;
static Region *code, *lapic;

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
	apicinit();
	lapic = regptr(Lapicbase);
	code = mkregion(0x80001000, 0x80002000, REGRWX);
	code->v = emalloc(BY2PG);
	code->ve = (uchar*)code->v + BY2PG;
	decoder();
	msrs();
	interrupts();
	iointerrupts();
	virtiotransport();
	print("PASS: MMIO decoding, MSRs, APIC priority, timers, IPI and I/O APIC\n");
	threadexitsall(nil);
}
