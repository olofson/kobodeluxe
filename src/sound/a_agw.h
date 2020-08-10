/*(LGPL)
---------------------------------------------------------------------------
	a_agw.h - "Algorithmically Generated Waveform" file support
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2003, David Olofson
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
 * The AGW v0.1 file format:
 *
 *	This extension to the EEL language makes it possible
 *	to use EEL scripts to construct waveforms using the
 *	"Waveform Construction API" of the engine.
 *
 *   Why!?
 *	Well, pages of hard-coded calls to the Waveform
 *	Construction API and recompiling for every turn
 *	while creating sounds got boring, so I decided to
 *	hack up something simple to get cleaner code and
 *	faster sound editing.
 *
 *	Over a few weeks, the file format turned into an
 *	interpreted language, which I eventually cleaned up
 *	and separated from the engine. This language is
 *	called "EEL" - Extensible Embeddable Language, and
 *	is not tied to the audio engine.
 *
 *   Built-in commands:
 *	//
 *		C++ style comment.
 *
 */	/* ... */
/*		C style comment.
 *
 *	w_reset;
 *		Resets the AGW engine state to the default.
 *		This is done automatically before a script
 *		is executed after being loaded with w_load().
 *		See below for default settings for w_env etc.
 *
 *	w_format <wid>, <sformat>, <samplerate>;
 *		Set format of <wid> to <sformat>;
 *		<samplerate> samples per second.
 *
 * 		Supported sample formats:
 *			MONO8	STEREO8
 *			MONO16	STEREO16
 *
 *	w_blank <wid>, <samples>, <loop>;
 *		Create <samples> samples of silent data
 *		for waveform <wid>. <loop> can be either 0
 *		or 1, and if it's 1, the waveform will be
 *		set up for looping playback.
 *
 *	w_env <mod_target>[, <time>, <level>[, <time>, <level>...]];
 *	w_env <mod_target>, <level>;
 *		Program the envelope generator modulating
 *		<mod_target> with the specified points.
 *		Times are in seconds, and levels are in Hz
 *		for frequency envelopes, while for other
 *		envelopes, 1.0 corresponds to 0 dB, "no
 *		change" etc.
 *
 *		Calling w_env with two arguments sets up
 *		modulator 'mod_target' to the fixed level of
 *		the second argument - which obviously is *not*
 *		a duration in this case.
 *
 *		Calling w_env with no other arguments than
 *		'mod_target' resets the envelope generator
 *		for that destination to the default.
 *
 *		Available modulation targets:
 *		(Defaults are constant levels.)
 *
 *		Name	  Controls		Default
 *		---------------------------------------------
 *		AMPLITUDE Amplitude		1.0
 *		BALANCE	  Mixing control	0.0
 *		FREQUENCY f0/cut-off		100.0
 *		LIMIT	  Max overtone freq.	100000.0
 *		MOD1	  PW ratio/FM depth/...	0.0
 *		MOD2	  Various stuff...	0.0
 *		MOD3	  Various stuff...	0.0
 *
TODO:
 *	w_env_taper <mod_target>, <x_coeff>[, <x2_coeff>[, <x3_coeff>]];
 *		Set transformation of the envelope for target
 *		'mod_target' to the function:
 *
 *			out =	in * x_coeff +
 *				in*in * x2_coeff +
 *				in*in*in * x3_coeff;
 *
 *		Examples:
 *			w_env_taper FREQUENCY, 1;
 *			w_env_taper AMPLITUDE, .5, .25, .25;
 *
 *	w_mod <destination>, <frequency>, <amplitude>, <depth>;
 *		If f(t) is the envelope as defined with
 *		audio_wave_env(), and w(t) is the modulator phase,
 *		the final result will be
 *
 *		  sin(w(t)) * amplitude + f(t) * (1.0 + sin(w(t)) * depth)
 *
 *		That is, the modulation will add a sine with
 *		'amplitude' to the envelope, and it will also add
 *		a sine with an amplitude of 'depth', scaled by
 *		the envelope. (Or if you will, the second component
 *		offset by +1.0 will modulate the envelope.)
 *
 *	w_osc <wid>, <waveform>, <mode>;
 *		Apply a sound using <mode> to waveform <wid>,
 *		generated using a <waveform> oscillator.
 *		Various parameters of the oscillator may
 *		be modulated.
 *
 *		Available modes:
 *			WCA_ADD		Add oscillator output to buffer
 *			WCA_MUL		AM or ring modulate output with buffer
 *				==> BALANCE:	0   ==> no effect
 *						0.5 ==> AM
 *						1.0 ==> Ring modulation
 *
 *			WCA_FM		FM oscillator with buffer, then overwrite
 *					the buffer with the result. Note that
 *					it's *not* the buffer that is modulated!
 *				==> BALANCE controls FM depth.
 *
 *			WCA_FM_ADD	Like AMM_FM, but adds the result to the
 *					buffer instead of overwriting it.
 *				==> BALANCE controls FM depth.
 *
 *			WCA_SYNC	Reset oscillator phase when a + -> -
 *					zero crossing is detected in the
 *					buffer. Overwrites the buffer with the
 *					result.
 *
 *			WCA_SYNC_ADD	Like AMM_SYNC, but adds the result to
 *					the buffer instead of overwriting it.
 *
 *		Available waveforms:
 *			DC		Add DC offset
 *
 *			SINE
 *				==> MOD1 sets 1.0f FM depth
 *
 *			HALFSINE	Half wave rectified sine
 *				==> MOD1 sets zero clip level
 *
 *			RECTSINE	Full wave rectified sine
 *				==> MOD1 sets zero limit level
 *
 *			PULSE		Oversampled PWM pulse oscillator.
 *				==> MOD1 sets pulse width; 0.5 <==> square
 *
 *			TRIANGLE
 *				==> MOD1 ==> ramp up...tri...ramp down
 *
 *			SINEMORPH	Sine/square/sawtooth morph, using
 *					recursive FM. If the sum of MOD1 and
 *					MOD2 exceeds 1.0, they are balanced
 *					and scaled to restrict the total to
 *					1.0 while preserving the saw/square
 *					ratio. MOD1 and MOD2 are also scaled
 *					according to the FREQUENCY/LIMIT
 *					ratio, to approximate bandlimiting.
 *				==> MOD1 sets saw "amount"
 *				==> MOD2 sets square "amount"
 *
 *			BLMORPH		Bandlimited sine/saw/square/triangle
 *					oscillator, based on additive synthesis
 *					using a bank of sine oscillators.
 *					The harmonics for the respective
 *					waveforms are scaled by MOD1, MOD2 and
 *					MOD3, and then are summed. The output
 *					is bandlimited to the frequency set by
 *					LIMIT, with a roll-off giving a
 *					harmonic at LIMIT a 12 dB attenuation.
 *				==> MOD1 modulates sawtooth harmonics
 *				==> MOD2 modulates square harmonics
 *				==> MOD3 modulates triangle harmonics
 *
 *			BLCROSS		Bandlimited sine/saw/square/triangle
 *					oscillator, based on additive synthesis
 *					using a bank of sine oscillators.
 *					The harmonics for the respective
 *					waveforms are bandlimited by the
 *					respective MODn mapped from [0, 1] to
 *					[FREQUENCY, LIMIT], with a roll-off
 *					towards the high end, giving the
 *					highest harmonic of each waveform a
 *					12 dB attenuation.
 *				==> MOD1 controls sawtooth cut-off
 *				==> MOD2 controls square cut-off
 *				==> MOD3 controls triangle cut-off
 *
 *			NOISE		"White" noise (6581/SID style :-)
 *					The noise is generated by a pseudo
 *					random number generator that is polled
 *					for a new value at a rate of 2 * f, to
 *					achieve a minimum period corresponding
 *					to FREQUENCY. The pseudo random number
 *					generator is reset at the start of the
 *					waveform, to guarantee consistent
 *					results. (Obviously, this means that
 *					applying noise more than once is rather
 *					pointless, as it would just increase
 *					the amplitude!)
 *				==> MOD1 sets *non-bandlimited* LP cutoff
 *
 *			SPECTRUM	Multiplying frequency spectrum.
 *					Overtone frequencies are calculated by
 *					recursively multiplying with MOD1.
 *			 	==> f[n] = f[n-1] * MOD1
 *				==> a[n] = a[n-1] * MOD2
 *
 *			ASPECTRUM	Adding frequency spectrum. MOD1 is
 *					the distance in Hz between adjacent
 *					overtones. As you might expect, an
 *					ASPECTRUM usually *will* hit the
 *					maximum # of overtones limit, unless
 *					you track it with LIMIT properly!
 *			 	==> f[n] = f[n-1] + MOD1
 *				==> a[n] = a[n-1] * MOD2
 *
 *			HSPECTRUM	Harmonic frequency spectrum with
 *					separate modulation of even and odd
 *					harmonics.
 *				==> f[n] = f[1] * n
 *				==> (Even n) a[n] = a[n-1] * MOD1
 *				==> (Odd n)  a[n] = a[n-1] * MOD2
 *
 *			PHSPECTRUM	Pseudo-Harmonic frequency spectrum.
 *					Basically a HSPECTRUM, where you
 *					redefine "harmonic" using MOD1. :-)
 *					MOD2 and MOD3 control even and odd
 *					harmonics respectively.
 *				==> f[n] = f[1] * n * MOD1
 *				==> (Even n) a[n] = a[n-1] * MOD2
 *				==> (Odd n)  a[n] = a[n-1] * MOD3
 *
 *	w_filter <wid>, <type>;
 *		Apply filter of <type> to waveform <wid>.
 *
 *		Supported filter types:
 *			ALLPASS		(Currently a NOP)
 *			LOWPASS_6
 *			HIGHPASS_6
 *			LOWPASS_12
 *			HIGHPASS_12
 *			BANDPASS_12
 *			NOTCH_12
 *			PEAK_12
 *
 *	w_gain <wid>;
 *		Scale the amplitude of waveform <wid> using the
 *		AMPLITUDE envelope.
 *
 *
 *	w_load <wid>, <filename>, <looped>;
 *
 *		WARNING: This command is recursive!
 *
 *		Load waveform, or load and execute AGW file
 *		<filename>, and put the result in waveform
 *		<wid>. It's recommended to pass either
 *		'target', something calculated from 'target'
 *		(when loading "banks" of sounds), or 'tempN'
 *		for <wid>, although it's possible (but usually
 *		very nasty!) to specify your own waveform IDs.
FIXME:
	The 'looped' argument is a remainder of the
	old engine code, and should be replaced with
	something sensible.
 *
 *		Note that in the current implementation, there
 *		is no notion of "scope" - "loading" an AGW
 *		script from within another AGW script treats
 *		all variables as globals in all respects.
 *
 *	w_save <wid>, <filename>;
 *		Save the rendered waveform 'wid' as 'filename'.
 *
 *	w_convert <from_wid>, <to_wid>, <format>[, <rate>[, <resamp>]];
 *		Convert 'from_wid' into 'format' and 'rate'
 *		using resampling method 'resamp' and render the
 *		result into 'to_wid'. Leaving out the rate, or
 *		setting it to 0 keeps the original rate. Leaving
 *		out 'inter' uses the generally best interpolation
 *		method available.
 *
 *		Supported resampling methods:
 *			NEAREST		Nearest sample ("useless")
 *			NEAREST4X	Nearest with 4x oversampling
 *			LINEAR		Linear Interpolation
 *			LINEAR2X	LI w/ 2x oversampling
 *			LINEAR4X	LI w/ 4x oversampling
 *			LINEAR8X	LI w/ 8x oversampling
 *			LINEAR16X	LI w/ 16x oversampling
 *			CUBIC		Cubic interpolation
 *
 *			WORST		Worst available method
 *			MEDIUM		Good performance/quality
 *			BEST		Best available method
 *
 *	w_enhance <wid>, <from_f>, <level>;
 *
 *	w_gate <wid>, <cutoff>, <min>, <threshold>, <attenuation>;
 *
 *   (See eel/eel.h for commands that are part of EEL.)
 *
 *	w_mix <from_wid>, <to_wid>;
 *		Mix waveform <from_wid> into waveform <to_wid>.
 *		The amplitude envelope controls the mixing
 * 		level, and the frequency envelope controls the
 *		playback frequency of the source waveform.
 *		
 */

#ifndef _A_AGW_H_
#define _A_AGW_H_

#ifdef __cplusplus
extern "C" {
#endif

int agw_open(void);
void agw_close(void);

/*
 * Load and parse AGW script 'name',
 * suggesting that the result be put
 * in waveform 'wid'.
 *
 * Returns the actual id of the generated
 * waveform, or -1 upon failure.
 */
int agw_load(int wid, const char *name);

#ifdef __cplusplus
};
#endif

#endif /* _A_AGW_H_ */
