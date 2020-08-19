/*(LGPL)
---------------------------------------------------------------------------
	a_patch.h - Audio Engine "instrument" definitions
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

#ifndef _A_PATCH_H_
#define _A_PATCH_H_

#include "a_globals.h"
#include "a_types.h"
#include "a_events.h"
#include "eel.h"

/*----------------------------------------------------------
	Audio Patch
----------------------------------------------------------*/
struct audio_channel_t;

typedef struct audio_patch_t
{
	appbank_t	param;

	/*
	 * Process input events, update internal state and
	 * generate output events for 'frames' frames.
	 *
	 * IMPORTANT!
	 *	The engine may occasionally call this
	 *	function with a 0 'frames' argument. This
	 *	doesn't mean "do nothing", but rather
	 *	"process all events available for the
	 *	first frame". (It's used whenever the
	 *	engine needs to split for an ACC_PATCH
	 *	while there are more events for the
	 *	same frame.)
	 */
	void (*process)(struct audio_patch_t *p,
			struct audio_channel_t *c, unsigned frames);

	eel_symbol_t	*eel_process;
} audio_patch_t;


typedef enum
{
	PES_START = 0,
	PES_START2,
	PES_DELAY,
	PES_ATTACK,
	PES_HOLD,
	PES_DECAY,
	PES_SUSTAIN,
	PES_WAIT,
	PES_RELEASE,
	PES_DEAD
} A_patch_env_states;

/*
 * Temporary kludge. Something more flexible will probably
 * be needed, but I'm using this for now, to avoid type
 * casting void pointers all over the place. (Not an issue
 * with EEL based patches.)
 */
typedef struct patch_closure_t
{
	/* Arguments from CE_START */
	int	velocity;
	int	pitch;		/* w/ RANDPITCH, but w/o channel PITCH */
	int	velvol;		/* For envelope */

	/* Envelope generator state */
	A_patch_env_states	env_state;
	int			queued_stop;
	aev_timestamp_t		env_next;
	int			lvol, rvol;
	int			lsend, rsend;
} patch_closure_t;


void audio_patch_open(void);
void audio_patch_close(void);

#endif /*_A_PATCH_H_*/
