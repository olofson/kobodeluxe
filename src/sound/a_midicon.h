/*(LGPL)
---------------------------------------------------------------------------
	a_midicon.h - Engine MIDI Control Implementation
---------------------------------------------------------------------------
 * Copyright (C) 2001. 2002, David Olofson
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

#ifndef _A_MIDICON_H_
#define _A_MIDICON_H_

#include "a_midisock.h"

int midicon_open(float framerate, int first_ch);
void midicon_close(void);

void midicon_process(unsigned frames);

/* Send MIDI messages here. */
extern midisock_t midicon_midisock;

#endif /*_A_MIDICON_H_*/
