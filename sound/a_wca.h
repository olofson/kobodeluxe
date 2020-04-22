/*(LGPL)
---------------------------------------------------------------------------
	a_wca.h - WCA, the Wave Construction API
---------------------------------------------------------------------------
 * Copyright (C) 2002, David Olofson
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

#ifndef _A_WCA_H_
#define _A_WCA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "a_wave.h"

#define WCA_MAX_ENV_STEPS	32

/* Reset all envelopes, modulators etc */
void wca_reset(void);


/*--------------
   Modulation
---------------*/

typedef enum wca_modtargets_t
{
	WCA_AMPLITUDE = 0,	/* Amplitude; level */
	WCA_BALANCE,		/* Oscillator output orig/modulated balance */
	WCA_FREQUENCY,		/* Frequency; cut-off */
	WCA_LIMIT,		/* Max overtone frequency */
	WCA_MOD1,		/* Ratio; crossfade; morph */
	WCA_MOD2,		/* Amount; roll-off */
	WCA_MOD3,		/* Secondary amount; roll-off */
	_WCA_MODTARGETS		/* # of modulation targets */
} wca_modtargets_t;

/* Clear modulator state, and prepare for programming. */
void wca_mod_reset(wca_modtargets_t target);

/* Program 'target' to produce a constant value 'v' */
void wca_val(wca_modtargets_t target, float v);

/* Add envelope point */
void wca_env(wca_modtargets_t target, float duration, float v);

/*
 * Set modulation component of envelope.
 * (See a_agw.h for details.)
 */
void wca_mod(wca_modtargets_t target, float frequency,
		float amplitude, float depth);

/*
TODO:
void wca_mod_wave(wca_modtargets_t target, int wid, float amp);
(FM/AM/...)
*/


/*--------------
   Oscillator
---------------*/

/* (Please refer to a_agw.h for details on waveforms, modulation etc.) */
typedef enum wca_mixmodes_t
{
	WCA_ADD = 0,
	WCA_MUL,
	WCA_FM,
	WCA_FM_ADD,
	WCA_SYNC,
	WCA_SYNC_ADD
} wca_mixmodes_t;

/* (Please refer to a_agw.h for details on waveforms, modulation etc.) */
typedef enum wca_waveform_t
{
	WCA_DC = 0,
	WCA_SINE,
	WCA_HALFSINE,
	WCA_RECTSINE,
	WCA_PULSE,
	WCA_TRIANGLE,
	WCA_SINEMORPH,
	WCA_BLMORPH,
	WCA_BLCROSS,
	WCA_NOISE,
	WCA_SPECTRUM,
	WCA_ASPECTRUM,
	WCA_HSPECTRUM,
	WCA_AHSPECTRUM
} wca_waveform_t;

/* Add a component to a waveform. */
void wca_osc(int wid, wca_waveform_t wf, wca_mixmodes_t mm);


/*--------------
   Filtering
---------------*/

typedef enum wca_filtertype_t
{
	WCA_ALLPASS = 0,
	WCA_LOWPASS_6DB,
	WCA_HIGHPASS_6DB,
	WCA_LOWPASS_12DB,
	WCA_HIGHPASS_12DB,
	WCA_BANDPASS_12DB,
	WCA_NOTCH_12DB,
	WCA_PEAK_12DB
} wca_filtertype_t;

/*
 * Apply filter to a waveform.
 * The frequency envelope controls cutoff frequency, and the
 * amplitude envelope controls the filter Q. The ratio controls
 * the original/filtered balance; 0.0 outputs only the filtered
 * signal.
 */
void wca_filter(int wid, wca_filtertype_t ft);

/*
 * Scale the amplitude of waveform 'wid' using the AMPLITUDE
 * envelope.
 */
void wca_gain(int wid);


/*
 * "Enhance" the treble of wave 'wid' by adding an octave shifted
 * version of the frequencies around 'f'. 'level' sets the amount
 * of artificial treble added.
 */
void wca_enhance(int wid, int f, float level);

/*
 * Noise gate. 'f' is the LP/HP split frequency, 'thr' is the
 * start threshold (which acts as the attack/release switch
 * level, related to the amplitude of the HP part of the signal),
 * and 'att' is the Auto Threshold Tracking frequency. 'min' is
 * the minimum treble gain, when the gate is closed.
 */
void wca_gate(int wid, int f, float min, float thr, float att);

/*
TODO:
 * Mix one waveform into another.
 *
 * The source waveform is converted and resampled into
 * the format/rate of the destination waveform.
 * AMPLITUDE controls the total gain, BALANCE the
 * source/destination balance (0.0 is all source,
 * 1.0 is all destination), FREQUENCY controls the
 * playback speed of the source waveform. (1.0
 * corresponds to the original playback speed.)
 */
void wca_mix(int src_wid, int dst_wid);

#ifdef __cplusplus
};
#endif

#endif /*_A_WCA_H_*/
