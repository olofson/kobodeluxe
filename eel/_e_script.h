/*(LGPL)
---------------------------------------------------------------------------
	_e_script.h - EEL source code management (Private)
---------------------------------------------------------------------------
 * Copyright (C) 2002, David Olofson
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

#ifndef _EEL__SCRIPT_H_
#define _EEL__SCRIPT_H_

#include "eel.h"
#include "e_script.h"

#define	MAX_SCRIPTS	256

typedef struct script_t
{
	char		*name;
	unsigned char	*data;
	unsigned	len;
} script_t;

extern script_t eel_scripttab[MAX_SCRIPTS];


#endif /*_EEL__SCRIPT_H_*/
