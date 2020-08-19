/*(LGPL)
---------------------------------------------------------------------------
	eel.c - The "Extensible Embeddable Language"
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2007 David Olofson
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
	* Eliminate the parser and use the expression
	  evaluator instead. "Everything is an expression."
	  The evaluator will need a few extra features, but
	  lots of messy code will be eliminated, and the
	  design will be much cleaner and more flexible.

	* Make all commands, functions and stuff operators.
	  Binary commands and operators should use the same
	  callback prototype. A way of describing return,
	  reference and input arguments will be needed. (Not
	  sure about multiple return arguments...)

	* Split the command and operator callbacks up
	  into one "parser" callback, and one or more
	  run-time callbacks. Leave argument grabbing/casting
	  and callback selection to the parser callbacks, for
	  simplicity and flexibility. (They can still use
	  tools like the EEL argument grabber.) Let them
	  generate byte code or similar (using some EEL API
	  calls) for one or more "operations", as required to
	  perform the desired actions.

	  Interpreting mode could still be implemented,
	  with some overhead, by compiling and executing one
	  statement at a time - although IMHO, one might as
	  well make a complete switch to compiling + VM.
	  That would eliminate issues with distinguishing
	  between compiled and interpreted functions and the
	  like.

	* To make the above work, we'll need a distinction
	  between compile time and run-time symbols. (In
	  fact, we don't really need actual symbols at run
	  time, other than for globals. Local variables
	  should be allocated from some VM stack.) For
	  example, some features of the current argument
	  grabber won't work for compiling, as they rely on
	  the actual contents of a symbol, to extract data.
	  Such features will have to be implemented though
	  run-time callbacks that execute the corresponding
	  operations.

	* Remove EDT_INTEGER? The lexer currently never
	  returns TK_INUM except for character literals,
	  and I'm starting to think it's a good idea to
	  keep it that way, for various reasons...

	* Functions should return values! Make sure
	  that errors are issued if a function returns
	  without an explicit return statement.

	* Add "procedure" keyword for "functions"
	  without return values. Issue an error if a
	  "return with value" statement is executed
	  within a procedure.

	* Implement conditional constructs.

	* Implement '&' ("address" of function) operator.

	* Add a procedure extension(name) to EEL, to allow
	  scripts to check for extension availability and
	  version. Should return version #, or 0 if the
	  extension is not available. There should probably
	  also be a uses(name), to avoid implicitly stuffing
	  all available extensions into the name space of
	  every script.

	* Eliminate all fixed size arrays and limits...

	* Implement the Compressed Source file format.

 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eel.h"
#include "e_lexer.h"
#include "e_util.h"
#include "e_builtin.h"

#include "_e_script.h"


/*----------------------------------------------------------
	Parser/Interpreter
----------------------------------------------------------*/

static int command(void)
{
	eel_symbol_t *sym = eel_current.lval->value.sym;

	if(!sym->data.value.op.cb)
	{
		eel_error("INTERNAL ERROR: Command callback missing!");
		return -1;
	}

	if(sym->data.value.op.retargs)
	{
		eel_error("INTERNAL ERROR: Return value ignored!");
		return -1;
	}

	if(eel_parse_args(",", ';') < 0)
		return -1;

	return sym->data.value.op.cb(eel_arg_count, eel_args);
}


static int assignment(int token)
{
	int res;

	/* Get target */
	switch(token)
	{
	  case TK_SYMREF:
		switch (eel_current.lval->value.sym->type)
		{
		  case EST_UNDEFINED:
		  case EST_VARIABLE:
			break;
		  default:
			eel_error("Cannot change the value of %s!",
					eel_symbol_is(eel_current.lval->value.sym));
			return -1;
		}
	  case TK_NEWSYM:
		if(eel_grab_arg() < 0)
		{
			eel_error("Argument overflow!");
			return -1;
		}
		break;
#if 0
	  case 0:
		eel_error("Unexpected end of file!");
		return -1;
	  default:
		eel_error("Unexpected token!");
		return -1;
#endif
	}

	/* Get operator */
	if(eel_lex(0) != '=')
	{
		eel_error("Expected '='!");
		return -1;
	}

	/* Get source */
	if(eel_parse_args(",", ';') < 0)
		return -1;

	/* End of declaration; execute! */
	res = eel_cmd_set(eel_arg_count, eel_args);

	return res;
}


static int directive(void)
{
	int res;
	eel_symbol_t *sym = eel_current.lval->value.sym;

	if(!sym->data.value.parse)
	{
		eel_error("INTERNAL ERROR: Directive callback missing!");
		return -1;
	}

	res = sym->data.value.parse();
	return res;
}


static int function(void)
{
	int arg, res;
	eel_symbol_t *func = eel_current.lval->value.sym;

	eel_push_scope();

	/* Grab arguments */
	res = eel_parse_args(",", ';');

	/* Enter function */
	eel_push_context();
	if(eel_current.script == func->token)
		eel_current.bio = bio_clone(eel_current.bio);
	else
		eel_current.bio = bio_open(eel_scripttab[func->token].data,
				eel_scripttab[func->token].len);
	if(!eel_current.bio)
	{
		eel_error("INTERNAL ERROR: Failed to set up script"
				" reading at start of function!");
		eel_pop_context();
		eel_pop_scope();
		return -1;
	}
	res = bio_seek(eel_current.bio, func->data.value.a, SEEK_SET);

	/*
	 * Find out where to stuff the arguments.
	 */
	for(arg = 0; (arg < eel_arg_count) && (res >= 0); )
		switch(eel_lex(0))
		{
		  case ',':
			/* Already verified, so we just skip them. */
			break;
		  case TK_NEWSYM:
		  {
			eel_symbol_t *sym;
			sym = eel_set_data(eel_current.lval->value.s,
					eel_args + arg);
			if(sym)
			{
				sym->type = EST_VARIABLE;
				++arg;
			}
			else
			{
				eel_error("INTERNAL ERROR: Failed to "
						"create argument variable!");
				res = -1;
			}
			break;
		  }
		  case TK_SYMREF:
			eel_error("Function argument clashes with "
					"previously defined symbol!");
			res = -1;
			break;
		  default:
			eel_error("Syntax error in argument list!");
			res = -1;
			break;
		}
	
	/* Find start of body ("the code") */
	if(res >= 0)
	{
		if(eel_lex(0) != ')')
		{
			eel_error("Expected ')'!");
			res = -1;
		}
		else if(eel_lex(0) != '{')
		{
			eel_error("Expected '{'!");
			res = -1;
		}
	}

	if(res >= 0)
	{
		/*
		 * Execute function
		 *
		 * Note that this is a simple, inefficient
		 * RECURSIVE implementation!
		 *
		 * Alternatively, we could set up a "trigger"
		 * to run "the rest of this function" when
		 * the function ends - and then just return.
		 */
		res = eel_call(eel_current.script, bio_tell(eel_current.bio));
	}

	/* Leave function */
	bio_close(eel_current.bio);
	eel_pop_context();
	eel_pop_scope();

	if(res >= 0)
		return 1;
	else
		return res;
}


int eel_call(int handle, int pos)
{
	int res = 1;
	int depth = 0;
	eel_push_context();
	eel_current.script = handle;
	eel_current.bio = bio_open(eel_scripttab[handle].data,
			eel_scripttab[handle].len);
	if(!eel_current.bio)
	{
		eel_error("INTERNAL ERROR: Couldn't read script!");
		eel_pop_context();
		return -1;
	}
	bio_seek(eel_current.bio, pos, SEEK_SET);
	
	eel_current.lval = NULL;
	while(res > 0)
	{
		/* Start of statement */
		eel_clear_args(0);
		eel_current.arg = 0;
		switch(eel_lex(0))
		{
		  case 0:
			/* End of script */
			res = 0;
			break;
		  case ';':
			/* Empty statement */
			res = 1;
			break;
		  case '{':
			/* Begin scope */
			eel_push_scope();
			++depth;
			res = 1;
			break;
		  case '}':
			/* End scope */
			if(depth)
			{
				eel_pop_scope();
				--depth;
			}
			else
				res = 0;
			break;
		  case TK_NEWSYM:
			res = assignment(TK_NEWSYM);
			break;
		  case TK_SYMREF:
			switch(eel_current.lval->value.sym->type)
			{
			  case EST_CONSTANT:	/* Just to get a more
			  			 * sensible error message...
						 */
			  case EST_UNDEFINED:
			  case EST_VARIABLE:
				res = assignment(TK_SYMREF);
				break;
/*			  case EST_COMMAND:
				res = command();
				break;*/
			  case EST_FUNCTION:
				res = function();
				break;
			  case EST_DIRECTIVE:
			  case EST_SPECIAL:
				res = directive();
				break;
			  case EST_ENUM:
			  case EST_LABEL:
				eel_error("Syntax error!");
				res = -1;
				break;
			  case EST_OPERATOR:
				res = command();
				break;
			  default:
				eel_error("INTERNAL ERROR: Lexer returned"
						" symbol of illegal type!");
				res = -1;
				break;
			}
			break;
		  default:
			eel_error("Unexpected character!");
			res = -1;
			break;
		}
	}
	eel_current.arg = 0;
	bio_close(eel_current.bio);
	eel_pop_context();
	return res;
}


int eel_run(int handle)
{
	return eel_call(handle, 0);
}


static int _eel_users = 0;

int eel_open(void)
{
	if(_eel_users++)
		return 0;

	memset(eel_scripttab, 0, sizeof(eel_scripttab));

	if(eel_s_open() < 0)
	{
		_eel_users = 0;
		return -1;
	}

	eel_register_operator("end", eel_cmd_end, 100, 0, 0);
	eel_register_operator("set", eel_cmd_set, 100, 0, 0);
	eel_register_operator("print", eel_cmd_print, 100, 0, 0);

	eel_register_operator("+", eel_op_add, 10, 0, 1);
	eel_register_operator("-", eel_op_sub, 10, 0, 1);

	eel_register_operator("*", eel_op_mul, 20, 0, 1);
	eel_register_operator("/", eel_op_div, 20, 0, 1);
	eel_register_operator("%", eel_op_mod, 20, 0, 1);

	eel_register_operator("^", eel_op_pow, 30, 0, 1);

	eel_register_directive("include", eel_dir_include);
	eel_register_directive("define", eel_dir_define);

	eel_register_special("function", eel_spec_function);
	eel_register_special("procedure", eel_spec_procedure);
/*	eel_register_special("global", eel_spec_global);*/

	return 0;
}

void eel_close()
{
	int h;
	if(!_eel_users)
		return;

	for(h = 0; h < MAX_SCRIPTS; ++h)
		eel_free(h);

	eel_s_freeall();
	eel_set_path(NULL);
	eel_s_close();
	eel_lexer_cleanup();

	--_eel_users;
}



/*----------------------------------------------------------
	EEL variable access
----------------------------------------------------------*/

eel_symbol_t *eel_set_integer(const char *name, int value)
{
	eel_symbol_t *sym = eel_s_get(name, EST_VARIABLE);
	if(!sym)
		return NULL;
	eel_d_freestring(&sym->data);
	sym->data.type = EDT_INTEGER;
	sym->data.value.i = value;
	return sym;
}


eel_symbol_t *eel_set_real(const char *name, double value)
{
	eel_symbol_t *sym = eel_s_get(name, EST_VARIABLE);
	if(!sym)
		return NULL;
	eel_d_freestring(&sym->data);
	sym->data.type = EDT_REAL;
	sym->data.value.r = value;
	return sym;
}


eel_symbol_t *eel_set_string(const char *name, const char *s)
{
	eel_symbol_t *sym = eel_s_get(name, EST_VARIABLE);
	if(!sym)
		return NULL;
	eel_d_setstring(&sym->data, s);
	return sym;
}


eel_symbol_t *eel_set_data(const char *name, const eel_data_t *dat)
{
	eel_symbol_t *sym = eel_s_get(name, EST_VARIABLE);
	if(!sym)
		return NULL;

	eel_d_freestring(&sym->data);
	switch (dat->type)
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
		sym->data = *dat;
		break;
	  case EDT_STRING:
	  case EDT_SYMNAME:
		sym->data.type = dat->type;
		if(eel_d_setstring(&sym->data, dat->value.s) < 0)
			return NULL;
		break;
	}
	return sym;
}


eel_symbol_t *eel_clone_symbol(const char *name, const eel_symbol_t *orig)
{
	eel_symbol_t *sym = eel_s_new(name, orig->type);
	if(!sym)
		return NULL;

	eel_d_freestring(&sym->data);
	switch (orig->data.type)
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
		sym->data = orig->data;
		break;
	  case EDT_STRING:
	  case EDT_SYMNAME:
		sym->data.type = orig->data.type;
		if(eel_d_setstring(&sym->data, orig->data.value.s) < 0)
			return NULL;
		break;
	}
	return sym;
}
