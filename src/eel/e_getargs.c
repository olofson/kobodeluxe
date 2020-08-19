/*(LGPL)
---------------------------------------------------------------------------
	e_getargs.c - scanf() style "argument fetcher" + expr. evaluator
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "kobolog.h"
#include "config.h"
#include "eel.h"
#include "e_lexer.h"
#include "e_util.h"
#include "e_builtin.h"

#define	DBG(x)


/*----------------------------------------------------------
	Argument list parser
----------------------------------------------------------*/

eel_data_t eel_arg_table[EEL_MAX_ARGS];
int eel_arg_token_table[EEL_MAX_ARGS];

int eel_arg_count = 0;
eel_data_t *eel_args = eel_arg_table;
int *eel_arg_tokens = eel_arg_token_table;


int eel_grab_arg(void)
{
	if(eel_arg_count >= EEL_MAX_ARGS)
		return -1;

	eel_arg_tokens[eel_arg_count] = eel_current.token;

	if(eel_current.lval)
	{
		if(EDT_SYMREF == eel_current.lval->type)
		{
			/* Kill "runaway indirection" right at it's root! */
			if(EDT_SYMREF == eel_current.lval->value.sym->data.type)
				eel_d_copy(eel_current.lval,
						&eel_current.lval->value.sym->data);
		}

		/*
		 * Important:
		 *	This is a *move* operation! This is
		 *	to avoid copying external string
		 *	buffers around, while still making
		 *	sure the new eel_data_t gains true
		 *	ownership of them.
		 */
		eel_args[eel_arg_count] = *eel_current.lval;
		free(eel_current.lval);
		eel_current.lval = NULL;
	}
	++eel_arg_count;
	return eel_arg_count;
}


void eel_clear_args(int first)
{
	int i;
	for(i = first; i < eel_arg_count; ++i)
		eel_d_freestring(eel_args + i);
	eel_arg_count = first;
}


void eel_remove_arg(int pos)
{
	int i;
	eel_d_freestring(eel_args + pos);
	for(i = pos + 1; i < eel_arg_count; ++i)
	{
		eel_args[i-1] = eel_args[i];
		eel_arg_tokens[i-1] = eel_arg_tokens[i];
	}
	--eel_arg_count;
}


/*
 * Remove all arguments in the range ]start, end[.
 * (That is, not including args 'start' or 'end'.)
 */
void eel_collapse(int start, int end)
{
	int i;
	for(i = start + 1; i < end; ++i)
		eel_remove_arg(i);
}


/*
 * Try to convert 'd' into a value. If it's not
 * possible, nothing will be done.
 */
static inline void resolve(eel_data_t *d)
{
	if(EDT_SYMREF == d->type)
		switch(d->value.sym->type)
		{
		  case EST_CONSTANT:
		  case EST_VARIABLE:
		  case EST_ENUM:
			/* These are OK. */
			*d = d->value.sym->data;
			break;
		  default:
			break;
		}
}


/*
 * Steenkin' special case for a single unary minus before a term.
 * (Could be recursive, call unary operator callbacks and stuff,
 * but there are too many screaming babies here right now... :-)
 */
static inline int neg(int pos)
{
	eel_data_t *d;
	if((EDT_SYMREF == eel_args[pos].type) &&
			(EST_OPERATOR == eel_args[pos].value.sym->type))
	{
		DBG(log_printf(DLOG, "neg\n");)
		if(eel_op_sub != eel_args[pos].value.sym->data.value.op.cb)
		{
			eel_error("No unary operator '%s'!",
					eel_args[pos].value.sym->name);
			return -1;
		}
		d = &eel_args[pos+1];
		if(!d)
		{
			eel_error("Expected operand for unary minus!");
			return -1;
		}
		resolve(d);
		switch(d->type)
		{
		  case EDT_REAL:	/* Real */
			d->value.r = -d->value.r;
			break;
		  case EDT_INTEGER:	/* Integer */
			d->value.i = -d->value.i;
			break;
		  case EDT_SYMREF:	/* Symbol reference */
			eel_error("Chained unary operators not yet supported!");
			return -1;
		  case EDT_SYMTAB:	/* Symbol table reference */
		  case EDT_STRING:	/* String */
		  case EDT_CADDR:	/* Code address */
		  case EDT_SYMNAME:	/* Name of a new symbol (String) */
		  case EDT_OPERATOR:	/* Operator callback */
		  case EDT_DIRECTIVE:	/* Directive parser callback */
		  case EDT_SPECIAL:	/* Special parser callback */
		  case EDT_ILLEGAL:
			eel_error("Illegal operation!");
			return -1;
		}
		eel_remove_arg(pos);
	}
	else
		resolve(&eel_args[pos]);
	return 0;
}


/*
 * Recursively evaluate expression, taking operator
 * precedence in account. The 'left' argument is the
 * index of the left operand, which is also the
 * target for the result, and must be followed by an
 * operator and a valid right term.
 *
FIXME:
 * Note that a single unary minus is allowed as a
 * part of each term, and will be evaluated here.
 * This feature should be removed as soon as the
 * new operator system is fully implemented. (Unary
 * minus can then be implemented as just another EEL
 * operator.)
/FIXME
 *
 * Returns the index of the last argument used, or a
 * negative value, if the operation fails.
 *
FIXME: Handle 0 returns from operators...?
 */
static int recursive_eval(int left)
{
	int right = left + 1;
	if(neg(left) < 0)
		return -1;

	while(right + 1 < eel_arg_count)
	{
		eel_symbol_t *op, *op2;
		op = eel_args[right].value.sym;
		if(neg(right + 1) < 0)
			return -1;

		if(right + 2 < eel_arg_count)
		{
			op2 = eel_args[right + 2].value.sym;
			if(op2->data.value.op.priority > op->data.value.op.priority)
			{
				int maxr = recursive_eval(right + 1);
				eel_remove_arg(right);	/* Remove operator */
				if(op->data.value.op.cb(2, eel_args + left) < 0)
					return -1;

				eel_remove_arg(right);	/* Remove right term */
				return maxr;
			}
		}
		eel_remove_arg(right);	/* Remove operator token */
		if(op->data.value.op.cb(2, eel_args + left) < 0)
			return -1;

		eel_remove_arg(right);	/* Remove right hand term */
	}
	return right;
}


/*
 * Evaluate expression, starting at position 'start'
 * in the internal argument list. The whole expression
 * will be replaced by a single term, stored in the
 * 'start' position, and all terms after it will be
 * deleted.
 *
 * Returns a token corresponding to the result type,
 * or a negative value in case of an error.
 */
static int do_eval(int start)
{
	if(eel_arg_count - start > 1)
	{
		int right;
#if 0
int i;
for(i = 0; i < start; ++i)
	log_printf(D3LOG, "  ");
log_printf(D3LOG, "do_eval(): ");
for(i = start; i < eel_arg_count; ++i)
{
	if(EDT_SYMREF == eel_args[i].type)
		log_printf(D3LOG, "<%s, %d>", eel_symbol_is(eel_args[i].value.sym),
				eel_arg_tokens[i]);
	else
		log_printf(D3LOG, "<%s, %d>", eel_data_is(&eel_args[i]),
				eel_arg_tokens[i]);
}
log_printf(D3LOG, "\n");
#endif
		right = recursive_eval(start);
		if(right < 0)
			return -1;

		/* Check for bogus "terminating operator" */
		if(right < eel_arg_count)
		{
			eel_error("Expression syntax error!");
			return -1;
		}
	}

	/* Prepare and return result */
/*	eel_clear_args(start + 1);*/
	switch (eel_args[start].type)
	{
	  case EDT_REAL:
		eel_arg_tokens[start] = TK_RNUM;
		break;
	  case EDT_INTEGER:
		eel_arg_tokens[start] = TK_INUM;
		break;
	  case EDT_STRING:
		eel_arg_tokens[start] = TK_STRN;
		break;
	  case EDT_SYMNAME:
		eel_arg_tokens[start] = TK_NEWSYM;
		break;
	  case EDT_SYMTAB:
		/*
		 * Note:
		 *	If we were to get a namespace reference, it would
		 *	actually be an EDT_SYMREF referring to the namespace
		 *	symbol, rather than a raw pointer to namespace
		 *	symbol table itself.
		 */
	  	eel_error("Weird... do_eval() saw a symbol table reference.");
		eel_arg_tokens[start] = -1;
		break;
	  case EDT_SYMREF:
	  case EDT_OPERATOR:
	  case EDT_DIRECTIVE:
	  case EDT_SPECIAL:
		eel_arg_tokens[start] = TK_SYMREF;
		break;
	  default:
	  	eel_error("Weird things going on in do_eval()!");
		eel_arg_tokens[start] = -1;
		break;
	}
	return eel_arg_tokens[start];
}


/*
 * Grab, and if required, evaluate the next "token".
 *
 * This function will return a token, and when applicable,
 * the corresponding eel_data_t already last in the argument
 * list.
 *
 * Note that returned arguments may be results of evaluated
 * expressions, rather than constants from the source.
 *
 * Also note that the function will stop at ')' tokens. This
 * functions as a way of supporting the alternative LISP
 * style function calls (required when functions are used
 * inside argument lists and expressions). It's also a
 * feature that's relied upon internally, to recursively
 * resolve expressions containing parentheses.
 */
static int eval(int report_eoln)
{
	int tk = EOF;
	int prev_tk;
	int expect_operator = 0;
	int tokens = 0;
	int start = eel_arg_count;
	int grab_and_return = 0;
	eel_symbol_t *sym;
	/*
	 * Get terms and operators, checking that we
	 * get term, operator(s), term,... etc. Stop
	 * and evaluate if the chain is broken, or
	 * any other token is seen.
	 */
	while(1)
	{
		prev_tk = tk;
		tk = eel_lex(report_eoln);
		switch(tk)
		{
		  case TK_SYMREF:
			sym = eel_current.lval->value.sym;
			if(expect_operator)
			{
				if(EST_OPERATOR != sym->type)
				{
					eel_unlex();
					return do_eval(start);
				}
				expect_operator = 0;
			}
			else
				if(EST_OPERATOR != sym->type)
					expect_operator = 1;
			break;
		  case TK_NEWSYM:
		  case TK_RNUM:
		  case TK_INUM:
		  case TK_STRN:
			if(expect_operator)
			{
				eel_unlex();
				return do_eval(start);
			}
			expect_operator = !expect_operator;
			break;
		  case '(':
			if(eval(report_eoln) < 0)
				return -1;
			tk = eel_lex(report_eoln);
			if(')' != tk)
			{
				eel_error("Unmatched '(' in expression!");
				return -1;
			}
			continue;
		  case ')':
			if(0 == tokens)
				/* Not part of the expression. */
				return ')';
			else
			{
				eel_unlex();
				return do_eval(start);
			}
			break;
		  default:
			if(0 == tokens)
			{
				return tk;
			}
			else
			{
				eel_unlex();
				return do_eval(start);
			}
		}
		if(eel_grab_arg() < 0)
		{
			eel_error("Argument overflow!");
			return -1;
		}
		if(grab_and_return)
			return tk;
		++tokens;
	}
}


int eel_parse_args(const char *separators, char terminator)
{
	int sep_required = (strchr(separators, ' ') == 0);
	int expect_separator = 0;
	int tk;
	eel_current.arg = 1;
	while( (tk = eval('\n' == terminator)) )
	{
		if(tk == terminator)
			return 0;
		switch(tk)
		{
		  case TK_SYMREF:
			/* No operators here, please! */
			if(EST_OPERATOR == eel_args[eel_arg_count-1].
					value.sym->type)
			{
				eel_error("Incorrectly used operator!");
				eel_current.arg = 0;
				return -1;
			}
		  case TK_NEWSYM:
		  case TK_RNUM:
		  case TK_INUM:
		  case TK_STRN:
			if(expect_separator)
			{
				eel_error("Argument separator expected!");
				eel_current.arg = 0;
				return -1;
			}
			expect_separator = sep_required;
			break;
		  default:
			if(tk < 256)
				if(strchr(separators, tk))
				{
					expect_separator = 0;
					++eel_current.arg;
					break;
				}
/*			eel_error("Unexpected token!");*/
			eel_current.arg = 0;
			return -1;
		}
	}
	eel_error("Unexpected end of file!");
	eel_current.arg = 0;
	return -1;
}


/*----------------------------------------------------------
	Argument list fetcher
----------------------------------------------------------*/

/*
 * Extract a "value" out of an argument.
 * Only types marked in 'types' are accepted.
 *
 * If symrefs are accepted, the 'stypes' mask
 * determines which symbol types are accepted.
 *
 * Returns NULL upon failure, or the address of
 * a eel_data_t struct.
 */
static eel_data_t *get_value(eel_data_t *arg, int types, int stypes)
{
	eel_data_t *dat;

	dat = arg;

	/* Find actual source data */
#if 0
	while(EDT_SYMREF == dat->type)
#else
	if(EDT_SYMREF == dat->type)
#endif
	{
		switch(dat->value.sym->type)
		{
		  case EST_ENUM:
			/* Keep symref if enums are desired! */
			if(types & EDTM_SYMREF)
				break;
		  case EST_CONSTANT:
		  case EST_VARIABLE:
			dat = &dat->value.sym->data;
			break;
		  default:
			eel_error("Invalid argument!");
			return NULL;
		}
	}

	if((EDT_SYMREF == dat->type) && (types & EDTM_SYMREF))
	{
		/* Check symbol type */
		if(!(stypes & (1<<dat->value.sym->type)))
		{
			eel_error("Wrong symbol type! (%s)",
					eel_data_is(dat));
			return NULL;
		}
		return arg;
	}
	else
	{
		/* Check data type */
		if(!(types & (1<<dat->type)))
		{
			eel_error("Wrong data type! (%s)",
					eel_data_is(dat));
			return NULL;
		}
		return dat;
	}
}


/*
 * Find the symbol table entry for the variable
 * referenced by 'arg'. If 'arg' is not a symbol
 * reference, or if the referenced symbol is not
 * a variable, NULL is returned, otherwise the
 * symbol is returned.
 */
static eel_symbol_t *get_variable(eel_data_t *arg)
{
	eel_symbol_t *sym;

	if(EDT_SYMREF != arg->type)
		return NULL;

	sym = arg->value.sym;

	if(sym->type != EST_VARIABLE)
	{
		eel_error("Expected a variable, not %s!", eel_symbol_is(sym));
		return NULL;
	}
	return sym;
}


static int eel_get_argsv(int argc, struct eel_data_t *argv,
		const char *fmt, va_list args)
{
	int parsing = 1;
	int optional = 0;
	int tuple = 0;
	int using_tuple = 0;
	const char *tuple_start = NULL;
	int got = 0;
	int flags;
	eel_data_t *dat;
	eel_symbol_t *sym;
	va_list	args_tuple_start;

	int *i;
	double *r;
	char **s;
	eel_data_t **datp;
	eel_symbol_t **symp;

	eel_current.arg = 1;

	va_copy(args_tuple_start, args);

	while(parsing > 0)
	{
		eel_data_t *a;

		/* Handle "harmless" control characters */
		switch(*fmt)
		{
		  case 0:
/*		  case ')':	For LISP style function calls */
			if(tuple_start)
			{
				/*
				 * We should never get here if
				 * we have an argument tuple!
				 */
				log_printf(ELOG,"eel eel_get_args(): Error in"
						" format string; "
						"unterminated argument"
						"tuple!\n");
				parsing = -1;
				continue;
			}
			if(!optional && (got < argc))
			{
				eel_error("%d arguments required; got %d!",
						got, argc);
				parsing = -1;
				continue;
			}
			parsing = 0;
			continue;
		  case '<':
			/* Argument tuple start */
			++fmt;
			va_end(args_tuple_start);
			if(using_tuple)
			{
				log_printf(ELOG,"eel eel_get_args(): Error in"
						" format string; "
						"'<' inside tuple!\n");
				parsing = -1;
				continue;
			}
			using_tuple = 1;
			tuple_start = fmt;
			va_copy(args_tuple_start, args);
			continue;
		  case '>':
			/* Argument tuple end */
			if(!tuple_start)
			{
				log_printf(ELOG,"eel eel_get_args(): Error in"
						" format string; "
						"'>' without '<'!\n");
				parsing = -1;
				continue;
			}
			fmt = tuple_start;
			va_end(args);
			va_copy(args, args_tuple_start);
			++tuple;
			continue;
		  case '[':
			/* Optional arguments */
			optional = 1;
		  case ']':
			++fmt;
			continue;
		  case '*':
			/* Throw away this argument */
			++fmt;
			++eel_current.arg;
			continue;
		  case ',':
			++fmt;
			continue;
		  default:
			if(got < argc)
				a = argv + got;
			else
			{
				parsing = 0;
				continue;
			}
		}

		/* Handle actual data extraction tokens */
		switch(*fmt)
		{
		  case '?':
			datp = va_arg(args, eel_data_t **);
			datp[tuple] = a;
			break;
		  case 'E':
			symp = va_arg(args, eel_symbol_t **);
			if(EDT_SYMREF != a->type)
			{
				eel_error("Not an enum constant!\n");
				parsing = -1;
				continue;
			}
			symp[tuple] = a->value.sym;

			/* Handle variables containing symrefs! */
			if( (symp[tuple]->type == EST_VARIABLE) &&
					(symp[tuple]->data.type == EDT_SYMREF) )
				symp[tuple] = symp[tuple]->data.value.sym;

			if(symp[tuple]->type != EST_ENUM)
			{
				eel_error("%s, not an enum constant!",
						eel_symbol_is(symp[tuple]));
				parsing = -1;
				continue;
			}
			break;
		  case 'V':
			datp = va_arg(args, eel_data_t **);
			if('(' == *(fmt+1))
			{
				fmt += 2;
				flags = 0;
				while(*fmt != ')')
				{
					switch(*fmt)
					{
					  case 0:
						log_printf(ELOG,"Unexpected end"
								" of control"
								"string!\n");
						parsing = -1;
						break;
					  case 'i':
						flags |= EDTM_INTEGER;
						break;
					  case 'r':
						flags |= EDTM_REAL;
						break;
					  case 's':
						flags |= EDTM_STRING;
						break;
					  case 'c':
						flags |= EDTM_CADDR;
						break;
					  case 'e':
						flags |= EDTM_SYMREF;
						break;
					  default:
						log_printf(ELOG,"Unknown type"
								" code '%c'!",
								*fmt);
						parsing = -1;
						break;
					}
					if(parsing < 0)
						break;
					++fmt;
				}
				if(parsing < 0)
					continue;
			}
			else
				flags = EDTM_REAL | EDTM_INTEGER |
					EDTM_STRING | EDTM_CADDR;
			datp[tuple] = get_value(a, flags, ESTM_ENUM);
			if(!datp[tuple])
			{
				parsing = -1;
				continue;
			}
			break;
		  case 'e':
			i = va_arg(args, int *);
			if(a->type != EDT_SYMREF)
			{
				eel_error("Enum argument must be a symref, "
						"not %s!", eel_data_is(a));
				parsing = -1;
				continue;
			}
			sym = a->value.sym;
			if(sym->type == EST_VARIABLE)
			{
				/* Follow reference */
				eel_data_t *sd = &sym->data;
				if(sd->type == EDT_SYMREF)
					if(sd->value.sym->type == EST_ENUM)
						sym = sd->value.sym;
			}
			if(sym->type != EST_ENUM)
			{
				eel_error("Expected enumerated type, not %s!",
						eel_symbol_is(sym));
				parsing = -1;
				continue;
			}
			if(sym->token != *i)
			{
				/* Oops, inter-enum namespace conflict!
				 * See if we can find the same identifier
				 * in the *right* class...
				 */
				sym = eel_s_find_n_tk(sym, sym->name, *i);
				if(!sym)
				{
					eel_error("Wrong enumeration class!");
					parsing = -1;
					continue;
				}
			}
			i[tuple] = sym->data.value.i;
			break;
		  case 'f':
			symp = va_arg(args, eel_symbol_t **);
			if(EDT_SYMREF != a->type)
			{
				eel_error("Not a function!\n");
				parsing = -1;
				continue;
			}
			symp[tuple] = a->value.sym;

			/* Handle variables containing symrefs! */
			if( (symp[tuple]->type == EST_VARIABLE) &&
					(symp[tuple]->data.type == EDT_SYMREF) )
				symp[tuple] = symp[tuple]->data.value.sym;

			if(symp[tuple]->type != EST_FUNCTION)
			{
				eel_error("%s, not a function!",
						eel_symbol_is(symp[tuple]));
				parsing = -1;
				continue;
			}
			break;
		  case 'i':
			i = va_arg(args, int *);
			dat = get_value(a, EDTM_INTEGER | EDTM_REAL | EDTM_STRING,
					0);
			if(!dat)
			{
				eel_error("Failed to cast argument to integer.");
				parsing = -1;
				continue;
			}
			switch(dat->type)
			{
			  case EDT_REAL:
				i[tuple] = (int)dat->value.r;
				break;
			  case EDT_INTEGER:
				i[tuple] = dat->value.i;
				break;
			  case EDT_STRING:
				i[tuple] = (int)atof(dat->value.s);
				break;
			  default:
				eel_error("INTERNAL: Got type I didn't ask for.");
				parsing = -1;
				continue;
			}
			break;
		  case 'n':
			symp = va_arg(args, eel_symbol_t **);
			symp[tuple] = get_variable(a);
			if(!symp[tuple])
			{
				if(a->type == EDT_SYMNAME)
					symp[tuple] = eel_set_integer(
							a->value.s, 0);
				if(!symp[tuple])
				{
					eel_error("Failed to create new symbol! '%s'",
							a->value.s);
					parsing = -1;
					continue;
				}
			}
			break;
		  case 'r':
			r = va_arg(args, double *);
			dat = get_value(a, EDTM_INTEGER | EDTM_REAL | EDTM_STRING,
					0);
			if(!dat)
			{
				eel_error("Failed to cast argument to real.");
				parsing = -1;
				continue;
			}
			switch(dat->type)
			{
			  case EDT_REAL:
				r[tuple] = dat->value.r;
				break;
			  case EDT_INTEGER:
				r[tuple] = (double)dat->value.i;
				break;
			  case EDT_STRING:
				r[tuple] = atof(dat->value.s);
				break;
			  default:
				eel_error("INTERNAL: Got type I didn't ask for.");
				parsing = -1;
				continue;
			}
			break;
		  case 's':
			s = va_arg(args, char **);
			dat = get_value(a, EDTM_STRING, 0);
			if(!dat)
			{
				eel_error("Failed to cast argument to string.");
				parsing = -1;
				continue;
			}
			switch(dat->type)
			{
#if 0
TODO: (String memory management problem)
			  case EDT_REAL:
				break;
			  case EDT_INTEGER:
				break;
#endif
			  case EDT_STRING:
				s[tuple] = dat->value.s;
				break;
			  default:
				eel_error("INTERNAL: Got type I didn't ask for.");
				parsing = -1;
				continue;
			}
			break;
		  case 'v':
			symp = va_arg(args, eel_symbol_t **);
			symp[tuple] = get_variable(a);
			if(!symp[tuple])
			{
				parsing = -1;
				continue;
			}
			break;
		  default:
			eel_error("eel_get_args(): Error in format string; "
					"unknown control char '%c'.\n", *fmt);
			parsing = -1;
			continue;
		}
		++fmt;
		++got;
		++eel_current.arg;	/* (For error messages) */
	}
	va_end(args_tuple_start);

	if(parsing < 0)
		return -1;	/* Error exit! */

	eel_current.arg = 0;

	if(!optional)
	{
		/*
		 * We *should* be at the end of the format string
		 * now. Let's check...
		 */
		while(*fmt)
		{
			switch(*fmt)
			{
			  case '>':
			  case ']':
				break;
			  default:
				eel_error("Insufficient number of arguments!");
				return -1;
			}
			++fmt;
		}
	}

	if(argc > got)
	{
		eel_error("Too many arguments!");
		eel_current.arg = 0;
		return -1;
	}

	return got;
}


int eel_get_args_from(int argc, struct eel_data_t *argv, const char *fmt, ...)
{
	int ret;
	va_list	args;
	va_start(args, fmt);
	ret = eel_get_argsv(argc, argv, fmt, args);
	va_end(args);
	return ret;
}


int eel_get_args(const char *fmt, ...)
{
	int ret;
	va_list	args;
	va_start(args, fmt);
	ret = eel_get_argsv(eel_arg_count, eel_args, fmt, args);
	va_end(args);
	return ret;
}

