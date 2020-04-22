/*(LGPL)
------------------------------------------------------------
	a_midifile.c - MIDI file loader and player
------------------------------------------------------------
 * Stolen from the AdPlug player, stripped down,
 * translated into C, and adapted for the Audio Engine
 * by David Olofson, 2002.
 *
 * 20020219:	Separated data from player.
 *
 * 20020613:	Removed code for all special formats
 *		and AdLib hardware specific stuff. (Can't
 *		use the data. Can't maintain the code.)
 *
 * 20020615:	* Removed all 'long' types. If this engine
 *		  is supposed to work in 16 bit environments,
 *		  there's a *lot* more than a few longs to it.
 *		* Cleaned up type namespace.
 *		* Fixed various stuff that Splint whines about.
 *		* Added musical time printout function.
 *
 * Below is the original copyright. Of course, the LGPL
 * license still applies.
------------------------------------------------------------
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999, 2000, 2001 Simon Peter, <dn.tlp@gmx.net>, et al.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * mid.h - LAA & MID & CMF Player by Philip Hassey (philhassey@hotmail.com)
 */

#ifndef _A_MIDIFILE_H_
#define _A_MIDIFILE_H_

#include "a_midisock.h"

/*----------------------------------------------------------
	midi_file_t
----------------------------------------------------------*/

typedef struct midi_file_t
{
	const char	*author;
	const char	*title;
	const char	*remarks;
	unsigned	flen;
	unsigned	subsongs;
	unsigned	format;
	unsigned	tracks;
	unsigned char	*data;
} midi_file_t;

midi_file_t *mf_open(const char *name);
void mf_close(midi_file_t *mf);


/*----------------------------------------------------------
	midi_player_t
----------------------------------------------------------*/

typedef struct mp_track_context_t
{
	int		on;
	unsigned	pos;
	unsigned	iwait;
	unsigned char	pv;
} mp_track_context_t;


typedef struct mp_track_t
{
	int			enabled;
	unsigned		tend;
	unsigned		spos;
	mp_track_context_t	c;
	mp_track_context_t	loop_start;
} mp_track_t;


typedef struct mp_timesig_t
{
	unsigned char	beats;
	unsigned char	value;
	unsigned char	clock;
	unsigned char	qn;
} mp_timesig_t;

typedef struct mp_context_t
{
	unsigned	ppqn;		/* Pulses Per Quarter Note */
	unsigned	usqtr;		/* microseconds per quarter note */
	float		spulse;		/* seconds per pulse */
	mp_timesig_t	timesig;
	unsigned	time;		/* Current position in pulses */
	unsigned	iwait;		/* Pulses to next event */
	float		fwait;		/* Seconds to next event */
} mp_context_t;


typedef struct midi_player_t
{
	midisock_t	*sock;
	midi_file_t	*mf;
	unsigned	pos;
	mp_context_t	c;
	mp_context_t	loop_start;
	mp_track_t	track[16];
	int		doing;
	int		pitch;
} midi_player_t;


midi_player_t *mp_open(midisock_t *ms);
void mp_close(midi_player_t *mp);

/* Select a previously loaded MIDI file for playback */
int mp_select(midi_player_t *mp, midi_file_t *midifile);

/* Play all events until the next delay */
int mp_update(midi_player_t *mp);

/* Play all events until current_position + dt seconds */
int mp_play(midi_player_t *mp, float dt);

/* Rewind to start of sub song 'subsong' */
void mp_rewind(midi_player_t *mp, unsigned subsong);

void mp_stop(midi_player_t *mp);

#endif /*_A_MIDIFILE_H_*/
