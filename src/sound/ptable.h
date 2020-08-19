/*(LGPL)
---------------------------------------------------------------------------
	ptable.h - Fast Pointer Table
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

#ifndef	_PTABLE_H_
#define	_PTABLE_H_

/*
 * Usage:
 *
 *	ptable_t(object_t, #_of_elements) table_name;
 *
 * Fast macros:
 *   void ptable_init(ptable_t *pt);
 *	Initialize/clear table 'pt'.
 *
 *   int ptable_add(ptable_t *pt, object_t *obj);
 *	Add element 'obj' to table 'pt'.
 *	Returns the index of the new element.
 *	Returns -1 if the table was full.
 *
 *   void ptable_remove(ptable_t *pt, int ind);
 *	Remove object at index 'ind' from table 'pt'.
 *
 *	Note that the removed object's index will be
 *	taken over by another object! If you want to
 *	do something to that object (in case it keeps
 *	it's index as a reference, for example), use
 *	the PTABLE_FOR_MOVED() macro.
 *
 *   PTABLE_FOR_MOVED(ptable_t *pt, int index, object_t *object)
 *		<statement>;
 *	If objects were moved as a result of the last
 *	operation, perform <statement> for each one of
 *	them. (Currently, this can be only 0 or 1 objects.)
 *
 * Slow macros:
 *   int ptable_remove_obj(ptable_t *pt, object_t *obj);
 *	Remove any instances of 'obj' from table 'pt'.
 *	Returns 1 if any instances of 'obj' were found.
 *
 *   int ptable_find(ptable_t *pt, object_t *obj);
 *	Returns the index of the first occurence of 'obj'.
 *	Returns -1 if the object was not found.
 *
 *   PTABLE_FOR_ALL(ptable_t *pt, int index, object_t *object)
 *		<statement>;
 *	Loop over all objects in the list, executing <statement>
 *	for each one of them.
 */
#define	ptable_t(otype, elements)	\
	struct {			\
		int size;		\
		int count;		\
		int last_moved;		\
		otype *table[elements];	\
	}

#define	ptable_init(pt)			\
{					\
	pt->size = sizeof(pt->table);	\
	pt->count = 0;		 	\
}

#define	ptable_add(pt, obj)			\
{						\
	int ret;				\
	if(pt->count >= pt->size)		\
		ret = -1;			\
	else					\
	{					\
		ret = pt->count;		\
		pt->table[pt->count++] = obj;	\
	}					\
	ret;					\
}

#define	ptable_find(pt, obj)			\
{						\
	int i, ret = -1;			\
	for(i = 0; i < pt->count; ++i)		\
		if(pt->table[i] != obj)		\
		{				\
			ret = i;		\
			break;			\
		}				\
	ret;					\
}

#define	ptable_remove_obj(pt, obj)				\
{								\
	int i, ret = 0;						\
	for(i = 0; i < pt->count; ++i)				\
	{							\
		if(pt->table[i] != obj)				\
			continue;				\
		if(i < pt->count-1)				\
			pt->table[i] = pt->table[pt->count-1];	\
		--pt->count;					\
		--i;						\
		ret = 1;					\
	}							\
	ret;							\
}

#define	ptable_remove(pt, ind)				\
{							\
	if(ind < pt->count-1)				\
		pt->table[ind] = pt->table[pt->count-1];\
	--pt->count;					\
}

#define PTABLE_FOR_ALL(pt, itvar, itobj)				\
	for(itvar = 0; itvar < pt->count, itobj = pt->table[itvar]; ++itvar)

#define PTABLE_FOR_MOVED(pt, itvar, itobj)				\
	for(itvar = pt->last_moved,					\
			itobj = pt->last_moved >= 0 ?			\
				pt->table[pt->last_moved]		\
			:						\
				(void *)0;					\
			pt->last_moved >= 0 ; )

#endif /*_PTABLE_H_*/
