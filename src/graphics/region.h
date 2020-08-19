/*(LGPL)
----------------------------------------------------------------------
	region.h - Graphics Engine
----------------------------------------------------------------------
 * Copyright (C) 2007 David Olofson
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

#ifndef	KOBO_REGION_H
#define	KOBO_REGION_H

#include "glSDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Region
 *	Describes an area as a number of pixel spaced rows,
 *	each containing zero or more spans on the form <start, end>.
 */
typedef struct RGN_region
{
	int	rows;
	Uint16	**spans;
} RGN_region;

/* Construct region by scanning an SDL surface */
RGN_region *RGN_ScanMask(SDL_Surface *src, Uint32 key);

/* Free a region */
void RGN_FreeRegion(RGN_region *rgn);

/* Set target for region rendering */
void RGN_Target(SDL_Surface *tgt);

/* Select and position region for subsequent operations */
void RGN_SetRegion(RGN_region *rgn, int xpos, int ypos);

/* Blit to region! */
int RGN_Blit(SDL_Surface *src, SDL_Rect *sr, int x, int y);

#ifdef __cplusplus
};
#endif

#endif	/* KOBO_REGION_H */
