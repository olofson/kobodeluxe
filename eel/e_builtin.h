/*(LGPL)
---------------------------------------------------------------------------
	e_builtin.c - EEL built-in commands and directives
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

#ifndef _EEL_BUILTIN_H_
#define _EEL_BUILTIN_H_

#include "eel.h"

/*----------------------------------------------------------
	Command Callbacks
----------------------------------------------------------*/
int eel_cmd_end(int argc, struct eel_data_t *argv);
int eel_cmd_set(int argc, struct eel_data_t *argv);
int eel_cmd_print(int argc, struct eel_data_t *argv);


/*----------------------------------------------------------
	Operator Callbacks
----------------------------------------------------------*/
int eel_op_add(int argc, struct eel_data_t *argv);
int eel_op_sub(int argc, struct eel_data_t *argv);
int eel_op_mul(int argc, struct eel_data_t *argv);
int eel_op_div(int argc, struct eel_data_t *argv);
int eel_op_mod(int argc, struct eel_data_t *argv);
int eel_op_pow(int argc, struct eel_data_t *argv);


/*----------------------------------------------------------
	Directive Callbacks
----------------------------------------------------------*/
int eel_dir_include(void);
int eel_dir_define(void);


/*----------------------------------------------------------
	Special Callbacks
----------------------------------------------------------*/
int eel_spec_function(void);
int eel_spec_procedure(void);

#endif /*_EEL_BUILTIN_H_*/
