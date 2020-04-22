/*(LGPL)
---------------------------------------------------------------------------
	a_bus.h - Audio Engine bus
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

#ifndef _A_BUS_H_
#define _A_BUS_H_

#include "a_globals.h"
#include "a_types.h"
#include "a_plugin.h"
#include "a_filters.h"

typedef struct
{
	/* Control */
	abcbank_t	bctl[AUDIO_MAX_INSERTS+1];

	/* DC filter (applied before all sends and inserts) */
	dcf6s_t		dcfilter;

	int		in_use;		/* Set by whoever sends to the bus */

	/* Insert FX plugins */
	audio_plugin_t	insert[AUDIO_MAX_INSERTS];
} audio_bus_t;

void audio_bus_open(void);
void audio_bus_close(void);

void bus_process_all(int *bufs[], int *master, unsigned frames);
void bus_ctl_set(unsigned bus, unsigned slot, unsigned ctl, int arg);

#endif /*_A_BUS_H_*/
