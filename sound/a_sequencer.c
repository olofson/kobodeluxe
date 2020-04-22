/*(LGPL)
---------------------------------------------------------------------------
	a_sequencer.c - MIDI Sequencer
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

#include "a_globals.h"
#include "a_struct.h"

#include "a_sequencer.h"
#include "a_midisock.h"

/*
 * Temporary MIDI playback hack.
 *
 * Only one sequence at a time, and it uses the
 * same midisock as the external MIDI input.
 */

midi_player_t *midiplayer = 0;
static float fs = 44100;


int sequencer_open(midisock_t *ms, float framerate)
{
	aev_client("sequencer_open()");
	fs = framerate;
#if 1
	midiplayer = mp_open(ms);
#else
	midiplayer = mp_open(&monitor_midisock);
#endif
	return (midiplayer != 0);
}


void sequencer_close(void)
{
	aev_client("sequencer_close()");
	if(!midiplayer)
		return;

	mp_close(midiplayer);
	midiplayer = 0;
}


int sequencer_play(midi_file_t *mf, int tag, int pitch, int volume)
{
	aev_client("sequencer_play()");
	if(!midiplayer)
		return -1;

	midiplayer->pitch = pitch - (60 << 16);
	mp_select(midiplayer, mf);
	return 0;
}


void sequencer_stop(int tag)
{
	aev_client("sequencer_stop()");
	if(!midiplayer)
		return;

	mp_stop(midiplayer);
}


int sequencer_playing(int tag)
{
	if(!midiplayer)
		return 0;

	return (midiplayer->mf != 0);
}


void sequencer_process(unsigned frames)
{
	int i;
	aev_client("sequencer_process()");
	if(!midiplayer)
		return;

	if(mp_play(midiplayer, (float)frames / fs))
		return;

	for(i = 0; i < AUDIO_MAX_CHANNELS; ++i)
	{
		int wave;
		if(!channeltab[i].playing)
			continue;
		wave = patchtab[channeltab[i].ctl[ACC_PATCH]].param[APP_WAVE];
		if((wave < 0) || (wave >= AUDIO_MAX_WAVES))
			continue;
		if(AF_MIDI != wavetab[wave].format)
			continue;
		channeltab[i].playing = 0;
	}
}
