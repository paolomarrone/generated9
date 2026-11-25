#include <u.h>
#include <libc.h>
#include "beat.h"

/* One five-byte OPL register write, followed by a delay in chip samples. */
static uchar*
reg(uchar *p, int r, int v, int delay)
{
	*p++ = r;
	*p++ = r>>8;
	*p++ = v;
	*p++ = delay;
	*p++ = delay>>8;
	return p;
}

int
kit(int fd)
{
	/* Rhythm mode uses channels 6-8. Channels 7 and 8 share the noise source. */
	static uchar op[][5] = {
		/* slot, multiplier, attenuation, attack/decay, sustain/release */
		{0x10, 0x01, 0x0a, 0xf9, 0x97},	/* kick modulator */
		{0x13, 0x01, 0x05, 0xf6, 0xb8},	/* kick carrier */
		{0x11, 0x0c, 0x11, 0xfb, 0xe9},	/* hi-hat */
		{0x14, 0x01, 0x0b, 0xf8, 0xc8},	/* snare */
		{0x15, 0x05, 0x0f, 0xfa, 0xd9},	/* clap */
	};
	uchar buf[(4*nelem(op)+8)*5], *p;
	int i;

	p = buf;
	for(i = 0; i < nelem(op); i++){
		p = reg(p, 0x20+op[i][0], op[i][1], 0);
		p = reg(p, 0x40+op[i][0], op[i][2], 0);
		p = reg(p, 0x60+op[i][0], op[i][3], 0);
		p = reg(p, 0x80+op[i][0], op[i][4], 0);
	}
	p = reg(p, 0xc6, 0x06, 0);
	p = reg(p, 0xa6, 0x62, 0);
	p = reg(p, 0xb6, 0x09, 0);
	p = reg(p, 0xa7, 0x50, 0);
	p = reg(p, 0xb7, 0x0e, 0);
	p = reg(p, 0xa8, 0xa0, 0);
	p = reg(p, 0xb8, 0x12, 0);
	p = reg(p, 0xbd, 0x20, 0);
	return write(fd, buf, p-buf) == p-buf ? 0 : -1;
}

int
step(int fd, Pattern *pat, int col, int *rem)
{
	static uchar bit[Nvoice] = {0x10, 0x08, 0x01, 0x02};
	uchar buf[6*5], *p;
	int i, mask, ticks, gap;

	ticks = Oplrate*15 + *rem;
	*rem = ticks % pat->bpm;
	ticks /= pat->bpm;
	mask = 0x20;
	for(i = 0; i < Nvoice; i++)
		if(pat->row[i] & 1<<col)
			mask |= bit[i];
	p = reg(buf, 0xbd, 0x20, 0);
	/* Three short attacks on the cymbal operator make the clap. */
	gap = 0;
	if(mask & bit[3]){
		gap = Oplrate/125;
		for(i = 0; i < 2; i++){
			p = reg(p, 0xbd, mask, gap);
			p = reg(p, 0xbd, mask & ~bit[3], 0);
		}
	}
	p = reg(p, 0xbd, mask, ticks-2*gap);
	return write(fd, buf, p-buf) == p-buf ? ticks : -1;
}
