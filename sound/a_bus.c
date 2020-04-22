/*(LGPL)
---------------------------------------------------------------------------
	a_bus.c - Audio Engine bus
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

#include <stdlib.h>
#include <string.h>
#include "a_bus.h"
#include "a_struct.h"
#include "a_delay.h"
#include "a_tools.h"

#define	BDBG(x)


static inline int __bus_run_fx(int bus, int slot, int *busses[], unsigned frames)
{
	audio_bus_t *b = &bustab[bus];
	--slot;	/* No IFX on slot 0! */
	switch(b->insert[slot].current_state)
	{
	  case FX_STATE_RUNNING:
	  case FX_STATE_SILENT:
	  case FX_STATE_RESTING:
		if(b->in_use)	/* Do we have input? */
			b->insert[slot].process(&b->insert[slot],
					busses[bus], frames);
		else
		{
			b->insert[slot].process_r(&b->insert[slot],
					NULL, busses[bus], frames);
			b->in_use = 1;
		}
		/* Check if the plugin actually produced valid output! */
		return (FX_STATE_RUNNING == b->insert[slot].current_state);
	  default:
		/* No plugin, or plugin not running. */
		return 0;
	}
}


static inline void __bus_process(int bus, int *busses[], int *master, unsigned frames)
{
	audio_bus_t *b = &bustab[bus];
	int i, s;

	/* Tail detection hack for the DC supression filter... */
	if(!b->in_use)
		if(!dcf6s_silent(&b->dcfilter))
		{
			b->in_use = 1;
			s32clear(busses[bus], frames);
		}

	if(b->in_use)	/* Only if we actually have input! */
	{
		/* DC suppression filter */
		dcf6s_process(&b->dcfilter, busses[bus], frames);

		/* Mix pre-insert sends */
		s32mix(busses[bus], master, b->bctl[0][ABC_SEND_MASTER], frames);
		for(i = bus + 1; i < AUDIO_MAX_BUSSES; ++i)
			if(s32mix(busses[bus], busses[i],
					b->bctl[0][ABC_SEND_BUS_0 + i],
					frames))
				bustab[i].in_use = 1;
	}

	/* Mix post-insert sends */
	for(s = 1; s <= AUDIO_MAX_INSERTS; ++s)
	{
		/* Run the insert */
		if(!__bus_run_fx(bus, s, busses, frames))
			continue;	/* No output, no sends! */

		s32mix(busses[bus], master, b->bctl[s][ABC_SEND_MASTER], frames);
		for(i = bus + 1; i < AUDIO_MAX_BUSSES; ++i)
			if(s32mix(busses[bus], busses[i],
					b->bctl[s][ABC_SEND_BUS_0 + i],
					frames))
				bustab[i].in_use = 1;
	}

	/* Prepare for the next engine cycle */
	if(b->in_use)
	{
		s32clear(busses[bus], frames);
		b->in_use = 0;
	}
}


void bus_process_all(int *bufs[], int *master, unsigned frames)
{
	int i;
	for(i = 0; i < AUDIO_MAX_BUSSES; ++i)
		__bus_process(i, bufs, master, frames);
}


static void __remove_insert(unsigned bus, unsigned insert)
{
	audio_bus_t *b = &bustab[bus];
	audio_plugin_close(&b->insert[insert]);
}


static void __change_insert(unsigned bus, unsigned insert, int fx)
{
	int reopen = 0;
	audio_bus_t *b = &bustab[bus];
	audio_plugin_t *ins;
	if(bus < 1)
		return;

	__remove_insert(bus, insert);
	ins = &b->insert[insert-1];

	switch(fx)
	{
	  case AFX_NONE:
		break;
	  case AFX_DELAY:
		break;
	  case AFX_ECHO:
		break;
	  case AFX_REVERB:
		delay_init(ins);
		reopen = 1;
		break;
	  case AFX_CHORUS:
		break;
	  case AFX_FLANGER:
		break;
	  case AFX_FILTER:
		break;
	  case AFX_DIST:
		break;
	  case AFX_WAH:
		break;
	  case AFX_EQ:
		break;
	  case AFX_ENHANCER:
		break;
	  case AFX_COMPRESSOR:
		break;
	  case AFX_MAXIMIZER:
		break;
	}

	if(!reopen)
		return;

	if(audio_plugin_open(ins, a_settings.buffersize,
			a_settings.samplerate, a_settings.quality) < 0)
		return;

/* FIXME: This should be done in a butler thread... */
	if(audio_plugin_state(ins, FX_STATE_RUNNING) < 0)
	{
		audio_plugin_close(ins);
		return;
	}
/* /FIXME */

/*
FIXME:	We rely on plugins loading their defaults. Plugins should
FIXME:	probably be able to export their default settings instead.
*/
#if 0
	for(i = 0; i < 5; ++i)
		ins->control(ins, FXC_PARAM_1 + i,
				b->bctl[insert][ABC_FX1_PARAM_1 + i]);
#endif

	b->bctl[insert][ABC_FX_TYPE] = fx;
}


void bus_ctl_set(unsigned bus, unsigned slot, unsigned ctl, int arg)
{
	if(0 BDBG(+ 1))
	{
		const char *ctls[] = {
			"ABC_FX_TYPE",
			"ABC_FX_PARAM_1",
			"ABC_FX_PARAM_2",
			"ABC_FX_PARAM_3",
			"ABC_FX_PARAM_4",
			"ABC_FX_PARAM_5",
			"ABC_FX_PARAM_6",

			"ABC_SEND_MASTER",

			"ABC_SEND_BUS_0",
			"ABC_SEND_BUS_1",
			"ABC_SEND_BUS_2",
			"ABC_SEND_BUS_3",
			"ABC_SEND_BUS_4",
			"ABC_SEND_BUS_5",
			"ABC_SEND_BUS_6",
			"ABC_SEND_BUS_7",
			"<ILLEGAL!>"
		};
		unsigned c = ctl;
		if(ctl < ABC_FIRST || ctl > ABC_LAST)
			c = (unsigned)(ABC_LAST + 1);
		log_printf(DLOG, "Bus %u, slot %u %s set to %d\n", bus, slot, ctls[c], arg);
	}

	switch(ctl)
	{
	  case ABC_FX_TYPE:
		if(arg != bustab[bus].bctl[slot][ctl])
			__change_insert(bus, slot, arg);
		break;
	  case ABC_FX_PARAM_1:
	  case ABC_FX_PARAM_2:
	  case ABC_FX_PARAM_3:
	  case ABC_FX_PARAM_4:
	  case ABC_FX_PARAM_5:
	  case ABC_FX_PARAM_6:
		if(slot < 1)
			break;
		if(bustab[bus].insert[slot].current_state < FX_STATE_READY)
			break;
		bustab[bus].insert[slot].control(&bustab[bus].insert[slot],
				ctl - ABC_FX_PARAM_1 + FXC_PARAM_1,
				arg);
		bustab[bus].bctl[slot][ctl] = arg;
		break;
	  default:
		if(slot >= AUDIO_MAX_INSERTS)
			break;
		if(ctl > ABC_LAST)
			break;
		bustab[bus].bctl[slot][ctl] = arg;
		break;
	}
	/*
	 * This is where the "connection table" would be
	 * updated in a more optimized implementation.
	 */
}


static int _is_open = 0;

void audio_bus_open(void)
{
	int i, s;
	if(_is_open)
		return;

	memset(bustab, 0, sizeof(bustab));
	for(i = 0; i < AUDIO_MAX_BUSSES; ++i)
	{
		for(s = 0; s < AUDIO_MAX_INSERTS; ++s)
			memcpy(bustab[i].bctl[s], a_bus_def_ctl,
					sizeof(a_bus_def_ctl));
		dcf6s_init(&bustab[i].dcfilter, a_settings.samplerate);
		dcf6s_set_f(&bustab[i].dcfilter, 10);
		bustab[i].in_use = 0;
	}
	_is_open = 1;
}


void audio_bus_close(void)
{
	unsigned i, j;
	if(!_is_open)
		return;

	for(i = 0; i < AUDIO_MAX_BUSSES; ++i)
		for(j = 0; j < AUDIO_MAX_INSERTS; ++j)
			__change_insert(i, j, AFX_NONE);
	memset(bustab, 0, sizeof(bustab));
	_is_open = 0;
}
