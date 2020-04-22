/*(LGPL)
---------------------------------------------------------------------------
	e_path.c - EEL file
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

#include <stdlib.h>
#include <string.h>

#include "eel.h"


/*----------------------------------------------------------
	File System Interface (Hah! :-)
----------------------------------------------------------*/

static char *_eel_path = NULL;


void eel_set_path(const char *path)
{
	if(_eel_path)
		free(_eel_path);
	if(path)
		_eel_path = strdup(path);
	else
		_eel_path = NULL;
}


const char *eel_path(void)
{
	if(!_eel_path)
	{
#if defined(WIN32)
		eel_set_path("");
#elif defined(MACOS)
		eel_set_path("");
#else
		eel_set_path("./");
#endif
	}
	return _eel_path;
}

