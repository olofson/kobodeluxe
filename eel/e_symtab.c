/*(LGPL)
---------------------------------------------------------------------------
	symtab.c - Simple symbol table for EEL
---------------------------------------------------------------------------
 * Copyright (C) 2000-2003, 2007 David Olofson
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kobolog.h"
#include "e_symtab.h"

#define	DBG(x)

#define	STRINGREP_SIZE	128

#define	MAX_SCOPES	32


/*------------------------------------------------
	Data container
------------------------------------------------*/

int eel_d_copy(eel_data_t *data, const eel_data_t *from)
{
	eel_d_freestring(data);
	switch (from->type)
	{
	  case EDT_ILLEGAL:
	  case EDT_REAL:
	  case EDT_INTEGER:
	  case EDT_CADDR:
	  case EDT_SYMREF:
	  case EDT_SYMTAB:
	  case EDT_OPERATOR:
	  case EDT_DIRECTIVE:
	  case EDT_SPECIAL:
		*data = *from;
		return 1;
	  case EDT_STRING:
	  case EDT_SYMNAME:
		data->type = from->type;
		return eel_d_setstring(data, from->value.s);
	}
	return -1;
}

int eel_d_setstring(eel_data_t *data, const char *s)
{
	if((EDT_STRING == data->type) || (EDT_SYMNAME == data->type))
		free(data->value.s);
	else
		data->type = EDT_STRING;
	data->value.s = (char *)malloc(strlen(s) + 1);
	if(data->value.s < 0)
		return -1;
	strcpy(data->value.s, s);
	return 0;
}

void eel_d_grabstring(eel_data_t *data, char *s)
{
	if((EDT_STRING == data->type) || (EDT_SYMNAME == data->type))
		free(data->value.s);
	else
		data->type = EDT_STRING;
	data->value.s = s;
}

void eel_d_freestring(eel_data_t *data)
{
	if((data->type != EDT_STRING) && (data->type != EDT_SYMNAME))
		return;
	free(data->value.s);
	data->value.s = 0;
}

/* NOTE: Output string only valid until next call. */
const char *eel_d_stringrep(eel_data_t * data)
{
	static char buf[STRINGREP_SIZE];
	switch (data->type)
	{
	  case EDT_ILLEGAL:
		return "<illegal data>";
	  case EDT_REAL:
		snprintf(buf, STRINGREP_SIZE, "%.10g", data->value.r);
		return buf;
	  case EDT_INTEGER:
		snprintf(buf, STRINGREP_SIZE, "%d", data->value.i);
		return buf;
	  case EDT_STRING:
	  case EDT_SYMNAME:
		if(data->value.s)
		{
			strncpy(buf, data->value.s, STRINGREP_SIZE);
			return buf;
		}
		else
			return "<undef string>";
	  case EDT_SYMTAB:
		return "<symbol table reference>";
	  case EDT_CADDR:
		snprintf(buf, STRINGREP_SIZE, "%x", data->value.a);
		return buf;
	  case EDT_SYMREF:
		return eel_s_stringrep(data->value.sym);
	  case EDT_OPERATOR:
		if(data->value.s)
		{
			snprintf(buf, STRINGREP_SIZE, "(%s)",
					data->value.s);
			return buf;
		}
		else
			return "<unnamed operator>";
	  case EDT_DIRECTIVE:
		if(data->value.s)
		{
			snprintf(buf, STRINGREP_SIZE, "(#%s)",
					data->value.s);
			return buf;
		}
		else
			return "<unnamed directive>";
	  case EDT_SPECIAL:
		if(data->value.s)
		{
			snprintf(buf, STRINGREP_SIZE, "(%s)",
					data->value.s);
			return buf;
		}
		else
			return "<unnamed special>";
	  default:
		return "<internal error: undef data type>";
	}
}


/*------------------------------------------------
	Symbol table
------------------------------------------------*/

/* The symbol table: a chain of `struct symrec'.  */
static eel_symbol_t *eel_s_table[MAX_SCOPES];

static int _current_scope = 0;
static int _deepest_scope = 0;


int eel_s_open(void)
{
	memset(eel_s_table, 0, sizeof(eel_s_table));
	_current_scope = 0;
	_deepest_scope = 0;
	return 0;
}


void eel_s_close(void)
{
	eel_s_freeall();
}


eel_symbol_t *eel_s_new(const char *name, eel_symtypes_t type)
{
	eel_symbol_t *sym;
	sym = (eel_symbol_t *) calloc(1, sizeof(eel_symbol_t));
	if(!sym)
	{
		log_printf(ELOG, "eel_s_new: Out of memory!\n");
		return 0;
	}
	sym->name = strdup(name);
	sym->type = type;
	switch (type)
	{
	  case EST_UNDEFINED:
		break;
	  case EST_CONSTANT:
	  case EST_VARIABLE:
		sym->data.type = EDT_REAL;	/* Default type */
		break;
	  case EST_LABEL:
	  case EST_FUNCTION:
		sym->data.type = EDT_CADDR;
		break;
	  case EST_OPERATOR:
		sym->data.type = EDT_OPERATOR;
		break;
	  case EST_DIRECTIVE:
		sym->data.type = EDT_DIRECTIVE;
		break;
	  case EST_SPECIAL:
		sym->data.type = EDT_SPECIAL;
		break;
	  case EST_ENUM:
		sym->data.type = EDT_INTEGER;	/* For the value */
		break;
	  case EST_NAMESPACE:
		sym->data.type = EDT_SYMTAB;
	  default:
		log_printf(ELOG, "internal error: created"
				" symbol of undef type\n");
	}
	sym->next = (struct eel_symbol_t *)eel_s_table[_current_scope];
	eel_s_table[_current_scope] = sym;
	return sym;
}

static void _free_scope(int scope)
{
	eel_symbol_t *sym, *nextptr;
	for(sym = eel_s_table[scope]; sym; sym = nextptr)
	{
		nextptr = (eel_symbol_t *) sym->next;
		eel_d_freestring(&sym->data);
		free(sym->name);
		free(sym);
	}
	eel_s_table[scope] = (eel_symbol_t *)0;
}

void eel_s_freeall(void)
{
	int scope;
	for(scope = 0; scope < MAX_SCOPES; ++scope)
		_free_scope(scope);
}

eel_symbol_t *eel_s_find(eel_symbol_t * sym, const char *eel_s_name)
{
	int scope = _current_scope;
	while(scope >= 0)
	{
		if(!sym)
			sym = eel_s_table[scope];
		for(; sym; sym = (eel_symbol_t *) sym->next)
			if(strcmp(sym->name, eel_s_name) == 0)
				return sym;
		sym = NULL;
		--scope;
	}
	return NULL;
}

eel_symbol_t *eel_s_find_n_tk(eel_symbol_t * sym, const char *eel_s_name, int token)
{
	int scope = _current_scope;
	while(scope >= 0)
	{
		if(!sym)
			sym = eel_s_table[scope];
		for(; sym; sym = (eel_symbol_t *) sym->next)
			if((sym->token == token) &&
					(strcmp(sym->name, eel_s_name) == 0))
				return sym;
		sym = NULL;
		--scope;
	}
	return NULL;
}

eel_symbol_t *eel_s_find_tk(eel_symbol_t * sym, int token)
{
	int scope = _current_scope;
	while(scope >= 0)
	{
		if(!sym)
			sym = eel_s_table[scope];
		for(; sym; sym = (eel_symbol_t *) sym->next)
			if(sym->token == token)
				return sym;
		sym = NULL;
		--scope;
	}
	return NULL;
}

eel_symbol_t *eel_s_find_n_t(eel_symbol_t * sym, const char *eel_s_name,
		eel_symtypes_t type)
{
	int scope = _current_scope;
	while(scope >= 0)
	{
		if(!sym)
			sym = eel_s_table[scope];
		for(; sym; sym = (eel_symbol_t *) sym->next)
			if((sym->type == type) &&
					(strcmp(sym->name, eel_s_name) == 0))
				return sym;
		--scope;
	}
	return NULL;
}

eel_symbol_t *eel_s_find_t(eel_symbol_t * sym, eel_symtypes_t type)
{
	int scope = _current_scope;
	while(scope >= 0)
	{
		if(!sym)
			sym = eel_s_table[scope];
		for(; sym; sym = (eel_symbol_t *) sym->next)
			if(sym->type == type)
				return sym;
		--scope;
	}
	return NULL;
}

/* NOTE: Output string only valid until next call. */
const char *eel_s_stringrep(eel_symbol_t * sym)
{
	static char buf[STRINGREP_SIZE];
	switch (sym->type)
	{
	  case EST_UNDEFINED:
		snprintf(buf, STRINGREP_SIZE, "<undef symbol '%s'>",
				sym->name);
		return buf;
	  case EST_CONSTANT:
		strncpy(buf, eel_d_stringrep(&sym->data), STRINGREP_SIZE);
		return buf;
	  case EST_VARIABLE:
	  case EST_LABEL:
	  case EST_FUNCTION:
	  case EST_OPERATOR:
	  case EST_DIRECTIVE:
	  case EST_SPECIAL:
	  case EST_ENUM:
	  case EST_NAMESPACE:
		strncpy(buf, sym->name, STRINGREP_SIZE);
		return buf;
	  default:
		return "<internal error: undef symbol type>";
	}
}


/*------------------------------------------------
	Scope/Namespace Management
------------------------------------------------*/

int eel_push_scope(void)
{
	if(_deepest_scope >= MAX_SCOPES-1)
	{
		log_printf(ELOG, "EEL ERROR: Too deep scope nesting!\n");
		return -1;
	}
	++_deepest_scope;
	_current_scope = _deepest_scope;
	DBG(log_printf(DLOG, "eel_push_scope(): current scope = %d\n", _current_scope);)
	return 0;
}


int eel_pop_scope(void)
{
	if(!_deepest_scope)
	{
		log_printf(ELOG, "EEL ERROR: Tried to pop root scope!\n");
		return -1;
	}
	_free_scope(_deepest_scope);
	--_deepest_scope;
	_current_scope = _deepest_scope;
	DBG(log_printf(DLOG, "eel_pop_scope(): current scope = %d\n", _current_scope);)
	return 0;
}


int eel_scope(void)
{
	return _current_scope;
}


int eel_set_scope(int ns)
{
	if(ns < 0 || ns > _deepest_scope)
	{
		log_printf(ELOG, "EEL ERROR: Tried to select illegal scope!\n");
		return -1;
	}
	_current_scope = ns;
	return 0;
}


void eel_restore_scope(void)
{
	_current_scope = _deepest_scope;
}


/*------------------------------------------------
	Symbol table tools
------------------------------------------------*/

eel_symbol_t *eel_s_get(const char *name, eel_symtypes_t type)
{
	eel_symbol_t *sym = eel_s_find(NULL, name);
	if(sym)
	{
		if(sym->type == type)
			return sym;
		else
		{
			log_printf(ELOG, "eel_s_integer: '%s' is of"
					" the wrong type!\n", name);
			return NULL;
		}
	}
	else
		return eel_s_new(name, type);
}


int eel_s_set(eel_symbol_t *sym, const eel_data_t *dat)
{
	return eel_d_copy(&sym->data, dat);
}
