/*(LGPL)
---------------------------------------------------------------------------
	a_midisock.h - Generic Interface for Modular MIDI Management
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

/*
 * TODO: System messages.
 */

#ifndef _A_MIDISOCK_H_
#define _A_MIDISOCK_H_

/* Note: NULL pointers are used for callbacks not desired. */
typedef struct midisock_t
{
	void (*note_off)(unsigned ch, unsigned pitch, unsigned vel);
	void (*note_on)(unsigned ch, unsigned pitch, unsigned vel);
	void (*poly_pressure)(unsigned ch, unsigned pitch, unsigned press);
	void (*control_change)(unsigned ch, unsigned ctrl, unsigned amt);
	void (*program_change)(unsigned ch, unsigned prog);
	void (*channel_pressure)(unsigned ch, unsigned press);
	void (*pitch_bend)(unsigned ch, int amt);
} midisock_t;

/* These two can be assumed to have all callbacks defined - no NULLs. */
extern midisock_t dummy_midisock;	/* NOP */
extern midisock_t monitor_midisock;	/* printf()s MIDI messages as text */

#endif /*_A_MIDISOCK_H_*/
