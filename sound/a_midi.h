/*(LGPL)
---------------------------------------------------------------------------
	a_midi.h - midi_socket_t MIDI Input Driver for OSS or ALSA
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

#ifndef _A_MIDI_H_
#define _A_MIDI_H_

#include "a_midisock.h"

/*
 * Define if you'd like to layer with
 * an external synth or something...
 */
#undef MIDI_THRU

#define MIDIMAXBYTES	1024


int midi_open(midisock_t *output);
void midi_close(void);

void midi_process(void);

#endif /*_A_MIDI_H_*/
