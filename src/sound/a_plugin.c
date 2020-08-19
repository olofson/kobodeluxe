/*(LGPL)
---------------------------------------------------------------------------
	a_plugin.c - Audio Engine Plugin API
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
#include "a_plugin.h"
#include "a_tools.h"

static int usecount = 0;


#define	EMULBUF_FRAMES	256

static int *emulbuf = NULL;

static int init_emulbuf(void)
{
	if(!emulbuf)
		emulbuf = malloc(EMULBUF_FRAMES * sizeof(int) * 2);
	if(!emulbuf)
		return -1;
	return 0;
}


int audio_plugin_state(audio_plugin_t *p, audio_fxstates_t new_state)
{
	int res, s = p->current_state;
	/* We don't care about SILENT and RESTING here. */
	if(s > FX_STATE_RUNNING)
		s = FX_STATE_RUNNING;

	/* Nor do we allow forced switches to SILENT or RESTING! */
	if(new_state > FX_STATE_RUNNING)
		return -1;

	while(new_state > s)
	{
		++s;
		res = p->state(p, s);
		if(res < 0)
			return res;

		p->current_state = s;
	}
	while(new_state < s)
	{
		--s;
		res = p->state(p, s);
		if(res < 0)
			return res;

		p->current_state = s;
		switch(new_state)
		{
		  case FX_STATE_CLOSED:
			free(p->ctl);
			p->ctl = NULL;
			--usecount;
			if(usecount <= 0)
			{
				free(emulbuf);
				emulbuf = NULL;
				usecount = 0;
			}
			break;
		  case FX_STATE_OPEN:
		  case FX_STATE_READY:
		  case FX_STATE_PAUSED:
		  case FX_STATE_RUNNING:	/* Can't happen. */
		  case FX_STATE_SILENT:		/* Can't happen. */
		  case FX_STATE_RESTING:	/* Can't happen. */
			break;
		}
	}
	return 0;
}


int default_state(struct audio_plugin_t *p, audio_fxstates_t new_state)
{
	if(new_state > p->current_state)
		switch(new_state)
		{
		  case FX_STATE_CLOSED:	/* Can't happen. */
		  case FX_STATE_OPEN:
			if(!audio_plugin_alloc_ctls(p, FXC_COUNT))
				return -1;
			break;
		  case FX_STATE_READY:
		  case FX_STATE_PAUSED:
		  case FX_STATE_RUNNING:
		  case FX_STATE_SILENT:		/* Can't happen. */
		  case FX_STATE_RESTING:	/* Can't happen. */
			break;
		}
	else
		switch(new_state)
		{
		  case FX_STATE_CLOSED:
		  case FX_STATE_OPEN:
		  case FX_STATE_READY:
		  case FX_STATE_PAUSED:
		  case FX_STATE_RUNNING:
		  case FX_STATE_SILENT:
		  case FX_STATE_RESTING:	/* Can't happen. */
			break;
		}
	return 0;
}


#define	SPLIT_BUFFER						\
	unsigned s, f;						\
	for(s = 0; f = (unsigned)(frames > EMULBUF_FRAMES ?	\
			EMULBUF_FRAMES : frames),		\
			s < frames; s += f)

void default_control(struct audio_plugin_t *p, unsigned ctl, int arg)
{
	if((ctl < p->controls) && p->ctl)
		p->ctl[ctl] = arg;
}


static void process_with_process_r(struct audio_plugin_t *p,
		int *buf, unsigned frames)
{
	p->process_r(p, buf, buf, frames);
}

static void process_with_process_m(struct audio_plugin_t *p,
		int *buf, unsigned frames)
{
	SPLIT_BUFFER
	{
		s32clear(emulbuf + s, f);
		p->process_r(p, buf + s, emulbuf + s, f);
		s32copy(emulbuf + s, buf + s, f);
	}
}


static void process_r_with_process(struct audio_plugin_t *p,
		int *in, int *out, unsigned frames)
{
	if(in)
		s32copy(in, out, frames);
	else
		s32clear(out, frames);
	p->process(p, out, frames);
}

static void process_r_with_process_m(struct audio_plugin_t *p,
		int *in, int *out, unsigned frames)
{
	s32clear(out, frames);
	p->process_m(p, in, out, frames);
}


static void process_m_with_process(struct audio_plugin_t *p,
		int *in, int *out, unsigned frames)
{
	SPLIT_BUFFER
	{
		if(in)
			s32copy(in + s, emulbuf + s, f);
		else
			s32clear(emulbuf + s, f);
		p->process(p, emulbuf + s, f);
		s32add(emulbuf + s, out + s, f);
	}
}

static void process_m_with_process_r(struct audio_plugin_t *p,
		int *in, int *out, unsigned frames)
{
	SPLIT_BUFFER
	{
		if(in)
			p->process_r(p, in + s, emulbuf + s, f);
		else
			p->process_r(p, NULL, emulbuf + s, f);
		s32add(emulbuf + s, out + s, f);
	}
}


int audio_plugin_init(audio_plugin_t *p)
{
	int uses_emulbuf = 0;
	/*
	 * This may change, but right now, a plugin cannot
	 * generate *any* useful form of output without at
	 * least one process*() callback, so...
	 */
	if(!p->process && !p->process_r && !p->process_m)
		return -1;	/* Must have at least one! */

	if(!p->state)
		p->state = default_state;
	if(!p->control)
		p->control = default_control;
	if(!p->process)
	{
		if(p->process_r)
			p->process = process_with_process_r;
		else if(p->process_m)
		{
			p->process = process_with_process_m;
			uses_emulbuf = 1;
		}
	}
	if(!p->process_r)
	{
		if(p->process)
			p->process_r = process_r_with_process;
		else if(p->process_m)
			p->process_r = process_r_with_process_m;
	}
	if(!p->process_m)
	{
		if(p->process)
		{
			p->process_m = process_m_with_process;
			uses_emulbuf = 1;
		}
		else if(p->process_r)
		{
			p->process_m = process_m_with_process_r;
			uses_emulbuf = 1;
		}
	}
	if(uses_emulbuf)
		return init_emulbuf();
	return 0;
}


int audio_plugin_open(audio_plugin_t *p, unsigned max_frames, int fs, int quality)
{
	int res = audio_plugin_init(p);
	if(res < 0)
		return res;

	res = audio_plugin_state(p, FX_STATE_OPEN);
	if(res < 0)
		return res;

	p->control(p, FXC_MAX_FRAMES, (int)max_frames);
	p->control(p, FXC_SAMPLERATE, fs);
	p->control(p, FXC_QUALITY, quality);
	++usecount;
	return 0;
}


void audio_plugin_close(audio_plugin_t *p)
{
	audio_plugin_state(p, FX_STATE_CLOSED);
}



/*----------------------------------------------------------
	Tools for plugins (NOT for hosts!)
----------------------------------------------------------*/

int *audio_plugin_alloc_ctls(audio_plugin_t *p, unsigned count)
{
	p->controls = count;
	p->ctl = calloc(p->controls, sizeof(int));
	return p->ctl;
}
