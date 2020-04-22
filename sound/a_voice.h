/*(LGPL)
---------------------------------------------------------------------------
	a_voice.h - Audio Engine low level mixer voices
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

/*
 * How To Keep Track Of Your Voices (for patch programmers):
 *	Before doing anything with a Voice, first check that it
 *	still actually *is* your voice, and hasn't been stolen
 *	by some one else. The 'channel' field will indicate this,
 *	of course, but that's actually only half a solution!
 *
 *	Why? Well, if you're doing somethingn polyphonic, the
 *	Voice allocator may steal *your* voices, just as any other
 *	voices, which is why it's quite possible to lose voices to
 *	your own channel.
 *
 *	To deal with this, each Voice has a 'tag' field that you
 *	may use in any way you see fit. The idea is to write a
 *	"locally unique" code into the 'tag' field whenever you
 *	get a "new" voice from the allocator. That way, you can
 *	just check that value later on, to see whether or not the
 *	channel is still doing what you told it to, or if you or
 *	someone else has started a new job on it.
 *
 *	As an example, MIDI code may store MIDI pitch in the
 *	'tag' field upon NoteOn, as there can usually only be one
 *	"voice context" (one or more voices) active for each MIDI
 *	key of each MIDI channel. When receiving other messages,
 *	just check for each voice before you touch, that 1) it's
 *	still owned by the engine Channel the MIDI channel is
 *	mapped to. When you eventually get the NoteOff, 2) check
 *	that the 'tag' fields of voices match the MIDI pitch of
 *	the NoteOff message.
 *
 *	Note that as long as everyone sticks with their own
 *	Channels, it's ok to use the 'tag' fields in different
 *	ways in the same engine, at the same time. Checking
 *	'channel' fields *first* will avoid any confusion.
 */

#ifndef _A_VOICE_H_
#define _A_VOICE_H_

#include "a_globals.h"
#include "a_types.h"
#include "a_events.h"
#include "a_patch.h"


typedef enum voice_states_t
{
	VS_STOPPED = 0,		/* "Detached" from the mixer */
	VS_RESERVED,		/* Just allocated; pending events */
	VS_PLAYING		/* Playing! */
} voice_states_t;


typedef enum voice_events_t
{
	VE_START = 0,		/* Start playing waveform <arg1>. */
	VE_STOP,		/* Stop and free voice, flush events.
				 * This *will* click unless you fade out first!
				 * NOTE: The voice disappears in a puff of
				 *       violet smoke after receiving this
				 *       event! Any subsequent events will
				 *       be deleted and IGNORED.
				 */
	VE_SET,			/* Set control <index> to <arg1>. */
//	VE_ISET,		/* Set interpolated control <index> to <arg1>. */
	VE_IRAMP		/* Ramp interpolated control <index> to <arg1>
				 * over <arg2> frames.
				 */
} voice_events_t;


/* Voice Controls that are *not* interpolated. */
typedef enum voice_controls_t
{
	VC_WAVE = 0,		/* (int) Waveform index (latched on loop) */
	VC_LOOP,		/* (boolean) */

	VC_PITCH,		/* (fixp) linear frequency; int <==> MIDI */

	VC_RETRIG,		/* (int) retrig after N samples */
	VC_RANDTRIG,		/* (fixp[0, 1]) random mod. to RETRIG */

	VC_PRIM_BUS,		/* (int) target bus for ACC_VOLUME */
	VC_SEND_BUS,		/* (int) target bus for ACC_SEND */

	VC_RESAMPLE,		/* (enum) Resampling mode */

	VC_COUNT
} voice_controls_t;

/* Voice Controls that *are* interpolated. */
typedef enum voice_icontrols_t
{
	VIC_LVOL = 0,		/* (fixp) Left volume */
	VIC_RVOL,		/* (fixp) Right volume */
	VIC_LSEND,		/* (fixp) Left send */
	VIC_RSEND,		/* (fixp) Right send */

	VIC_COUNT
} voice_icontrols_t;


/* Voice control interpolator */
typedef struct voice_ci_t
{
	int	v;
	int	dv;
} voice_ci_t;


struct audio_channel_t;

typedef struct audio_voice_t
{
	/* Voice Management */
	struct audio_voice_t *next;	/* For channel voice list */
	struct audio_voice_t *prev;
	struct audio_channel_t *channel;
	int		tag;		/* tag from patch */
	int		priority;	/* Stealing priority (from owner) */

	/* Non-interpolated Controls */
	int		c[VC_COUNT];

	/* Interpolated Controls */
	voice_ci_t	ic[VIC_COUNT];

	/* Event Control */
	aev_port_t	port;		/* Event input port */

	/* Oscillator */
	voice_states_t	state;
	int		wave;
	unsigned int	section_end;	/* index of section end point */
	unsigned int	position;	/* current pos, integer part */
	unsigned int	position_frac;	/* current pos, fractional part */
	unsigned int	step;		/* position inc per output frame */
#ifdef AUDIO_USE_VU
	int		vu;	/* Pre-volume, linear peak, [0, 65535].
				 * Reset or fade after reading!
				 */
#endif
	/* Output routing control */
	int		use_double;	/* Use double output mixers */
	int		fx1, fx2;	/* FX send bus indices */

	patch_closure_t	closure;	/* Per-voice data for patch */
} audio_voice_t;

/* Get a free voice, or steal one if necessary. */
int voice_alloc(struct audio_channel_t *c);

/*
 * The *real*, brutal stop action.
 * No mercy. No click elimination.
 * Use only in case of emergency.
 */
void voice_kill(audio_voice_t *v);

/* Dirty crap function used to "wake a voice up" if needed... */
void voice_check_retrig(audio_voice_t *v);

/*
 * Mix 'frames' stereo samples into one or more of the 'busses',
 * as specified by the voice's output controls.
 *
 * (Can be driven directly "from outside" the engine. The Waveform
 * Construction API does this for resampling/conversion, for example.)
 */
void voice_process_mix(audio_voice_t *v, int *busses[], unsigned frames);

/* Process all voices. Simple, eh? */
void voice_process_all(int *busses[], unsigned frames);

void audio_voice_open(void);
void audio_voice_close(void);

#endif /*_A_VOICE_H_*/
