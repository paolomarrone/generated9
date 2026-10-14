#include <u.h>
#include <libc.h>
#include "dat.h"
#include "fns.h"

/* APIC registers, indexed by their 16-byte MMIO slots or x2APIC MSRs. */
enum {
	Id = 0x02, Version = 0x03, Tpr = 0x08, Ppr = 0x0a,
	Eoi = 0x0b, Ldr = 0x0d, Dfr = 0x0e, Svr = 0x0f,
	Isr = 0x10, Tmr = 0x18, Irr = 0x20, Esr = 0x28,
	Icrlo = 0x30, Icrhi = 0x31, Timer = 0x32,
	Thermal = 0x33, Perf = 0x34, Lint0 = 0x35, Lint1 = 0x36,
	Error = 0x37, Initial = 0x38, Current = 0x39, Divide = 0x3e, Self = 0x3f,
	Mask = 1<<16, Periodic = 1<<17, Enabled = 1<<8,
	X2 = 1<<10, Global = 1<<11,
	Npin = 24,
	Nmipending = 1, Nmiactive = 2,
	/* 100 MHz local APIC clock, before the programmable divider. */
	Tick = 1000000000 / Apichz,
};

static int picvec = -1;
static struct {
	u32int sel, id;
	uvlong redir[Npin];
	u32int lines;
} ioapic;

static int
highest(u32int *p)
{
	int i, j;

	for(i = 7; i >= 0; i--)
		if(p[i] != 0){
			for(j = 31; (p[i] & 1U<<j) == 0; j--)
				;
			return i*32 + j;
		}
	return -1;
}

static int
priority(Vcpu *c)
{
	int v, t;

	v = highest(c->apic.reg + Isr);
	t = c->apic.reg[Tpr];
	return v >= 0 && (v & 0xf0) > (t & 0xf0) ? v & 0xf0 : t;
}

static void
wake(Vcpu *c, int nmi)
{
	Vcpu *old;

	if(c->state != VMHALT)
		return;
	old = cpu;
	cpu = c;
	if(nmi || (rget(RFLAGS) & 1<<9) != 0)
		c->state = VMRUNNING;
	cpu = old;
}

void
apicupdate(Vcpu *c)
{
	int v, pic;

	if(c->state == VMINIT || c->state == VMDEAD)
		return;
	if(c->nmi == Nmipending){
		if(fprint(c->ctlfd, "nmi") < 0)
			sysfatal("cpu%d: nmi: %r", c->id);
		c->nmi = Nmiactive;
		wake(c, 1);
	}
	v = -1;
	pic = 0;
	if(ncpu > 1 && (c->apic.base & 1<<11) != 0 && (c->apic.reg[Svr] & Enabled) != 0){
		v = highest(c->apic.reg + Irr);
		if(v >= 0 && (v & 0xf0) <= (priority(c) & 0xf0))
			v = -1;
	}
	if(v < 0 && c == cpus && picvec >= 0 &&
	   (ncpu == 1 || (c->apic.base & 1<<11) == 0 || (c->apic.reg[Svr] & Enabled) == 0 ||
	    (c->apic.reg[Lint0] & (Mask|0x700)) == 0x700)){
		v = picvec;
		pic = 1;
	}
	/* A running CPU may already have accepted its offered interrupt. */
	if(c->irqactive >= 0 && c->getexit != 0)
		return;
	if(c->irqactive == v && c->irqpic == pic)
		return;
	if(v < 0){
		if(fprint(c->ctlfd, "irq") < 0)
			sysfatal("cpu%d: cancel irq: %r", c->id);
	}else{
		if(fprint(c->ctlfd, "irq %d", v) < 0)
			sysfatal("cpu%d: irq: %r", c->id);
		wake(c, 0);
	}
	c->irqactive = v;
	c->irqpic = pic;
}

void
apicpic(int v)
{
	picvec = v;
	apicupdate(cpus);
}

static void
request(Vcpu *c, int v, int level)
{
	if(v < 16){
		c->apic.reg[Esr] |= 1<<6;
		return;
	}
	c->apic.reg[Irr + v/32] |= 1U<<(v&31);
	if(level)
		c->apic.reg[Tmr + v/32] |= 1U<<(v&31);
	else
		c->apic.reg[Tmr + v/32] &= ~(1U<<(v&31));
	apicupdate(c);
}

void
apicack(Vcpu *c, int v)
{
	if(c->irqactive != v)
		sysfatal("cpu%d: unexpected irq acknowledgement %d (pending %d)", c->id, v, c->irqactive);
	c->irqactive = -1;
	if(c->irqpic)
		irqack(v);
	else{
		c->apic.reg[Irr + v/32] &= ~(1U<<(v&31));
		c->apic.reg[Isr + v/32] |= 1U<<(v&31);
	}
	apicupdate(c);
}

static int
matches(Vcpu *c, u32int dest, int logical)
{
	int l;

	if(dest == ~0U)
		return 1;
	if(!logical)
		return dest == c->id;
	if((c->apic.base & X2) != 0)
		return dest >> 16 == c->id >> 4 && (dest & 1U<<(c->id&15)) != 0;
	l = c->apic.reg[Ldr] >> 24;
	if(c->apic.reg[Dfr] >> 28 == 15)
		return (l & dest) != 0;
	return (l & 0xf0) == (dest & 0xf0) && (l & dest & 15) != 0;
}

void
apicnmiack(Vcpu *c)
{
	c->nmi &= ~Nmiactive;
	apicupdate(c);
}

static int
deliver(u32int icr, u32int dest, Vcpu *sender)
{
	Vcpu *c, *best;
	int mode, shorthand, match, n;

	mode = icr >> 8 & 7;
	if(mode == 5 || mode == 6)
		vmdebug("apic: cpu%d ICR %#ux dest %#ux", sender == nil ? -1 : sender->id, icr, dest);
	shorthand = icr >> 18 & 3;
	best = nil;
	n = 0;
	for(c = cpus; c < cpus + ncpu; c++){
		switch(shorthand){
		default: match = matches(c, dest, icr & 1<<11); break;
		case 1: match = c == sender; break;
		case 2: match = 1; break;
		case 3: match = c != sender; break;
		}
		if(!match)
			continue;
		if(mode <= 1 && (c->state == VMINIT || (c->apic.base & 1<<11) == 0 ||
		   (c->apic.reg[Svr] & Enabled) == 0))
			continue;
		switch(mode){
		case 0:
			request(c, icr & 255, icr & 1<<15);
			break;
		case 1:
			if(best == nil || priority(c) < priority(best))
				best = c;
			break;
		case 4:
			if(c->state == VMINIT)
				continue;
			c->nmi |= Nmipending;
			apicupdate(c);
			break;
		case 5:
			/* INIT deassert does not reset the target. */
			if((icr & (1<<15|1<<14)) != 1<<15)
				cpustart(c, -1);
			break;
		case 6:
			if(c->state == VMINIT)
				cpustart(c, icr & 255);
			break;
		default:
			vmerror("apic: unsupported delivery mode %d", mode);
			continue;
		}
		n++;
	}
	if(best != nil)
		request(best, icr & 255, icr & 1<<15);
	return n;
}

static void
ioapicdeliver(int pin)
{
	uvlong r;

	r = ioapic.redir[pin];
	if((r & Mask) != 0 || (r & (1<<15|1<<14)) == (1<<15|1<<14))
		return;
	if(deliver(r, r >> 56 == 255 ? ~0U : (u32int)(r >> 56), nil) != 0 &&
	   (r & 1<<15) != 0 && (r >> 8 & 7) <= 1)
		ioapic.redir[pin] |= 1<<14;
}

static int
ioapicactive(int pin)
{
	return (ioapic.lines >> pin & 1) ^ (ioapic.redir[pin] >> 13 & 1);
}

void
ioapicline(int pin, int level)
{
	int old;

	if(ncpu == 1)
		return;
	old = ioapicactive(pin);
	if(level)
		ioapic.lines |= 1U<<pin;
	else
		ioapic.lines &= ~(1U<<pin);
	if(ioapicactive(pin) && (!old || (ioapic.redir[pin] & 1<<15) != 0))
		ioapicdeliver(pin);
}

static void
endinterrupt(Vcpu *c)
{
	int v, i;

	v = highest(c->apic.reg + Isr);
	if(v < 0)
		return;
	c->apic.reg[Isr + v/32] &= ~(1U<<(v&31));
	if((c->apic.reg[Tmr + v/32] & 1U<<(v&31)) != 0){
		for(i = 0; i < Npin; i++)
			if((ioapic.redir[i] & (1<<14|255)) == (1<<14|v)){
				ioapic.redir[i] &= ~(1ULL<<14);
				if(ioapicactive(i))
					ioapicdeliver(i);
			}
	}
	apicupdate(c);
}

static uvlong
period(Lapic *a)
{
	int d;

	d = a->reg[Divide];
	d = ((d & 3) | (d >> 1 & 4)) + 1;
	return Tick * (1 << (d & 7));
}

static u32int
current(Lapic *a, uvlong now)
{
	uvlong elapsed;
	u32int n;

	n = a->count;
	if(n == 0)
		return 0;
	elapsed = (now - a->epoch) / period(a);
	if(elapsed >= n && (a->reg[Timer] & Periodic) != 0)
		return a->reg[Initial] - (elapsed - n) % a->reg[Initial];
	return elapsed >= n ? 0 : n - elapsed;
}

void
apicadvance(void)
{
	Vcpu *c;
	Lapic *a;
	uvlong now, duration, end;

	if(ncpu == 1)
		return;
	now = nanosec();
	for(c = cpus; c < cpus + ncpu; c++){
		a = &c->apic;
		if(a->count == 0)
			continue;
		duration = a->count * period(a);
		end = a->epoch + duration;
		if(now >= end){
			if((a->reg[Timer] & Periodic) != 0){
				a->count = a->reg[Initial];
				duration = a->count * period(a);
				a->epoch = end + (now - end) / duration * duration;
				end = a->epoch + duration;
			}else
				a->count = 0;
			if((a->reg[Timer] & Mask) == 0)
				request(c, a->reg[Timer] & 255, 0);
		}
		if(a->count != 0)
			settimer(end);
	}
}

void
apictpr(u32int value)
{
	cpu->apic.reg[Tpr] = value & 255;
	apicupdate(cpu);
}

static u32int
lapicread(int r)
{
	Lapic *a;

	a = &cpu->apic;
	if((a->base & X2) != 0){
		if(r == Id) return cpu->id;
		if(r == Ldr) return (cpu->id >> 4)<<16 | 1U<<(cpu->id&15);
		if(r == Svr) return a->reg[r] & 0x1ff;
	}
	if(r == Ppr) return priority(cpu);
	if(r == Current) return current(a, nanosec());
	return a->reg[r];
}

static void
lapicwrite(int r, u32int v)
{
	Lapic *a;
	u32int dest;
	int i;
	uvlong now;

	a = &cpu->apic;
	if(r == Timer || r == Initial || r == Divide)
		apicadvance();
	switch(r){
	case Tpr: apictpr(v); break;
	case Eoi: endinterrupt(cpu); break;
	case Ldr: a->reg[r] = v & 0xff000000; break;
	case Dfr: a->reg[r] = v | 0x0fffffff; break;
	case Svr:
		a->reg[r] = v & 0x3ff;
		if((v & Enabled) == 0)
			for(i = Timer; i <= Error; i++)
				a->reg[i] |= Mask;
		apicupdate(cpu);
		break;
	case Esr: a->reg[r] = 0; break;
	case Icrhi: a->reg[r] = v & 0xff000000; break;
	case Icrlo:
		a->reg[r] = v & ~0x1000;
		if((v >> 8 & 7) == 0 && (v & 255) < 16){
			a->reg[Esr] |= 1<<5;
			break;
		}
		dest = a->reg[Icrhi];
		if((a->base & X2) == 0){
			dest >>= 24;
			if(dest == 255) dest = ~0U;
		}
		deliver(v, dest, cpu);
		break;
	case Self:
		if(v < 16) a->reg[Esr] |= 1<<5;
		else deliver(1<<18 | v, 0, cpu);
		break;
	case Timer: case Thermal: case Perf: case Lint0: case Lint1: case Error:
		a->reg[r] = v;
		if((a->reg[Svr] & Enabled) == 0)
			a->reg[r] |= Mask;
		apicupdate(cpu);
		break;
	case Initial:
		a->reg[r] = v;
		a->count = v;
		a->epoch = nanosec();
		apicadvance();
		break;
	case Divide:
		now = nanosec();
		a->count = current(a, now);
		a->epoch = now;
		a->reg[r] = v & 11;
		apicadvance();
		break;
	}
}

static int
lapicmmio(uintptr pa, void *p, int n, int wr)
{
	int r;

	if(n != 4 || (pa & 15) != 0)
		return -1;
	if((cpu->apic.base & (Global|X2)) != Global){
		if(!wr) PUT32(p, 0, -1);
		return 0;
	}
	r = (pa & 4095) >> 4;
	if(r >= Self)
		return -1;
	if(wr)
		lapicwrite(r, GET32(p, 0));
	else
		PUT32(p, 0, lapicread(r));
	return 0;
}

static int
ioapicmmio(uintptr pa, void *p, int n, int wr)
{
	u32int v;
	int r, pin, shift;

	if(n != 4)
		return -1;
	if((pa & 4095) == 0){
		if(wr) ioapic.sel = GET32(p, 0) & 255;
		else PUT32(p, 0, ioapic.sel);
		return 0;
	}
	if((pa & 4095) != 16)
		return -1;
	r = ioapic.sel;
	v = 0;
	if(r >= 0x10 && r < 0x10 + 2*Npin){
		pin = (r - 0x10)/2;
		shift = (r & 1)*32;
		if(wr){
			v = GET32(p, 0);
			if(shift == 0)
				v = v & ~0x5000 | (u32int)ioapic.redir[pin] & 0x4000;
			ioapic.redir[pin] = ioapic.redir[pin] & ~(0xffffffffULL<<shift) | (uvlong)v<<shift;
			if(ioapicactive(pin) && (ioapic.redir[pin] & 1<<15) != 0)
				ioapicdeliver(pin);
		}else
			v = ioapic.redir[pin] >> shift;
	}else if(r == 0){
		if(wr) ioapic.id = GET32(p, 0) & 0xff000000;
		v = ioapic.id;
	}else if(r == 1)
		v = (Npin-1)<<16 | 0x11;
	else if(r == 2)
		v = ioapic.id;
	if(!wr)
		PUT32(p, 0, v);
	return 0;
}

int
apicmsr(int rd, u32int msr, u64int *value)
{
	Lapic *a;
	u32int mask;
	int r, old, mode;

	if(ncpu == 1)
		return 0;
	a = &cpu->apic;
	if(msr == 0x1b){
		if(rd)
			*value = a->base;
		else{
			old = a->base & (Global|X2);
			mode = *value & (Global|X2);
			if((*value & ~0xd00ULL) != Lapicbase || mode == X2 ||
			   old == 0 && mode == (Global|X2) || old == (Global|X2) && mode == Global)
				goto fault;
			a->base = *value & ~0x100ULL | (cpu->id == 0 ? 0x100 : 0);
			if(old == 0 && mode == Global)
				apicreset(cpu);
			apicupdate(cpu);
		}
		return 1;
	}
	if(msr < 0x800 || msr > 0xbff)
		return 0;
	if((a->base & X2) == 0)
		goto fault;
	r = msr - 0x800;
	/* Reads of write-only/reserved registers and writes to reserved bits fault. */
	mask = 0;
	switch(r){
	case Tpr: case Self: mask = 0xff; break;
	case Eoi: case Esr: break;
	case Svr: mask = 0x1ff; break;
	case Icrlo: mask = 0xcdfff; break;
	case Timer: mask = 0x310ff; break;
	case Thermal: case Perf: mask = 0x117ff; break;
	case Lint0: case Lint1: mask = 0x1f7ff; break;
	case Error: mask = 0x110ff; break;
	case Initial: mask = ~0U; break;
	case Divide: mask = 11; break;
	default:
		if(!rd || !(r == Id || r == Version || r == Ppr || r == Ldr ||
		   r >= Isr && r < Esr || r == Current))
			goto fault;
	}
	if(rd){
		if(r == Eoi || r == Self)
			goto fault;
		*value = lapicread(r);
		if(r == Icrlo) *value |= (uvlong)a->reg[Icrhi]<<32;
	}else{
		if(((u32int)*value & ~mask) != 0 || r != Icrlo && *value >> 32 != 0)
			goto fault;
		if(r == Icrlo){
			a->reg[Icrhi] = *value >> 32;
			if((*value >> 8 & 7) == 1){
				a->reg[Esr] |= 1<<4;
				return 1;
			}
		}
		/* LVT delivery status and remote IRR are read-only, not reserved. */
		if(r >= Timer && r <= Error)
			lapicwrite(r, (u32int)*value & ~0x5000U);
		else
			lapicwrite(r, *value);
	}
	return 1;
fault:
	postexc("#gp", 0);
	return -1;
}

void
apicreset(Vcpu *c)
{
	int i;

	memset(c->apic.reg, 0, sizeof(c->apic.reg));
	c->nmi = 0;
	c->apic.count = 0;
	c->apic.reg[Id] = c->id << 24;
	c->apic.reg[Version] = 0x14 | 5<<16;
	c->apic.reg[Dfr] = -1;
	c->apic.reg[Svr] = 0xff;
	for(i = Timer; i <= Error; i++)
		c->apic.reg[i] = Mask;
}

void
apicinit(void)
{
	Vcpu *c;
	Region *r;
	int i;

	if(ncpu == 1)
		return;
	r = mkregion(Lapicbase, (uvlong)Lapicbase + BY2PG, 0);
	r->mmio = lapicmmio;
	r = mkregion(Ioapicbase, (uvlong)Ioapicbase + BY2PG, 0);
	r->mmio = ioapicmmio;
	for(c = cpus; c < cpus + ncpu; c++){
		c->apic.base = (uvlong)Lapicbase | 0x800 | (c->id == 0 ? 0x100 : 0);
		apicreset(c);
	}
	ioapic.id = ncpu << 24;
	for(i = 0; i < Npin; i++)
		ioapic.redir[i] = Mask;
}
