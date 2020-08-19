/*(GPL)
---------------------------------------------------------------------------
	audio_p.c - Private global audio engine stuff
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2002, David Olofson
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "a_globals.h"

/* For debug oscilloscope */
#ifdef DEBUG
	int *oscbufl = NULL;
	int *oscbufr = NULL;
	int oscframes = 0;
	int oscpos = 0;
#endif

int _audio_running = 0;


/* Global engine settings: defaults  */
struct settings_t a_settings = {
	44100,		/* samplerate */
	256,		/* output_buffersize */
	32,		/* buffersize */
	AQ_HIGH		/* quality */
};


/*
 * Default control values
 */
accbank_t a_channel_def_ctl = {
	0,		/* ACC_GROUP */
	1000,		/* ACC_PRIORITY */
	0,		/* ACC_PATCH */

	0,		/* ACC_PRIM_BUS */
	-1,		/* ACC_SEND_BUS	[Off] */

	0,		/* ACC_PAN	[0.0] */
	60<<16,		/* ACC_PITCH	[1.0 * sample_rate] */

	65536,		/* ACC_VOLUME	[1.0] */
	0,		/* ACC_SEND	[0.0] */

	0,		/* ACC_MOD1	[0.0] */
	0,		/* ACC_MOD2	[0.0] */
	0,		/* ACC_MOD3	[0.0] */

	0,		/* ACC_X	[0.0] */
	0,		/* ACC_Y	[0.0] */
	0		/* ACC_Z	[0.0] */
};

accbank_t a_group_def_ctl = {
	0,		/* ACC_GROUP */
	1000,		/* ACC_PRIORITY */
	0,		/* ACC_PATCH */

	0,		/* ACC_PRIM_BUS */
	-1,		/* ACC_SEND_BUS	[Off] */

	0,		/* ACC_PAN	[0.0] */
	60<<16,		/* ACC_PITCH	[1.0 * sample_rate] */

	65536,		/* ACC_VOLUME	[1.0] */
	65536,		/* ACC_SEND	[1.0] */

	0,		/* ACC_MOD1	[0.0] */
	0,		/* ACC_MOD2	[0.0] */
	0,		/* ACC_MOD3	[0.0] */

	0,		/* ACC_X	[0.0] */
	0,		/* ACC_Y	[0.0] */
	0		/* ACC_Z	[0.0] */
};


abcbank_t a_bus_def_ctl = {
	0,	/* ABC_FX_TYPE[None - allpass] */
	0,	/* ABC_FX_PARAM_1 */
	0,	/* ABC_FX_PARAM_2 */
	0,	/* ABC_FX_PARAM_3 */
	0,	/* ABC_FX_PARAM_4 */
	0,	/* ABC_FX_PARAM_5 */
	0,	/* ABC_FX_PARAM_6 */

	0,	/* ABC_SEND_MASTER */

	0,	/* ABC_SEND_BUS_0 */
	0,	/* ABC_SEND_BUS_1 */
	0,	/* ABC_SEND_BUS_2 */
	0,	/* ABC_SEND_BUS_3 */
	0,	/* ABC_SEND_BUS_4 */
	0,	/* ABC_SEND_BUS_5 */
	0,	/* ABC_SEND_BUS_6 */
	0	/* ABC_SEND_BUS_7 */
};
