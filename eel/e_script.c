/*(LGPL)
---------------------------------------------------------------------------
	e_script.c - EEL source code management
---------------------------------------------------------------------------
 * Copyright (C) 2002, David Olofson
 * Copyright (C) 2002, Florian Schulze <fs@crowproductions.de>
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
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "_e_script.h"
#include "e_util.h"

#define	DBG(x)


/*----------------------------------------------------------
	Script source management
----------------------------------------------------------*/

script_t eel_scripttab[MAX_SCRIPTS];

int eel_script_alloc()
{
        int h = 0;
        while(h < MAX_SCRIPTS)
        {
                if(!eel_scripttab[h].data)
                        return h;
                ++h;
        }
        return -1;
}


int eel_load(const char *filename)
{
	FILE *f;
	int h = eel_script_alloc();
	if(h < 0)
		return h;

	/* Construct full name with path */
	eel_scripttab[h].name = malloc(strlen(filename) +
			strlen(eel_path()) + 2);
	if(!eel_scripttab[h].name)
		return -1;
	strcpy(eel_scripttab[h].name, eel_path());
#ifdef WIN32
	strcat(eel_scripttab[h].name, "\\");
#elif defined MACOS
	strcat(eel_scripttab[h].name, ":");
#else
	strcat(eel_scripttab[h].name, "/");
#endif
	strcat(eel_scripttab[h].name, filename);

	f = fopen(eel_scripttab[h].name, "rb");
	if(!f)
		return -1;

	if(fseek(f, 0, SEEK_END) == 0)
	{
		eel_scripttab[h].len = ftell(f);
		if(fseek(f, 0, SEEK_SET) == 0)
		{
			eel_scripttab[h].data = (unsigned char *)malloc(
					eel_scripttab[h].len + 1);
			if(eel_scripttab[h].data)
			{
				if(fread(eel_scripttab[h].data,
						eel_scripttab[h].len,
						1, f) == 1)
				{
					fclose(f);
					DBG(log_printf(DLOG, "Loaded script \"%s\"."
							" (Handle %d)\n",
							eel_scripttab[h].name,
							h);)
					return h;
				}
				DBG(printf("eel_load(): Read error!\n");)
				eel_free(h);
			}
			DBG(printf("eel_load(): Memory allocation error!\n");)
		}
		DBG(printf("eel_load(): Rewind error!\n");)
	}
	DBG(printf("eel_load(): Seek-to-end error!\n");)
	fclose(f);
	return -2;
}


int eel_load_from_mem(const char *script, unsigned len)
{
	char name_buffer[128];
	int h = eel_script_alloc();
	if(h < 0)
		return h;

	/* Construct name */
	snprintf(name_buffer, 127, "mem_script (%p, %u)", script, len);
	name_buffer[127] = '\0';
	eel_scripttab[h].name = (char *)malloc(strlen(name_buffer) + 1);
	if(!eel_scripttab[h].name)
		return -1;

	strcpy(eel_scripttab[h].name, name_buffer);

	eel_scripttab[h].len = len;
	eel_scripttab[h].data =
			(unsigned char *)malloc(eel_scripttab[h].len + 1);
	if(!eel_scripttab[h].data)	
	{
		DBG(printf("eel_load_from_mem(): Memory allocation error!\n");)
		return -2;
	}
	memcpy(eel_scripttab[h].data, script, len);
	DBG(printf("Loaded script \"%s\" (Handle %d)\n",
				eel_scripttab[h].name, h);)
	return h;
}


void eel_free(int handle)
{
	if(handle < 0)
		return;
	if(handle > MAX_SCRIPTS)
		return;
	if(!eel_scripttab[handle].data)
		return;

	DBG(printf("Freeing script \"%s\". (Handle %d)\n",
			eel_scripttab[handle].name, handle);)
	free(eel_scripttab[handle].data);
	eel_scripttab[handle].data = NULL;
	free(eel_scripttab[handle].name);
	eel_scripttab[handle].name = NULL;
	eel_scripttab[handle].len = 0;
}
