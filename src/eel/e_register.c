/*(LGPL)
---------------------------------------------------------------------------
	e_register.c - EEL extension registry
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

#include "e_register.h"
#include "e_symtab.h"
#include "e_lexer.h"

eel_symbol_t *_register(const char *name, eel_symtypes_t stype,
		eel_datatypes_t type)
{
	eel_symbol_t *s = eel_s_new(name, stype);
	if(!s)
		return (eel_symbol_t *)0;
	s->data.type = type;
	return s;
}


int eel_register_operator(const char *name, eel_operator_cb_t handler,
		int priority, int retargs, int preargs)
{
	eel_symbol_t *s = _register(name, EST_OPERATOR, EDT_OPERATOR);
	if(!s)
		return -1;

	s->token = eel_get_unique_token();
	s->data.value.op.priority = priority;
	s->data.value.op.retargs = retargs;
	s->data.value.op.preargs = preargs;
	s->data.value.op.cb = handler;
	return s->token;
}


int eel_register_enum_class(void)
{
	return eel_get_unique_token();
}


int eel_register_enum(int eclass, const char *name, int value)
{
	eel_symbol_t *s = _register(name, EST_ENUM, EDT_INTEGER);
	if(!s)
		return -1;
	s->token = eclass;
	s->data.value.i = value;
	return 0;
}


int eel_register_directive(const char *name, eel_parser_cb_t parser)
{
	eel_symbol_t *s = _register(name, EST_DIRECTIVE, EDT_DIRECTIVE);
	if(!s)
		return -1;
	s->token = eel_get_unique_token();
	s->data.value.parse = parser;
	return 0;
}


int eel_register_special(const char *name, eel_parser_cb_t parser)
{
	eel_symbol_t *s = _register(name, EST_SPECIAL, EDT_SPECIAL);
	if(!s)
		return -1;
	s->token = eel_get_unique_token();
	s->data.value.parse = parser;
	return 0;
}
