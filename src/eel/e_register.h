/*(LGPL)
---------------------------------------------------------------------------
	e_register.h - EEL extension registry
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

#ifndef _EEL_REGISTER_H_
#define _EEL_REGISTER_H_

#include "eel.h"

/*----------------------------------------------------------
	Language Extension API
----------------------------------------------------------*/

/*
 * Register an EEL operator. ("The real thing")
 *
 * (This is the "direct to the metal" interface; you may want
 * to use eel_register_cmd_operator(), eel_register_func_operator()
 * or eel_register_math_operator() instead most of the time.)
 *
 * The callback should check the types and number of
 * arguments (easily done with eel_get_args()), and return
 * 1 upon success, -1 in case of an error, or 0 if the script
 * should be terminated.
 *
 * Operator precendence in expressions is controlled by means
 * of operator priorities specified when operators are
 * registered. EEL built-in operators also have priorities,
 * and these are of course put in relation to the ones
 * specified for extension operators;
 *	'='		(assignment)		0 (always lowest)
 *	'+' '-'		(add, sub)		10
 *	'*' '/' '%'	(mul, div, mod)		20
 *	'^'		(power)			30
 *	(command and function operators)	100
 *
 * 'retargs' is the number of return arguments, and 'preargs'
 * is the number of arguments that must be placed *before* the
 * operator token. (Only normal, "post operator token" argument
 * lists can be variable length!)
 *
 * The return value is the command's unique token, or a
 * negative value upon failure.
 */
int eel_register_operator(const char *name, eel_operator_cb_t handler,
		int priority, int retargs, int preargs);

/* Register a new enum class. The unique token is returned. */
int eel_register_enum_class(void);

/*
 * Add enum constant to specified enum class.
 *
 * A negative value is returned if the operation fails.
 */
int eel_register_enum(int eclass, const char *name, int value);

/*
 * Register a directive.
 *
 * A directive is similar to a command, but can optionally be
 * invoked with a '#' before it's name. It does argument
 * parsing itself, which provides more detailed control.
 *
 * A negative value is returned if the operation fails.
 */
int eel_register_directive(const char *name, int (*parser)(void));

/*
 * Register a special.
 *
 * A special does argument parsing itself, like a directive,
 * but the #<name> syntax is not valid.
 *
 * A negative value is returned if the operation fails.
 */
int eel_register_special(const char *name, int (*parser)(void));

#endif /*_EEL_REGISTER_H_*/
