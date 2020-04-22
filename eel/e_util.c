/*(LGPL)
---------------------------------------------------------------------------
	e_util.c - EEL engine utilities
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2003, David Olofson
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
#include <stdarg.h>

#include "kobolog.h"
#include "e_util.h"
#include "eel.h"

#include "_e_script.h"


/*----------------------------------------------------------
	"stdio" style API for raw memory buffers
----------------------------------------------------------*/

bio_file_t *bio_open(void *data, int len)
{
	bio_file_t *bio = malloc(sizeof(bio_file_t));
	if(!bio)
		return NULL;
	bio->data = (unsigned char *)data;
	bio->len = len;
	bio->pos = 0;
	return bio;
}


bio_file_t *bio_clone(bio_file_t *bio)
{
	bio_file_t *c = malloc(sizeof(bio_file_t));
	if(!c)
		return NULL;
	*c = *bio;
	return c;
}


void bio_close(bio_file_t *bio)
{
	free(bio);
}


int bio_seek(bio_file_t *bio, int offset, int whence)
{
	switch(whence)
	{
	  case SEEK_SET:
		bio->pos = offset;
		break;
	  case SEEK_CUR:
		bio->pos += offset;
		break;
	  case SEEK_END:
		bio->pos = bio->len - offset;
		break;
	}
	if(bio->pos < 0)
		bio->pos = 0;
	else if(bio->pos > bio->len)
		bio->pos = bio->len;
	return 0;
}


int bio_read(bio_file_t *bio, char *buf, int count)
{
	if(bio->pos + count > bio->len)
		count = bio->len - bio->pos;
	memcpy(buf, bio->data + bio->pos, count);
	bio->pos += count;
	return count;
}


double bio_strtod(bio_file_t *bio)
{
	char *p = (char *)bio->data + bio->pos;
	double val;
	val = strtod(p, &p);
	bio->pos = p - (char *)bio->data;
	return val;
}


/*----------------------------------------------------------
	Context management ("VM stack")
----------------------------------------------------------*/

eel_context_t eel_current = {
	NULL,
	NULL,
	-1,
	0,
	0,
	EOF,
	NULL
};

static eel_context_t *context_stack = NULL;


void eel_push_context(void)
{
	eel_context_t *c = malloc(sizeof(eel_context_t));
	if(!c)
	{
		eel_error("INTERNAL ERROR: Failed to push context!");
		return;
	}
	memcpy(c, &eel_current, sizeof(eel_context_t));
	c->previous = context_stack;
	context_stack = c;
}


void eel_pop_context(void)
{
	eel_context_t *c = context_stack;
	if(!c)
	{
		eel_error("INTERNAL ERROR: No context to pop!");
		return;
	}
	memcpy(&eel_current, c, sizeof(eel_context_t));
	context_stack = c->previous;
	if(c->lval)
	{
		eel_d_freestring(c->lval);
		free(c->lval);
	}
	free(c);
}



/*----------------------------------------------------------
	Error handling tools
----------------------------------------------------------*/

static int last_script = -1;

int eel_error(const char *format, ...)
{
	int count = 0;
	va_list	args;
	int i, pos, line = 1;

	/* Figure out which line we're at... */
	pos = bio_tell(eel_current.bio);
	bio_seek(eel_current.bio, 0, SEEK_SET);
	for(i = 0; i < pos; ++i)
		if('\n' == bio_getchar(eel_current.bio))
			++line;

	if((eel_current.script >= 0) && (eel_current.script != last_script))
	{
		count = log_printf(ELOG, "(EEL) in file \"%s\":\n",
				eel_scripttab[eel_current.script].name);
		last_script = eel_current.script;
	}
	if(eel_current.arg)
		count += log_printf(ELOG, "(EEL) line %d, arg %d: ",
				line, eel_current.arg);
	else
		count += log_printf(ELOG, "(EEL) line %d: ", line);
	va_start(args, format);
	count += vfprintf(stderr, format, args);
	va_end(args);
	count += log_printf(ELOG, "\n");
	return count;
}

const char *eel_symbol_is(eel_symbol_t *s)
{
	switch(s->type)
	{
	  case EST_UNDEFINED:	return "an undedined symbol";
	  case EST_CONSTANT:	return "a constant";
	  case EST_VARIABLE:	return "a variable";
	  case EST_LABEL:	return "a label";
	  case EST_FUNCTION:	return "a function";
	  case EST_OPERATOR:	return "an operator";
	  case EST_DIRECTIVE:	return "a directive";
	  case EST_SPECIAL:	return "a special";
	  case EST_ENUM:	return "an enum constant";
	  case EST_NAMESPACE:	return "a namespace";
	}
	/*
	 * Should be here, and not under default:, to get
	 * a warning if we forget to add new types. :-)
	 */
	return "a broken symbol! (illegal type)";
}

const char *eel_data_is(eel_data_t *d)
{
	switch(d->type)
	{
	  case EDT_ILLEGAL:	return "an illegal value";
	  case EDT_REAL:	return "a real value";
	  case EDT_INTEGER:	return "an integer value";
	  case EDT_STRING:	return "a string";
	  case EDT_CADDR:	return "a code address";
	  case EDT_SYMREF:	return "a symbol reference";
	  case EDT_SYMTAB:	return "a symbol table pointer";
	  case EDT_SYMNAME:	return "a (new) symbol name";
	  case EDT_OPERATOR:	return "an operator callback pointer";
	  case EDT_DIRECTIVE:	return "a directive callback pointer";
	  case EDT_SPECIAL:	return "a special callback pointer";
	}
	return "a broken value! (illegal type)";
}


