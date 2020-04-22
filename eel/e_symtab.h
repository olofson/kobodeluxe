/*(LGPL)
---------------------------------------------------------------------------
	symtab.h - Simple symbol table for EEL
---------------------------------------------------------------------------
 * Copyright (C) 2000, 2002, David Olofson
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

#ifndef _EEL_SYMTAB_H_
#define _EEL_SYMTAB_H_


/*------------------------------------------------
	Data container
------------------------------------------------*/

struct eel_symbol_table_t;
struct eel_symbol_t;
struct eel_data_t;

typedef int (*eel_operator_cb_t)(int argc, struct eel_data_t *argv);
typedef int (*eel_parser_cb_t)(void);

/* Native datatypes */
typedef enum
{
	EDT_ILLEGAL = 0,
	EDT_REAL,	/* Real */
	EDT_INTEGER,	/* Integer */
	EDT_STRING,	/* String */
	EDT_CADDR,	/* Code address */
	EDT_SYMREF,	/* Symbol reference */
	EDT_SYMTAB,	/* Symbol Table Reference (for namespaces) */
	EDT_SYMNAME,	/* Name of a new symbol (String) */
	EDT_OPERATOR,	/* Operator callback */
	EDT_DIRECTIVE,	/* Directive parser callback */
	EDT_SPECIAL	/* Special parser callback */
} eel_datatypes_t;

#define	EDTM_ILLEGAL	(1<<EDT_ILLEGAL)
#define	EDTM_REAL	(1<<EDT_REAL)
#define	EDTM_INTEGER	(1<<EDT_INTEGER)
#define	EDTM_STRING	(1<<EDT_STRING)
#define	EDTM_CADDR	(1<<EDT_CADDR)
#define	EDTM_SYMREF	(1<<EDT_SYMREF)
#define	EDTM_SYMNAME	(1<<EDT_SYMNAME)
#define	EDTM_OPERATOR	(1<<EDT_OPERATOR)
#define	EDTM_DIRECTIVE	(1<<EDT_DIRECTIVE)
#define	EDTM_SPECIAL	(1<<EDT_SPECIAL)

/* Data container */
typedef struct eel_data_t
{
	eel_datatypes_t type;
	union {
		double	r;	/* real constant */
		int	i;	/* integer constant; token */
		int	a;	/* code address */
		char	*s;	/* string constant; symbol name */
		struct eel_symbol_t		*sym;	/* Symbol ref */
		struct eel_symbol_table_t	*st;	/* Symbol table */
		struct
		{
			eel_operator_cb_t	cb;
			char			priority;
			char			retargs;
			char			preargs;
		} op;
		eel_parser_cb_t		parse;
	} value;
} eel_data_t;

int eel_d_copy(eel_data_t *data, const eel_data_t *from);

/* Copy string */
int eel_d_setstring(eel_data_t *data, const char *s);

/*
 * Take over string. (No copying)
 *
 * The source string pointer *must* have been returned
 * by malloc(), calloc() or realloc(), and the string
 * MUST NOT be freed after this call. (The string will
 * be owned by the data_t.)
 */
void eel_d_grabstring(eel_data_t *data, char *s);

void eel_d_freestring(eel_data_t *data);

/* NOTE: Output string only valid until next call. */
const char *eel_d_stringrep(eel_data_t *data);


/*------------------------------------------------
	Symbol table
------------------------------------------------*/

typedef enum eel_symtypes_t
{
	EST_UNDEFINED,
	EST_CONSTANT,
	EST_VARIABLE,
	EST_LABEL,
	EST_FUNCTION,	/* token = function index; data.a = start "address" */
	EST_OPERATOR,	/* data -> callback; token = priority */
	EST_DIRECTIVE,	/* data -> callback */
	EST_SPECIAL,	/* data -> callback */
	EST_ENUM,	/* token = enum type; data.i = enum value */
	EST_NAMESPACE	/* data -> namespace symbol table */
} eel_symtypes_t;

#define	ESTM_UNDEFINED	(1<<EST_UNDEFINED)
#define	ESTM_CONSTANT	(1<<EST_CONSTANT)
#define	ESTM_VARIABLE	(1<<EST_VARIABLE)
#define	ESTM_LABEL	(1<<EST_LABEL)
#define	ESTM_FUNCTION	(1<<EST_FUNCTION)
#define	ESTM_OPERATOR	(1<<EST_OPERATOR)
#define	ESTM_DIRECTIVE	(1<<EST_DIRECTIVE)
#define	ESTM_SPECIAL	(1<<EST_SPECIAL)
#define	ESTM_ENUM	(1<<EST_ENUM)
#define	ESTM_NAMESPACE	(1<<EST_NAMESPACE)

/* symbol list node */
typedef struct eel_symbol_t
{
	struct eel_symbol_t	*next;
	char			*name;
	eel_symtypes_t		type;
	eel_data_t		data;
	int			token;
} eel_symbol_t;

/* Open/close symbol table */
int eel_s_open(void);
void eel_s_close(void);

eel_symbol_t *eel_s_new(const char *name, eel_symtypes_t type);

void eel_s_freeall(void);

eel_symbol_t *eel_s_find(eel_symbol_t *sym, const char *sym_name);
eel_symbol_t *eel_s_find_n_tk(eel_symbol_t *sym, const char *sym_name,
		int token);
eel_symbol_t *eel_s_find_tk(eel_symbol_t *sym, int token);
eel_symbol_t *eel_s_find_n_t(eel_symbol_t *sym, const char *sym_name,
		eel_symtypes_t type);
eel_symbol_t *eel_s_find_t(eel_symbol_t *sym, eel_symtypes_t type);

/* NOTE: Output string only valid until next call. */
const char *eel_s_stringrep(eel_symbol_t *sym);


/*------------------------------------------------
	Scope Management
--------------------------------------------------
 * The EEL symbol table manages namespace
 * separation using a stack of separate symbol
 * tables. A "scope" consists of a namespace and
 * all namespaces above it.
 *
 * In the normal case, the "current scope" includes
 * all namespaces currently defined, but it's
 * possible to temporarily move the scope up to an
 * arbitrary namespace level.
 *
 * Symbol lookups start at the namespace level set
 * by the "current scope", and moves up from there
 * until the symbol is found, or it is concluded
 * that the symbol does not exist.
 *
 * New symbols are added to the namespace set by
 * the "current scope". Higher level namespaces are
 * never considered.
 */

/*
 * Create a new, local scope inside the deepest
 * level of the current scope, and make it current.
 */
int eel_push_scope(void);

/*
 * Destroy the deepest level of the scope, and move
 * up to the next higher level.
 */
int eel_pop_scope(void);

/*
 * Returns the current scope level.
 */
int eel_scope(void);

/*
 * Directly set scope level for subsequent eel_s_*()
 * operations.
 */
int eel_set_scope(int ns);

/*
 * Return to the current deepest level scope.
 */
void eel_restore_scope(void);


/*------------------------------------------------
	Symbol table tools
------------------------------------------------*/

/*
 * Find symbol 'name' and verify that it is of of type 'type',
 * or if it doesn't exist, create new symbol 'name' of type
 * 'type'.
 *
 * Returns the found/new symbol, or NULL in the case of
 * failure.
 */
eel_symbol_t *eel_s_get(const char *name, eel_symtypes_t type);

/*
 * Load 'dat' into symbol 'sym'.
 * Returns 0 upon success, or a negative value if the operation
 * fails.
 */
int eel_s_set(eel_symbol_t *sym, const eel_data_t *dat);

#endif /* _EEL_SYMTAB_H_ */
