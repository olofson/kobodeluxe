/*(LGPL)
---------------------------------------------------------------------------
	a_limiter.h - Simple limiter
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

#ifndef _A_LIMITER_H_
#define _A_LIMITER_H_

typedef struct limiter_t
{
	int		samplerate;
	unsigned	threshold;	/* Reaction threshold */
	int		release;	/* Release "speed" */
	unsigned	peak;		/* Filtered peak value */
	unsigned	attenuation;	/* Current output attenuation */
} limiter_t;

enum lim_params_t
{
	LIM_THRESHOLD = 0,	/* 16 bit sample units */
	LIM_RELEASE		/* Units per second */
};


int lim_open(limiter_t *lim, int samplerate);
void lim_close(limiter_t *lim);

void lim_control(limiter_t *lim, int param, int value);

/*
 * Limiter stereo process function.
 * Supports in-place processing.
 */
void lims_process(limiter_t *lim, int *in, int *out, unsigned frames);

/*
 * Smart Stereo version.
 *
 * This algorithm takes both channels in account in a way
 * that reduces the effect of the center appearing to have
 * more power after compression of signals with unbalanced
 * stereo images.
 *
 * A dead center signal can only get 3 dB louder than the
 * same signal in one channel only, as opposed to the
 * normal 6 dB of a limiter that only looks at max(L, R).
 *
 * Meanwhile, with "normal" material (ie most power
 * relatively centered), this limiter gets an extra 3 dB
 * compared to a limiter that checks (L+R).
 */
void limss_process(limiter_t *lim, int *in, int *out, unsigned frames);

#endif /*_A_LIMITER_H_*/
