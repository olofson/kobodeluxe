/*(LGPL)
---------------------------------------------------------------------------
	a_filters.c - Various Audio Signal Filters
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2002, David Olofson
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

#include <stdlib.h>

#include "a_math.h"
#include "a_tools.h"
#include "a_filters.h"

/*--------------------------------------------------------------------
	f6_t - Simple mono 6 dB/oct state variable filter
--------------------------------------------------------------------*/
void f6_process_lp(f6_t *fil, int *buf, unsigned frames)
{
	int f = fil->f;
	int d = fil->d;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		d += (buf[s] - (d>>12)) * f;
		buf[s] = d>>12;
	}
	fil->d = d;
}

void f6_process_hp(f6_t *fil, int *buf, unsigned frames)
{
	int f = fil->f;
	int d = fil->d;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		d += (buf[s] - (d>>12)) * f;
		buf[s] -= d>>12;
	}
	fil->d = d;
}

void f6_set_f(f6_t *fil, int f)
{
	fil->f = (f<<13) / fil->rate;
	if(fil->f > 1<<13)
		fil->f = 1<<13;
}

void f6_init(f6_t *fil, int fs)
{
	fil->rate = fs;
	fil->d = 0;
	f6_set_f(fil, 400);
}


/*--------------------------------------------------------------------
	f6s_t - Simple stereo 6 dB/oct state variable filter
--------------------------------------------------------------------*/
void f6s_process_lp(f6s_t *fil, int *buf, unsigned frames)
{
	int f = fil->f;
	int dl = fil->dl;
	int dr = fil->dr;
	unsigned s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		dl += (buf[s] - (dl>>12)) * f;
		dr += (buf[s+1] - (dr>>12)) * f;
		buf[s] = dl>>12;
		buf[s+1] = dr>>12;
	}
	fil->dl = dl;
	fil->dr = dr;
}

void f6s_process_hp(f6s_t *fil, int *buf, unsigned frames)
{
	int f = fil->f;
	int dl = fil->dl;
	int dr = fil->dr;
	unsigned s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		dl += (buf[s] - (dl>>12)) * f;
		dr += (buf[s+1] - (dr>>12)) * f;
		buf[s] -= dl>>12;
		buf[s+1] -= dr>>12;
	}
	fil->dl = dl;
	fil->dr = dr;
}

void f6s_set_f(f6s_t *fil, int f)
{
	fil->f = (f<<13) / fil->rate;
	if(fil->f > 1<<13)
		fil->f = 1<<13;
}

void f6s_init(f6s_t *fil, int fs)
{
	fil->rate = fs;
	fil->dl = fil->dr = 0;
	f6s_set_f(fil, 400);
}


/*--------------------------------------------------------------------
	dcf6s_t - Fast stereo 6 dB/oct HPF for DC suppression
--------------------------------------------------------------------*/
void dcf6s_process(dcf6s_t *fil, int *buf, unsigned frames)
{
	int f = fil->f;
	int dl = fil->dl;
	int dr = fil->dr;
	unsigned s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		dl += (buf[s] - (dl>>12)) >> f;
		dr += (buf[s+1] - (dr>>12)) >> f;
		buf[s] -= dl>>12;
		buf[s+1] -= dr>>12;
	}
	fil->dl = dl;
	fil->dr = dr;
}

int dcf6s_silent(dcf6s_t *fil)
{
	if(labs(fil->dl) > 4096)
		return 0;
	if(labs(fil->dr) > 4096)
		return 0;
	return 1;	
}

void dcf6s_set_f(dcf6s_t *fil, int f)
{
	fil->f = int2shift(fil->rate / f) - 13;
	if(fil->f < 0)
		fil->f = 0;
}

void dcf6s_init(dcf6s_t *fil, int fs)
{
	fil->rate = fs;
	fil->dl = fil->dr = 0;
	dcf6s_set_f(fil, 10);
}



/*--------------------------------------------------------------------
	resof12_t - Resonant mono 12 dB/oct state variable filter
--------------------------------------------------------------------*/
/* In-place LP */
void resof12_process_lp(resof12_t *rf, int *buf, unsigned frames)
{
	int d1 = rf->d1;
	int d2 = rf->d2;
	int f = rf->f;
	int q = rf->q;
	int l, b, h;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		l = d2 + (f*d1 >> 8);
		h = (buf[s] << 4) - l - (q*d1 >> 8);
		b = (f*h >> 8) + d1;
		buf[s] = l >> 4;
		d1 = b;
		d2 = l;
	}
	rf->d1 = d1;
	rf->d2 = d2;
}

/* In-place HP */
void resof12_process_hp(resof12_t *rf, int *buf, unsigned frames)
{
	int d1 = rf->d1;
	int d2 = rf->d2;
	int f = rf->f;
	int q = rf->q;
	int l, b, h;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		l = d2 + (f*d1 >> 8);
		h = (buf[s] << 4) - l - (q*d1 >> 8);
		b = (f*h >> 8) + d1;
		buf[s] = h >> 4;
		d1 = b;
		d2 = l;
	}
	rf->d1 = d1;
	rf->d2 = d2;
}

/* In-place BP */
void resof12_process_bp(resof12_t *rf, int *buf, unsigned frames)
{
	int d1 = rf->d1;
	int d2 = rf->d2;
	int f = rf->f;
	int q = rf->q;
	int l, b, h;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		l = d2 + (f*d1 >> 8);
		h = (buf[s] << 4) - l - (q*d1 >> 8);
		b = (f*h >> 8) + d1;
		buf[s] = b >> 4;
		d1 = b;
		d2 = l;
	}
	rf->d1 = d1;
	rf->d2 = d2;
}

/*
 * LP filter the 'buf' buffer, while separating off
 * the HP part into buffer 'out'.
 */
void resof12_process_split(resof12_t *rf, int *buf, int *out, unsigned frames)
{
	int d1 = rf->d1;
	int d2 = rf->d2;
	int f = rf->f;
	int q = rf->q;
	int l, b, h;
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		l = d2 + (f*d1 >> 8);
		h = (buf[s] << 4) - l - (q*d1 >> 8);
		b = (f*h >> 8) + d1;
		buf[s] = l >> 4;
		out[s] = h >> 4;
		d1 = b;
		d2 = l;
	}
	rf->d1 = d1;
	rf->d2 = d2;
}

void resof12_set_f(resof12_t *rf, int f)
{
	rf->f = (int)(512.0 * sin(M_PI * (float)f / rf->rate));
	if(rf->f > 256)
		rf->f = 256;
}

void resof12_set_q(resof12_t *rf, int q)
{
	if(q < 1)
		rf->q = 256;
	else
		rf->q = 256 / q;
}

void resof12_init(resof12_t *rf, int fs)
{
	rf->rate = fs;
	rf->d1 = rf->d2 = 0;
	resof12_set_f(rf, 400);
	resof12_set_q(rf, 10);
}



/*--------------------------------------------------------------------
	resof12s_t - Resonant stereo 12 dB/oct state variable filter
--------------------------------------------------------------------*/

/* In-place LP */
void resof12s_process_lp(resof12s_t *rf, int *buf, unsigned frames)
{
	int d1l = rf->d1l;
	int d1r = rf->d1r;
	int d2l = rf->d2l;
	int d2r = rf->d2r;
	int f = rf->f;
	int q = rf->q;
	int ll, bl, hl;
	int lr, br, hr;
	unsigned s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		ll = d2l + (f*d1l >> 8);
		lr = d2r + (f*d1r >> 8);
		hl = (buf[s] << 4) - ll - (q*d1l >> 8);
		hr = (buf[s+1] << 4) - lr - (q*d1r >> 8);
		bl = (f*hl >> 8) + d1l;
		br = (f*hr >> 8) + d1r;
		buf[s] = ll >> 4;
		buf[s+1] = lr >> 4;
		d1l = bl;
		d1r = br;
		d2l = ll;
		d2r = lr;
	}
	rf->d1l = d1l;
	rf->d1r = d1r;
	rf->d2l = d2l;
	rf->d2r = d2r;
}

void resof12s_set_f(resof12s_t *rf, int f)
{
	rf->f = (int)(512.0 * sin(M_PI * (float)f / rf->rate));
	if(rf->f > 256)
		rf->f = 256;
}

void resof12s_set_q(resof12s_t *rf, int q)
{
	if(q < 1)
		rf->q = 256;
	else
		rf->q = 256 / q;
}

void resof12s_init(resof12s_t *rf, int fs)
{
	rf->rate = fs;
	rf->d1l = rf->d2l = 0;
	rf->d1r = rf->d2r = 0;
	resof12s_set_f(rf, 400);
	resof12s_set_q(rf, 10);
}

