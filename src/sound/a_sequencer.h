/*(LGPL)
---------------------------------------------------------------------------
	a_sequencer.h - MIDI Sequencer
---------------------------------------------------------------------------
 * Copyright (C) 2002, David Olofson
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

#ifndef _A_SEQUENCER_H_
#define _A_SEQUENCER_H_

#include "a_midisock.h"
#include "a_midifile.h"

int sequencer_open(midisock_t *ms, float framerate);
void sequencer_close(void);

/*
 * Starts playing 'mf' at 'pitch' (0 ==> normal) and 'volume'.
 * The sequencer context is marked with the positive value 'tag'
 * for later reference. 'pitch' and 'volume' are 16:16 fixed
 * point values, to match the engine control data type.
 * Returns a negative value if an error occurred.
 */
int sequencer_play(midi_file_t *mf, int tag, int pitch, int volume);

/*
 * Stop the sequence marked with 'tag'.
 * Pass -1 for 'tag' to stop all playing sequences.
 */
void sequencer_stop(int tag);

/* Returns 1 if the sequence marked 'tag' is still playing. */
int sequencer_playing(int tag);

void sequencer_process(unsigned frames);

#endif /*_A_SEQUENCER_H_*/
