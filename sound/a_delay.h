/*(LGPL)
---------------------------------------------------------------------------
	a_delay.h - Feedback delay w/ LP filter
---------------------------------------------------------------------------
 * Copyright (C) 2001, David Olofson
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

#ifndef _A_DELAY_H_
#define _A_DELAY_H_

#include "a_plugin.h"

typedef enum
{
	/*
	 * Uniform parameters.
	 *
	 *
	 */
	DC_EARLY_TIME = FXC_PARAM_1,	/* Early Reflections Time Scale */
	DC_TAIL_TIME = FXC_PARAM_2,	/* Tail Time Scale */
/*TODO:*/	DC_EARLY_LEVEL = FXC_PARAM_3,	/* Early Reflections Level */
/*TODO:*/	DC_FEEDBACK = FXC_PARAM_4,	/* Tail Feedback Level */
	DC_LP_FILTER = FXC_PARAM_5,	/* Feedback LP Filter Cutoff */

	/*
	 * Early reflection taps.
	 *
	 * Times are in ms, 16:16 fixp, and levels are in the
	 * usual 16:16 fixp format, where 1.0 [65536] is unity
	 * gain.
	 *
	 * Note that there is an implicit "tap 0" that always
	 * taps at full level directly from the tail feedback
	 * signal as it's sent back into the delay buffer. That
	 * is, using only tail feedback taps still results in
	 * an effect - more specifically, a multi tap feedback
	 * delay.
	 *
	 * (Tech note: In lower quality modes, taps can be
	 * grouped according to level, or even scaled down to
	 * the nearest power-of-two values for speed.)
	 */
	DC_EARLY_TAP_1_TIME = FXC_USER,
	DC_EARLY_TAP_1_LEVEL,
	DC_EARLY_TAP_2_TIME,
	DC_EARLY_TAP_2_LEVEL,
	DC_EARLY_TAP_3_TIME,
	DC_EARLY_TAP_3_LEVEL,
	DC_EARLY_TAP_4_TIME,
	DC_EARLY_TAP_4_LEVEL,
	DC_EARLY_TAP_5_TIME,
	DC_EARLY_TAP_5_LEVEL,
	DC_EARLY_TAP_6_TIME,
	DC_EARLY_TAP_6_LEVEL,
	DC_EARLY_TAP_7_TIME,
	DC_EARLY_TAP_7_LEVEL,
	DC_EARLY_TAP_8_TIME,
	DC_EARLY_TAP_8_LEVEL,
	DC_EARLY_TAP_9_TIME,
	DC_EARLY_TAP_9_LEVEL,
	DC_EARLY_TAP_10_TIME,
	DC_EARLY_TAP_10_LEVEL,
	DC_EARLY_TAP_11_TIME,
	DC_EARLY_TAP_11_LEVEL,
	DC_EARLY_TAP_12_TIME,
	DC_EARLY_TAP_12_LEVEL,
	DC_EARLY_TAP_13_TIME,
	DC_EARLY_TAP_13_LEVEL,
	DC_EARLY_TAP_14_TIME,
	DC_EARLY_TAP_14_LEVEL,
	DC_EARLY_TAP_15_TIME,
	DC_EARLY_TAP_15_LEVEL,
	DC_EARLY_TAP_16_TIME,
	DC_EARLY_TAP_16_LEVEL,

	/*
	 * Tail feedback taps.
	 *
	 * These work like the early reflection taps, but the
	 * resulting signal is LP filtered and sent back into
	 * the start of the delay buffer, together with the
	 * input signal.
	 */
	DC_TAIL_TAP_1_TIME,
	DC_TAIL_TAP_1_LEVEL,
	DC_TAIL_TAP_2_TIME,
	DC_TAIL_TAP_2_LEVEL,
	DC_TAIL_TAP_3_TIME,
	DC_TAIL_TAP_3_LEVEL,
	DC_TAIL_TAP_4_TIME,
	DC_TAIL_TAP_4_LEVEL,
	DC_TAIL_TAP_5_TIME,
	DC_TAIL_TAP_5_LEVEL,
	DC_TAIL_TAP_6_TIME,
	DC_TAIL_TAP_6_LEVEL,
	DC_TAIL_TAP_7_TIME,
	DC_TAIL_TAP_7_LEVEL,
	DC_TAIL_TAP_8_TIME,
	DC_TAIL_TAP_8_LEVEL,

	DC_COUNT
} delay_controls_t;

void delay_init(struct audio_plugin_t *p);

#endif /*_A_DELAY_H_*/

