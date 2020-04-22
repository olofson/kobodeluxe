/*(LGPL)
---------------------------------------------------------------------------
	a_control.h - Group/Channel/Voice Control Implementation
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2002, David Olofson
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

#ifndef _A_CONTROL_H_
#define _A_CONTROL_H_

#include <string.h>
#include "a_struct.h"


/*----------------------------------------------------------
	Tools
----------------------------------------------------------*/

/* Controller bank copy macros */
static inline void acc_copy(accbank_t *to, accbank_t *from,
		unsigned first, unsigned last)
{
	memcpy(to + first, from + first, (last - first + 1) * sizeof(int));
}

static inline void acc_copya(accbank_t *to, accbank_t *from)
{
	memcpy(to, from, sizeof(accbank_t));
}

#define	ACC_IS_FIXEDPOINT(x)	(((x) >= ACC_PAN) && ((x) <= ACC_Z))

#define	ACC_NOTRANSFORM_FIRST	ACC_GROUP
#define	ACC_NOTRANSFORM_LAST	ACC_SEND_BUS
#define	case_ACC_NOTRANSFORM	\
	case ACC_GROUP:		\
	case ACC_PRIORITY:	\
	case ACC_PATCH:		\
	case ACC_PRIM_BUS:	\
	case ACC_SEND_BUS
#define	for_ACC_NOTRANSFORM(x)	\
	for(x = ACC_NOTRANSFORM_FIRST; x <= ACC_NOTRANSFORM_LAST; ++x)

#define	ACC_ADDTRANSFORM_FIRST	ACC_PAN
#define	ACC_ADDTRANSFORM_LAST	ACC_PITCH
#define	case_ACC_ADDTRANSFORM	\
	case ACC_PAN:		\
	case ACC_PITCH
#define	for_ACC_ADDTRANSFORM(x)	\
	for(x = ACC_ADDTRANSFORM_FIRST; x <= ACC_ADDTRANSFORM_LAST; ++x)
extern const int addtrans_min[2];
extern const int addtrans_max[2];
extern const int addtrans_bias[2];

#define	ACC_MULTRANSFORM_FIRST	ACC_VOLUME
#define	ACC_MULTRANSFORM_LAST	ACC_SEND
#define	case_ACC_MULTRANSFORM	\
	case ACC_VOLUME:	\
	case ACC_SEND
#define	for_ACC_MULTRANSFORM(x)	\
	for(x = ACC_MULTRANSFORM_FIRST; x <= ACC_MULTRANSFORM_LAST; ++x)


/*----------------------------------------------------------
	Group Control
----------------------------------------------------------*/

/*
 * Set a group control, and update all channels that belong
 * to the group. (Expensive!)
 */
void acc_group_set(unsigned gid, unsigned ctl, int arg);

void audio_group_open(void);
void audio_group_close(void);

#endif /*_A_CONTROL_H_*/
