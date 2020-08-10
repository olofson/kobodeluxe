/*(LGPL)
---------------------------------------------------------------------------
	a_filters.h - Various Audio Signal Filters
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

#ifndef _A_FILTERS_H_
#define _A_FILTERS_H_

/*--------------------------------------------------------------------
	f6_t - Simple mono 6 dB/oct state variable filter
--------------------------------------------------------------------*/
typedef struct f6_t
{
	int rate;
	int d;
	int f;
} f6_t;

void f6_init(f6_t *fil, int fs);
void f6_set_f(f6_t *fil, int f);
void f6_process_lp(f6_t *fil, int *buf, unsigned frames);
void f6_process_hp(f6_t *fil, int *buf, unsigned frames);

/*--------------------------------------------------------------------
	f6s_t - Simple stereo 6 dB/oct state variable filter
--------------------------------------------------------------------*/
typedef struct f6s_t
{
	int rate;
	int f;
	int dl, dr;
} f6s_t;

void f6s_init(f6s_t *fil, int fs);
void f6s_set_f(f6s_t *fil, int f);
void f6s_process_lp(f6s_t *fil, int *buf, unsigned frames);
void f6s_process_hp(f6s_t *fil, int *buf, unsigned frames);

/*--------------------------------------------------------------------
	dcf6s_t - Fast stereo 6 dB/oct HPF for DC suppression
--------------------------------------------------------------------*/
typedef struct dcf6s_t
{
	int rate;
	int f;
	int dl, dr;
} dcf6s_t;

void dcf6s_init(dcf6s_t *fil, int fs);
void dcf6s_set_f(dcf6s_t *fil, int f);
void dcf6s_process(dcf6s_t *fil, int *buf, unsigned frames);
int dcf6s_silent(dcf6s_t *fil);

/*--------------------------------------------------------------------
	resof12_t - Resonant mono 12 dB/oct state variable filter
--------------------------------------------------------------------*/
typedef struct resof12_t
{
	int rate;
	int d1, d2;
	int f;
	int q;
} resof12_t;

void resof12_init(resof12_t *rf, int fs);
void resof12_set_f(resof12_t *rf, int f);
void resof12_set_q(resof12_t *rf, int q);
void resof12_process_lp(resof12_t *rf, int *buf, unsigned frames);	/*In-place LP*/
void resof12_process_hp(resof12_t *rf, int *buf, unsigned frames);	/*In-place HP*/
void resof12_process_bp(resof12_t *rf, int *buf, unsigned frames);	/*In-place BP*/
/*
 * LP filter the 'buf' buffer, while separating off
 * the HP part into buffer 'out'.
 */
void resof12_process_split(resof12_t *rf, int *buf, int *out, unsigned frames);

/*--------------------------------------------------------------------
	resof12s_t - Resonant stereo 12 dB/oct state variable filter
--------------------------------------------------------------------*/
typedef struct resof12s_t
{
	int rate;
	int d1l, d2l;
	int d1r, d2r;
	int f;
	int q;
} resof12s_t;

void resof12s_init(resof12s_t *rf, int fs);
void resof12s_set_f(resof12s_t *rf, int f);
void resof12s_set_q(resof12s_t *rf, int q);
void resof12s_process_lp(resof12s_t *rf, int *buf, unsigned frames);	/*In-place LP*/

#endif /*_A_FILTERS_H_*/
