/*(LGPL)
---------------------------------------------------------------------------
	a_delay.c - Feedback delay w/ LP filter
---------------------------------------------------------------------------
 * Copyright (C) 2001, David Olofson
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
#include "a_globals.h"
#include "a_delay.h"
#include "a_tools.h"

#define	DELAY_BUFSIZE		65536
#define	DELAY_MAX_TAPS		16
#define	DELAY_MAX_TAIL_TAPS	8

typedef struct dtap_t
{
	unsigned	delay;
	int		shift;
} dtap_t;

typedef struct delay_t
{
	int		*delaybuf;
	int		inspos;
	int		cl, cr, lpf;	/* LPF */
	int		level;
	unsigned	taps, tailtaps;
	int		ttimer;
	float		tlevel;
	dtap_t		tap[DELAY_MAX_TAPS];
	dtap_t		tailtap[DELAY_MAX_TAIL_TAPS];
} delay_t;

#define	DELAY_MASK	(DELAY_BUFSIZE-1)

#define	INTERNAL_BITS	8

static int _init(delay_t *d)
{
	d->inspos = 0;
	d->cl = 0;
	d->cr = 0;
	d->delaybuf = calloc(1, sizeof(int)*DELAY_BUFSIZE);
	if(!d->delaybuf)
		return -2;
	return 0;
}

#define	DB(x)	delaybuf[(inspos-(x)) & DELAY_MASK]
#define	TAP(n)	(DB(d->tap[n].delay) >> d->tap[n].shift)
#define	TTAP(n)	(DB(d->tailtap[n].delay) >> d->tailtap[n].shift)

#if 0
/* Stereo in, stereo out */
static void o_delay_process_mix_s(delay_t *d, int *in, int *out, unsigned frames)
{
	int *delaybuf = d->delaybuf;
	int inspos = d->inspos;
	int cl = d->cl;
	int cr = d->cr;
	unsigned s = frames;
	int outl, outr;
	int vm = d->level >> 8;
	int i;
	while(s--)
	{
		/* Feedback with LP filters */
		cl += (TTAP(0) + TTAP(3) + TTAP(4) - cl) >> 3;
		cr += (TTAP(1) + TTAP(2) + TTAP(5) - cr) >> 3;
		DB(0) = cl;
		DB(1) = cr;

		/* "Tap 0" */
		outl = cl;
		outr = cr;

		/* Taps */
		for(i = 0; i < d->taps; i += 2)
		{
			outl += TAP(i);
			outr += TAP(i+1);
		}

		/* Input */
		DB(0) += in[0];
		DB(1) += in[1];

		/* Output */
		out[0] += (outl*vm)>>8;
		out[1] += (outr*vm)>>8;

		inspos += 2;
		in += 2;
		out += 2;
	}
	d->cl = cl;
	d->cr = cr;
	d->inspos = inspos;
}
#endif

/*
 * Stereo, replacing, without level control.
 */
static void o_delay_process_r_s(delay_t *d, int *in, int *out, unsigned frames)
{
	int *delaybuf = d->delaybuf;
	int inspos = d->inspos;
	int cl = d->cl;
	int cr = d->cr;
	unsigned s = frames;
	int outl, outr;
	unsigned i;
	while(s--)
	{
		/* Tail taps */
		outl = outr = 0;
		for(i = 0; i < d->tailtaps; i += 2)
		{
			outl += TTAP(i);
			outr += TTAP(i+1);
		}

		/* LPF */
//		cl += (outl - (cl>>INTERNAL_BITS)) * d->lpf;
//		cr += (outr - (cr>>INTERNAL_BITS)) * d->lpf;
		cl += (outl - cl) * d->lpf >> INTERNAL_BITS;
		cr += (outr - cr) * d->lpf >> INTERNAL_BITS;

		/* Feedback */
		DB(0) = cl;// >> INTERNAL_BITS;
		DB(1) = cr;// >> INTERNAL_BITS;

		/* "Tap 0" */
		outl = cl;
		outr = cr;

		/* Taps */
		for(i = 0; i < d->taps; i += 2)
		{
			outl += TAP(i);
			outr += TAP(i+1);
		}

		/* Input */
		DB(0) += in[0] << INTERNAL_BITS;
		DB(1) += in[1] << INTERNAL_BITS;

		/* Output */
		out[0] = outl >> INTERNAL_BITS;
		out[1] = outr >> INTERNAL_BITS;

		inspos += 2;
		in += 2;
		out += 2;
	}
	d->cl = cl;
	d->cr = cr;
	d->inspos = inspos;
}

/*
 * Stereo, replacing in-place, without output level control
 * (for inserts)
 */
static void o_delay_process_s(delay_t *d, int *buf, unsigned frames)
{
	int *delaybuf = d->delaybuf;
	int inspos = d->inspos;
	int cl = d->cl;
	int cr = d->cr;
	int outl, outr;
	unsigned i, s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		/* Tail taps */
		outl = outr = 0;
		for(i = 0; i < d->tailtaps; i += 2)
		{
			outl += TTAP(i);
			outr += TTAP(i+1);
		}

		/* LP filters */
//		cl += (outl - (cl>>INTERNAL_BITS)) * d->lpf;
//		cr += (outr - (cr>>INTERNAL_BITS)) * d->lpf;
		cl += (outl - cl) * d->lpf >> INTERNAL_BITS;
		cr += (outr - cr) * d->lpf >> INTERNAL_BITS;

		/* Input + Feedback */
		DB(0) = cl + (buf[s] << INTERNAL_BITS);
		DB(1) = cr + (buf[s+1] << INTERNAL_BITS);

		/* "Tap 0" - the Feedback Signal */
		outl = cl;
		outr = cr;

		/* Early Reflection Taps */
		for(i = 0; i < d->taps; i += 2)
		{
			outl += TAP(i);
			outr += TAP(i+1);
		}

		/* Output */
		buf[s] = outl >> INTERNAL_BITS;
		buf[s+1] = outr >> INTERNAL_BITS;

		inspos += 2;
	}
	d->cl = cl;
	d->cr = cr;
	d->inspos = inspos;
}

/*
 * Stereo, replacing, with silent input and no level control.
 * Returns the approximate peak level of the generated output.
 */
static int o_delay_process_tail_s(delay_t *d, int *out, unsigned frames)
{
	int level = 0;
	int *delaybuf = d->delaybuf;
	int inspos = d->inspos;
	int cl = d->cl;
	int cr = d->cr;
	int outl, outr;
	unsigned i, s;
	frames <<= 1;
	for(s = 0; s < frames; s += 2)
	{
		/* Tail taps */
		outl = outr = 0;
		for(i = 0; i < d->tailtaps; i += 2)
		{
			outl += TTAP(i);
			outr += TTAP(i+1);
		}

		/* LP filters */
//		cl += (outl - (cl>>INTERNAL_BITS)) * d->lpf;
//		cr += (outr - (cr>>INTERNAL_BITS)) * d->lpf;
		cl += (outl - cl) * d->lpf >> INTERNAL_BITS;
		cr += (outr - cr) * d->lpf >> INTERNAL_BITS;

		/* Feedback (No input!) */
		DB(0) = cl;
		DB(1) = cr;

		/* "Tap 0" - the Feedback Signal */
		outl = cl;
		outr = cr;

		/* Early Reflection Taps */
		for(i = 0; i < d->taps; i += 2)
		{
			outl += TAP(i);
			outr += TAP(i+1);
		}

		/* Output */
		out[s] = outl >> INTERNAL_BITS;
		out[s+1] = outr >> INTERNAL_BITS;

		/* Level meter */
		level |= labs(outl) | labs(outr);

		inspos += 2;
	}
	d->cl = cl;
	d->cr = cr;
	d->inspos = inspos;
	return level >> INTERNAL_BITS;
}

#if 0
/* UNTESTED: Mono in, stereo out. Obviously not in-place capable! */
void o_delay_process_mix_ms(delay_t *d, int *in, int *out, unsigned frames)
{
	int *delaybuf = d->delaybuf;
	int inspos = d->inspos;
	int cl = d->cl;
	int cr = d->cr;
	int s = frames;
	int outl, outr;
	int vm = d->level >> 8;
	int i;
	while(s--)
	{
		/* Feedback with LP filters */
		cl += (TTAP(0) + TTAP(3) + TTAP(4) - cl) >> 3;
		cr += (TTAP(1) + TTAP(2) + TTAP(5) - cr) >> 3;
		DB(0) = cl;
		DB(1) = cr;

		/* "Tap 0" */
		outl = cl;
		outr = cr;

		/* Taps */
		for(i = 0; i < d->taps; i += 2)
		{
			outl += TAP(i);
			outr += TAP(i+1);
		}

		/* Input */
		DB(0) += in[0];
		DB(1) += in[0];

		/* Output */
		out[0] += (outl*vm)>>8;
		out[1] += (outr*vm)>>8;

		inspos += 2;
		in += 1;
		out += 2;
	}
	d->cl = cl;
	d->cr = cr;
	d->inspos = inspos;
}
#endif

#undef	DB
#undef	TAP


/*
 * New API
======================================================================
 */

static void _load_taps(struct audio_plugin_t *p, const float *taps)
{
	int i;
	p->ctl[DC_EARLY_TIME] = (int)(1.0 * 65536.0);
	p->ctl[DC_TAIL_TIME] = (int)(1.0 * 65536.0);
	p->ctl[DC_EARLY_LEVEL] = (int)(1.0 * 65536.0);
	p->ctl[DC_FEEDBACK] = (int)(1.0 * 65536.0);
	p->ctl[DC_LP_FILTER] = (int)(6500.0 * 65536.0);
	for(i = 0; i < DELAY_MAX_TAPS + DELAY_MAX_TAIL_TAPS; ++i)
	{
		p->ctl[DC_EARLY_TAP_1_TIME + i*2] = (int)(*taps++ * 65536.0);
		p->ctl[DC_EARLY_TAP_1_LEVEL + i*2] = (int)(*taps++ * 65536.0);
	}
}


static void _scale_early_taps(struct audio_plugin_t *p)
{
	delay_t *d = (delay_t *)p->user;
	unsigned i, t, scale;
	/* samples per ms */
	float s_ms = (float)p->ctl[FXC_SAMPLERATE] * 0.001;
	s_ms *= (float)p->ctl[DC_EARLY_TIME];	/* scale */
	s_ms *= 1.0/1024.0;	/* Throw away 10 of the 16 fraction bits */
	scale = (unsigned)s_ms;
	for(t = 0, i = 0; i < DELAY_MAX_TAPS; ++i)
	{
		unsigned delay;
		int shift;
		if(!p->ctl[DC_EARLY_TAP_1_LEVEL + i*2])
			continue;

		shift = fixp2shift(p->ctl[DC_EARLY_TAP_1_LEVEL + i*2]);
		delay = (unsigned)p->ctl[DC_EARLY_TAP_1_TIME + i*2] >> 8;
		delay *= scale;
		delay >>= 14;
		if(delay > DELAY_BUFSIZE-2)
			delay = DELAY_BUFSIZE-2;
		else if(delay < 0)
			delay = 0;
		delay &= (DELAY_MASK-1);
		delay |= t & 1;		/* L->L, R->R, L->L, R->R,... */
		d->tap[t].delay = delay;
		d->tap[t].shift = shift;
		++t;
	}
	d->taps = t;
}


static void _scale_tail_taps(struct audio_plugin_t *p)
{
	delay_t *d = (delay_t *)p->user;
	unsigned i, t, scale;
	/* samples per ms */
	float s_ms = (float)p->ctl[FXC_SAMPLERATE] * 0.001;
	s_ms *= (float)p->ctl[DC_TAIL_TIME];	/* scale */
	s_ms *= 1.0/1024.0;	/* Throw away 10 of the 16 fraction bits */
	scale = (unsigned)s_ms;
	for(t = 0, i = 0; i < DELAY_MAX_TAIL_TAPS; ++i)
	{
		unsigned delay;
		int shift;
		if(!p->ctl[DC_TAIL_TAP_1_LEVEL + i*2])
			continue;

		shift = fixp2shift(p->ctl[DC_TAIL_TAP_1_LEVEL + i*2]);
		delay = (unsigned)p->ctl[DC_TAIL_TAP_1_TIME + i*2] >> 8;
		delay *= scale;
		delay >>= 14;
		if(delay > DELAY_BUFSIZE-2)
			delay = DELAY_BUFSIZE-2;
		delay &= (DELAY_MASK-1);
		delay |= (t>>1) & 1;	/* L->L, L->R, R->L, R->R,... */
		d->tailtap[t].delay = delay;
		d->tailtap[t].shift = shift;
		++t;
	}
	d->tailtaps = t;
}


static inline void __calc_filters(audio_plugin_t *p)
{
	delay_t *d = (delay_t *)p->user;
	d->lpf = p->ctl[DC_LP_FILTER] /
			p->ctl[FXC_SAMPLERATE] >> (16-INTERNAL_BITS);
	if(d->lpf > (1<<(16-INTERNAL_BITS)))
		d->lpf = (1<<(16-INTERNAL_BITS));
}


static const float default_taps[(DELAY_MAX_TAPS + DELAY_MAX_TAIL_TAPS)*2] = {
	/* Early Reflection Taps */
	17, 1.0/4,	31, 1.0/4,
	87, 1.0/4,	179, 1.0/4,
	379, 1.0/16,	246, 1.0/16,
	0, 0,		0, 0,

	0, 0,		0, 0,
	0, 0,		0, 0,
	0, 0,		0, 0,
	0, 0,		0, 0,

	/* Tail Feedback Taps */
	552, 1.0/4,	642, 1.0/4,
	883, 1.0/4,	851, 1.0/4,
	1204, 1.0/4,	1176, 1.0/4,
	0, 0,		0, 0
};


static int delay_state(struct audio_plugin_t *p, audio_fxstates_t new_state)
{
	delay_t *d;
	if(new_state > p->current_state)
		switch(new_state)
		{
		  case FX_STATE_CLOSED:	/* Can't happen. */
		  case FX_STATE_OPEN:
			if(!audio_plugin_alloc_ctls(p, DC_COUNT))
				return -1;
			d = calloc(1, sizeof(delay_t));
			if(!d)
				return -2;
			p->user = d;
			/*_load_taps(d, default_taps, p->ctl[FXC_SAMPLERATE]);*/
			_load_taps(p, default_taps);
			break;
		  case FX_STATE_READY:
			d = (delay_t *)p->user;
			if(_init(d) < 0)
				return -2;
			_scale_early_taps(p);
			_scale_tail_taps(p);
			__calc_filters(p);
			break;
		  case FX_STATE_PAUSED:
		  case FX_STATE_RUNNING:
		  case FX_STATE_SILENT:
		  case FX_STATE_RESTING:
			p->current_state = FX_STATE_RESTING;
			break;
		}
	else
		switch(new_state)
		{
		  case FX_STATE_CLOSED:
			free(p->user);
			p->user = NULL;
			break;
		  case FX_STATE_OPEN:
			d = (delay_t *)p->user;
			free(d->delaybuf);
			d->delaybuf = NULL;
			break;
		  case FX_STATE_READY:
			break;
		  case FX_STATE_PAUSED:
		  case FX_STATE_RUNNING:	/* Can't happen. */
		  case FX_STATE_SILENT:		/* Can't happen. */
		  case FX_STATE_RESTING:	/* Can't happen. */
			break;
		}
	return 0;
}


static void delay_control(struct audio_plugin_t *p, unsigned ctl, int arg)
{
	/* 1: Early reflections duration
	 * 2: Tail duration
	 * 3: Early reflections level
	 * 4: Feedback Level
	 * 5: Feedback LPF Cutoff
	 * 6: (Unused)
	 */
	p->ctl[ctl] = arg;
	switch(ctl)
	{
	  case DC_EARLY_TIME:
		_scale_early_taps(p);
		break;
	  case DC_TAIL_TIME:
		_scale_tail_taps(p);
		break;
	  case DC_EARLY_LEVEL:
	  case DC_FEEDBACK:
		break;
	  case DC_LP_FILTER:
		__calc_filters(p);
		break;
	}
}


static void delay_process(struct audio_plugin_t *p, int *buf, unsigned frames)
{
	delay_t *d = (delay_t *)p->user;
	d->tlevel = 1000;
	d->ttimer = 0;
	p->current_state = FX_STATE_RUNNING;
	o_delay_process_s(d, buf, frames);
}


static void delay_process_r(struct audio_plugin_t *p, int *in, int *out, unsigned frames)
{
	delay_t *d = (delay_t *)p->user;
	if(in)
	{
		d->tlevel = 1000;
		d->ttimer = 0;
		p->current_state = FX_STATE_RUNNING;
		o_delay_process_r_s(d, in, out, frames);
	}
	else
	{
		int level;
		if(FX_STATE_RESTING == p->current_state)
			return;
		level = o_delay_process_tail_s(d, out, frames);
		d->tlevel += (float)((level - d->tlevel) * frames) /
				(float)(p->ctl[FXC_SAMPLERATE] * 0.1);
		d->ttimer += frames;
		if(d->ttimer < p->ctl[FXC_SAMPLERATE] * 2)
			return;
		if(d->tlevel < 5.0)
		{
			d->tlevel = 1000;
			p->current_state = FX_STATE_RESTING;
			return;
		}
	}
}


void delay_init(struct audio_plugin_t *p)
{
	p->state = delay_state;
	p->control = delay_control;
	p->process = delay_process;
	p->process_r = delay_process_r;
}
