#include <u.h>
#include <libc.h>
#include <tos.h>
#include "dat.h"
#include "fns.h"
#include "x86.h"

int persist = 1;

typedef struct ExitInfo ExitInfo;
struct ExitInfo {
	char *raw;
	char *name;
	uvlong qual;
	uvlong pa, va;
	u32int ilen, iinfo;
};

char *x86reg[16] = {
	RAX, RCX, RDX, RBX,
	RSP, RBP, RSI, RDI,
	R8, R9, R10, R11,
	R12, R13, R14, R15
};
char *x86segreg[8] = {
	"cs", "ds", "es", "fs", "gs", "ss",
};

static void
skipinstr(ExitInfo *ei)
{
	rset(RPC, rget(RPC) + ei->ilen);
}

static void
iohandler(ExitInfo *ei)
{
	int port, len, inc, isin;
	int asz, seg;
	uintptr addr;
	u32int val;
	uvlong vval;
	uintptr cx;
	static int seglook[8] = {SEGES, SEGCS, SEGSS, SEGDS, SEGFS, SEGGS};
	TLB tlb;
	
	port = ei->qual >> 16 & 0xffff;
	len = (ei->qual & 7) + 1;
	isin = (ei->qual & 8) != 0;
	if((ei->qual & 1<<4) == 0){ /* not a string instruction */
		if(isin){
			val = io(1, port, 0, len);
			rsetsz(RAX, val, len);
		}else
			io(0, port, rget(RAX), len);
		skipinstr(ei);
		return;
	}
	if((rget("flags") & 0x400) != 0) inc = -len;
	else inc = len;
	switch(ei->iinfo >> 7 & 7){
	case 0: asz = 2; break;
	default: asz = 4; break;
	case 2: asz = 8; break;
	}
	if((ei->qual & 1<<5) != 0)
		cx = rgetsz(RCX, asz);
	else
		cx = 1;
	addr = isin ? rget(RDI) : rget(RSI);
	if(isin)
		seg = SEGES;
	else
		seg = seglook[ei->iinfo >> 15 & 7];
	memset(&tlb, 0, sizeof(TLB));
	for(; cx > 0; cx--){
		if(isin){
			vval = io(1, port, 0, len);
			if(x86access(seg, addr, asz, &vval, len, ACCW, &tlb) < 0)
				goto err;
		}else{
			if(x86access(seg, addr, asz, &vval, len, ACCR, &tlb) < 0)
				goto err;
			io(0, port, vval, len);
		}
		addr += inc;
	}
	skipinstr(ei);
err:
	if((ei->qual & 1<<5) != 0)
		rsetsz(RCX, cx, asz);
	if(isin)
		rsetsz(RDI, addr, asz);
	else
		rsetsz(RSI, addr, asz);
}

static uvlong
defaultmmio(int op, uvlong addr, uvlong val)
{
	switch(op){
	case MMIORD:
		vmerror("read from unmapped address %#ullx (pc=%#ullx)", addr, rget(RPC));
		break;
	case MMIOWR:
		vmerror("write to unmapped address %#ullx (val=%#ullx,pc=%#ullx)", addr, val, rget(RPC));
		break;
	}
	return 0;
}

static void
eptfault(ExitInfo *ei)
{
	if(x86step() > 0)
		skipinstr(ei);
}

typedef struct CPUID CPUID;
struct CPUID {
	u32int ax, bx, cx, dx;
};
static u32int cpuidmax;
static u32int cpuidmaxext;
static CPUID leaf1;
static struct {
	uvlong miscen;
}msr;

static uchar _cpuid[] = {
	0x5E,			/* POP SI (PC) */
	0x5D,			/* POP BP (CPUID&) */
	0x58,			/* POP AX */
	0x59,			/* POP CX */

	0x51,			/* PUSH CX */
	0x50,			/* PUSH AX */
	0x55,			/* PUSH BP */
	0x56,			/* PUSH SI */

	0x31, 0xDB,		/* XOR BX, BX */
	0x31, 0xD2,		/* XOR DX, DX */

	0x0F, 0xA2,		/* CPUID */

	0x89, 0x45, 0x00,	/* MOV AX, 0(BP) */
	0x89, 0x5d, 0x04,	/* MOV BX, 4(BP) */
	0x89, 0x4d, 0x08,	/* MOV CX, 8(BP) */
	0x89, 0x55, 0x0C,	/* MOV DX, 12(BP) */
	0xC3,			/* RET */
};

static CPUID (*getcpuid)(ulong ax, ulong cx) = (CPUID(*)(ulong, ulong)) _cpuid;

void
cpuidinit(void)
{
	CPUID r;
	int f;

	if(sizeof(uintptr) == 8) /* patch out POP BP -> POP AX */
		_cpuid[1] = 0x58;
	segflush(_cpuid, sizeof(_cpuid));

	r = getcpuid(0, 0);
	cpuidmax = r.ax;
	r = getcpuid(0x80000000, 0);
	cpuidmaxext = r.ax;
	leaf1 = getcpuid(1, 0);

	memset(&msr, 0, sizeof(msr));
	if((f = open("/dev/msr", OREAD)) >= 0){
		pread(f, &msr.miscen, 8, 0x1a0);
		msr.miscen &= 1<<0; /* fast strings */
		close(f);
	}
}

static int xsavesz[] = {
	[1] = 512+64,
	[3] = 512+64,
	[7] = 512+64+256,
};

u32int
cpusignature(void)
{
	return leaf1.ax;
}

static void
cpuid(ExitInfo *ei)
{
	u32int ax, bx, cx, dx;
	CPUID cp;

	ax = rget(RAX);
	cx = rget(RCX);
	bx = dx = 0;
	cp = getcpuid(ax, cx);
	switch(ax){
	case 0x00: /* highest register & GenuineIntel */
		ax = MIN(MAX(cpuidmax, ncpu > 1 ? 0x15 : 0), 0x18);
		bx = cp.bx;
		dx = cp.dx;
		cx = cp.cx;
		break;
	case 0x01: /* features */
		ax = cp.ax;
		bx = cp.bx & 0xffff;
		if(ncpu > 1) bx |= ncpu<<16 | cpu->id<<24;
		/* some features removed, hypervisor added */
		cx = cp.cx & 0x76de3217 | 0x80000000UL;
		if(ncpu > 1) cx |= 1<<21; /* x2APIC */
		dx = cp.dx & 0x0f8aa579;
		if(ncpu > 1) dx |= 1<<9 | 1<<28;
		if(leaf1.cx & 1<<27){
			if(rget("cr4real") & Cr4Osxsave)
				cx |= 1<<27;
		}else{
			cx &= ~0x1c000000;
		}
		break;
	case 0x02: goto literal; /* cache stuff */
	case 0x03: goto zero; /* processor serial number */
	case 0x04:
		ax = cp.ax & ~0xffffc000U;
		if((ax & 31) != 0 && ncpu > 1) ax |= (ncpu-1)<<26;
		bx = cp.bx; cx = cp.cx; dx = cp.dx;
		break;
	case 0x05: goto zero; /* monitor/mwait */
	case 0x06: goto zero; /* thermal management */
	case 0x07: /* more features */
		if(cx == 0){
			ax = 0;
			bx = cp.bx & 0x2369;
			if(rhave("tscadjust")) bx |= 1<<1;
			cx = 0;
			if((leaf1.cx & 1<<27) == 0)
				bx &= ~0xdc230020;
		}else{
			goto zero;
		}
		break;
	case 0x08: goto zero;
	case 0x09: goto literal; /* direct cache access */
	case 0x0a: goto zero; /* performance counters */
	case 0x0b:
	case 0x1f:
		if(ncpu == 1) goto zero;
		dx = cpu->id;
		if(cx == 0){
			ax = 0; bx = 1; cx = 1<<8;
		}else if(cx == 1){
			for(ax = 0; (1U<<ax) < ncpu; ax++)
				;
			bx = ncpu; cx = 2<<8 | 1;
		}else{
			ax = bx = 0; cx &= 255;
		}
		break;
	case 0x0c: goto zero;
	case 0x0d: /* extended state */
		if((leaf1.cx & 1<<27) == 0)
			goto zero;
		if(cx == 0){ /* main leaf */
			ax = cp.ax & 7; /* x87, sse, avx */
			bx = xsavesz[rget("xcr0")]; /* current xsave size */
			cx = xsavesz[ax]; /* max xsave size */
		}else if(cx == 1){ /* sub leaf */
			ax = cp.ax & 7; /* xsaveopt, xsavec, xgetbv1 */
			bx = xsavesz[rget("xcr0")];
			cx = 0;
		}else if(cx == 2){
			ax = xsavesz[7] - xsavesz[3];
			bx = xsavesz[3];
			cx = 0;
		}else{
			goto zero;
		}
		break;
	case 0x0f: goto zero; /* RDT */
	case 0x10: goto zero; /* RDT */
	case 0x12: goto zero; /* SGX */
	case 0x14: goto zero; /* PT */
	case 0x15: /* TSC and local APIC share the virtual crystal clock. */
		if(ncpu == 1 || _tos->cyclefreq == 0) goto zero;
		/* MHz resolution keeps the ratio small, as on physical CPUs. */
		ax = Apichz / 1000000;
		bx = (_tos->cyclefreq + 500000) / 1000000;
		cx = Apichz;
		break;
	case 0x16: goto zero; /* cpu clock */
	case 0x17: goto zero; /* SoC */
	case 0x18: goto literal; /* pages, tlb */

	case 0x40000000: /* hypervisor */
		ax = 0;
		bx = 0x4b4d564b; /* act as KVM */
		cx = 0x564b4d56;
		dx = 0x4d;
		break;

	case 0x80000000: /* highest register */
		ax = MIN(cpuidmaxext, 0x80000008);
		cx = 0;
		break;
	case 0x80000001: /* signature & ext features */
		ax = cp.ax;
		cx = cp.cx & 0x121;
		if(sizeof(uintptr) == 8)
			dx = cp.dx & 0x24100800;
		else
			dx = cp.dx & 0x04100000;
		break;
	case 0x80000002: goto literal; /* brand string */
	case 0x80000003: goto literal; /* brand string */
	case 0x80000004: goto literal; /* brand string */
	case 0x80000005: goto zero; /* reserved */
	case 0x80000006: goto literal; /* cache info */
	case 0x80000007: /* invariant TSC, when the host provides it */
		ax = bx = cx = 0;
		dx = cp.dx & 1<<8;
		break;
	case 0x80000008: goto literal; /* address bits */
	literal:
		ax = cp.ax;
		bx = cp.bx;
		cx = cp.cx;
		dx = cp.dx;
		break;
	default:
		if((ax & 0xf0000000) != 0x40000000)
			vmdebug("unknown cpuid field eax=%#ux", ax);
	zero:
		ax = cx = 0;
		break;
	}
	rset(RAX, ax);
	rset(RBX, bx);
	rset(RCX, cx);
	rset(RDX, dx);
	skipinstr(ei);
}

static void
rdwrmsr(ExitInfo *ei)
{
	u32int cx;
	u64int val;
	int rd, rc;
	char *reg;
	
	rd = ei->name[1] == 'r';
	cx = rget(RCX);
	val = rget(RDX) << 32 | (u32int)rget(RAX);
	rc = apicmsr(rd, cx, &val);
	if(rc < 0)
		return;
	if(rc == 0)
	switch(cx){
	case 0x10: /* IA32_TIME_STAMP_COUNTER */
	case 0x3B: /* IA32_TSC_ADJUST */
		reg = cx == 0x10 ? "tsc" : "tscadjust";
		if(!rhave(reg)) goto unknown;
		if(rd) val = rget(reg);
		else if(cx == 0x10){
			/* TSC keeps running: even writing the cached value has an effect. */
			rcflush(0);
			if(fprint(cpu->regsfd, "%s %#ullx\n", reg, val) < 0)
				sysfatal("write tsc: %r");
		}else rset(reg, val);
		break;
	case 0x277:
		if(rd) val = rget("pat");
		else rset("pat", val);
		break;
	case 0x8B: val = 0; break; /* microcode update */
	case 0x1A0: /* IA32_MISC_ENABLE */
		if(rd) val = msr.miscen;
		break;
	default:
	unknown:
		if(rd){
			vmdebug("read from unknown MSR %#ux ignored", cx);
			val = 0;
		}else
			vmdebug("write to unknown MSR %#ux ignored (val=%#ullx)", cx, val);
		break;
	}
	if(rd){
		rset(RAX, (u32int)val);
		rset(RDX, (u32int)(val >> 32));
	}
	skipinstr(ei);
}

static void
movdr(ExitInfo *ei)
{
	static char *dr[8] = { "dr0", "dr1", "dr2", "dr3", nil, nil, "dr6", "dr7" };
	int q;
	
	q = ei->qual;
	if((q & 6) == 4){
		postexc("#gp", 0);
		return;
	}
	if((q & 16) != 0)
		rset(x86reg[q >> 8 & 15], rget(dr[q & 7]));
	else
		rset(dr[q & 7], rget(x86reg[q >> 8 & 15]));
	skipinstr(ei);
}

static void
movcr(ExitInfo *ei)
{
	u32int q;
	
	q = ei->qual;
	switch(q & 15){
	case 0:
		switch(q >> 4 & 3){
		case 0:
			vmdebug("illegal CR0 write, value %#ux", (u32int)rget(x86reg[q >> 8 & 15]));
			rset("cr0real", rget(x86reg[q >> 8 & 15]));
			skipinstr(ei);
			break;
		case 1:
			vmerror("shouldn't happen: trap on MOV from CR0");
			rset(x86reg[q >> 8 & 15], rget("cr0fake"));
			skipinstr(ei);
			break;
		case 2:
			vmerror("shouldn't happen: trap on CLTS");
			rset("cr0real", rget("cr0real") & ~8);
			skipinstr(ei);
			break;
		case 3:
			vmerror("LMSW handler unimplemented");
			postexc("#ud", NOERRC);
		}
		break;
	case 8:
		if(ncpu == 1){
			postexc("#gp", 0);
			break;
		}
		if((q & 0x30) == 0x10)
			rset(x86reg[q >> 8 & 15], cpu->apic.reg[8] >> 4);
		else if((q & 0x30) == 0 && rget(x86reg[q >> 8 & 15]) < 16)
			apictpr(rget(x86reg[q >> 8 & 15]) << 4);
		else{
			postexc("#gp", 0);
			break;
		}
		skipinstr(ei);
		break;
	case 4:
		switch(q >> 4 & 3){
		case 0:
			vmdebug("illegal CR4 write, value %#ux", (u32int)rget(x86reg[q >> 8 & 15]));
			rset("cr4real", rget(x86reg[q >> 8 & 15]));
			skipinstr(ei);
			break;
		case 1:
			vmerror("shouldn't happen: trap on MOV from CR4");
			rset(x86reg[q >> 8 & 15], rget("cr3fake"));
			skipinstr(ei);
			break;
		default:
			vmerror("unknown CR4 operation %d", q);
			postexc("#ud", NOERRC);
		}
		break;
	default:
		vmerror("access to unknown control register CR%ud", q & 15);
		postexc("#ud", NOERRC);
	}
}

static void
dbgexc(ExitInfo *ei)
{
	rset("dr6", rget("dr6") | ei->qual);
	postexc("#db", NOERRC);
}

static void
hlt(ExitInfo *ei)
{
	if(cpu->nmi == 0 && (cpu->irqactive < 0 || (rget(RFLAGS) & 1<<9) == 0))
		cpu->state = VMHALT;
	skipinstr(ei);
}

static void
irqackhand(ExitInfo *ei)
{
	apicack(cpu, ei->qual);
}

static void
nmiackhand(ExitInfo *)
{
	apicnmiack(cpu);
}

static void
xsetbv(ExitInfo *ei)
{
	uvlong v;

	/* this should also #ud if LOCK prefix is used */

	v = rget(RAX)&0xffffffff | rget(RDX)<<32;
	if(rget(RCX) & 0xffffffff)
		postexc("#gp", 0);
	else if(v != 1 && v != 3 && v != 7)
		postexc("#gp", 0);
	else if((leaf1.cx & 1<<26) == 0 || (rget("cr4real") & Cr4Osxsave) == 0)
		postexc("#ud", NOERRC);
	else{
		rset("xcr0", v);
		skipinstr(ei);
	}
}

typedef struct ExitType ExitType;
struct ExitType {
	char *name;
	void (*f)(ExitInfo *);
};
static ExitType etypes[] = {
	{"io", iohandler},
	{".cpuid", cpuid},
	{".hlt", hlt},
	{"eptfault", eptfault},
	{"*ack", irqackhand},
	{"*nmi", nmiackhand},
	{".rdmsr", rdwrmsr},
	{".wrmsr", rdwrmsr},
	{".movdr", movdr},
	{"#db", dbgexc},
	{"movcr", movcr},
	{".xsetbv", xsetbv},
};

void
processexit(char *msg)
{
	static char msgc[1024];
	char *f[32];
	int nf;
	ExitType *et;
	int i;
	ExitInfo ei;

	strcpy(msgc, msg);
	nf = tokenize(msgc, f, nelem(f));
	if(nf < 2) sysfatal("invalid wait message: %s", msg);
	memset(&ei, 0, sizeof(ei));
	ei.raw = msg;
	ei.name = f[0];
	ei.qual = strtoull(f[1], nil, 0);
	for(i = 2; i < nf; i += 2){
		if(strcmp(f[i], "pc") == 0)
			rpoke(RPC, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "sp") == 0)
			rpoke(RSP, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "ax") == 0)
			rpoke(RAX, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "flags") == 0)
			rpoke(RFLAGS, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "cx") == 0)
			rpoke(RCX, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "dx") == 0)
			rpoke(RDX, strtoull(f[i+1], nil, 0), 1);
		else if(strcmp(f[i], "ilen") == 0)
			ei.ilen = strtoul(f[i+1], nil, 0);
		else if(strcmp(f[i], "iinfo") == 0)
			ei.iinfo = strtoul(f[i+1], nil, 0);
		else if(strcmp(f[i], "pa") == 0)
			ei.pa = strtoull(f[i+1], nil, 0);
		else if(strcmp(f[i], "va") == 0)
			ei.va = strtoull(f[i+1], nil, 0);
	}
	for(et = etypes; et < etypes + nelem(etypes); et++)
		if(strcmp(et->name, f[0]) == 0){
			et->f(&ei);
			return;
		}
	if(*f[0] == '.'){
		vmerror("vmx: unknown instruction %s", f[0]+1);
		postexc("#ud", NOERRC);
		return;
	}
	if(*f[0] == '*'){
		vmerror("vmx: unknown notification %s", f[0]+1);
		return;
	}
	if(persist){
		vmerror("unknown exit: %s", msg);
		cpu->state = VMDEAD;
	}else
		sysfatal("unknown exit: %s", msg);
}
