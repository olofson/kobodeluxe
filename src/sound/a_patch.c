/*(LGPL)
---------------------------------------------------------------------------
	a_patch.c - Audio Engine "instrument" definitions
---------------------------------------------------------------------------
 * Copyright (C) 2001-2003, David Olofson
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

#include <string.h>

#include "kobolog.h"
#include "a_struct.h"
#include "a_patch.h"
#include "a_sequencer.h"

#define	EDBG(x)
#define	EGDBG(x)

static int _is_open = 0;
#define	CHECKINIT	if(!_is_open) audio_patch_open();

/*
 * Common tools for mono and poly...
 */

static inline int __calc_randpitch(audio_patch_t *p, int pitch)
{
	static int rnd = 16576;
	int rp = p->param[APP_RANDPITCH];
	if(rp)
	{
		rnd *= 1566083941UL;
		rnd++;
		rnd &= 0xffffffffUL;
		pitch += (rnd % rp) - rp/2;
	}
	return pitch;
}


static inline int __get_wave(audio_patch_t *p)
{
	int wave = p->param[APP_WAVE];
	if((wave < 0) || (wave >= AUDIO_MAX_WAVES))
		return -1;

	if(AF_MIDI == wavetab[wave].format)
		return -1;

	return wave;
}


/* Update base vol/send levels after control change. */
static inline void __env_control_recalc(audio_patch_t *p, audio_channel_t *c,
		audio_voice_t *v)
{
	patch_closure_t *clos = &v->closure;

	clos->lvol = clos->rvol = clos->velvol;
	clos->lvol *= 64 - (c->ctl[ACC_PAN] >> 10);
	clos->rvol *= 64 + (c->ctl[ACC_PAN] >> 10);
	clos->lvol >>= 6;
	clos->rvol >>= 6;

	clos->lsend = c->ctl[ACC_SEND] >> (16-VOL_BITS);
	clos->lsend *= clos->velvol;
	clos->lsend >>= 16;
	clos->rsend = clos->lsend;
	clos->lsend *= 64 - (c->ctl[ACC_PAN] >> 10);
	clos->rsend *= 64 + (c->ctl[ACC_PAN] >> 10);
	clos->lsend >>= 6;
	clos->rsend >>= 6;
}


/* Initialize envelope to PES_START. */
static inline void __env_start(audio_patch_t *p, audio_channel_t *c,
		audio_voice_t *v)
{
	patch_closure_t *clos = &v->closure;
	clos->env_state = PES_START;
	clos->env_next = aev_timer;
}


static inline void __env_stop(audio_patch_t *p, audio_channel_t *c,
		audio_voice_t *v)
{
	patch_closure_t *clos = &v->closure;
	if(p->param[APP_ENV_T3] >= 0)
		return;

EGDBG(log_printf(D3LOG, "stop!\n");)
	switch (clos->env_state)
	{
	  case PES_START:
	  case PES_START2:
	  case PES_DELAY:
	  case PES_ATTACK:
	  case PES_HOLD:
	  case PES_DECAY:
		clos->queued_stop = 1;
		if(p->param[APP_ENV_SKIP])
		{
			clos->env_next = aev_timer;
			clos->env_state = PES_SUSTAIN;
		}
		break;
	  case PES_SUSTAIN:	/* No interactive sustain! */
	  case PES_RELEASE:	/* Release already active... */
	  case PES_DEAD:
		break;
	  case PES_WAIT:
		clos->env_next = aev_timer;
		clos->env_state = PES_SUSTAIN;
		break;
	}
}

/*
FIXME: Support longer times than 65535 timestamp units.
FIXME: When that's done, this will need fixing as well...
*/
#define	S2S(x)	((((x)>>2) * a_settings.samplerate) >> 14)
static inline void __env_run(audio_patch_t *p, audio_channel_t *c,
		audio_voice_t *v, unsigned frames)
{
	patch_closure_t *clos = &v->closure;
	int lvol, rvol, lsend, rsend;
	aev_timestamp_t t;

	while((t = (unsigned)((clos->env_next - aev_timer) &
			AEV_TIMESTAMP_MASK)) < frames)
	{
		int duration;
		int target = 0;
		aev_timestamp_t timestamp = (aev_timestamp_t)
				(clos->env_next - aev_timer) &
				AEV_TIMESTAMP_MASK;
EGDBG(log_printf(D3LOG, "%d: ", clos->env_next);)
		switch(clos->env_state)
		{
		  case PES_START:
			duration = 0;
			target = p->param[APP_ENV_L0];
			clos->env_state = PES_START2;
			clos->queued_stop = 0;
			break;
		  case PES_START2:
			duration = S2S(p->param[APP_ENV_DELAY]);
			target = p->param[APP_ENV_L0];
			clos->env_state = PES_DELAY;
			clos->queued_stop = 0;
			break;
		  case PES_DELAY:
			duration = S2S(p->param[APP_ENV_T1]);
			target = p->param[APP_ENV_L1];
			clos->env_state = PES_ATTACK;
			break;
		  case PES_ATTACK:
			duration = S2S(p->param[APP_ENV_HOLD]);
			target = p->param[APP_ENV_L1];
			clos->env_state = PES_HOLD;
			break;
		  case PES_HOLD:
			duration = S2S(p->param[APP_ENV_T2]);
			target = p->param[APP_ENV_L2];
			clos->env_state = PES_DECAY;
			break;
		  case PES_DECAY:
			if(p->param[APP_ENV_T3] >= 0)
			{
				/* Timed sustain or no sustain */
				target = p->param[APP_ENV_L2];
				duration = S2S(p->param[APP_ENV_T3]);
				clos->env_state = PES_SUSTAIN;
				break;
			}
			else if(!clos->queued_stop)
			{
				/* Interactive sustain */
				target = p->param[APP_ENV_L2];
				duration = 30000;
				clos->env_state = PES_WAIT;
				break;
			}
			/* Fall though if interactive and stopped! */
		  case PES_SUSTAIN:
			duration = S2S(p->param[APP_ENV_T4]);
			target = 0;
			clos->env_state = PES_RELEASE;
			break;
		  case PES_WAIT:
			/* Interactive sustain */
			target = p->param[APP_ENV_L2];
			duration = 30000;
			break;
		  case PES_RELEASE:
			(void)aev_send0(&v->port, timestamp, VE_STOP);
			--c->playing;
		  case PES_DEAD:
			clos->env_next += 30000;
		  default:	/* Warning supressor */
			continue;
		}

EGDBG(log_printf(D3LOG, " (duration: %d, target: %d)\n", duration, target);)

		lvol = (clos->lvol >> 1) * target >> 15;
		rvol = (clos->rvol >> 1) * target >> 15;
		lsend = (clos->lsend >> 1) * target >> 15;
		rsend = (clos->rsend >> 1) * target >> 15;
		(void)aev_sendi2(&v->port, t, VE_IRAMP,
				VIC_LVOL, lvol, duration);
		(void)aev_sendi2(&v->port, t, VE_IRAMP,
				VIC_RVOL, rvol, duration);
		if(v->use_double)
		{
			(void)aev_sendi2(&v->port, t, VE_IRAMP,
					VIC_LSEND, lsend, duration);
			(void)aev_sendi2(&v->port, t, VE_IRAMP,
					VIC_RSEND, rsend, duration);
		}
		clos->env_next += duration;
	}
}


static void _env_run_all(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	audio_voice_t *v = chan_get_first_voice(c);
	while(v)
	{
		__env_run(p, c, v, frames);
		v = chan_get_next_voice(v);
	}
}


static inline void __start_voice(audio_patch_t *p, audio_channel_t *c,
		audio_voice_t *v, int wave, int pitch, int velocity)
{
	patch_closure_t *clos = &v->closure;

	/* Apply random pitch */
	pitch = __calc_randpitch(p, pitch);

	/* Store base pitch and vel to closure */
 	clos->pitch = pitch;
	clos->velocity = velocity;

	/* Transform to get actual pitch */
	pitch += c->ctl[ACC_PITCH];
	pitch -= 60<<16;

	/* Start voice! */
	(void)aev_sendi1(&v->port, 0, VE_SET, VC_PRIM_BUS,
			c->ctl[ACC_PRIM_BUS]);
	(void)aev_sendi1(&v->port, 0, VE_SET, VC_SEND_BUS,
			c->ctl[ACC_SEND_BUS]);
	(void)aev_send1(&v->port, 0, VE_START, wave);
	(void)aev_sendi1(&v->port, 0, VE_SET, VC_PITCH, pitch);

	/* Calculate "base volume" for envelope */
	clos->velvol = c->ctl[ACC_VOLUME] >> (16-VOL_BITS);
	clos->velvol *= velocity;
	clos->velvol >>= 16;
	__env_control_recalc(p, c, v);

	/* Initialize and start envelope */
	__env_start(p, c, v);

	++c->playing;
}


/*----------------------------------------------------------
	Default patches for polyphonic wave playback
----------------------------------------------------------*/

static inline void poly_start(audio_patch_t *p, audio_channel_t *c, int tag,
		int pitch, int velocity)
{
	int voice;
	int wave = __get_wave(p);
	if(wave < 0)
		return;

	voice = voice_alloc(c);
	if(voice < 0)
		return;

	__start_voice(p, c, voicetab + voice, wave, pitch, velocity);
	voicetab[voice].tag = tag;
}


static inline void poly_stop(audio_patch_t *p, audio_channel_t *c, int tag, int velocity)
{
	if(tag < 0)
	{
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			__env_stop(p, c, v);
			v = chan_get_next_voice(v);
		}
	}
	else
	{
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			if(v->tag == tag)
			{
				__env_stop(p, c, v);
				return;
			}
			v = chan_get_next_voice(v);
		}
	}
}


static inline void poly_control(audio_patch_t *p, audio_channel_t *c, int tag,
		int ctl, int arg)
{
	int mask;
	if(AVT_ALL == tag)
		mask = tag = 0;
	else
		mask = -1;

	switch(ctl)
	{
	  case ACC_GROUP:
	  case ACC_PRIORITY:
	  case ACC_PATCH:
		break;
	  case ACC_PRIM_BUS:
	  {
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			if((v->tag & mask) == tag)
				v->c[VC_PRIM_BUS] = arg;
			v = chan_get_next_voice(v);
		}
		break;
	  }
	  case ACC_SEND_BUS:
	  {
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			if((v->tag & mask) == tag)
				v->c[VC_SEND_BUS] = arg;
			v = chan_get_next_voice(v);
		}
		break;
	  }
	  case ACC_PITCH:
	  {
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			int pitch;
			if((v->tag & mask) == tag)
			{
				pitch = c->ctl[ACC_PITCH];
				pitch += v->closure.pitch;
				pitch -= 60<<16;
				(void)aev_sendi1(&v->port, 0, VE_SET,
						VC_PITCH, pitch);
			}
			v = chan_get_next_voice(v);
		}
		break;
	  }
	  case ACC_PAN:
	  case ACC_VOLUME:
	  case ACC_SEND:
	  {
		audio_voice_t *v = chan_get_first_voice(c);
		while(v)
		{
			if((v->tag & mask) == tag)
			{
/*
TODO:	Ramping...
*/
				__env_control_recalc(p, c, v);
			}
			v = chan_get_next_voice(v);
		}
		break;
	  }
	}
}


static void poly_process(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	unsigned eframes = frames ? frames : 1;
	while(aev_next(&c->port, 0) < eframes)
	{
		aev_event_t *ev = aev_read(&c->port);
		switch(ev->type)
		{
		  case CE_START:
EDBG(log_printf(DLOG, "poly: CE_START %d\n", p->param[APP_WAVE]);)
			poly_start(p, c, ev->arg1, ev->arg2, ev->arg3);
			break;
		  case CE_STOP:
EDBG(log_printf(DLOG, "poly: CE_STOP\n");)
			poly_stop(p, c, ev->arg1, ev->arg2);
			break;
		  case CE_CONTROL:
EDBG(log_printf(DLOG, "poly: CE_CONTROL %d = %d\n", ev->index, ev->arg2);)
			c->ctl[ev->index] = ev->arg2;
			if(AVT_FUTURE != ev->arg1)
				poly_control(p, c, ev->arg1, ev->index, ev->arg2);
			break;
		}
		aev_free(ev);
	}
/*
FIXME: We need to split buffers or something for control
FIXME: events that affect the envelope generators!
*/
	if(frames)
		_env_run_all(p, c, frames);
}


/*----------------------------------------------------------
	Default patches for monophonic wave playback
----------------------------------------------------------*/

static void mono_start(audio_patch_t *p, audio_channel_t *c, int tag,
		int pitch, int velocity)
{
	int wave = __get_wave(p);
	if(wave < 0)
		return;

	if(voice_alloc(c) < 0)
		return;

	__start_voice(p, c, c->voices, wave, pitch, velocity);
	c->voices->tag = tag;
	c->playing = 1;
}


static void mono_stop(audio_patch_t *p, audio_channel_t *c, int tag, int velocity)
{
	if(!c->voices)
		return;
	if(c->voices->channel != c)
		return;
	__env_stop(p, c, c->voices);
	c->playing = 0;
}


static void mono_control(audio_patch_t * p, audio_channel_t *c, int tag,
		int ctl, int arg)
{
	audio_voice_t *v = c->voices;
	if(!v)
		return;
	if(v->channel != c)
		return;

	switch (ctl)
	{
	  case ACC_GROUP:	/* We don't care. */
	  case ACC_PRIORITY:	/* Only for *new* voices. */
	  case ACC_PATCH:	/* We just got selected. And? :-) */
		break;
	  case ACC_PRIM_BUS:
		(void)aev_sendi1(&v->port, 0, VE_SET, VC_PRIM_BUS, arg);
		break;
	  case ACC_SEND_BUS:
		(void)aev_sendi1(&v->port, 0, VE_SET, VC_SEND_BUS, arg);
		break;
	  case ACC_PITCH:
		(void)aev_sendi1(&v->port, 0, VE_SET, VC_PITCH, arg);
		break;
	  case ACC_PAN:
	  case ACC_VOLUME:
	  case ACC_SEND:
		__env_control_recalc(p, c, v);
		break;
	}
}


static void mono_process(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	unsigned eframes = frames ? frames : 1;
	while(aev_next(&c->port, 0) < eframes)
	{
		aev_event_t *ev = aev_read(&c->port);
		switch(ev->type)
		{
		  case CE_START:
EDBG(log_printf(DLOG, "mono: CE_START %d\n", p->param[APP_WAVE]);)
			mono_start(p, c, ev->arg1, ev->arg2, ev->arg3);
			break;
		  case CE_STOP:
EDBG(log_printf(DLOG, "mono: CE_STOP\n");)
			mono_stop(p, c, ev->arg1, ev->arg2);
			break;
		  case CE_CONTROL:
EDBG(log_printf(DLOG, "mono: CE_CONTROL %d = %d\n", ev->index, ev->arg2);)
			c->ctl[ev->index] = ev->arg2;
			if(ACC_PATCH == ev->index)
				p = patchtab + c->ctl[ACC_PATCH];
			if(AVT_FUTURE != ev->arg1)
				mono_control(p, c, ev->arg1, ev->index, ev->arg2);
			break;
		}
		aev_free(ev);
	}
	if(frames)
		_env_run_all(p, c, frames);
}


/*----------------------------------------------------------
	Default patch for MIDI playback
----------------------------------------------------------*/

static void midi_start(audio_patch_t *p, audio_channel_t *c, int tag,
		int pitch, int velocity)
{
	int wave = p->param[APP_WAVE];
	if((wave < 0) || (wave >= AUDIO_MAX_WAVES))
		return;

	if(AF_MIDI != wavetab[wave].format)
		return;		/* Can't play this! */

	pitch = __calc_randpitch(p, pitch);
	pitch += c->ctl[ACC_PITCH];
	pitch -= 60<<16;
	if(pitch < 0)
		pitch = 0;
	else if(pitch > 128<<16)
		pitch = 128<<16;
	if(sequencer_play(wavetab[wave].data.midi, tag, pitch, velocity) < 0)
		return;

	++c->playing;
}


static void midi_process(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	unsigned eframes = frames ? frames : 1;
	while(aev_next(&c->port, 0) < eframes)
	{
		aev_event_t *ev = aev_read(&c->port);
		switch(ev->type)
		{
		  case CE_START:
EDBG(log_printf(DLOG, "midi: CE_START %d\n", p->param[APP_WAVE]);)
			midi_start(p, c, ev->arg1, ev->arg2, ev->arg3);
			break;
		  case CE_STOP:
EDBG(log_printf(DLOG, "midi: CE_STOP\n");)
/*
FIXME:	Handle cid == -1 correctly!
*/
			c->playing = 0;
			sequencer_stop(ev->arg1);
			break;
		  case CE_CONTROL:
EDBG(log_printf(DLOG, "midi: CE_CONTROL %d = %d\n", ev->index, ev->arg2);)
			c->ctl[ev->index] = ev->arg2;
			break;
		}
		aev_free(ev);
	}

	/* This is where to run the sequencer. */
}


/*----------------------------------------------------------
	User EEL Patch Driver
----------------------------------------------------------*/

static void eel_process(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	unsigned eframes = frames ? frames : 1;
	while(aev_next(&c->port, 0) < eframes)
	{
		aev_event_t *ev = aev_read(&c->port);
		switch(ev->type)
		{
		  case CE_START:
EDBG(log_printf(DLOG, "eel: CE_START %d\n", p->param[APP_WAVE]);)
			break;
		  case CE_STOP:
EDBG(log_printf(DLOG, "eel: CE_STOP\n");)
			break;
		  case CE_CONTROL:
EDBG(log_printf(DLOG, "eel: CE_CONTROL %d = %d\n", ev->index, ev->arg2);)
			c->ctl[ev->index] = ev->arg2;
			break;
		}
		aev_free(ev);
	}

	/* Run timer driven EEL code here. */
}


/*----------------------------------------------------------
	Dummy Patch Driver
----------------------------------------------------------*/

static void dummy_process(audio_patch_t *p, audio_channel_t *c, unsigned frames)
{
	unsigned eframes = frames ? frames : 1;
	while(aev_next(&c->port, 0) < eframes)
	{
		aev_event_t *ev = aev_read(&c->port);
		switch(ev->type)
		{
		  case CE_START:
EDBG(log_printf(DLOG, "dummy: CE_START %d\n", p->param[APP_WAVE]);)
			break;
		  case CE_STOP:
EDBG(log_printf(DLOG, "dummy: CE_STOP\n");)
			break;
		  case CE_CONTROL:
EDBG(log_printf(DLOG, "dummy: CE_CONTROL %d = %d\n", ev->index, ev->arg2);)
			c->ctl[ev->index] = ev->arg2;
			break;
		}
		aev_free(ev);
	}
}


/*----------------------------------------------------------
	Patch programming API
----------------------------------------------------------*/

/* Set parameter for a patch */
void audio_patch_param(int pid, int pparam, int value)
{
	CHECKINIT

	if(pid < 0)
		return;
	if(pid >= AUDIO_MAX_PATCHES)
		return;

	if(pparam < 0)
		return;
	if(pparam > APP_LAST)
		return;

	if(APP_DRIVER == pparam)
	{
		switch(value)
		{
		  case PD_MONO:
			patchtab[pid].process = mono_process;
			break;
		  case PD_POLY:
			patchtab[pid].process = poly_process;
			break;
		  case PD_MIDI:
			patchtab[pid].process = midi_process;
			break;
		  case PD_EEL:
			patchtab[pid].process = eel_process;
			break;
		  default:
			patchtab[pid].process = dummy_process;
			log_printf(ELOG, "a_patch.c: Illegal patch"
					" driver selected!\n");
			break;
		}
	}

	patchtab[pid].param[pparam] = value;
}


/*----------------------------------------------------------
	Global init
----------------------------------------------------------*/

void audio_patch_open(void)
{
	int i;
	if(_is_open)
		return;

	memset(patchtab, 0, sizeof(patchtab));
	for(i = 0; i < AUDIO_MAX_PATCHES; ++i)
	{
/*
		patchtab[i].param[APP_DRIVER] = PD_NONE;
		patchtab[i].process = dummy_process;
*/
/*
FIXME: This hack is just until the EEL<->patch binding is sorted out.
*/
		patchtab[i].param[APP_DRIVER] = PD_POLY;
		patchtab[i].process = poly_process;

		patchtab[i].param[APP_WAVE] = i;
		patchtab[i].param[APP_ENV_L0] = 1<<16;
		patchtab[i].param[APP_ENV_L1] = 1<<16;
		patchtab[i].param[APP_ENV_L2] = 1<<16;
		patchtab[i].param[APP_ENV_T3] = -1;
		patchtab[i].param[APP_ENV_T4] = (1<<16) / 100;
		patchtab[i].param[APP_ENV2VOL] = 1<<16;
	}
	_is_open = 1;
}


void audio_patch_close(void)
{
	if(!_is_open)
		return;

	memset(patchtab, 0, sizeof(patchtab));
	_is_open = 0;
}
