/*(LGPL)
---------------------------------------------------------------------------
	e_util.h - EEL engine utilities
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

#ifndef _EEL_UTIL_H_
#define _EEL_UTIL_H_

#include <stdio.h>
#include "eel.h"


/*----------------------------------------------------------
	"stdio" style API for raw memory buffers
----------------------------------------------------------*/

typedef struct bio_file_t
{
	unsigned char	*data;
	int		len;
	int		pos;
} bio_file_t;

bio_file_t *bio_open(void *data, int len);
bio_file_t *bio_clone(bio_file_t *bio);
void bio_close(bio_file_t *bio);

/* Get the next character from the buffer */
static inline int bio_getchar(bio_file_t *bio)
{
	if(bio->pos < bio->len)
		return bio->data[bio->pos++];
	else
		return EOF;
}

/* Push last character back */
static inline void bio_ungetc(bio_file_t *bio)
{
	if(bio->pos > 0)
		--bio->pos;
}

/*
 * Get the current source buffer position
 */
static inline int bio_tell(bio_file_t *bio)
{
	return bio->pos;
}

/*
 * Set the current source buffer position
 */
int bio_seek(bio_file_t *bio, int offset, int whence);

/*
 * Read 'count' bytes from the source code,
 * starting at the current position, into
 * 'buf'. Returns the number of bytes
 * actually read.
 */
int bio_read(bio_file_t *bio, char *buf, int count);

double bio_strtod(bio_file_t *bio);


/*----------------------------------------------------------
	Context management ("VM stack")
------------------------------------------------------------
 * This supports skipping between scripts, and also keeps
 * track of the arg count for error messages.
 */

typedef struct eel_context_t
{
	struct eel_context_t	*previous;
	bio_file_t		*bio;
	int			script;
	int			arg;
	int			unlexed;
	int			token;
	eel_data_t		*lval;
} eel_context_t;

extern eel_context_t eel_current;

void eel_push_context(void);
void eel_pop_context(void);

#endif /*_EEL_UTIL_H_*/
