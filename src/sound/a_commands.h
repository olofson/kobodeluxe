/*(LGPL)
---------------------------------------------------------------------------
	asynccmd.h - Asynchronous Command Interface for the audio engine
---------------------------------------------------------------------------
 * Copyright (C) 2001-2003, 2007 David Olofson
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

#ifndef	_ASYNCCMD_H_
#define	_ASYNCCMD_H_

#include "sfifo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	MAX_COMMANDS	128

/*----------------------------------------------------------
	Asynchronous command interface stuff
----------------------------------------------------------*/

typedef struct
{
	enum
	{
		CMD_STOP = 0,
		CMD_STOP_ALL,
		CMD_PLAY,
		CMD_CCONTROL,	/* Channel Control */
		CMD_GCONTROL,	/* Group Control */
		CMD_MCONTROL,	/* Mixer Control */
		CMD_WAIT	/* Advance Audio Time */
	} action;
	signed char	cid;
	unsigned char	index;
	int		tag;
	int		arg1;
	int		arg2;
} command_t;

extern sfifo_t commands;


/*----------------------------------------------------------
	Mixer Control
----------------------------------------------------------*/
void audio_bus_controlf(unsigned bus, unsigned slot, unsigned ctl, float arg);
void audio_bus_control(unsigned bus, unsigned slot, unsigned ctl, int arg);


/*----------------------------------------------------------
	Group Control
----------------------------------------------------------*/
//void audio_group_default(unsigned gid, unsigned ctl, int arg);

void audio_group_stop(unsigned gid);
void audio_group_controlf(unsigned gid, unsigned ctl, float arg);
void audio_group_control(unsigned gid, unsigned ctl, int arg);


/*----------------------------------------------------------
	Channel Control
----------------------------------------------------------*/
void audio_channel_play(int cid, int tag, int pitch, int velocity);
void audio_channel_controlf(int cid, int tag, int ctl, float arg);
void audio_channel_control(int cid, int tag, int ctl, int arg);
void audio_channel_stop(int cid, int tag);	/* -1 to stop all */

/*
FIXME: Broken API. Broken implementation. It just "seems" to work.
*/
int audio_channel_playing(int cid);

/*
 * Set audio time for subsequent calls to 'ms'.
 */
void audio_bump(unsigned ms);

#ifdef __cplusplus
};
#endif

#endif /*_ASYNCCMD_H_*/
