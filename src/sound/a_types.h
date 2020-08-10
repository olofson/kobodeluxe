/*(LGPL)
---------------------------------------------------------------------------
	a_types.h - Commonly used types in the engine
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

#ifndef _A_TYPES_H_
#define _A_TYPES_H_

#include "glSDL.h"

/* Builtin patch drivers */
typedef enum
{
	PD_NONE = -1,	/* No driver! */
	PD_MONO = 0,	/* Basic monophonic */
	PD_POLY,	/* Basic polyphonic */
	PD_MIDI,	/* Basic MIDI player */
	PD_EEL		/* User EEL code driver */
} patch_drivers_t;


/* Voice tag special codes */
typedef enum
{
	AVT_FUTURE = -2,
	AVT_ALL = -1
} audio_voice_tags_t;


/*
 * DAHDSR envelope generator parameters:
 *
 *   Level
 *     ^
 *     |         __L1__
 *     |        /|    |\
 *     |       / |    | \
 *     |__L0__/  |    |  \__L2__
 *     |     |   |    |   |    |\
 *     |  D  | A | H  | D | S  |R\
 *     |_____|___|____|___|____|__\____\
 *     |     |   |    |   |    |   |   / Time
 *      DELAY T1  HOLD T2   T3  T4
 *
 * State by state description:
 *
 *	D: Set to L0, and hold for DELAY seconds.
 *	A: Ramp to L1 over T1 seconds.
 *	H: Hold at L1 for HOLD seconds.
 *	D: Ramp to L2 over T2 seconds.
 *	S: if T3 < 0
 *		Hold at L2 until CE_STOP is received.
 *	   else
 *		Hold at L2 for T3 seconds.
 *	R: Ramp to 0 over T3 seconds.
 *
 * If T3 is set to a negative value, sustain lasts until
 * a CE_STOP event is received. If T3 is 0 of higher,
 * sustain lasts for T3 seconds.
 *
 * The EG will *always* load L0, but from then on, if a
 * phase has a zero duration, that phase is skipped.
 * Note that skipping any of the A, D and R phases will
 * produce clicks, unless the adjacent levels are
 * identical.
 *
 * In some cases, it's desirable that the EG immediately
 * skips to the release phase when a CE_STOP event is
 * received, whereas in other cases, this would result
 * in problems with very short notes. Therefore, there is
 * a boolean control 'ENV_SKIP' that make it possible to
 * specify which behavior is desired.
 * '1' will make the EG skip to release upon receiving
 * CE_STOP. '0' will result in the EG taking no action
 * on CE_STOP until the sustain phase is reached the
 * normal way.
 * 'ENV_SKIP' has no effect if "timed sustain" is used;
 * T3 must be < 0, or CE_STOP is entirely ignored.
 *
TODO:
	* Turn this into a generic EG, with "unlimited"
	  number of phases. The current way of marking
	  the sustain hold point would still work,
	  although one would have to replace the ramp/
	  wait mechanism with a test for change in sc.

	* Implement "quadratic taper" or similar non-
	  linear transform of the output. The EG will
	  have to generate a reasonably dense stream
	  of voice control events, since voice ramps
	  are still linear.
 */

/* Audio Patch Parameters */
typedef enum
{
	APP_FIRST = 0,
	/* Patch Driver Plugin */
	APP_DRIVER = 0,	/* (int) Patch driver index */

	/* Oscillator */
	APP_WAVE,	/* (int) Waveform index */

	/* "Special" features */
	APP_RANDPITCH,	/* (fixp) Random pitch depth */
	APP_RANDVOL,	/* (fixp) Random volume depth */

	/* Envelope Generator */
	APP_ENV_SKIP,	/* (bool) skip to sustain on CE_STOP */
	APP_ENV_L0,	/* (fixp) envelope level 0 (initial) */
	APP_ENV_DELAY,	/* (fixp) envelope delay-to-attack */
	APP_ENV_T1,	/* (fixp) envelope attack time */
	APP_ENV_L1,	/* (fixp) envelope hold level */
	APP_ENV_HOLD,	/* (fixp) envelope hold time */
	APP_ENV_T2,	/* (fixp) envelope decay time */
	APP_ENV_L2,	/* (fixp) envelope sustain level */
	APP_ENV_T3,	/* (fixp) envelope sustain time */
	APP_ENV_T4,	/* (fixp) envelope release time */

	/* LFO Generator */
	APP_LFO_DELAY,	/* (fixp) LFO delay-to-start */
	APP_LFO_ATTACK,	/* (fixp) LFO attack time-to-full */
	APP_LFO_DECAY,	/* (fixp) LFO decay time-to-zero */
	APP_LFO_RATE,	/* (fixp) LFO frequency */
	APP_LFO_SHAPE,	/* (int) LFO modulation waveform */

	/* Modulation Matrix */
	APP_ENV2PITCH,	/* (fixp) */
	APP_ENV2VOL,	/* (fixp) */
	APP_LFO2PITCH,	/* (fixp) */
	APP_LFO2VOL,	/* (fixp) */
#if 0
	APP_ENV2CUT,	/* (fixp) */
	APP_LFO2CUT,	/* (fixp) */
TODO:
	/*
	 * Controls for the resonant filter
	 * (Setting CUT to 0 disables the filter.)
	 */
	APP_FILT_CUT,	/* (fixp) Filter cutoff */
	APP_FILT_RES,	/* (fixp) Filter resonance */

	/*
	 * Distortion control
	 */
	APP_DIST_GAIN,	/* (fixp) Pre-limiter gain */
	APP_DIST_CLIP,	/* (fixp) Limiter clip threshold */
	APP_DIST_DRIVE,	/* (fixp) Feedback from the clipped peaks */
#endif
	APP_COUNT,
	APP_LAST = APP_COUNT - 1
} audio_pparams_t;
typedef int appbank_t[APP_COUNT];


/*
 * The voice structure:
 *      OSC -> VOL/PAN/SEND
 *                |   |
 *                | : '-> o-> FX BUS 0
 *                '-----> o-> FX BUS 1
 *                  :  :  ...    ...
 *                  :  :  o-> FX BUS N-1
 *
 *	That's it! No filters, and just one oscillator. The
 *	interesting part is the output section.
 *
 *	Every voice has two output bus selectors, each on
 *	referencing a stereo bus, on which further
 *	processing may be applied. By setting either bus
 *	selector to -1, or by pointing both at the same bus,
 *	only the Primary Output will be used, and some some
 *	CPU power is saved during mixing. :-)
 *
 *	As opposed to most MIDI synths, this engine allows
 *	you to select one or two output busses for each
 *	voice, using one as the Primary Output (level set
 *	by CC7: Volume) and the other as the Secondary
 *	Output (level set by CC91: Reverb Depth, multiplied
 *	by CC7.)
 *
 *	The MIDI implementation uses CC92 and CC94 to select
 *	Primary and Secondary Output busses, respectively.
 */

/*
 * Audio Channel & Group Controls
FIXME:	Channels use all, groups use a subset - which sucks!
FIXME:	I've considered using the OpenGL approach with a single,
FIXME:	global enum - but that can only provide runtime "enum
FIXME:	type checking". Seems like only languages other than the
FIXME:	native language of this engine can do it right...
FIXME:	(Another reason to use EEL wherever possible.)
 *
 * The Priority System:
 *	Set ACC_PRIORITY to 0 to have a channel grab a
 *	voice, and have it excluded from the dynamic
 *	voice allocation system.
 *
 *	Other values specify channel priority for voice
 *	stealing - higher values mean lower priority.
 *	Higher priority channels win when fighting for
 *	voices. Priority 0 voices cannot be stolen.
 */

typedef enum
{
	ACC_FIRST = 0,
/*--- Controls that are NOT transformed ---*/
	ACC_GROUP = 0,	/* mixer group */
	ACC_PRIORITY,	/* Voice Allocation Priority */
	ACC_PATCH,	/* (int) Patch index */

	ACC_PRIM_BUS,	/* (int) target bus for ACC_VOLUME */
	ACC_SEND_BUS,	/* (int) target bus for ACC_SEND */

/*--- Controls that are transformed by adding ---*/
	ACC_PAN,	/* (fixp[-1, 1]) pre-send pan */
	ACC_PITCH,	/* (fixp) linear frequency; int <==> MIDI */

/*--- Controls that are transformed by multiplying ---*/
	ACC_VOLUME,	/* (fixp) primary "send" level */
	ACC_SEND,	/* (fixp) secondary send level */

/*--- Patch Controls (Not transformed) ---*/
	ACC_MOD1,	/* (fixp) Modulation 1 depth */
	ACC_MOD2,	/* (fixp) Modulation 2 depth */
	ACC_MOD3,	/* (fixp) Modulation 3 depth */

	ACC_X,		/* (fixp) X position */
	ACC_Y,		/* (fixp) Y position */
	ACC_Z,		/* (fixp) Z position */

	ACC_COUNT,
	ACC_LAST = ACC_COUNT - 1
} audio_ctls_t;
typedef int accbank_t[ACC_COUNT];


/*
 * Bus Control:
 *	The first 8 MIDI channels can be used for controlling
 *	the signal processing done between the busses, and
 *	between the busses and the output buffer.
 *
 *	A set of non-standard MIDI CCs are used for this:
 *		CC 39: Select Insert Slot for subsequent CCs.
 *
 *	Slot 0 is used to access the dry/pre insert send level
 *	controls. Thus, CCs 40..45 are ignored for slot 0.
 *
 *	The FX parameter CC range is mapped to according to
 *	[0, 128] ==> [0.0, 2.0]. (That is, you can't reach the
 *	exact value of 2.0, as the highest CC value is 127.)
 *		CC 40: Insert Effect Type
 *		CC 41: Insert Effect Parameter 1 (Time 1)
 *		CC 42: Insert Effect Parameter 2 (Time 2)
 *		CC 43: Insert Effect Parameter 3 (Depth 1)
 *		CC 44: Insert Effect Parameter 4 (Depth 2)
 *		CC 45: Insert Effect Parameter 5 (Rate)
 *
 *	This one is not transformed at all - MIDI CCs 0..127
 *	are used directly. Obviously, it's preferred that
 *	plugins map integer parameters that fit in that range
 *	to Parameter 6.
 *		CC 46: Insert Effect Parameter 6 (Mode/Special)
 *
 *	The following CC doubles as "dry send to master", when
 *	used on slot 0.
 *		CC 47: Post insert send to master ("Wet")
 *
 *	The next set of CC are IMHO, a bit dodgy in the current
 *	implementation. Indeed, they work, but you have to
 *	accept that the engine processes the busses in a fixed
 *	order, meaning that you can only send from lower number
 *	busses to higher number busses - not the other way
 *	around. (In fact, the engine won't even consider the
 *	controls for the "broken" connections when mixing.)
 *
 *	Note that when used on slot 0, these act as *dry* send
 *	levels, as there cannot be an insert in slot 0.
 *		CC 48: Post insert send to bus 1
 *		CC 49: Post insert send to bus 2
 *		CC 50: Post insert send to bus 3
 *		CC 51: Post insert send to bus 4
 *		CC 52: Post insert send to bus 5
 *		CC 53: Post insert send to bus 6
 *		CC 54: Post insert send to bus 7
 *		CC 55: Post insert send to bus 8
 *
 *	Note that the post insert sends won't be considered
 *	if no effect is enabled. (They would just double the
 *	functionality of the pre insert sends, for extra cost
 *	and no use.)
 */

/* Audio Bus Controls */
typedef enum
{
	ABC_FIRST = 0,

	/* NOTE: The ABC_FX_* controls of slot 0 are ignored! */
	ABC_FX_TYPE = 0,	/* (int) */
	ABC_FX_PARAM_1,		/* (fixp) Time/f/Amount 1 */
	ABC_FX_PARAM_2,		/* (fixp) Time/f/Amount 2 */
	ABC_FX_PARAM_3,		/* (fixp) Depth/Level 1 */
	ABC_FX_PARAM_4,		/* (fixp) Depth/Level 2 */
	ABC_FX_PARAM_5,		/* (fixp) Rate/Level */
	ABC_FX_PARAM_6,		/* (int)  Mode/Special */

	ABC_SEND_MASTER,	/* (fixp) Post FX send to master */

	ABC_SEND_BUS_0,		/* (fixp) Post FX send to bus 0 */
	ABC_SEND_BUS_1,		/* (fixp) Post FX send to bus 1 */
	ABC_SEND_BUS_2,		/* (fixp) Post FX send to bus 2 */
	ABC_SEND_BUS_3,		/* (fixp) Post FX send to bus 3 */
	ABC_SEND_BUS_4,		/* (fixp) Post FX send to bus 4 */
	ABC_SEND_BUS_5,		/* (fixp) Post FX send to bus 5 */
	ABC_SEND_BUS_6,		/* (fixp) Post FX send to bus 6 */
	ABC_SEND_BUS_7,		/* (fixp) Post FX send to bus 7 */

	ABC_COUNT,
	ABC_LAST = ABC_COUNT -1
} audio_busctls_t;
typedef int abcbank_t[ABC_COUNT];

/*
 * Generic Effect API - for busses and inserts.
 */
typedef enum
{
	AFX_NONE = 0,
	AFX_DELAY,	/* 1: Tap 1	2: Tap 2
			 * 3: Gain 1	4: Gain 2
			 * 5: Feedback Level
			 */
	AFX_ECHO,	/* 1: Ldelay	2: Rdelay
			 * 3: Lgain	4: Rgain
			 * 5: Feedback Delay
			 */
	AFX_REVERB,	/* 1: Early reflections duration
			 * 2: Tail duration
			 * 3: Early reflections level
			 * 4: Tail Feedback Level
			 * 5: Feedback LP Cutoff
			 */
	AFX_CHORUS,	/* 1: Left Vibrato Speed
			 * 2: Right Vibrato Speed
			 * 3: Left Vibrato Depth
			 * 4: Right Vibrato Depth
			 * 5: Feedback Level
			 */
	AFX_FLANGER,	/* Same as chorus; only different scale. */
	AFX_FILTER,	/* 1: Left Cut	2: Right Cut
			 * 3: Left Reso	4: Right Resonance
			 * 5: <unused>
			 */
	AFX_DIST,	/* 1: Bass	2: Treble
			 * 3: Pre Gain	4: Post Gain
			 * 4: Drive
			 */
	AFX_WAH,	/* 1: Low Limit		2: High Limit
			 * 3: Sensitivity	4: Gain
			 * 5: Resonance
			 */
	AFX_EQ,		/* 1: Bass f	2: Treble f
			 * 3: Bass Gain	4: Treble Gain
			 * 4: Mid Gain
			 */
	AFX_ENHANCER,	/* 1: Mid f	2: High f
			 * 3: Mid Sens.	4: High Sensitivity
			 * 5: Artificial Treble Strength
			 */
	AFX_COMPRESSOR,	/* 1: Attack	2: Release
			 * 3: Gain	4: Threshold
			 * 5: Knee Softness
			 */
	AFX_MAXIMIZER	/* 1: Low Split	2: High Split
			 * 3: Low Boost	4: High Boost
			 * 5: Mid Boost
			 */
} audio_fxtypes_t;


typedef enum
{
	AR_WORST = -3,
	AR_MEDIUM = -2,
	AR_BEST = -1,

	AR_NEAREST = 0,	/* Nearest sample ("useless") */
	AR_NEAREST_4X,	/* Nearest with 4x oversampling */
	AR_LINEAR,	/* Linear Interpolation */
	AR_LINEAR_2X_R,	/* LI w/ 2x oversampling + ramp smoothing */
	AR_LINEAR_4X_R,	/* LI w/ 4x oversampling + ramp smoothing */
	AR_LINEAR_8X_R,	/* LI w/ 8x oversampling + ramp smoothing */
	AR_LINEAR_16X_R,/* LI w/ 16x oversampling + ramp smoothing */
	AR_CUBIC_R	/* Cubic interpolation + ramp smoothing */
} audio_resample_t;


typedef enum
{
	AF_MONO8 = 0,	/* Mono Sint8 */
	AF_STEREO8,	/* Stereo Sint8 */
	AF_MONO16,	/* Mono Sint16 */
	AF_STEREO16,	/* Stereo Sint16 */
	AF_MONO32,	/* Mono float32 */
	AF_STEREO32,	/* Stereo float32 */
	AF_MIDI		/* Midi sequence */
} audio_formats_t;


typedef enum
{
	AQ_VERY_LOW,	/* Nearest, no oversampling */
	AQ_LOW,		/* Nearest, 4x oversampling */
	AQ_NORMAL,	/* Linear interpolation */
	AQ_HIGH,	/* LI with adaptive oversampling 2x..8x */
	AQ_VERY_HIGH	/* LI with fixed 16x oversampling */
} audio_quality_t;

#undef	A_USE_INT64
#ifdef SDL_HAS_64BIT_TYPE
#	define	A_USE_INT64
#endif

#endif /*_A_TYPES_H_*/
