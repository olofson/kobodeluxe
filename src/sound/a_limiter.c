/*(LGPL)
---------------------------------------------------------------------------
	a_limiter.h - Simple limiter
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

#include <stdlib.h>
#include "a_limiter.h"
#include "a_types.h"

int lim_open(limiter_t *lim, int samplerate)
{
	lim->samplerate = samplerate;
	lim_control(lim, LIM_THRESHOLD, 32768);
	lim_control(lim, LIM_RELEASE, 300000);
	lim->peak = (unsigned)(32768<<8);
	lim->attenuation = 0;
	return 0;
}

void lim_close(limiter_t *lim)
{
	/* Nothing to destruct */
}

void lim_control(limiter_t *lim, int param, int value)
{
	switch(param)
	{
	  case LIM_THRESHOLD:
		lim->threshold = (unsigned)(value << 8);
		if(lim->threshold < 256)
			lim->threshold = 256;
		break;
	  case LIM_RELEASE:
		if(lim->samplerate < 1)
			lim->samplerate = 44100;
		lim->release = (value << 8) / lim->samplerate;
		break;
	}
}

void lims_process(limiter_t *lim, int *in, int *out, unsigned frames)
{
	unsigned i;
	frames <<= 1;
	for(i = 0; i < frames; i += 2)
	{
		int gain;
		int lp = abs(in[i]) << 8;
		int rp = abs(in[i+1]) << 8;
		unsigned maxp = (unsigned)(lp > rp ? lp : rp);
		if(maxp > lim->peak)
		{
			lim->peak = maxp;
			gain = (32767<<16) / ((lim->peak+255)>>8);
		}
		else
		{
			gain = (32767<<16) / ((lim->peak+255)>>8);
			lim->peak -= lim->release;
			if(lim->peak < lim->threshold)
				lim->peak = lim->threshold;
		}
#ifdef A_USE_INT64
		out[i] = (Sint64)in[i] * (Sint64)gain >> 16;
		out[i+1] = (Sint64)in[i+1] * (Sint64)gain >> 16;
#else
		if(gain < 8192)
		{
			out[i] = in[i] * gain >> 16;
			out[i+1] = in[i+1] * gain >> 16;
		}
		else
		{
			out[i] = in[i] * (gain>>4) >> 12;
			out[i+1] = in[i+1] * (gain>>4) >> 12;
		}
#endif
	}
	lim->attenuation = (lim->peak - lim->threshold) >> 8;
}

void limss_process(limiter_t *lim, int *in, int *out, unsigned frames)
{
	unsigned i;
	frames <<= 1;
	for(i = 0; i < frames; i += 2)
	{
		int gain;
		int lp = abs(in[i]);
		int rp = abs(in[i+1]);
		unsigned maxp = (unsigned)(lp > rp ? lp : rp);
		unsigned p = maxp << 8;
		p += (maxp - abs(lp - rp)) << 7;
		if(p > lim->peak)
		{
			lim->peak = p;
			gain = (32767<<16) / ((lim->peak+255)>>8);
		}
		else
		{
			gain = (32767<<16) / ((lim->peak+255)>>8);
			lim->peak -= lim->release;
			if(lim->peak < lim->threshold)
				lim->peak = lim->threshold;
		}
#ifdef A_USE_INT64
		out[i] = (Sint64)in[i] * (Sint64)gain >> 16;
		out[i+1] = (Sint64)in[i+1] * (Sint64)gain >> 16;
#else
		if(gain < 8192)
		{
			out[i] = in[i] * gain >> 16;
			out[i+1] = in[i+1] * gain >> 16;
		}
		else
		{
			out[i] = in[i] * (gain>>4) >> 12;
			out[i+1] = in[i+1] * (gain>>4) >> 12;
		}
#endif
	}
	lim->attenuation = (lim->peak - lim->threshold) >> 8;
}
