/*(LGPL)
---------------------------------------------------------------------------
	a_commands.c - Asynchronous Command Interface for the audio engine
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

#include <stdlib.h>

#include "kobolog.h"
#include "a_globals.h"

/*
 * These really shouldn't be here, but are
 * required for some shortcuts around here...
 */
#include "a_struct.h"
#include "a_sequencer.h"
#include "a_control.h"

#undef	DBG2D

sfifo_t commands;

static inline void __push_command(command_t *cmd)
{
	if(sfifo_space(&commands) < sizeof(command_t))
	{
		if(_audio_running)
			log_printf(WLOG, "Audio command FIFO overflow!\n");
		return;
	}
	sfifo_write(&commands, cmd, (unsigned)sizeof(command_t));
}


/*----------------------------------------------------------
	Mixer Control
----------------------------------------------------------*/
void audio_bus_controlf(unsigned bus, unsigned slot, unsigned ctl, float arg)
{
	audio_bus_control(bus, slot, ctl, (int)(arg * 65536.0));
}

void audio_bus_control(unsigned bus, unsigned slot, unsigned ctl, int arg)
{
	command_t cmd;
#ifdef AUDIO_SAFE
	if(bus >= AUDIO_MAX_BUSSES)
	{
		log_printf(ELOG, "audio_bus_control(): Bus out of range!\n");
		return;
	}
	if(ctl >= ABC_COUNT)
	{
		log_printf(ELOG, "audio_bus_control(): Control out of range!\n");
		return;
	}
#endif
	cmd.action = CMD_MCONTROL;
	cmd.cid = (signed char)bus;
	cmd.index = ctl;
	cmd.arg1 = (int)slot;
	cmd.arg2 = arg;
	__push_command(&cmd);
}


/*----------------------------------------------------------
	Group Control
----------------------------------------------------------*/

void audio_group_stop(unsigned gid)
{
	int i;
#ifdef AUDIO_SAFE
	if(gid >= AUDIO_MAX_GROUPS)
	{
		log_printf(ELOG, "audio_group_stop(): Group out of range!\n");
		return;
	}
#endif
/*
FIXME: This isn't exactly thread safe...
*/
	for(i = 0; i < AUDIO_MAX_VOICES; i++)
	{
		audio_channel_t *c;
		audio_voice_t *v = voicetab + i;
		if(VS_STOPPED == v->state)
			continue;
		c = v->channel;
		if((unsigned)c->ctl[ACC_GROUP] == gid)
			audio_channel_stop(i, -1);
	}
}

void audio_group_controlf(unsigned gid, unsigned ctl, float arg)
{
	audio_group_control(gid, ctl, (int)(arg * 65536.0));
}

void audio_group_control(unsigned gid, unsigned ctl, int arg)
{
	command_t cmd;
#ifdef AUDIO_SAFE
	if(gid >= AUDIO_MAX_GROUPS)
	{
		log_printf(ELOG, "audio_group_control(): Group out of range!\n");
		return;
	}
	if(ctl >= ACC_COUNT)
	{
		log_printf(ELOG, "audio_group_control(): Control out of range!\n");
		return;
	}
#endif
	cmd.action = CMD_GCONTROL;
	cmd.cid = (char)gid;
	cmd.index = ctl;
	cmd.arg1 = arg;
	__push_command(&cmd);
}


/*----------------------------------------------------------
	Channel Control
----------------------------------------------------------*/

void audio_channel_play(int cid, int tag, int pitch, int velocity)
{
	command_t cmd;
#ifdef AUDIO_SAFE
	if(cid < 0 || cid >= AUDIO_MAX_CHANNELS)
	{
		log_printf(ELOG, "audio_play(): Channel out of range!\n");
		return;
	}
#endif
	cmd.action = CMD_PLAY;
	cmd.cid = cid;
	cmd.tag = tag;
	cmd.arg1 = pitch;
	cmd.arg2 = velocity;
	__push_command(&cmd);
}


void audio_channel_controlf(int cid, int tag, int ctl, float arg)
{
	if(ACC_IS_FIXEDPOINT(ctl))
		arg *= 65536.0;
	audio_channel_control(cid, tag, ctl, (int)arg);
}

void audio_channel_control(int cid, int tag, int ctl, int arg)
{
	command_t cmd;
#ifdef AUDIO_SAFE
	if(cid < 0 || cid >= AUDIO_MAX_CHANNELS)
	{
		log_printf(ELOG, "audio_channel_control(): Channel out of range!\n");
		return;
	}
	if(ctl < 0 || ctl >= ACC_COUNT)
	{
		log_printf(ELOG, "audio_channel_control(): Control out of range!\n");
		return;
	}
#endif
	cmd.action = CMD_CCONTROL;
	cmd.cid = cid;
	cmd.tag = tag;
	cmd.index = (unsigned char)ctl;
	cmd.arg1 = arg;
	__push_command(&cmd);
}


void audio_channel_stop(int cid, int tag)
{
	command_t cmd;
#ifdef AUDIO_SAFE
	if(cid >= AUDIO_MAX_CHANNELS)
	{
		log_printf(ELOG, "audio_stop(): Channel out of range!\n");
		return;
	}
#endif
	if(cid >= 0)
		cmd.action = CMD_STOP;
	else
		cmd.action = CMD_STOP_ALL;
	cmd.cid = cid;
	cmd.tag = tag;
	__push_command(&cmd);
}


int audio_channel_playing(int cid)
{
	return channeltab[cid].playing;
}


void audio_bump(unsigned ms)
{
	command_t cmd;
	cmd.action = CMD_WAIT;
	cmd.arg1 = (int)ms;
	__push_command(&cmd);
}
