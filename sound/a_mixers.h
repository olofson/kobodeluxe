/*(LGPL)
---------------------------------------------------------------------------
	a_mixers.h - Mixer Core Code
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2002, 2007 David Olofson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This is to be included multiple times, with various definitions
 * of some macros - that's why it looks so weird for a "header"...
 */

#define	__FOR_SAMPLES	frames <<= 1; for(s = 0; s < frames; s += 2)

#ifdef	__STEREO
/* Paste the stereo-only lines */
#	define	__ST(x)		x
/* Index unit: two per sample */
#	define	__INDEX		((sp >> (FREQ_BITS - 1)) & 0xfffffffe)
/* Right output from right input */
#	define	__INDINC	2
#	define	__R		r
#else
/* Don't paste stereo stuff */
#	define	__ST(x)
/* Index unit: one per sample */
#	define	__INDEX		(sp >> FREQ_BITS)
#	define	__INDINC	1
/* Right output from mono input */
#	define	__R		l
#endif

#ifdef	__16BIT
#	define	__NORMALIZE	VOL_BITS
#else
#	define	__NORMALIZE	(VOL_BITS-8)
#endif

#if defined(__16BIT) && (FREQ_BITS > 15)
#	define	__FRAC		(int)((sp >> (FREQ_BITS - 15)) & 0x7fff)
#	define	__IFRAC(x)	(0x8000 - x)
#	define	__FRACBITS	15
#else
#	define	__FRAC		(int)(sp & ((1 << FREQ_BITS) - 1))
#	define	__IFRAC(x)	((1 << FREQ_BITS) - x)
#	define	__FRACBITS	FREQ_BITS
#endif

#ifdef	__SEND
#	define	__SND(x)	x
#else
#	define	__SND(x)
#endif

#define	LVOL	(v->ic[VIC_LVOL].v >> RAMP_BITS)
#define	RVOL	(v->ic[VIC_RVOL].v >> RAMP_BITS)
#define	LSEND	(v->ic[VIC_LSEND].v >> RAMP_BITS)
#define	RSEND	(v->ic[VIC_RSEND].v >> RAMP_BITS)
#define	CSEND	((v->ic[VIC_LSEND].v + v->ic[VIC_RSEND].v) >> (RAMP_BITS + 1))

#define	__OUTPUT						\
	out[s] += l * LVOL >> __NORMALIZE;			\
	out[s + 1] += __R * RVOL >> __NORMALIZE;		\
	__SND(	sout[s] += l * LSEND >> __NORMALIZE;		\
		sout[s + 1] += __R * RSEND >> __NORMALIZE;)

#define	__RAMP							\
	v->ic[VIC_LVOL].v += v->ic[VIC_LVOL].dv;		\
	v->ic[VIC_RVOL].v += v->ic[VIC_RVOL].dv;		\
	__SND(	v->ic[VIC_LSEND].v += v->ic[VIC_LSEND].dv;	\
		v->ic[VIC_RSEND].v += v->ic[VIC_RSEND].dv;)

#ifdef AUDIO_USE_VU
int vu;
#endif

unsigned s;
unsigned int sp = ((v->position << FREQ_BITS) & 0x7fffffff) +
		(v->position_frac >> (32 - FREQ_BITS));
#ifdef	__16BIT
Sint16 *in = (Sint16 *)(wavetab[v->c[VC_WAVE]].data.si16) +
#else
Sint8 *in = (Sint8 *)(wavetab[v->c[VC_WAVE]].data.si8) +
#endif
/*
 * Adjust pointer according to bits 31+ of the position.
 * Note that this overlap in bit 31 is important; it
 * allows windows to cross "block" boundaries!
 */
#ifdef __STEREO
		2 *
#endif
		(v->position & ~((1<<(31-FREQ_BITS)) - 1));

#ifdef AUDIO_USE_VU
#ifdef __STEREO
vu = in[__INDEX] + in[__INDEX+1];
#else
vu = in[__INDEX] << 1;
#endif
#ifndef __16BIT
vu <<= 8;
#endif
if(vu > v->vu)
	v->vu = vu;
else if(-vu > v->vu)
	v->vu = -vu;
#endif /*AUDIO_USE_VU*/

switch(v->c[VC_RESAMPLE])
{
  default:
  case AR_NEAREST:
  {
	int lvol = LVOL;
	int rvol = RVOL;
	__SND(int csend = CSEND;)
	v->ic[VIC_LVOL].v += v->ic[VIC_LVOL].dv * frames;
	v->ic[VIC_RVOL].v += v->ic[VIC_RVOL].dv * frames;
	__SND(	v->ic[VIC_LSEND].v += v->ic[VIC_LSEND].dv * frames;
		v->ic[VIC_RSEND].v += v->ic[VIC_RSEND].dv * frames;)
	__FOR_SAMPLES
	{
		unsigned ind = __INDEX;
		int l = in[ind];
		__ST(int r = in[ind + 1];)
		__SND(int ss;)
		out[s] += l * lvol >> __NORMALIZE;
		out[s + 1] += __R * rvol >> __NORMALIZE;
		__SND(ss = (l + __R) * csend >> __NORMALIZE;)
		__SND(sout[s] += ss;)
		__SND(sout[s + 1] += ss;)
		sp += v->step;
	}
	break;
  }
  case AR_NEAREST_4X:
  {
	/* 
	 * Should be cheaper than AR_LINEAR on machines
	 * with slow multiplications. Doesn't sound much
	 * better than NEAREST most of the time, though...
	 */
	int lvol = LVOL;
	int rvol = RVOL;
	unsigned step = v->step >> 2;
	__SND(int csend = CSEND;)
	v->ic[VIC_LVOL].v += v->ic[VIC_LVOL].dv * frames;
	v->ic[VIC_RVOL].v += v->ic[VIC_RVOL].dv * frames;
	__SND(	v->ic[VIC_LSEND].v += v->ic[VIC_LSEND].dv * frames;
		v->ic[VIC_RSEND].v += v->ic[VIC_RSEND].dv * frames;)
	__FOR_SAMPLES
	{
		unsigned ind;
		int l;
		__ST(int r;)
		__SND(int ss;)
		ind = __INDEX; l  = in[ind]; __ST(r  = in[ind + 1];) sp += step;
		ind = __INDEX; l += in[ind]; __ST(r += in[ind + 1];) sp += step;
		ind = __INDEX; l += in[ind]; __ST(r += in[ind + 1];) sp += step;
		ind = __INDEX; l += in[ind]; __ST(r += in[ind + 1];) sp += step;

		l >>= 2;
		__ST(r >>= 2);
		out[s] += l * lvol >> __NORMALIZE ;
		out[s + 1] += __R * rvol >> __NORMALIZE;
		__SND(ss = (l + __R) * csend >> __NORMALIZE;)
		__SND(sout[s] += ss;)
		__SND(sout[s + 1] += ss;)
	}
	break;
  }
  case AR_LINEAR:
  {
	int lvol = LVOL;
	int rvol = RVOL;
	__SND(	int lsend = LSEND;
		int rsend = RSEND;)
	v->ic[VIC_LVOL].v += v->ic[VIC_LVOL].dv * frames;
	v->ic[VIC_RVOL].v += v->ic[VIC_RVOL].dv * frames;
	__SND(	v->ic[VIC_LSEND].v += v->ic[VIC_LSEND].dv * frames;
		v->ic[VIC_RSEND].v += v->ic[VIC_RSEND].dv * frames;)
	__FOR_SAMPLES
	{
		int l;
		__ST(int r;)
		unsigned ind = __INDEX;
		int frac = __FRAC;
		int ifrac = __IFRAC(frac);
		l = in[ind + __INDINC] * frac;
		__ST(r = in[ind + 3] * frac;)
		l += in[ind] * ifrac;
		__ST(r += in[ind + 1] * ifrac;)
		l >>= __FRACBITS;
		__ST(r >>= __FRACBITS;)
		out[s] += l * lvol >> __NORMALIZE;
		out[s + 1] += __R * rvol >> __NORMALIZE;
		__SND(sout[s] += l * lsend >> __NORMALIZE;)
		__SND(sout[s + 1] += __R * rsend >> __NORMALIZE;)
		sp += v->step;
	}
	break;
  }
  case AR_LINEAR_2X_R:
  {
	unsigned step1 = v->step >> 1;
	unsigned step2 = (v->step + 1) >> 1;
	__FOR_SAMPLES
	{
		int l, l2;
		__ST(int r;)
		__ST(int r2;)
		unsigned ind = __INDEX;
		int frac = __FRAC;
		int ifrac = __IFRAC(frac);
		l = in[ind + __INDINC] * frac;
		__ST(r = in[ind + 3] * frac;)
		l += in[ind] * ifrac;
		__ST(r += in[ind + 1] * ifrac;)
		sp += step1;

		ind = __INDEX;
		frac = __FRAC;
		ifrac = __IFRAC(frac);
		l2 = in[ind + __INDINC] * frac;
		__ST(r2 = in[ind + 3] * frac;)
		l2 += in[ind] * ifrac;
		__ST(r2 += in[ind + 1] * ifrac;)
		sp += step2;

		l = ((l >> 1) + (l2 >> 1)) >> __FRACBITS;
		__ST(r = ((r >> 1) + (r2 >> 1)) >> __FRACBITS;)

		__OUTPUT
		__RAMP
	}
	break;
  }
  case AR_LINEAR_4X_R:
  case AR_LINEAR_8X_R:
  case AR_LINEAR_16X_R:
  {
	int over_shift = v->c[VC_RESAMPLE] - AR_LINEAR_4X_R + 2;
	int over = 1 << over_shift;
	unsigned step = v->step >> over_shift;
	__FOR_SAMPLES
	{
		int i, l = 0;
		__ST(int r = 0;)
		for(i = 0; i < over; ++i)
		{
			int ll;
			__ST(int rr;)
			unsigned ind = __INDEX;
			int frac = __FRAC;
			int ifrac = __IFRAC(frac);
			ll = in[ind + __INDINC] * frac;
			__ST(rr = in[ind + 3] * frac;)
			ll += in[ind] * ifrac;
			__ST(rr += in[ind + 1] * ifrac;)
			l += ll >> over_shift;
			__ST(r += rr >> over_shift;)
			sp += step;
		}
		l >>= __FRACBITS;
		__ST(r >>= __FRACBITS;)

		__OUTPUT
		__RAMP
	}
	break;
  }
  case AR_CUBIC_R:
  {
	/*
	 * Interpolation formulae by Olli Niemitalo.
	 * Converted to integer arithmetics by David Olofson.
	 */
	__FOR_SAMPLES
	{
		int a, b, c, lm1, l, l1, l2;
#ifdef	__STEREO
		int rm1, r, r1, r2;
#endif
		int ind = __INDEX;
		int frac = __FRAC;

		lm1 = in[ind];
		l = in[ind + __INDINC];
		l1 = in[ind + __INDINC*2];
		l2 = in[ind + __INDINC*3];
		a = (3 * (l-l1) - lm1 + l2) >> 1;
		b = (l1 << 1) + lm1 - ((5*l + l2) >> 1);
		c = (l1 - lm1) >> 1;
		a = a * frac >> __FRACBITS;
		a = (a + b) * frac >> __FRACBITS;
		l += (a + c) * frac >> __FRACBITS;

		sp += v->step;
#ifdef	__STEREO
		rm1 = in[ind + 1];
		r = in[ind + 3];
		r1 = in[ind + 5];
		r2 = in[ind + 7];
		a = (3 * (r-r1) - rm1 + r2) >> 1;
		b = (r1 << 1) + rm1 - ((5*r + r2) >> 1);
		c = (r1 - rm1) >> 1;
		a = a * frac >> __FRACBITS;
		a = (a + b) * frac >> __FRACBITS;
		r += (a + c) * frac >> __FRACBITS;
#endif
		__OUTPUT
		__RAMP
	}

	break;
  }
}

/*
 * Put the position back, *adding* to the integer part,
 * to handle the bit 31 overlap properly. (The block
 * hack requires us to start with bit 31 == 0, so it can
 * serve as a "carry flag" as an extra bonus.)
 */
v->position &= ~(0x7fffffff >> FREQ_BITS);
v->position += sp >> FREQ_BITS;

/*
 * For the fractional part, we just leave the bits
 * below our resolution unaffected.
 */
v->position_frac &= (1<<(32-FREQ_BITS))-1;
v->position_frac |= sp << (32-FREQ_BITS);

#undef	__FOR_SAMPLES
#undef	__NORMALIZE
#undef	__SND
#undef	__ST
#undef	__INDEX
#undef	__INDINC
#undef	__R
#undef	__FRAC
#undef	__IFRAC
#undef	__FRACBITS
