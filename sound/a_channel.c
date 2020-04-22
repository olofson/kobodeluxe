/*(LGPL)
---------------------------------------------------------------------------
	a_channel.c - Audio Engine high level control abstraction
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

#include <stdlib.h>

#include "kobolog.h"
#include "a_globals.h"
#include "a_struct.h"
#include "a_control.h"
#include "a_sequencer.h"
#include "a_tools.h"

#define	SDBG(x)

void channel_stop_all(void)
{
	int i;
	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
		voice_kill(voicetab + i);
	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
		channeltab[i].playing = 0;	/* Reset "voice" count */
	sequencer_stop(-1);
}


/*
FIXME:	What if there are sounds playing when an ACC_PATCH comes in!?
FIXME:	Maybe patch plugins should have a "detach + release" feature?
FIXME:
FIXME:	Another alternative would be some way of forwarding events to
FIXME:	"unknown" tags (easy now that the controller must provide the
FIXME:	tags!) on to whatever patch they were started by. That is, an
FIXME:	actively managed, per channel "patch history". "Affect all"
FIXME:	events (-1 tags) would have to be processed *and* forwarded.
FIXME:
FIXME:	Patch plugins could just pass events for unknown tags to some
FIXME:	"forward to previous patch" event port, that the engine checks
FIXME:	after running a patch plugin. Of course, this would require
FIXME:	that all patches actually remember tags, even if they don't
FIXME:	really care about them... In fact, it would be a problem even
FIXME:	for patches that *do* use tags, as those will generally
FIXME:	"remember" only tags that actually get the required resources.
FIXME:	(That is, if a poly patch runs out of voices, it'll start
FIXME:	forgetting tags...)
FIXME:
FIXME:	A much simpler solution would be to just keep patches running
FIXME:	until they say they have no more "contexts" active. Events
FIXME:	would have to be duplicated - or we could have all plugins
FIXME:	just pass events on to a "garbage port", that actually becomes
FIXME:	a forwarding port when needed. In the normal case, the engine
FIXME:	would just flush this port when appropriate.
*/
/*
FIXME:	Maybe we actually want to pass ACC_PATCH change events
FIXME:	on to patch drivers? If so, should they be sent to
FIXME:	the old or new patch, or both? That question alone
FIXME:	seems to suggest that ACC_PATCH should die here, but
FIXME:	then again, some drivers might need these events...
*/

static inline void channel_process(audio_channel_t *c, unsigned frames)
{
	aev_event_t *first_ev = c->port.first;
	unsigned split_frames = frames;

	/*
	 * This one's rather ugly...  Better ideas, anyone?
	 */
	aev_timestamp_t timer_save = aev_timer;

	while(1)
	{
		aev_event_t *ev = c->port.first;
		aev_event_t *prev_ev = NULL;
		aev_event_t *rerun_ev = NULL;
		while(ev && (AEV_TIME(ev->frame, 0) < split_frames))
		{
			if(ev->type != CE_CONTROL)
			{
				prev_ev = ev;
				ev = ev->next;
				continue;
			}

			c->rctl[ev->index] = ev->arg2;
			switch(ev->index)
			{
/*			  case_ACC_NOTRANSFORM:*/
			  case ACC_GROUP:
			  case ACC_PRIORITY:
				break;
			  case ACC_PATCH:
			  {
				aev_event_t *del_ev = ev;
SDBG(log_printf(D3LOG, "(patch) ");)
				if(c->ctl[ACC_PATCH] == c->rctl[ACC_PATCH])
				{
SDBG(log_printf(D3LOG, "[IGNORE]\n");)
					ev = ev->next;
					if(prev_ev)
						prev_ev->next = ev;
					else
						c->port.first = ev;
				}
				else if(ev == first_ev)
				{
SDBG(log_printf(D3LOG, "[FIRST]\n");)
					c->ctl[ACC_PATCH] = c->rctl[ACC_PATCH];
					ev = ev->next;
					c->port.first = ev;
					first_ev = NULL;
				}
				else
				{
SDBG(log_printf(D3LOG, "[SPLIT]\n");)
					split_frames = AEV_TIME(ev->frame, 0);
					rerun_ev = ev->next;
					/*
					 * Note that we really must break the list!
					 * Going by the timestams is insufficient,
					 * as there may be more events for the same
					 * frame.
					 */
					if(prev_ev)
						prev_ev->next = NULL;
					else
						c->port.first = NULL;
					ev = NULL;
				}
				aev_free(del_ev);
				continue;
			  }
			  case ACC_PRIM_BUS:
			  case ACC_SEND_BUS:
				break;
			  case_ACC_ADDTRANSFORM:
			  {
				audio_group_t *g;
				int min, max;
				g = grouptab + c->rctl[ACC_GROUP];
				min = addtrans_min[ev->index -
						ACC_ADDTRANSFORM_FIRST];
				max = addtrans_max[ev->index -
						ACC_ADDTRANSFORM_FIRST];
				ev->arg2 += g->ctl[ev->index];
				ev->arg2 -= addtrans_bias[ev->index -
						ACC_ADDTRANSFORM_FIRST];
				if(ev->arg2 < min)
					ev->arg2 = min;
				else if(ev->arg2 > max)
					ev->arg2 = max;
				break;
			  }
			  case_ACC_MULTRANSFORM:
			  {
				audio_group_t *g;
				g = grouptab + c->rctl[ACC_GROUP];
				ev->arg2 = fixmul(ev->arg2, g->ctl[ev->index]);
				break;
			  }
			}
			prev_ev = ev;
			ev = ev->next;
		}

		/* Run the patch plugin! */
/*		if(c->ctl[ACC_PATCH] < AUDIO_MAX_PATCHES)*/
		{
			audio_patch_t *p = patchtab + c->ctl[ACC_PATCH];
			p->process(p, c, split_frames);
		}

		/* Apply last patch change, if any */
		c->ctl[ACC_PATCH] = c->rctl[ACC_PATCH];

		frames -= split_frames;
		if(!frames)
			break;	/* Done! */

SDBG(log_printf(D3LOG, "split!\n");)
		/* Set up event port and timing for next fragment */
		aev_advance_timer(split_frames);
		split_frames = frames;
		c->port.first = rerun_ev;
	}
	aev_timer = timer_save;
}


void channel_process_all(unsigned frames)
{
	int i;
	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
		channel_process(channeltab + i, frames);
}


static int _wasinit = 0;

void audio_channel_open(void)
{
	int i;
	if(_wasinit)
		return;

	memset(channeltab, 0, sizeof(channeltab));
	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
	{
		char *buf = malloc(64);
		snprintf(buf, 64, "Audio Channel %d", i);
		aev_port_init(&channeltab[i].port, buf);
		acc_copya(&channeltab[i].rctl, &a_channel_def_ctl);
		acc_copya(&channeltab[i].ctl, &a_channel_def_ctl);
		channeltab[i].voices = NULL;
	}
	_wasinit = 1;
}


void audio_channel_close(void)
{
	int i;
	if(!_wasinit)
		return;
	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
	{
		aev_flush(&channeltab[i].port);
		free((char *)channeltab[i].port.name);
	}
	memset(channeltab, 0, sizeof(channeltab));
	_wasinit = 0;
}
