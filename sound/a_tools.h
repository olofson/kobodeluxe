/*(LGPL)
---------------------------------------------------------------------------
	a_tools.h - Various useful audio macros and inlines
---------------------------------------------------------------------------
 * Copyright (C) 2001 - 2003, David Olofson
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

#ifndef	_A_TOOLS_H_
#define	_A_TOOLS_H_

#include "glSDL.h"
#include <string.h>
#include "kobolog.h"
#include "a_types.h"

/*----------------------------------------------------------
	Integer Arithmetics
----------------------------------------------------------*/

/*
 * This operation can be implemented very nicely in ML
 * on virtually any CPU - but this is C... *heh*
 */
#ifdef A_USE_INT64
/* Ok, but suboptimal - unless the compiler is sufficiently smart. */
#define fixmul(a, b)	((int)((Sint64)(a) * (Sint64)(b) >> 16))
#else
/* Slooooow, but portable... :-/ */
#define fixmul(a, b)	((int)((float)(a) * (float)(b) * (1.0/65536.0)))
#endif

/*
 * Given an integer value 'value', and
 *
 *	x = y * value
 *
 * find the 's' that best approximates that function using
 * the (on practically any CPU) faster function
 *
 *	x = y << s
 *
 * The approximated x will always be equal to, or smaller
 * than the exact x.
 */
static inline int int2shift(int value)
{
	int res;
	if(value >> 16)
		res = 16;
	else
		res = 0;
	if(value >> (res + 8))
		res += 8;
	if(value >> (res + 4))
		res += 4;
	if(value >> (res + 2))
		res += 2;
	if(value >> (res + 1))
		res += 1;
	return res;
}

/*
 * Given a 16:16 fixp value 'value', and
 *
 *	x = y * value
 *
 * find the 's' that best approximates that function using
 * the (on practically any CPU) faster function
 *
 *	x = y >> s
 *
 * The approximated x will always be equal to, or smaller
 * than the exact x.
 */
static inline int fixp2shift(int value)
{
	int res;
	if(value >> 16)
		res = 16;
	else
		res = 0;
	if(value >> (res + 8))
		res += 8;
	if(value >> (res + 4))
		res += 4;
	if(value >> (res + 2))
		res += 2;
	if(value >> (res + 1))
		res += 1;
	return 16 - res;
}

/*
 * Like fixp2shift(), but here, 'value' is clamped to the
 * [4, 1/64] range (8 bits of dynamic range - hence the
 * suffix) for speed.
 */
static inline int fixp2shift8(int value)
{
	int res;
	value >>= 10;
	if(value >> 4)
		res = 4;
	else
		res = 0;
	if(value >> (res + 2))
		res += 2;
	if(value >> (res + 1))
		res += 1;
	return 6 - res;
}



/*----------------------------------------------------------
	Basic Audio Processing
------------------------------------------------------------
 * WARNING:
 *	Most of these macros and inlines will crash if
 *	told to process 0 samples! In DEBUG mode, this is
 *	checked and will cause error messages to be printed.
 *
 * Naming conventions:
 *	m16	16 bit mono
 *	s16	16 bit stereo
 *	m32	32 bit mono
 *	s32	32 bit stereo
 * All samples are expected to be signed integers, wherever
 * sample format matters.
 */

/* Clear 'frames' samples starting at 'buf'. */
#define	m16clear(buf, frames)		memset((buf), 0, (frames) << 1)
#define	s16clear(buf, frames)		memset((buf), 0, (frames) << 2)
#define	m32clear(buf, frames)		memset((buf), 0, (frames) << 2)
#define	s32clear(buf, frames)		memset((buf), 0, (frames) << 3)

/* Copy 'frames' samples from 'in' to 'out'. */
#define	m16copy(in, out, frames)	memcpy((out), (in), (frames) << 1)
#define	s16copy(in, out, frames)	memcpy((out), (in), (frames) << 2)
#define	m32copy(in, out, frames)	memcpy((out), (in), (frames) << 2)
#define	s32copy(in, out, frames)	memcpy((out), (in), (frames) << 3)

/* Add the contents of 'from' into buffer 'to' */
#define	m16add(from, to, frames)	_add16(from, to, frames)
#define	s16add(from, to, frames)	_add16(from, to, (frames)<<1)
static inline void _add16(Sint16 *from, Sint16 *to, unsigned samples)
{
	unsigned s;
	for(s = samples & 3; s; --s)
		*to++ += *from++;
	samples >>= 2;
	s = 0;
	while(samples--)
	{
		to[s] += from[s];
		to[s+1] += from[s+1];
		to[s+2] += from[s+2];
		to[s+3] += from[s+3];
		s += 4;
	}
}

#define	m32add(from, to, frames)	_add32(from, to, frames)
#define	s32add(from, to, frames)	_add32(from, to, (frames)<<1)
static inline void _add32(Sint32 *from, Sint32 *to, unsigned samples)
{
	unsigned s = -((4 - samples) & 3);
#ifdef DEBUG
	if(!samples)
	{
		log_printf(ELOG, "a_tools.h: _add32(); zero samples!\n");
		return;
	}
#endif
	switch(samples & 3) do
	{
	  case 0:
		to[s] += from[s];
	  case 3:
		to[s+1] += from[s+1];
	  case 2:
		to[s+2] += from[s+2];
	  case 1:
		to[s+3] += from[s+3];
		s += 4;
	} while(s < samples);
}

/*
 * Returns 1 if mixing was done, 0 if the volume
 * is too low for actual output to be produced.
 */
#define	m32mix(from, to, vol, frames)	_mix32(from, to, vol, frames)
#define	s32mix(from, to, vol, frames)	_mix32(from, to, vol, (frames)<<1)
static inline int _mix32(Sint32 *from, Sint32 *to, int vol, unsigned samples)
{
	unsigned s = -((4 - samples) & 3);
#ifdef DEBUG
	if(!samples)
	{
		log_printf(ELOG, "a_tools.h: _mix32(); zero samples!\n");
		return 0;
	}
#endif
	vol >>= 8;
	if(!vol)
		return 0;
	switch(samples & 3) do
	{
	  case 0:
		to[s] += from[s] * vol >> 8;
	  case 3:
		to[s+1] += from[s+1] * vol >> 8;
	  case 2:
		to[s+2] += from[s+2] * vol >> 8;
	  case 1:
		to[s+3] += from[s+3] * vol >> 8;
		s += 4;
	} while(s < samples);
	return 1;
}

#define	m32fastmix(f, t, vol, frames)	_fastmix32(f, t, vol, frames)
#define	s32fastmix(f, t, vol, frames)	_fastmix32(f, t, vol, (frames)<<1)
static inline void _fastmix32(Sint32 *from, Sint32 *to, int vol, unsigned samples)
{
	unsigned s = -((4 - samples) & 3);
#ifdef DEBUG
	if(!samples)
	{
		log_printf(ELOG, "a_tools.h: _fastmix32(); zero samples!\n");
		return;
	}
#endif
	vol = fixp2shift(vol);
	switch(samples & 3) do
	{
	  case 0:
		to[s] += from[s] >> vol;
	  case 3:
		to[s+1] += from[s+1] >> vol;
	  case 2:
		to[s+2] += from[s+2] >> vol;
	  case 1:
		to[s+3] += from[s+3] >> vol;
		s += 4;
	} while(s < samples);
}

#endif /*_A_TOOLS_H_*/
