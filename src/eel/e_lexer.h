/*(LGPL)
---------------------------------------------------------------------------
	e_lexer.h - EEL engine lexer
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

#ifndef _EEL_LEXER_H_
#define _EEL_LEXER_H_

#include "eel.h"


/*----------------------------------------------------------
	Tokens and Unique User Token management
----------------------------------------------------------*/

/* Errors... */
#define	TK_ERROR	-1	/* Lexer complains about something  */

/* Immediate arguments */
#define	TK_RNUM		300	/* Real number */
#define	TK_INUM		301	/* Integer number */
#define	TK_STRN		302	/* String literal */

/* Symbol reference */
#define	TK_SYMREF	303

/* New symbol */
#define	TK_NEWSYM	304

#define	TK_USER		1024

int eel_get_unique_token();


/*----------------------------------------------------------
	The Lexer
----------------------------------------------------------*/
/* Get next "token" */
int eel_lex(int report_eoln);

/*
 * Push back last token + lval.
 * Works only for *one* token per context!
 */
void eel_unlex(void);

void eel_lexer_cleanup(void);

#endif /*_EEL_LEXER_H_*/
