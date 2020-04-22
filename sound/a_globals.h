/*(GPL)
---------------------------------------------------------------------------
	a_globals.h - Private global audio engine stuff
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2002, David Olofson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _A_GLOBALS_H_
#define _A_GLOBALS_H_

#ifndef	DBG
#	define	DBG(x)	x
#	define	DBG2(x)
#	define	DBG3(x)
#endif

#include "config.h"

#include "audio.h"
#include "a_pitch.h"
#include "a_commands.h"

#ifdef DEBUG
#define	PROFILE_AUDIO
#endif

#define AUDIO_USE_VU
#undef	AUDIO_LOCKING

/*
 * Various private types, defaults and limits...
 */

/* Voices; virtually no per-unit overhead. */
#define AUDIO_MAX_VOICES	32

/* Busses; some overhead, but only for busses that are actually used. */
#define	AUDIO_MAX_BUSSES	8

/* Bus inserts; minimal per-unit overhead. */
#define	AUDIO_MAX_INSERTS	2

#define DEFAULT_VOL		65536
#define DEFAULT_RVB		65536

#define	DEFAULT_LIM_RELEASE	(65536 * 1 / 44100)
#define	DEFAULT_LIM_THRESHOLD	32768

#define SDL_FRAGMENTS		2
#define OSS_FRAGMENTS		3
#define	FREQ_BITS		20
#define	RAMP_BITS		15
#define	VOL_BITS		15

/*
 * Restriction caused by the limited fixed point
 * range in the resampling voice mixers.
 */
#define	MAX_FRAGMENT_SPAN	(1<<(32 - FREQ_BITS))

/*
 * Maximum size of internal mixing buffers, in frames.
 *
 * This is a cache thrashing limiter! Tune
 * to best trade-off between cache footprint
 * and looping overhead.
 */
#define	MAX_BUFFER_SIZE		128

/*
 * Minimum *output* buffer size, in frames.
 * This effectively sets a minimum latency limit.
 */
#define	MIN_BUFFER_SIZE		16

/*
 * Safety limits to prevent locking up the engine with silly input.
 */
#define	MAX_STEP ((AUDIO_MAX_MIX_RATE / AUDIO_MIN_OUTPUT_RATE) << FREQ_BITS)
#define MIN_LOOP ((AUDIO_MAX_MIX_RATE / AUDIO_MIN_OUTPUT_RATE) >> 1)

/* Global engine settings  */
struct settings_t
{
	int		samplerate;
	unsigned	output_buffersize;
	unsigned	buffersize;
	audio_quality_t	quality;
};
extern struct settings_t a_settings;

/* Defaults... */
extern accbank_t a_channel_def_ctl;
extern accbank_t a_group_def_ctl;

extern abcbank_t a_bus_def_ctl;


/* For debug oscilloscope */
#ifdef DEBUG
#	define	OSCFRAMES	240
#	include "a_limiter.h"
	extern int *oscbufl;
	extern int *oscbufr;
	extern int oscframes;
	extern limiter_t limiter;
	extern int oscpos;
	void audio_print_info(void);
#endif

/* For audio engine profiling */
#ifdef PROFILE_AUDIO
#	define AUDIO_CPU_FUNCTIONS	10
	extern int audio_cpu_ticks;
	extern float audio_cpu_total;
	extern float audio_cpu_function[AUDIO_CPU_FUNCTIONS];
	extern char audio_cpu_funcname[AUDIO_CPU_FUNCTIONS][20];
#endif

extern int _audio_running;

#endif /*_A_GLOBALS_H_*/
