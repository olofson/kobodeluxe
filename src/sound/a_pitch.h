/*(LGPL)
---------------------------------------------------------------------------
	a_pitch.h - Simple fixed point linear pitch table.
---------------------------------------------------------------------------
 * Copyright (C) 2001. 2002, David Olofson
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

#ifndef	_PITCHTAB_H_
#define	_PITCHTAB_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int *__pitchtab;

/*
 * 'middle_c' is the output value you want with an input of
 * 60; MIDI standard middle C. The table will start 5 octaves
 * down from there, and will contain 129 semitone steps. (The
 * last step is for sub-semitone interpolation.)
 */
int ptab_init(int middle_c);

void ptab_close(void);

/*
 * Interpolated linear frequency -> frequency factor lookup.
 *
 * 'pith' is a 16:16 fixed point linear frequency value.
 *
 * Returns engine pitch as 16:16 fixed point.
 */
static inline int ptab_convert(int pitch)
{
	int bfrac;
	int btone = pitch >> 16;
	if(btone < 0)
		return __pitchtab[0];
	else if(btone > 127)
		return __pitchtab[128];
	bfrac = (pitch & 0xffff) >> 8;
	return (__pitchtab[btone+1] * bfrac +
			__pitchtab[btone] * (256-bfrac)) >> 8;
}


/*
 * Interpolated linear frequency -> frequency factor lookup,
 * MIDI style.
 *
 * 'pith' is a MIDI pitch code (0..127; 60 <==> middle C,
 * which maps to the original sample rate of the current
 * waveform).
 *
 * 'bend' is a 16:16 fixed point value in the same unit.
 * That is, the integer part of bend is added to pitch,
 * whereas the fraction part is used for fractional tone
 * interpolation.
 *
 * Returns engine pitch as 16:16 fixed point.
 */
#define	ptab_convert_midi(p, b)	ptab_convert((b) + ((p)<<16))
#if 0
static inline int ptab_convert_midi(int pitch, int bend)
{
	int bfrac;
	int btone = pitch + (bend >> 16);
	if(btone < 0)
		return __pitchtab[0];
	else if(btone > 127)
		return __pitchtab[128];
	bfrac = (bend & 0xffff) >> 8;
	return (__pitchtab[btone+1] * bfrac +
			__pitchtab[btone] * (256-bfrac)) >> 8;
}
#endif

/*
 * Simple semitone resolution lookup - nonchecking.
 */
#define	_ptab_lookup(p)	(__pitchtab[p])

/*
 * ...and with checking.
 */
#define	ptab_lookup(p)	(__pitchtab ? _ptab_lookup(p) : 65536)

#ifdef __cplusplus
};
#endif

#endif	/*_PITCHTAB_H_*/
