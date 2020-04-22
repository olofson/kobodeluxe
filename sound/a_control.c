/*(LGPL)
---------------------------------------------------------------------------
	a_control.c - Group/Channel/Voice Control Implementation
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
 *	The control routing model:
 *
 *		         Group
 *		         ^   |
 *		         |   V
 *		MIDI -> Channel -> Patch ---> Voice
 *		 |                        |-> Voice
 *		 V                        ...  ...
 *		Bus Control               '-> Voice
 *
 *	Groups are used by Channels, to transform some of the control
 *	data sent to them, before passing it on to the current Patch.
 *	That is, they would effectively fit in in between Channels
 *	and Patches, from the data flow POV.
 *
 *	Note that while Groups act as passive data containers most of
 *	the time, they *can* become active, and force Channels to
 *	refresh some Controls. This can happen when changing the
 *	"music volume" in an application, for example.
FIXME: What about busses with non-linear insert effects? Looks like
FIXME: at least *some* Group Controls should not be applied on the
FIXME: Channel or Patch level, but rather on the Busses... Maybe
FIXME: Busses should have 'group' fields? That way, one can still
FIXME: use them as with the current design, although for example
FIXME: ACC_VOLUME would be sent off to the bus send that acts as
FIXME: master volume for all Voices of the Group.
FIXME:   Then again, this may result in general confusion! It's
FIXME: theoretically possible to select busses on a *per-Voice
FIXME: basis* (via Patches), and quite easy to select busses
FIXME: directly using MIDI CCs. Good bye to application control
FIXME: over the mixing...!
FIXME:   The only simple solution I can see is to prevent sending
FIXME: between busses across group boundaries.
 */

#include "a_globals.h"
#include "a_control.h"
#include "a_tools.h"

const int addtrans_min[2] = {	-65536,	0	};
const int addtrans_max[2] = {	65536,	128<<16	};
const int addtrans_bias[2] = {	0,	60<<16	};


/*----------------------------------------------------------
	Group Control
----------------------------------------------------------*/

void acc_group_set(unsigned gid, unsigned ctl, int arg)
{
	int i;
	audio_group_t *g = grouptab + gid;
	g->ctl[ctl] = arg;

	if(!_audio_running)
		return;

	/* Update all channels of the group */
	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
	{
		audio_channel_t *c = channeltab + i;
		if((unsigned)c->ctl[ACC_GROUP] != gid)
			continue;
		(void)ce_control_i(c, 0, -1, ctl, c->rctl[ctl]);
	}
}


static int _is_open = 0;

void audio_group_open(void)
{
	int i;
	if(_is_open)
		return;

	memset(grouptab, 0, sizeof(grouptab));
	for(i = 0; i < AUDIO_MAX_GROUPS; ++i)
		acc_copya(&grouptab[i].ctl, &a_group_def_ctl);
	_is_open = 1;
}


void audio_group_close(void)
{
	if(!_is_open)
		return;

	memset(grouptab, 0, sizeof(grouptab));
	_is_open = 0;
}
