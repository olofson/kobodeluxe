/*(LGPL)
---------------------------------------------------------------------------
	a_midisock.c - Generic Interface for Modular MIDI Management
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

#include <stdio.h>
#include "a_midisock.h"
#include "kobolog.h"


/*----------------------------------------------------------
	Dummy MIDI Socket implementation
----------------------------------------------------------*/
static void dummy_note_off(unsigned ch, unsigned pitch, unsigned vel) {}
static void dummy_note_on(unsigned ch, unsigned pitch, unsigned vel) {}
static void dummy_poly_pressure(unsigned ch, unsigned pitch, unsigned press) {}
static void dummy_control_change(unsigned ch, unsigned ctrl, unsigned amt) {}
static void dummy_program_change(unsigned ch, unsigned prog) {}
static void dummy_channel_pressure(unsigned ch, unsigned press) {}
static void dummy_pitch_bend(unsigned ch, int amt) {}

midisock_t dummy_midisock = {
	dummy_note_off,
	dummy_note_on,
	dummy_poly_pressure,
	dummy_control_change,
	dummy_program_change,
	dummy_channel_pressure,
	dummy_pitch_bend
};


/*----------------------------------------------------------
	Monitor MIDI Socket implementation
----------------------------------------------------------*/
static const char notenames[12][4] = {
	"C-", "C#", "D-", "D#", "E-",
	"F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

static const char *controlnames[128] = {
	/*0..7*/
	"Bank Select MSB",
	"Modulation",
	"Breath Controller",
	NULL,
	"Foot Controller",
	"Portamento Time",
	"Data Entry MSB",
	"Volume",
	/*8..15*/
	"Balance",
	NULL,
	"Pan",
	"Expression",
	"FX Control 1",
	"FX Control 2",
	NULL,
	NULL,
	/*16..23*/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/*24..31*/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/*32..39*/
	"Bank Select LSB",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"Data Entry LSB",
	"BC: Select Insert Slot",
	/*40..47*/
	"BC: IFX Type",
	"BC: IFX Param 1 (Time 1)",
	"BC: IFX Param 2 (Time 2)",
	"BC: IFX Param 3 (Depth 1)",
	"BC: IFX Param 4 (Depth 2)",
	"BC: IFX Param 5 (Rate)",
	"BC: IFX Param 6 (Mode)",
	"BC: Send to Master",
	/*48..55*/
	"BC: Send to Bus 1",
	"BC: Send to Bus 2",
	"BC: Send to Bus 3",
	"BC: Send to Bus 4",
	"BC: Send to Bus 5",
	"BC: Send to Bus 6",
	"BC: Send to Bus 7",
	"BC: Send to Bus 8",
	/*56..63*/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/*64..71*/
	"Hold 1",
	"Portamento",
	"Sostenuto",
	"Soft Pedal",
	"Legato",
	"Hold 2",
	"SC 1",
	"SC 2: Harmonic Content",
	/*72..79*/
	"SC 3: Release Time",
	"SC 4: Attack Time",
	"SC 5: Brightness",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	/*80..87*/
	NULL,
	NULL,
	NULL,
	NULL,
	"Portamento Control",
	NULL,
	NULL,
	NULL,
	/*88..95*/
	"BC: Primary Bus Select",
	"BC: Send Bus Select",
	NULL,
	"FX 1 Depth: Reverb",
	"FX 2 Depth",
	"FX 3 Depth: Chorus",
	"FX 4 Depth: Variation",
	"FX 5 Depth",
	/*96..103*/
	"RPN Increment",
	"RPN Decrement",
	"NRPN LSB",
	"NRPN MSB",
	"RPN LSB",
	"RPN MSB",
	NULL,
	NULL,
	/*104..111*/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/*112..119*/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	/*120..127*/
	"All Sound Off",
	"Reset All Controllers",
	NULL,
	"All Notes Off",
	"Omni Off",
	"Omni On",
	"Mono",
	"Poly"
};

#if 0
#define	KNOWN_RPNS	5
static const char RPNnames[KNOWN_RPNS][24] = {
	"Pitch Bend Sensitivity",
	"Fine Tuning",
	"Coarse Tuning",
	"Tuning Program Select",
	"Tuning Bank Select"
};
#endif

static void monitor_note_off(unsigned ch, unsigned pitch, unsigned vel)
{
	log_printf(DLOG, "NoteOff(%u, %s%u(%u), %u)\n", ch,
			notenames[pitch % 12], pitch / 12,
			pitch, vel);
}

static void monitor_note_on(unsigned ch, unsigned pitch, unsigned vel)
{
	log_printf(DLOG, "NoteOn(%u, %s%u(%u), %u)\n", ch,
			notenames[pitch % 12], pitch / 12,
			pitch, vel);
}

static void monitor_poly_pressure(unsigned ch, unsigned pitch, unsigned press)
{
	log_printf(DLOG, "PolyPress(%u, %s%u(%u), %u)\n", ch,
			notenames[pitch % 12], pitch / 12,
			pitch, press);
}

static void monitor_control_change(unsigned ch, unsigned ctrl, unsigned amt)
{
	const char *cn = controlnames[ctrl];
	if(!cn)
		cn = "<unknown>";
	log_printf(DLOG, "ControlChg(%u, %s(%u), %u)\n", ch,
			controlnames[ctrl], ctrl,
			amt);
}

static void monitor_program_change(unsigned ch, unsigned prog)
{
	log_printf(DLOG, "ProgramChg(%u, %u)\n", ch, prog);
}

static void monitor_channel_pressure(unsigned ch, unsigned press)
{
	log_printf(DLOG, "ChannelPress(%u, %u)\n", ch, press);
}

static void monitor_pitch_bend(unsigned ch, int amt)
{
	log_printf(DLOG, "PitchBend(%u, %d)\n", ch, amt);
}

midisock_t monitor_midisock = {
	monitor_note_off,
	monitor_note_on,
	monitor_poly_pressure,
	monitor_control_change,
	monitor_program_change,
	monitor_channel_pressure,
	monitor_pitch_bend
};
