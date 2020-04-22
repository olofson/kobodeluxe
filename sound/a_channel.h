/*(LGPL)
---------------------------------------------------------------------------
	a_channel.h - Audio Engine high level control abstraction
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

#ifndef _A_CHANNEL_H_
#define _A_CHANNEL_H_

#include "a_globals.h"
#include "a_types.h"
#include "a_voice.h"


/*----------------------------------------------------------
	Audio Group (Abstraction for group control)
----------------------------------------------------------*/
typedef struct audio_group_t
{
	int		changed;
	accbank_t	ctl;
} audio_group_t;


/*----------------------------------------------------------
	Audio Channel (Abstraction for channel control)
----------------------------------------------------------*/
typedef struct audio_channel_t
{
	audio_voice_t	*voices;	/* For "real" voices only */

	int		playing;	/* # of "voices" playing */
	accbank_t	rctl;		/* Raw controls */
	accbank_t	ctl;		/* Transformed controls */

	/* Event Control */
	aev_port_t	port;		/* Event input port */

	patch_closure_t	closure;	/* Per-channel data for patch */
} audio_channel_t;


/*----------------------------------------------------------
	Audio Channel Event Interface
------------------------------------------------------------
 * These events are passed through the channel management
 * code of the engine, where they are transformed through
 * the groups. Then they're passed on directly to the patch
 * plugins for custom processing.
 *
 * Notes:
 *	* The rctl array (raw controls) is kept up to date
 *	  by the engine.
 *
 *	* The ctl array (transformed controls) must be
 *	  updated by the patch plugin as it receives the
 *	  corresponding events.
 *
 *	* For things to work properly, patch plugins must
 *	  remove all events for each frame processed.
 *
 *	* Tags passed to CE_CONTROL[2] can be any positive
 *	  value including 0. Negative values are reserved
 *	  for special uses: -1 means "all voices", whereas
 *	  -2 means "new voices only"; ie -2 will cause no
 *	  other action than updating the channel control
 *	  values.
 */
typedef enum channel_events_t
{
	CE_START = 0,	/* arg1 = tag, arg2 = pitch, arg3 = velocity */
	CE_STOP,	/* arg1 = tag, arg2 = velocity */
	CE_CONTROL	/* arg1 = tag, index = ctl, arg2 = value */
} channel_events_t;


/*
 * Inline interface for sending the above events...
 */

static inline aev_event_t *ce_start_i(audio_channel_t *c, Uint16 delay,
		int tag, int pitch, int velocity)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_START;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->arg2 = pitch;
	ev->arg3 = velocity;
	aev_insert(&c->port, ev);
	return ev;
}

static inline aev_event_t *ce_stop_i(audio_channel_t *c, Uint16 delay,
		int tag, int velocity)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_STOP;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->arg2 = velocity;
	aev_insert(&c->port, ev);
	return ev;
}

static inline aev_event_t *ce_control_i(audio_channel_t *c, Uint16 delay,
		int tag, Uint8 ctl, int value)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_CONTROL;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->index = ctl;
	ev->arg2 = value;
	aev_insert(&c->port, ev);
	return ev;
}


/*
 * Fast, non-sorting versions.
 */

static inline aev_event_t *ce_start(audio_channel_t *c, Uint16 delay,
		int tag, int pitch, int velocity)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_START;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->arg2 = pitch;
	ev->arg3 = velocity;
	aev_write(&c->port, ev);
	return ev;
}

static inline aev_event_t *ce_stop(audio_channel_t *c, Uint16 delay,
		int tag, int velocity)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_STOP;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->arg2 = velocity;
	aev_write(&c->port, ev);
	return ev;
}

static inline aev_event_t *ce_control(audio_channel_t *c, Uint16 delay,
		int tag, Uint8 ctl, int value)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = CE_CONTROL;
	ev->frame = delay + aev_timer;
	ev->arg1 = tag;
	ev->index = ctl;
	ev->arg2 = value;
	aev_write(&c->port, ev);
	return ev;
}


/*
 * Brutally stop all voices on all channels.
 *
 * (Obviously, any other method is better - short of
 * crashing the engine, perhaps... ;-)
 */
void channel_stop_all(void);


/*
 * Process events and patch plugins for all channels.
 */
void channel_process_all(unsigned frames);

void audio_channel_open(void);
void audio_channel_close(void);


/*----------------------------------------------------------
	Voice allocation stuff
----------------------------------------------------------*/

/* Link voice 'v' to channel 'c'. */
static inline void chan_link_voice(audio_channel_t *c, audio_voice_t *v)
{
	v->next = c->voices;
	if(v->next)
		v->next->prev = v;
	v->prev = NULL;
	v->channel = c;
	c->voices = v;
}

/* Unlink voice 'v' from channel 'c'. */
static inline void chan_unlink_voice(audio_voice_t *v)
{
	if(v->prev)
	{
		v->prev->next = v->next;
		if(v->next)
			v->next->prev = v->prev;
	}
	else
	{
		v->channel->voices = v->next;
		if(v->next)
			v->next->prev = NULL;
	}
	v->channel = NULL;
}

static inline audio_voice_t *chan_get_first_voice(audio_channel_t *c)
{
	return c->voices;
}

static inline audio_voice_t *chan_get_next_voice(audio_voice_t *v)
{
	return v->next;
}

#endif /*_A_CHANNEL_H_*/
