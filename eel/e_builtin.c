/*(LGPL)
---------------------------------------------------------------------------
	e_builtin.c - EEL built-in commands and directives
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

#define	DBG(x)

#include "kobolog.h"
#include "math.h"
#include "e_builtin.h"
#include "e_util.h"
#include "e_lexer.h"


/*----------------------------------------------------------
	Command Callbacks
----------------------------------------------------------*/

int eel_cmd_end(int argc, struct eel_data_t *argv)
{
	DBG(log_printf(DLOG, "end;\n");)
	return 0;
}


int eel_cmd_set(int argc, struct eel_data_t *argv)
{
	eel_symbol_t *to;
	eel_data_t *val;

	if(eel_get_args("nV(irsce)", &to, &val) != 2)
		return -1;

	DBG(log_printf(DLOG, "set %s, ", eel_s_stringrep(to));)
	DBG(log_printf(DLOG, "%s;\n", eel_d_stringrep(val));)

	eel_s_set(to, val);

	return 1;
}


int eel_cmd_print(int argc, struct eel_data_t *argv)
{
	int i;
	for(i = 0; i < argc; ++i)
	{
		if(EDT_SYMREF == argv[i].type)
			switch(argv[i].value.sym->type)
			{
			  case EST_CONSTANT:
			  case EST_VARIABLE:
				/* Print the *value* of these... */
				log_printf(ULOG, "%s", eel_d_stringrep(
						&argv[i].value.sym->data));
				continue;
			  default:
				break;
			}
		log_printf(ULOG, "%s", eel_d_stringrep(argv + i));
	}

	return 1;
}


/*----------------------------------------------------------
	Operator Callbacks
----------------------------------------------------------*/

static inline eel_datatypes_t promote(struct eel_data_t *left,
		struct eel_data_t *right)
{
	if(left->type == right->type)
	{
		if(left->type == EDT_INTEGER || left->type == EDT_REAL)
			return left->type;
		else
		{
			eel_error("Operands of unsupported type!");
			return -1;
		}
	}

	if(left->type == EDT_INTEGER)
	{
		left->value.r = (double)left->value.i;
		left->type = EDT_REAL;
	}
	else if(left->type != EDT_REAL)
	{
		eel_error("Left hand operand of unsupported type!");
		return -1;
	}

	if(right->type == EDT_INTEGER)
	{
		right->value.r = (double)right->value.i;
		right->type = EDT_REAL;
	}
	else if(right->type != EDT_REAL)
	{
		eel_error("Right hand operand of unsupported type!");
		return -1;
	}

	return EDT_REAL;
}


int eel_op_add(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "add ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r += right->value.r;
		break;
	  case EDT_INTEGER:
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i += right->value.i;
		break;
	  default:
		return -1;
	}
	return 0;
}


int eel_op_sub(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "sub ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r -= right->value.r;
		break;
	  case EDT_INTEGER:
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i -= right->value.i;
		break;
	  default:
		return -1;
	}
	return 0;
}


int eel_op_mul(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "mul ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r *= right->value.r;
		break;
	  case EDT_INTEGER:
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i *= right->value.i;
		break;
	  default:
		return -1;
	}
	return 0;
}


int eel_op_div(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "div ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		if(!right->value.r)
		{
			eel_error("Division by zero!");
			return -1;
		}
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r /= right->value.r;
		break;
	  case EDT_INTEGER:
		if(!right->value.i)
		{
			eel_error("Division by zero!");
			return -1;
		}
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i /= right->value.i;
		break;
	  default:
		return -1;
	}
	return 0;
}


int eel_op_mod(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "mod ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		if(!right->value.r)
		{
			eel_error("Zero modulus!");
			return -1;
		}
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r -= right->value.r *
				floor(left->value.r / right->value.r);
		break;
	  case EDT_INTEGER:
		if(!right->value.i)
		{
			eel_error("Zero modulus!");
			return -1;
		}
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i %= right->value.i;
		break;
	  default:
		return -1;
	}
	return 0;
}


int eel_op_pow(int argc, struct eel_data_t *argv)
{
	eel_data_t *left = argv, *right = argv + 1;
	DBG(log_printf(DLOG, "pow ");)
	if(argc != 2)
	{
		eel_error("Operator needs exactly two arguments!");
		return -1;
	}
	switch(promote(left, right))
	{
	  case EDT_REAL:
		DBG(log_printf(DLOG, "%f, %f\n", left->value.r, right->value.r);)
		left->value.r = pow(left->value.r, right->value.r);
		break;
	  case EDT_INTEGER:
		DBG(log_printf(DLOG, "%d, %d\n", left->value.i, right->value.i);)
		left->value.i = pow(left->value.i, right->value.i);
		break;
	  default:
		return -1;
	}
	return 0;
}


/*----------------------------------------------------------
	Directive Callbacks
----------------------------------------------------------*/

int eel_dir_include(void)
{
	int h, res;
	const char *fn;

	if(eel_parse_args(" ", '\n') < 0)
		return -1;

	if(eel_get_args("s", &fn) < 0)
		return -1;

	h = eel_load(fn);
	if(h < 0)
	{
		eel_error("Couldn't include file \"%s\"!", fn);
		return -1;
	}
	res = eel_run(h);
	eel_free(h);

	if(res < 0)
		return res;
	else
		return 1;
}


int eel_dir_define(void)
{
	eel_symbol_t *to;
	eel_data_t *val;
	int res;

	if(eel_parse_args(" ", '\n') < 0)
		return -1;

	res = eel_get_args("n[V(irsce)]", &to, &val);

	if(res < 0)
		return -1;

	switch(res)
	{
	  case 1:
		DBG(log_printf(DLOG, "#define\t%s\n", eel_s_stringrep(to));)
		to->data.type = EDT_ILLEGAL;
		to->data.value.i = 0;
		to->type = EST_CONSTANT;
		break;
	  case 2:
		DBG(log_printf(DLOG, "#define\t%s\t%s\n",
				eel_s_stringrep(to), eel_d_stringrep(val));)
		eel_s_set(to, val);
		to->type = EST_CONSTANT;
		break;
	  default:
		eel_error("Expected either one or two arguments!");
		return -1;
	}
	return res;
}


/*----------------------------------------------------------
	Special Callbacks
----------------------------------------------------------*/

static int _proc_func(void)
{
	const char *funcname;
	eel_symbol_t *s;
	int tk, res;

	/* Get function name */
	tk = eel_lex(0);
	if(tk != TK_NEWSYM)
	{
		eel_error("Function/procedure name must be unique!");
		return -1;
	}
	funcname = eel_current.lval->value.s;

	/* Create function symbol */
	s = eel_s_new(funcname, EST_FUNCTION);
	if(!s)
	{
		eel_error("INTERNAL ERROR: Failed to create "
				"function/procedure symbol!");
		return -1;
	}

	/* Check argument list */
	if(eel_lex(0) != '(')
	{
		eel_error("Expected '('!");
		return -1;
	}

	/* Store the start of the *argument list*... :-) */
	s->data.value.a = bio_tell(eel_current.bio);
	s->token = eel_current.script;
	eel_push_scope();
	res = eel_parse_args(",", ')');
	eel_pop_scope();
	if(res < 0)
		return -1;

	DBG(log_printf(DLOG, "Function/procedure '%s' defined; starting at %d.\n",
			s->name, s->data.value.a);)

	/* Find start of body ("the code") */
	if(eel_lex(0) != '{')
	{
		eel_error("Expected '{'!");
		return -1;
	}

	return 1;
}

int eel_spec_function(void)
{
	int c;

	if(_proc_func() < 0)
		return -1;

	/* QUICK HACK: Skip function body */
	while((c = bio_getchar(eel_current.bio)) != '}')
		if(EOF == c)
		{
			eel_error("EOF inside function body!");
			return -1;
		}

	return 1;
}

int eel_spec_procedure(void)
{
	int c;

	if(_proc_func() < 0)
		return -1;

	/* QUICK HACK: Skip procedure body */
	while((c = bio_getchar(eel_current.bio)) != '}')
		if(EOF == c)
		{
			eel_error("EOF inside procedure body!");
			return -1;
		}

	return 1;
}
