/*(LGPL)
---------------------------------------------------------------------------
	e_lexer.c - EEL engine lexer
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "e_lexer.h"
#include "e_util.h"


int eel_get_unique_token()
{
	static int last = TK_USER;
	return last++;
}


static char *lexbuf = NULL;
static int lexbuf_length = 0;


static int lexbuf_realloc(int len)
{
	char *nsb;
	lexbuf_length = len * 5 / 4;
	nsb = (char *)realloc(lexbuf, lexbuf_length + 1);
	if(!nsb)
	{
		eel_error("EEL lexer out of memory!");
		return -1;
	}
	lexbuf = nsb;
	return 0;
}


static inline int lexbuf_bump(int len)
{
	if(len >= lexbuf_length)
		return lexbuf_realloc(len);
	return 0;
}


void eel_lexer_cleanup(void)
{
	if(eel_current.lval)
	{
		eel_d_freestring(eel_current.lval);
		free(eel_current.lval);
		eel_current.lval = NULL;
	}
	free(lexbuf);
	lexbuf = NULL;
	lexbuf_length = 0;
}


static int skipwhite(int report_eoln)
{
	int c;
	/* Ignore whitespace and get first nonwhite character. */
	while((c = bio_getchar(eel_current.bio)) == ' ' || c == '\t' ||
			c == '\r' || c == '\n')
		if((c == '\n') && report_eoln)
				return c;
	return c;
}


static eel_data_t *newdata(eel_datatypes_t type)
{
	eel_data_t *d = (eel_data_t *)calloc(1, sizeof(eel_data_t));
	if(!d)
		return NULL;
	d->type = type;
	return d;
}


static int get_figure(bio_file_t *h, int base)
{
	int n = bio_getchar(h);
	if(n >= '0' && n <= '9')
		n -= '0';
	else if(n >= 'a' && n <= 'z')
		n -= 'a';
	else if(n >= 'A' && n <= 'Z')
		n -= 'A';
	else
		return -1;
	if(n >= base)
		return -1;
	return n;
}


static int get_num(bio_file_t *h, int base, int figures)
{
	int value = 0;
	while(figures--)
	{
		int n = get_figure(h, base);
		if(n < 0)
			return n;
		value *= base;
		value += n;
	}
	return value;
}


/*
 * Parse quoted string.
 * C printf "backslash codes" are supported,
 * as well as "\dXXX", for decimal codes.
 */
static int parse_string(bio_file_t *h)
{
	int c, len = 0;
	while((c = bio_getchar(h)) != '"')
	{
		switch(c)
		{
		  case EOF:
			eel_error("EOF inside string literal!");
			return TK_ERROR;
		  case '\\':
			c = bio_getchar(h);
			switch(c)
			{
			  case EOF:
				eel_error("EOF inside string escape sequence!");
				return -1;
			  case '0':
				c = get_num(h, 8, 3);
				if(c < 0)
				{
					eel_error("Illegal octal figure!");
					return TK_ERROR;
				}
				break;
			  case 'a':
				c = '\a';
				break;
			  case 'b':
				c = '\b';
				break;
			  case 'c':
				c = '\0';
				break;
			  case 'd':
				c = get_num(h, 10, 2);
				if(c < 0)
				{
					eel_error("Illegal decimal figure!");
					return TK_ERROR;
				}
				break;
			  case 'f':
				c = '\f';
				break;
			  case 'n':
				c = '\n';
				break;
			  case 'r':
				c = '\r';
				break;
			  case 't':
				c = '\t';
				break;
			  case 'v':
				c = '\v';
				break;
			  case 'x':
				c = get_num(h, 16, 2);
				if(c < 0)
				{
					eel_error("Illegal hex figure!");
					return TK_ERROR;
				}
				break;
			  default:
				break;
			}
			break;
		  default:
			break;
		}
		lexbuf_bump(len + 1);
		lexbuf[len++] = c;
	}
	lexbuf[len] = 0;
	eel_current.lval = newdata(EDT_STRING);
	if(!eel_current.lval)
		return TK_ERROR;
	eel_d_setstring(eel_current.lval, lexbuf);
	return TK_STRN;
}


static inline int token(int tk)
{
	eel_current.token = tk;
	return tk;
}

/*
 * Pass 1 for 'report_eoln' to get '\n' back whenever a
 * newline occurs in the source.
 *
 * Note that newlines inside comments are *always* treated
 * as white space!
 */
int eel_lex(int report_eoln)
{
	int c, prevc;
	int negative = 0, directive = 0;
	bio_file_t *h = eel_current.bio;
	eel_symbol_t *s;

	/* Handle eel_unlex() pushbacks */
	if(eel_current.unlexed)
	{
		eel_current.unlexed = 0;
		return eel_current.token;
	}

	/* In case someone should ignore an lval... */
	if(eel_current.lval)
	{
		eel_d_freestring(eel_current.lval);
		free(eel_current.lval);
		eel_current.lval = NULL;
	}

	while(1)
	{
		c = skipwhite(report_eoln);
		if(c == EOF)
			return token(0);

		/* /X comment tokens */
		if(c == '/')
		{
			switch ((c = bio_getchar(h)))
			{
			  case '/':	/* C++ style single line comment */
				while(EOF != (c = bio_getchar(h)))
					if('\n' == c)
						break;
				continue;
			  case '*':	/* C style comment */
				prevc = 0;
				while(EOF != (c = bio_getchar(h)))
				{
					if(('/' == c) && ('*' == prevc))
						break;
					else
						prevc = c;
				}
				continue;
			  default:
				bio_ungetc(h);
				c = '/';
				break;
			}
		}
		break;
	}

	/*
	 * Char starts a number => parse the number.
	 * Note: This never returns an EDT_INTEGER!
	 */
	if(c == '.' || isdigit(c))
	{
		bio_ungetc(h);
		eel_current.lval = newdata(EDT_REAL);
		if(!eel_current.lval)
			return token(TK_ERROR);
		eel_current.lval->value.r = bio_strtod(h);
		if(negative)
			eel_current.lval->value.r = -eel_current.lval->value.r;
		return token(TK_RNUM);
	}

	negative = 0;

	/* 
	 * (Multi)character literal (cast to integer)
	 * Note that this is always little endian, so
	 * it's safe to use for "FourCC" identifiers
	 * and the like.
	 */
	if('\'' == c)
	{
		eel_current.lval = newdata(EDT_INTEGER);
		if(!eel_current.lval)
			return token(TK_ERROR);
		eel_current.lval->value.i = 0;
		while((c = bio_getchar(h)) != '\'')
		{
			if(EOF == c)
			{
				eel_error("EOF inside character literal!");
				return token(TK_ERROR);
			}
			eel_current.lval->value.i <<= 8;
			eel_current.lval->value.i |= c;
		}
		return token(TK_INUM);
	}

	/* String literal */
	if('"' == c)
		return token(parse_string(h));

	/* Check for "preprocessor" directives */
	if('#' == c)
	{
		directive = 1;
		c = bio_getchar(h);
	}

	/* Char starts an identifier => read the name. */
	if(isalpha(c) || ('_' == c))
	{
		int len = 1;
		int start = bio_tell(h) - 1;
		while((isalnum(c = bio_getchar(h)) || ('_' == c)) && (EOF != c))
			++len;

		if(lexbuf_bump(len) < 0)
			return token(TK_ERROR);
		bio_seek(h, start, SEEK_SET);
		bio_read(h, lexbuf, len);
		lexbuf[len] = '\0';

		if(directive)
		{
			directive = 0;
			s = eel_s_find_n_t(NULL, lexbuf, EST_DIRECTIVE);
			if(!s)
			{
				eel_error("Unknown directive '%s'!",
						lexbuf);
				return token(TK_ERROR);
			}
			if(s->type != EST_DIRECTIVE)
			{
				eel_error("Preprocessor directive expected!");
				return token(TK_ERROR);
			}
			eel_current.lval = newdata(EDT_SYMREF);
			eel_current.lval->value.sym = s;
			return token(TK_SYMREF);
		}
		else
		{
			s = eel_s_find(NULL, lexbuf);
			if(s)
			{
				eel_current.lval = newdata(EDT_SYMREF);
				eel_current.lval->value.sym = s;
				return token(TK_SYMREF);
			}
			else
			{
				eel_current.lval = newdata(EDT_SYMNAME);
				eel_d_setstring(eel_current.lval, lexbuf);
				return token(TK_NEWSYM);
			}
		}
	}

	/* Look for valid operator characters */
	if(strchr("+-*/^<>~&%@!|$", (char)c))
	{
		lexbuf[0] = c;
		lexbuf[1] = 0;
		s = eel_s_find_n_t(NULL, lexbuf, EST_OPERATOR);
		if(!s)
		{
			eel_error("Unknown operator '%s'!", lexbuf);
			return token(TK_ERROR);
		}
		eel_current.lval = newdata(EDT_SYMREF);
		eel_current.lval->value.sym = s;
		return token(TK_SYMREF);		
	}

	return token(c);
}


void eel_unlex(void)
{
	eel_current.unlexed = 1;
}
