/*
---------------------------------------------------------------------------
	a_pitch.c - Simple fixed point linear pitch table.
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

#include <stdlib.h>
#include "a_globals.h"
#include "a_pitch.h"

/*
 * Start pitch, based on the assumption that 1.0 (original
 * sample playback speed) gives you a middle C (MIDI #60).
 */
#define	FIVE_OCTAVES		(1 << 5)

/* 1 + twelfth root of two */
#define	SEMITONE_MULTIPLIER	1.0594630943592953

int *__pitchtab = NULL;

int ptab_init(int middle_c)
{
	int p;
	double pitch;
	free(__pitchtab);
	__pitchtab = malloc(129 * sizeof(int));
	if(!__pitchtab)
		return -1;

	pitch = (double)middle_c / FIVE_OCTAVES;
	for(p = 0; p < 129; ++p)
	{
		__pitchtab[p] = (int)pitch;
		pitch *= SEMITONE_MULTIPLIER;
	}
	return 0;
}

void ptab_close(void)
{
	free(__pitchtab);
	__pitchtab = NULL;
}

