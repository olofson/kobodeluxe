/*(LGPL)
---------------------------------------------------------------------------
	eel.c - The "Extensible Embeddable Language"
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

/*
TODO:
	...or at least think about: Extend the dynamic typing to
	include functions. Ex:

		set some_var, some_function;

	will connect 'some_function' to 'some_var', so that the
	function is called to calculate the value whenever the
	variable is read. Obviously, such a variable becomes
	read-only! This could be worked around with the following
	syntax:

		set some_var, read_function, write_function;

	A handy syntax extension:

		set some_var, some_function(...);

	or

		set some_other_var,
		{
			...
			some code
			...
		};

	which would be interpreted as a nameless, single statement
	function being connected to 'some_var', and a nameless
	function with a normal body being connected to
	'some_other_var'.

	The whole point with this is that it allows the way a
	variable gets written to change at run time, and that it's
	even applicable to code that isn't aware of this feature.

	BTW, this is pretty similar to Delphi 'properties' -
	although you cannot turn variables into properties or vice
	versa, at run time, as you could do with this feature.

----------------------------------------------------------------------

	Scope/Namespace management sucks. Namespaces probably need
	to be arranged in a tree rather than a stack.

	This becomes obvious when considering what happens when you
	call a function... The function executes in a scope that's
	the calling scope + a local namespace!

	Indeed, this could be considered an interesting "feature",
	but I'm not sure it's as useful as it is confusing...

	As a quick hack solution, one could reserve the first few
	namespaces (ie highest levels) for language extensions, to
	make it easy to install and remove extensions without them
	interfering with each other.

	One could possible change the "scope level" into a "scope
	handle", and make a scope a "list of namespaces" rather
	than the current "lowest namespace index". That would allow
	multiple totally independent scopes under the same scripting
	engine.

 */

#ifndef _EEL_H_
#define _EEL_H_

#include "e_script.h"
#include "e_symtab.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	EEL_MAX_ARGS		128

#define _EEL_API_

#include "e_register.h"


/*----------------------------------------------------------
	Toolkit for directive callbacks
----------------------------------------------------------*/

/*
 * The internal argument list. Avoid using this directly!
 */
extern int eel_arg_count;
extern eel_data_t *eel_args;

/*
 * Add the last lval returned from the lexer to the internal
 * argument list.
 */
int eel_grab_arg(void);

/*
 * Clear the internal argument list starting at argument
 * 'first'.
 */
void eel_clear_args(int first);

/*
 * Parse arguments separated by one of the characters in
 * 'separators', until the character 'terminator' is
 * matched, or an error occurs.
 *
 * The arguments that are "grabbed" are added to the
 * internal argument list using eel_grab_arg(), and can
 * be retrieved, decoded and typechecked with eel_get_args()
 * if desired.
 *
 * Returns a negative value in case of an error.
 */
int eel_parse_args(const char *separators, char terminator);


/*----------------------------------------------------------
	Toolkit for command callbacks
----------------------------------------------------------*/

/*
 * Grab arguments from the internal argument list, verifying
 * that their types and count are in accordance with 'fmt'.
 * Results will be stored in the variables passed by
 * reference after the 'fmt' argument.
 *
 * Normally, the argument count not matching the number of
 * arguments specified in the format string will be reported
 * as an error. However, this can be changed by means of
 * argument tipples and optional arguments, denoted by the
 * '<', '>' and '[', ']' characters respectively.
 *
 * The return value is the number of arguments correctly
 * retrieved, or a negative value if something goes wrong.
 *
 * Format string syntax:
 *
 *	Char	Meaning					Type
 *--------------------------------------------------------------------
 *	*	Skip argument				N/A
 *
 *	,	Next argument (optional)		N/A
 *
 *	?	Grab any argument as is			eel_data_t *
 *
 *	< >	Argument tipple. The argument spec in	N/A
 *		between the < and > will be repeated
 *		until all arguments have been grabbed,
 *		or an error occurs. The target args
 *		should be ARRAYS and *must* be large
 *		enough to fit all arguments retrieved,
 *		or bad things will happen. Argument
 *		tipples cannot be nested. There must
 *		be no argument specifiers after the
 *		'>', as an argument tipple repeats
 *		"forever". Normally, the arguments
 *		must match an *integer* number of
 *		tipple repetitions - but this can be
 *		changed using the Optional Argument
 *		format codes.
 *
 *	[ ]	Optional arguments. Arguments beyond	N/A
 *		the '[' character after the 'argv'
 *		array has been exhausted will not
 *		generate an error, as this situation
 *		normally would.
 *
 *		Optional arguments inside argument
 *		tipples apply only to the *last*
 *		tipple repetition.
 *
 *		Note that no argument specifiers must
 *		follow the ']' character - or; optional
 *		arguments must be the last ones in the
 *		list.
 *
 *	E	Get any enum symbol. Any enum class	eel_symbol_t *
 *		is accepted. (Do check, if you care!)
 *
 *	V	Get value of any allowed type. This	eel_data_t *
 *		code can optionally be followed by a
 *		list of allowed types, enclosed within
 *		parentheses. If no list is specified,
 *		all *value* data types are accepted,
 *		including EDT_CADDR. Available types:
 *			i	integer (EDT_INTEGER)
 *			r	real (EDT_REAL)
 *			s	string (EDT_STRING)
 *			c	code address (EDT_CADDR)
 *			e	enum constant (EDT_SYMREF)
 *		All but 'e' are set by default, if no
 *		explicit list is provided.
 *
 *		Note that symrefs are followed, to get
 *		the values of the symbols they refer to,
 *		except that when 'e' (enum constant) is
 *		listed, symrefs to enum constants will
 *		stay symrefs.
 *
 *	e	Get enum value. The argument should be	int
 *		INITIALIZED with the enum class. Only
 *		enum constants from the right class
 *		are accepted.
 *
 *	i	Get integer value			int
 *
 *	n	Get or create variable. In the case of	eel_symbol_t *
 *		a new variable being created, the
 *		argument has to be a string holding the
 *		name of the new variable. The type of
 *		a new variable is undefined.
 *
 *	r	Get real value				double
 *
 *	s	Get string value			char *
 *		NOTE: Currently it's NOT possible to
 *		get non-string values casted to strings.
 *		(We need a fast, reference counting
 *		string package...)
 *
 *	v	Get variable (must be defined)		eel_symbol_t *
 *
 *	f	Get function "address". The returned	eel_symbol_t *
 *		symbol will be of type EST_FUNCTION.
 *
 * Any other character in the format string in matched with that
 * respective character in the input.
 */
int eel_get_args(const char *fmt, ...);


/*
 * Like eel_get_args(), but reads from the specified eel_data_t
 * array instead of the internal argument list. 
 */
int eel_get_args_from(int argc, struct eel_data_t *argv, const char *fmt, ...);

/*
 * Print a nice error "prompt" with file name, line #
 * and arg #, followed by your formatted error message,
 * followed by a newline ('\n') where appropriate. (Don't
 * add this yourself, unless you need multiple lines.)
 *
 *	Note that syntax checking and most type
 *	and error checking is done automatically
 *	by EEL (mostly in eel_get_args()). This
 *	function is only for errors that EEL
 *	can't detect.
 *
 * Returns total number of characters printed.
 */
int eel_error(const char *format, ...);

/*
 * Returns a string describing the specified symbol
 * in a way that would complete the sentences:
 *	"This is ..."
 *	"Expected ..."
 */
const char *eel_symbol_is(eel_symbol_t *s);

/*
 * As eel_symbol_is(), but for data containers.
 */
const char *eel_data_is(eel_data_t *d);



/*----------------------------------------------------------
	Runtime API
----------------------------------------------------------*/

/*
 * Set the EEL path for scripts and includes.
 *
 * This must be a valid path, terminated by a
 * '/', '\', ':' or whatever "directory separator"
 * the current platform is using, and will be
 * prepended to file names passed to the #include
 * directive.
 */
void eel_set_path(const char *path);

/* Returns the current EEL path. */
const char *eel_path(void);

/* Open/Close EEL. (Keep track of nested calls!) */
int eel_open(void);
void eel_close();

/*
 * Execute code inside script 'handle', starting at 'pos'.
 *
 * Execution will take place as a "sub procedure", with it's
 * own context¨, but *not* automatically it's own *scope*.
 * This is because most real applications will require
 * scripts to be passed some "arguments", which is actually
 * done by creating a few variables before the script is
 * called. Obviously, the code that sets up the "argument
 * variables" and calls eel_run() will also have to do any
 * scope pushing and popping *around* these calls - which is
 * why it can't be done inside eel_run().
 *
 * The following events will end execution:
 *	- A statement gives a "stop execution" return code.
 *	- The top level scope is terminated; ie an unmatched
 *	  curly brace ('}') is found.
 *	- EOF is reached.
 *	- An error occurs.
 *
 * Returns the result, which will be negative in the case
 * of an error.
 */
int eel_call(int handle, int pos);

/*
 * Run a script from the start.
 *
 * Currently 100% equivalent to eel_call('handle', 0);
 */
int eel_run(int handle);

/*
 * Create or set variable 'name' to the specified value.
 * Existing symbols *must* be of type ST_VARIABLE, or
 * these calls will fail.
 *
 * Returns the changed/created symbol, or NULL if the
 * operation failed.
 */
eel_symbol_t *eel_set_integer(const char *name, int value);
eel_symbol_t *eel_set_real(const char *name, double value);
eel_symbol_t *eel_set_string(const char *name, const char *s);
eel_symbol_t *eel_set_data(const char *name, const eel_data_t *dat);

/*
 * Make an exact copy of symbol 'orig', except for the name,
 * which will be 'name'. Symbol 'name' must not exist before
 * this call.
 *
 * Returns the created symbol, or NULL if the operation failed. 
 */
eel_symbol_t *eel_clone_symbol(const char *name, const eel_symbol_t *orig);

#undef _EEL_API_

#ifdef __cplusplus
};
#endif

#endif /* _EEL_H_ */
