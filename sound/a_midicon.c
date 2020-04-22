/*(LGPL)
---------------------------------------------------------------------------
	a_midicon.c - Engine MIDI Control Implementation
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

#include "kobolog.h"
#include "a_midicon.h"
#include "a_control.h"
#include "a_math.h"
#include "a_sequencer.h"


static int first_channel = 0;

#define	MIDI_MAP_CH(x)	((x)+first_channel)
#define	CH_MAP_MIDI(x)	((x)-first_channel)

static float fs = 44100;

/* Exponential table which maps [0, 127] ==> [0, 65535] */
static unsigned short explut[128];

typedef struct midi_channel_t
{
	/* Volume & velocity */
	int volume;		/* Linear [0, 65535] */
	int pan;		/* Linear [-65536, 65535] */
	int velocity[128];	/* For each key, linear [0, 65535] */

	/* Send control */
	int send;		/* Linear [0, 65535+] */

	/* Pitch */
	int bend_depth;
	int bend;
	int f_pitch;
	/* Key Management */
	int last;
	signed char next[128];	/* Index-linked list :-) */
	signed char prev[128];	/* Index-linked list :-) */

	/* Voice Management */	
	int poly;
	int tag[128];		/* To keep track of individual notes */

	/* Modulation */
	unsigned mod;

	/* Bus Control */
	unsigned ifx_slot;
} midi_channel_t;

static midi_channel_t m[16];

static void __init(void)
{
	int i, j;
	for(i = 0; i < 16; ++i)
	{
		m[i].volume = 100*512;
		for(j = 0; j < 128; ++j)
			m[i].velocity[j] = 0;
		m[i].bend_depth = 12;
		m[i].bend = 0;
		m[i].f_pitch = 0;
		m[i].last = -1;
		memset(m[i].next, -1, sizeof(m[i].next));
		memset(m[i].prev, -1, sizeof(m[i].prev));
		m[i].poly = 1;
		memset(m[i].tag, -1, sizeof(m[i].tag));
		m[i].mod = 0;
	}
	for(i = 0; i < 128; ++i)
	{
		float linear = (float)(i+1) / 128;
		explut[i] = (unsigned short)((0.25*linear + 0.5*linear*linear +
				0.25*linear*linear*linear) * 65535.0);
	}
#if 0
	for(i = 0; i < 128; ++i)
	{
		int j;
		for(j=0; j < (explut[i]>>9); ++j)
			log_printf(D3LOG, "#");
		log_printf(D3LOG, "\n");
	}
	for(i = 0; i < 128; ++i)
		log_printf(D3LOG, "%d ==> %f\n", i, (float)explut[i]/65536.0);
#endif
}


static inline void __poly(unsigned ch, unsigned on)
{
	m[ch].poly = (int)on;
/*
FIXME:	Maybe reimplement this somehow...?
FIXME:	acc_set(MIDI_MAP_CH(ch), ACC_DETACH, on ? ADM_POLY : ADM_MONO);
*/
}


static inline void __press(unsigned ch, unsigned key)
{
	m[ch].next[key] = -1;
	m[ch].prev[key] = m[ch].last;
	m[ch].next[m[ch].last] = (char)key;
	m[ch].last = (char)key;
}


/*
 * Returns the new last key (-1 if none),
 * or -2 if there's no change.
 */
static inline int __release(unsigned ch, unsigned key)
{
	if(m[ch].prev[key] != -1)
		m[ch].next[m[ch].prev[key]] = m[ch].next[key];
	if(m[ch].next[key] != -1)
	{
		m[ch].prev[m[ch].next[key]] = m[ch].prev[key];
		m[ch].next[key] = m[ch].prev[key] = -1;
		return -2;
	}
	else
	{
		m[ch].last = m[ch].prev[key];
		m[ch].next[key] = m[ch].prev[key] = -1;
		return m[ch].last;
	}
}


/* Returns 1 if the specified key on the specified channel is down. */
static inline int __is_down(unsigned ch, int key)
{
	return (m[ch].tag[key] != -1);
}

	
static void midicon_note_off(unsigned ch, unsigned pitch, unsigned vel)
{
	__release(ch, pitch);
	(void)ce_stop(channeltab + MIDI_MAP_CH(ch), 0,
			(int)pitch, (int)explut[vel]);
}


static void midicon_note_on(unsigned ch, unsigned pitch, unsigned vel)
{
	if(!vel)
	{
		midicon_note_off(ch, pitch, 64);
		return;
	}
	m[ch].velocity[pitch] = (int)explut[vel];
	(void)ce_start(channeltab + MIDI_MAP_CH(ch), 0, (int)pitch,
			(int)pitch << 16, m[ch].velocity[pitch]);
	__press(ch, pitch);
}


static void midicon_pitch_bend(unsigned ch, int bend)
{
	m[ch].bend = bend << 3;
	m[ch].bend *= m[ch].bend_depth;
	(void)ce_control(channeltab + MIDI_MAP_CH(ch), 0,
			-1, ACC_PITCH, m[ch].bend + (60<<16));
}



static void midicon_control_change(unsigned ch, unsigned ctrl, unsigned amt)
{
	audio_channel_t *c = channeltab + MIDI_MAP_CH(ch);
	switch (ctrl)
	{
	  case 1:
		m[ch].mod = amt;
		break;
	  case 7:
		m[ch].volume = (int)explut[amt];
		(void)ce_control(c, 0, -1, ACC_VOLUME, m[ch].volume);
		break;
	  case 10:
		m[ch].pan = (amt << 10) - 65536;
		(void)ce_control(c, 0, -1, ACC_PAN, m[ch].pan);
		break;

	  /* Bus Control */
	  case 39: /* Select IFX Slot */
		m[ch].ifx_slot = (unsigned)amt;
		break;

	  case 40: /* IFX Type */
		if(ch >= AUDIO_MAX_BUSSES)
			break;
		if(m[ch].ifx_slot > AUDIO_MAX_INSERTS-1)
			break;
		bus_ctl_set(ch, m[ch].ifx_slot, ABC_FX_TYPE, (int)amt);
		break;
	  case 41: /* IFX P1 (Time 1) */
	  case 42: /* IFX P2 (Time 2) */
	  case 43: /* IFX P3 (Depth 1) */
	  case 44: /* IFX P4 (Depth 2)*/
	  case 45: /* IFX P5 (Rate) */
	  case 46: /* IFX P6 (Mode/Special) */
		if(ch >= AUDIO_MAX_BUSSES)
			break;
		if(m[ch].ifx_slot > AUDIO_MAX_INSERTS-1)
			break;
		if(ctrl != 46)
			amt = (unsigned)(explut[amt]) << 1;
		bus_ctl_set(ch, m[ch].ifx_slot, ctrl - 41 + ABC_FX_PARAM_1,
				(int)amt);
		break;

	  case 47: /* IFX Wet -> master */
		if(ch >= AUDIO_MAX_BUSSES)
			break;
		if(m[ch].ifx_slot > AUDIO_MAX_INSERTS-1)
			break;
		bus_ctl_set(ch, m[ch].ifx_slot, ABC_SEND_MASTER,
				explut[amt] << 1);
		break;

	  case 48: /* Sends to other busses */
	  case 49:
	  case 50:
	  case 51:
	  case 52:
	  case 53:
	  case 54:
	  case 55:
		if(ch >= AUDIO_MAX_BUSSES)
			break;
		if(m[ch].ifx_slot > AUDIO_MAX_INSERTS-1)
			break;
		bus_ctl_set(ch, m[ch].ifx_slot, ABC_SEND_BUS_0 + ctrl-48,
				explut[amt] << 1);
		break;

	  case 88:	/* Primary output bus */
		(void)ce_control(c, 0, -1, ACC_PRIM_BUS, amt-1);
		break;
	  case 89:	/* Send bus */
		(void)ce_control(c, 0, -1, ACC_SEND_BUS, amt-1);
		break;
	  case 91:	/* Send Level ("Reverb") */
		m[ch].send = (int)explut[amt] << 1;
		(void)ce_control(c, 0, -1, ACC_SEND, m[ch].send);
		break;

	  case 120:	/* All Sound Off */
	  case 123:	/* All Notes Off */
		if(m[ch].poly)
		{
			int note = m[ch].last;
			while(note >= 0)
			{
				int n = note;
				note = m[ch].prev[note];
				midicon_note_off(ch, (unsigned)n, 64);
			}
		}
		else
			midicon_note_off(ch, (unsigned)m[ch].last, 64);
		break;
	  case 121:	/* Reset All Controllers */
		__poly(ch, 1);
		m[ch].mod = 0;
		m[ch].volume = 100*512;
		m[ch].send = 0;
		m[ch].pan = 0;
		(void)ce_control(c, 0, -1, ACC_VOLUME, m[ch].volume);
		(void)ce_control(c, 0, -1, ACC_SEND, m[ch].send);
		(void)ce_control(c, 0, -1, ACC_PAN, m[ch].pan);
		midicon_pitch_bend(ch, 0);
		break;
	  case 126:	/* Mono */
		__poly(ch, 0);
		break;
	  case 127:	/* Poly */
		__poly(ch, 1);
		break;
	  default:
		break;
	}
}


static void midicon_program_change(unsigned ch, unsigned prog)
{
	(void)ce_control(channeltab + MIDI_MAP_CH(ch), 0, -2, ACC_PATCH, (int)prog);
}


midisock_t midicon_midisock = {
	midicon_note_off,
	midicon_note_on,
	NULL,		/* poly_pressure */
	midicon_control_change,
	midicon_program_change,
	NULL,		/* channel_pressure */
	midicon_pitch_bend
};


int midicon_open(float framerate, int first_ch)
{
	unsigned i;
	aev_client("midicon_open()");
	fs = framerate;
	first_channel = first_ch;
	__init();
	for(i = 0; i < 16; ++i)
		__poly(i, 1);
	return 0;
}


void midicon_close(void)
{
	aev_client("midicon_close()");
}


void midicon_process(unsigned frames)
{
	aev_client("midicon_process()");
	/*
	 * This is where MIDI clock and stuff goes.
	 */
}
