/*(LGPL)
---------------------------------------------------------------------------
	a_wca.c - WCA, the Wave Construction API
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

/* # of frames to process per loop in most functions */
#define BLOCK_FRAMES			64

#define MAX_SPECTRUM_OSCILLATORS	128

#define	ONEDIV32K	3.0517578125e-5
#define	ONEDIV65K	1.52587890625e-5

#include <stdlib.h>
#include <string.h>

#include "kobolog.h"
#include "a_wca.h"
#include "a_math.h"


/*----------------------------------------------------------
	Framework for Buffer Based Processing
----------------------------------------------------------*/

/*
 * Parameters
 */
static audio_wave_t *s_w = NULL;	/* Target waveform */
static int s_stereo = 0;		/* 1 if waveform is stereo */
static float s_fs = 44100.0f;		/* Target sample rate (Hz) */
static float s_dt = 1.0f/44100.0f;	/* Target delta time (s) */
/*
 * State
 */

/*
 * IMPORTANT:	These two MUST NOT, under any circumstances,
 *		be allowed to increment beyond s_w->samples!
 *		If they do, and you're use this framework
 *		stuff, all hell will break lose.
 *
 * (One could use signs instead, but what's the point?
 * You have no business outside waveforms anyway.)
 */
static unsigned s_rpos = 0;		/* Current target read position */
static unsigned s_wpos = 0;		/* Current target write position */


static void _init_processing(audio_wave_t *w)
{
	s_w = w;

	s_fs = (float)w->rate;
	s_dt = 1.0f / s_fs;

	s_rpos = s_wpos = 0;

	switch(s_w->format)
	{
	  case AF_STEREO8:
	  case AF_STEREO16:
	  case AF_STEREO32:
		s_stereo = 1;
		break;
	  case AF_MONO8:
	  case AF_MONO16:
	  case AF_MONO32:
	  case AF_MIDI:
		s_stereo = 0;
		break;
	}
}


/*
 * Returns the number of frames left to process, if 'pos'
 * is the current position. If there are more than 'limit'
 * frames left to process, 'limit' is returned.
 */
static inline unsigned _next_block(unsigned pos, unsigned limit)
{
	unsigned frames;
	frames = s_w->samples - (pos >> s_stereo);
	if(frames > limit)
		return limit;
	else
		return frames;
}


/*----------------------------------------------------------
	Internal Toolkit
----------------------------------------------------------*/

/*
 * NOTE:
 *	These work with floats in the 0 dB range [-32768, 32767],
 *	regardless of waveform format.
 */

static inline void read_sample(audio_wave_t *w, unsigned s, float *inl, float *inr)
{
	switch(w->format)
	{
	  case AF_MONO8:
		*inr = *inl = (float)(w->data.si8[s]<<8);
		break;
	  case AF_STEREO8:
		*inl = (float)(w->data.si8[s*2]<<8);
		*inr = (float)(w->data.si8[s*2+1]<<8);
		break;
	  case AF_MONO16:
		*inr = *inl = (float)w->data.si16[s];
		break;
	  case AF_STEREO16:
		*inl = (float)w->data.si16[s*2];
		*inr = (float)w->data.si16[s*2+1];
		break;
	  case AF_MONO32:
		*inr = *inl = w->data.f32[s] * 32768.0f;
		break;
	  case AF_STEREO32:
		*inl = w->data.f32[s*2] * 32768.0f;
		*inr = w->data.f32[s*2+1] * 32768.0f;
		break;
	  case AF_MIDI:
		*inr = *inl = *inr = 0;
		break;
	}
}


static inline void write_sample(audio_wave_t *w, unsigned s, float outl, float outr)
{
	if(outl < -32768.0f)
		outl = -32768.0f;
	else if(outl > 32767.0f)
		outl = 32767.0f;
	if(outr < -32768.0f)
		outr = -32768.0f;
	else if(outr > 32767.0f)
		outr = 32767.0f;

	switch(w->format)
	{
	  case AF_MONO8:
		w->data.si8[s] = (int)outl >> 8;
		break;
	  case AF_STEREO8:
		w->data.si8[s*2] = (int)outl >> 8;
		w->data.si8[s*2+1] = (int)outr >> 8;
		break;
	  case AF_MONO16:
		w->data.si16[s] = (Sint16)outl;
		break;
	  case AF_STEREO16:
		w->data.si16[s*2] = (Sint16)outl;
		w->data.si16[s*2+1] = (Sint16)outr;
		break;
	  case AF_MONO32:
		w->data.f32[s] = outl * ONEDIV32K;
		break;
	  case AF_STEREO32:
		w->data.f32[s*2] = outl * ONEDIV32K;
		w->data.f32[s*2+1] = outr * ONEDIV32K;
		break;
	  case AF_MIDI:
		break;
	}
}


static inline void add_sample(audio_wave_t *w, unsigned s, float outl, float outr)
{
	float l, r;
	switch(w->format)
	{
	  case AF_MONO8:
		l = (float)(w->data.si8[s]<<8) + outl;
		if(l > 32767.0f)
			w->data.si8[s] = 127;
		else if(l < -32768.0f)
			w->data.si8[s] = -128;
		else
			w->data.si8[s] = (int)l >> 8;
		break;
	  case AF_STEREO8:
		s <<= 1;
		l = (float)(w->data.si8[s]<<8) + outl;
		if(l > 32767.0f)
			w->data.si8[s] = 127;
		else if(l < -32768.0f)
			w->data.si8[s] = -128;
		else
			w->data.si8[s] = (int)l >> 8;
		++s;
		r = (float)(w->data.si8[s]<<8) + outr;
		if(r > 32767.0f)
			w->data.si8[s] = 127;
		else if(r < -32768.0f)
			w->data.si8[s] = -128;
		else
			w->data.si8[s] = (int)r >> 8;
		break;
	  case AF_MONO16:
		l = (float)w->data.si16[s] + outl;
		if(l > 32767.0f)
			w->data.si16[s] = 32767;
		else if(l < -32768.0f)
			w->data.si16[s] = -32768;
		else
			w->data.si16[s] = (Sint16)l;
		break;
	  case AF_STEREO16:
		s <<= 1;
		l = (float)w->data.si16[s] + outl;
		if(l > 32767.0f)
			w->data.si16[s] = 32767;
		else if(l < -32768.0f)
			w->data.si16[s] = -32768;
		else
			w->data.si16[s] = (Sint16)l;
		++s;
		r = (float)w->data.si16[s] + outr;
		if(r > 32767.0f)
			w->data.si16[s] = 32767;
		else if(r < -32768.0f)
			w->data.si16[s] = -32768;
		else
			w->data.si16[s] = (Sint16)r;
		break;
	  case AF_MONO32:
		w->data.f32[s] += outl * ONEDIV32K;
		break;
	  case AF_STEREO32:
		w->data.f32[s*2] += outl * ONEDIV32K;
		w->data.f32[s*2+1] += outr * ONEDIV32K;
		break;
	  case AF_MIDI:
		break;
	}
}


/*
 * Block based versions
 */

/*
 * Read 'frames' samples into the array(s).
 *
 * Only 'l' is used on mono waveforms!
 * Reading past the end of the waveform is NOT ALLOWED.
 *
 * Upon returning, s_rpos will index the first sample after the read block.
 */
static void read_samples(float *inl, float *inr, unsigned frames)
{
	unsigned wp = 0;
	switch(s_w->format)
	{
	  case AF_MONO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(wp < frames)
			inl[wp++] = (float)(d[s_rpos++]<<8);
		break;
	  }
	  case AF_STEREO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(wp < frames)
		{
			inl[wp] = (float)(d[s_rpos++]<<8);
			inr[wp] = (float)(d[s_rpos++]<<8);
			++wp;
		}
		break;
	  }
	  case AF_MONO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(wp < frames)
			inl[wp++] = (float)(d[s_rpos++]);
		break;
	  }
	  case AF_STEREO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(wp < frames)
		{
			inl[wp] = (float)(d[s_rpos++]);
			inr[wp] = (float)(d[s_rpos++]);
			++wp;
		}
		break;
	  }
	  case AF_MONO32:
	  {
		float *d = s_w->data.f32;
		while(wp < frames)
			inl[wp++] = d[s_rpos++] * 32768.0f;
		break;
	  }
	  case AF_STEREO32:
	  {
		float *d = s_w->data.f32;
		while(wp < frames)
		{
			inl[wp] = d[s_rpos++] * 32768.0f;
			inr[wp] = d[s_rpos++] * 32768.0f;
			++wp;
		}
		break;
	  }
	  case AF_MIDI:
		break;
	}
}


/*
 * Write 'frames' samples from the array(s).
 *
 * Only 'l' is used on mono waveforms!
 * Writing past the end of the waveform is NOT ALLOWED.
 * Data is clipped to the limits of the waveform data format.
 *
 * Upon returning, s_wpos will index the first sample after the written block.
 */
#define __CLIP(s)		\
	if(s < -32768.0f)	\
		s = -32768.0f;	\
	else if(s > 32767.0f)	\
		s = 32767.0f;
static void write_samples(float *outl, float *outr, unsigned frames)
{
	unsigned rp = 0;
	switch(s_w->format)
	{
	  case AF_MONO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(rp < frames)
		{
			float l = outl[rp++];
			__CLIP(l)
			d[s_wpos++] = (int)l >> 8;
		}
		break;
	  }
	  case AF_STEREO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(rp < frames)
		{
			float l = outl[rp];
			float r = outr[rp];
			__CLIP(l)
			__CLIP(r)
			d[s_wpos++] = (int)l >> 8;
			d[s_wpos++] = (int)r >> 8;
			++rp;
		}
		break;
	  }
	  case AF_MONO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(rp < frames)
		{
			float l = outl[rp++];
			__CLIP(l)
			d[s_wpos++] = (Sint16)l;
		}
		break;
	  }
	  case AF_STEREO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(rp < frames)
		{
			float l = outl[rp];
			float r = outr[rp];
			__CLIP(l)
			__CLIP(r)
			d[s_wpos++] = (Sint16)l;
			d[s_wpos++] = (Sint16)r;
			++rp;
		}
		break;
	  }
	  case AF_MONO32:
	  {
		float *d = s_w->data.f32;
		while(rp < frames)
			d[s_wpos++] = outl[rp++] * ONEDIV32K;
		break;
	  }
	  case AF_STEREO32:
	  {
		float *d = s_w->data.f32;
		while(rp < frames)
		{
			d[s_wpos++] = outl[rp] * ONEDIV32K;
			d[s_wpos++] = outl[rp] * ONEDIV32K;
			++rp;
		}
		break;
	  }
	  case AF_MIDI:
		break;
	}
}

#if 0
/*
 * Add 'frames' samples from the array(s) to the waveform contents.
 *
 * Only 'l' is used on mono waveforms!
 * Working past the end of the waveform is NOT ALLOWED.
 * Resulting data is clipped to the limits of the waveform data format.
 *
 * Upon returning, s_wpos will index the first sample after the written block.
 */
static void add_samples(float *outl, float *outr, unsigned frames)
{
	unsigned rp = 0;
	switch(s_w->format)
	{
	  case AF_MONO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(rp < frames)
		{
			float l = outl[rp++] + (float)(d[s_wpos]<<8);
			__CLIP(l)
			d[s_wpos++] = (int)l >> 8;
		}
		break;
	  }
	  case AF_STEREO8:
	  {
		Sint8 *d = s_w->data.si8;
		while(rp < frames)
		{
			float l = outl[rp] + (float)(d[s_wpos]<<8);
			float r = outr[rp] + (float)(d[s_wpos+1]<<8);
			__CLIP(l)
			__CLIP(r)
			d[s_wpos++] = (int)l >> 8;
			d[s_wpos++] = (int)r >> 8;
			++rp;
		}
		break;
	  }
	  case AF_MONO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(rp < frames)
		{
			float l = outl[rp++] + (float)d[s_wpos];
			__CLIP(l)
			d[s_wpos++] = (Sint16)l;
		}
		break;
	  }
	  case AF_STEREO16:
	  {
		Sint16 *d = s_w->data.si16;
		while(rp < frames)
		{
			float l = outl[rp] + (float)d[s_wpos];
			float r = outr[rp] + (float)d[s_wpos+1];
			__CLIP(l)
			__CLIP(r)
			d[s_wpos++] = (Sint16)l;
			d[s_wpos++] = (Sint16)r;
			++rp;
		}
		break;
	  }
	  case AF_MONO32:
	  {
		float *d = s_w->data.f32;
		while(rp < frames)
			d[s_wpos++] += outl[rp++] * ONEDIV32K;
		break;
	  }
	  case AF_STEREO32:
	  {
		float *d = s_w->data.f32;
		while(rp < frames)
		{
			d[s_wpos++] += outl[rp] * ONEDIV32K;
			d[s_wpos++] += outl[rp] * ONEDIV32K;
			++rp;
		}
		break;
	  }
	  case AF_MIDI:
		break;
	}
}
#endif
#undef __CLIP


/*----------------------------------------------------------
	The WCA calls
----------------------------------------------------------*/


void wca_reset(void)
{
	int i;
	for(i = 0; i < _WCA_MODTARGETS; ++i)
		wca_mod_reset(i);
	wca_val(WCA_AMPLITUDE, 1.0f);
	wca_val(WCA_FREQUENCY, 100.0f);
	wca_val(WCA_LIMIT, 100000.0f);
}


/*
 * Simple envelope generator.
TODO: This could use some serious optimizations...
 */

typedef struct modulator_t
{
	/* Parameters */
	unsigned	steps;			/* # of sections */
	float		v[WCA_MAX_ENV_STEPS];	/* target value */
	float		t[WCA_MAX_ENV_STEPS];	/* section start time */
	float		d[WCA_MAX_ENV_STEPS];	/* duration of section */
	float		mod_f, mod_a, mod_d;	/* Modulation component */

	/* State */
	unsigned	step;		/* Current section */
	unsigned	done;		/* samples of current section done */
	unsigned	remain;		/* samples left of current section */
} modulator_t;

void _env_reset(modulator_t *e)
{
	e->steps = 0;
	e->v[0] = e->t[0] = e->d[0] = 0.0f;
}

void _env_add(modulator_t *e, float duration, float v)
{
	if(e->steps >= WCA_MAX_ENV_STEPS)
	{
		log_printf(ELOG, "audio: Envelope overflow!\n");
		return;
	}
	e->v[e->steps] = v;
	e->d[e->steps] = duration;
	if(e->steps)
		e->t[e->steps] = e->t[e->steps-1] + e->d[e->steps-1];
	else
		e->t[e->steps] = 0.0f;
	++e->steps;
}

#if 0
/*
 * Dog slow sample-by-sample API. (KILLME)
 */
static inline float _env_output(modulator_t *e, float t)
{
	float output, w;
	int step = 0;
	while(step < e->steps)
		if(e->t[step] + e->d[step] > t)
			break;
		else
			++step;
	if(step >= e->steps)
		output = e->v[e->steps-1];
	else if(0 == step)
		output = e->v[0] * t / e->d[0];
	else
	{
		float ip = (t - e->t[step]) / e->d[step];
		output = e->v[step - 1] * (1.0f - ip) + e->v[step] * ip;
	}

	w = t * e->mod_f * 2.0f * M_PI;
	output *= 1.0f + sin(w) * e->mod_d;
	output += sin(w) * e->mod_a;
	return output;
}
#endif

/*
 * New block based interface
 */

/* Initialize modulator 'e' for block based processing. */
static void _env_start(modulator_t *e)
{
	e->step = 0;
	e->remain = e->d[0] * s_fs;
	e->done = 0;
}

/* Generate 'frame' samples of output from modulator 'e'. */
static void _env_process(modulator_t *e, float *out, unsigned frames)
{
	while(frames)
	{
		unsigned i = 0;
		unsigned frag;
		float begv, endv, dv, ip, t;

		if(e->step >= e->steps)
		{
			/* Beyond the end ==> flat forever */
			if(0 == e->steps)
				endv = 0.0;
			else
				endv = e->v[e->steps-1];
			for(; i < frames; ++i)
				out[i] = endv;
			return;
		}

		frag = frames < e->remain ? frames : e->remain;

		if(frag)
		{
			if(0 == e->step)
			{
				/* First section */
				begv = 0.0f;
				endv = e->v[0];
			}
			else
			{
				/* All other sections */
				begv = e->v[e->step - 1];
				endv = e->v[e->step];
			}

			t = (float)e->done * s_dt;
			dv = (endv - begv) / e->d[e->step] * s_dt;
			ip = t / e->d[e->step];
			begv = endv * ip + begv * (1.0f - ip);
			while(i < frag)
			{
				out[i++] = begv;
				begv += dv;
			}

			out += frag;
			frames -= frag;
			e->done += frag;
			e->remain -= frag;
		}

		if(!e->remain)
		{
			/* Next section! */
			++e->step;
			if(e->step < e->steps)
			{
/*
FIXME: This rounds the start of each section to the nearest sample.
FIXME: Normally, that wouldn't be an issue (although a proper band
FIXME: limited rendition of envelopes would be nice), but here, the
FIXME: errors will add up! This might matter with lots of sections
FIXME: and/or low sample rates.
*/
				e->remain = e->d[e->step] * s_fs;
				e->done = 0;
			}
			/*
			 * NOTE:
			 *	e->step does the whole job in the 'else'
			 *	case, so we don't have to set the others.
			 */
		}
	}
}


/*
 * Global envelope generators.
 */
static modulator_t env[_WCA_MODTARGETS];


static void _env_start_all(void)
{
	int i;
	for(i = 0; i < _WCA_MODTARGETS; ++i)
		_env_start(env + i);
}


void wca_mod_reset(wca_modtargets_t target)
{
	if(target < 0)
		return;
	if(target >= _WCA_MODTARGETS)
		return;
	_env_reset(&env[target]);
	wca_mod(target, 0, 0, 0);
}


void wca_env(wca_modtargets_t target, float duration, float v)
{
	if(target < 0)
		return;
	if(target >= _WCA_MODTARGETS)
		return;
	_env_add(&env[target], duration, v);
}

void wca_mod(wca_modtargets_t target, float frequency,
		float amplitude, float depth)
{
	if(target < 0)
		return;
	if(target >= _WCA_MODTARGETS)
		return;
	env[target].mod_f = frequency;
	env[target].mod_a = amplitude;
	env[target].mod_d = depth;
}


void wca_val(wca_modtargets_t target, float v)
{
	wca_mod_reset(target);
	wca_env(target, 0, v);
	wca_mod(target, 0, 0, 0);
}


#include "a_wcaosc.h"

void wca_osc(int wid, wca_waveform_t wf, wca_mixmodes_t mm)
{
	unsigned s, frames;
	char sync[BLOCK_FRAMES];
	float olev = 1.0f;
	float nyqvist = s_fs * 0.5f;

	audio_wave_t *wave = audio_wave_get(wid);
	if(!wave)
		return;

	_init_processing(wave);
	_env_start_all();
	noise_reset();
	osc_w = 0.0;
	osc_yit = 0.0f;
	noise_out = 0.0f;

	switch(mm)
	{
	  case WCA_ADD:
	  case WCA_MUL:
	  case WCA_FM:
	  case WCA_FM_ADD:
		memset(sync, 0, sizeof(sync));
		break;
	  case WCA_SYNC:
	  case WCA_SYNC_ADD:
		break;
	}

	while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
	{
		float inl[BLOCK_FRAMES];
		float inr[BLOCK_FRAMES];

		float a[BLOCK_FRAMES];
		float bal[BLOCK_FRAMES];
		float f[BLOCK_FRAMES];
		float limit[BLOCK_FRAMES];
		float mod1[BLOCK_FRAMES];
		float mod2[BLOCK_FRAMES];
		float mod3[BLOCK_FRAMES];

		float out[BLOCK_FRAMES];

		_env_process(&env[WCA_AMPLITUDE], a, frames);
		_env_process(&env[WCA_BALANCE], bal, frames);
		_env_process(&env[WCA_FREQUENCY], f, frames);
		_env_process(&env[WCA_LIMIT], limit, frames);
		_env_process(&env[WCA_MOD1], mod1, frames);
		_env_process(&env[WCA_MOD2], mod2, frames);
		_env_process(&env[WCA_MOD3], mod3, frames);

		for(s = 0; s < frames; ++s)
			if(limit[s] > nyqvist)
				limit[s] = nyqvist;

		read_samples(inl, inr, frames);

		/* Handle FM and SYNC modes*/
		switch(mm)
		{
		  case WCA_ADD:
		  case WCA_MUL:
			break;
		  case WCA_FM:
		  case WCA_FM_ADD:
			if(s_stereo)
				for(s = 0; s < frames; ++s)
					f[s] *= 1.0f + (inl[s] + inr[s]) *
							ONEDIV65K * bal[s];
			else
				for(s = 0; s < frames; ++s)
					f[s] *= 1.0f + inl[s] *
							ONEDIV65K * bal[s];
			break;
		  case WCA_SYNC:
		  case WCA_SYNC_ADD:
/*
FIXME: Storing retrig points as a list of "timestamps" would probably
FIXME: be more efficient than this per-sample array hack...
FIXME: More importantly, that can provide sub-sample accurate sync
FIXME: timing. Fractional timing would have to be derived by looking
FIXME: at the samples before and after each zero crossing.
*/
			if(s_stereo)
				for(s = 0; s < frames; ++s)
				{
					float lev = inl[s] + inr[s];
					sync[s] = (olev > 0.0f) &&
							(lev < 0.0f);
					olev = lev;
				}
			else
				for(s = 0; s < frames; ++s)
				{
					float lev = inl[s];
					sync[s] = (olev > 0.0f) &&
							(lev < 0.0f);
					olev = lev;
				}
			break;
		}

		/* Oscillators! */
		switch(wf)
		{
		  case WCA_DC:
			for(s = 0; s < frames; ++s)
				out[s] = 1.0f;
			break;
		  case WCA_SINE:
			_osc_sine(sync, f, mod1, out, frames);
			break;
		  case WCA_HALFSINE:
			_osc_halfsine(sync, f, mod1, out, frames);
			break;
		  case WCA_RECTSINE:
			_osc_rectsine(sync, f, mod1, out, frames);
			break;
		  case WCA_PULSE:
			_osc_pulse(sync, f, mod1, out, frames);
			break;
		  case WCA_TRIANGLE:
			_osc_triangle(sync, f, mod1, out, frames);
			break;
		  case WCA_SINEMORPH:
			_osc_sinemorph(sync, f, mod1, mod2, limit, out, frames);
			break;
		  case WCA_BLMORPH:
			_osc_blmorph(sync, f, mod1, mod2, mod3, limit,
					out, frames);
			break;
		  case WCA_BLCROSS:
			_osc_blcross(sync, f, mod1, mod2, mod3, limit,
					out, frames);
			break;
		  case WCA_NOISE:
			_osc_noise(sync, f, out, frames);
			break;
		  case WCA_SPECTRUM:
			_osc_spectrum(sync, f, mod1, mod2, limit, out, frames);
			break;
		  case WCA_ASPECTRUM:
			_osc_aspectrum(sync, f, mod1, mod2, limit, out, frames);
			break;
		  case WCA_HSPECTRUM:
			_osc_hspectrum(sync, f, mod1, mod2, mod3, limit,
					out, frames);
			break;
		  case WCA_AHSPECTRUM:
			_osc_ahspectrum(sync, f, mod1, mod2, mod3, limit,
					out, frames);
			break;
		}

		/* Output */
		switch(mm)
		{
		  case WCA_ADD:
		  case WCA_FM_ADD:
		  case WCA_SYNC_ADD:
			if(s_stereo)
				for(s = 0; s < frames; ++s)
				{
					float sout = out[s] * a[s] * 32767.0f;
					inl[s] += sout;
					inr[s] += sout;
				}
			else
				for(s = 0; s < frames; ++s)
				{
					float sout = out[s] * a[s] * 32767.0f;
					inl[s] += sout;
				}
			break;
		  case WCA_MUL:
			if(s_stereo)
				for(s = 0; s < frames; ++s)
				{
					float sout = inl[s] * out[s] * 0.5f;
					sout *= bal[s];
					sout *= a[s];
					inl[s] = inl[s] * (1.0f - bal[s]) + sout;
					inr[s] = inr[s] * (1.0f - bal[s]) + sout;
				}
			else
				for(s = 0; s < frames; ++s)
				{
					float sout = inl[s] * out[s] * 0.5f;
					sout *= bal[s];
					sout *= a[s];
					inl[s] = inl[s] * (1.0f - bal[s]) + sout;
				}
			break;
		  case WCA_FM:
		  case WCA_SYNC:
			if(s_stereo)
				for(s = 0; s < frames; ++s)
				{
					float sout = out[s] * a[s] * 32767.0f;
					inl[s] = sout;
					inr[s] = sout;
				}
			else
				for(s = 0; s < frames; ++s)
				{
					float sout = out[s] * a[s] * 32767.0f;
					inl[s] = sout;
				}
			break;
		}
		write_samples(inl, inr, frames);
	}
}


void wca_filter(int wid, wca_filtertype_t ft)
{
	unsigned s, frames;
	float ll = 0.0f, bl = 0.0f, hl = 0.0f;
	float lr = 0.0f, br = 0.0f, hr = 0.0f;
	float d1l = 0.0f;
	float d1r = 0.0f;
	audio_wave_t *wave = audio_wave_get(wid);
	if(!wave)
		return;

	switch(ft)
	{
	  case WCA_ALLPASS:
		return;
	  default:
		break;
	}

	_init_processing(wave);
	_env_start_all();

	while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
	{
		float f[BLOCK_FRAMES];
		float q[BLOCK_FRAMES];
		float l[BLOCK_FRAMES];
		float r[BLOCK_FRAMES];
		float amp[BLOCK_FRAMES];
		float fe[BLOCK_FRAMES];
		float mod1[BLOCK_FRAMES];

		_env_process(&env[WCA_AMPLITUDE], amp, frames);
		_env_process(&env[WCA_FREQUENCY], fe, frames);
		_env_process(&env[WCA_MOD1], mod1, frames);

		read_samples(l, r, frames);

		/* Generate f and q buffers */
		switch(ft)
		{
		  case WCA_ALLPASS:
		  case WCA_LOWPASS_6DB:
		  case WCA_HIGHPASS_6DB:
			for(s = 0; s < frames; ++s)
				if(fe[s] > s_fs)
					f[s] = 1.0f;
				else
					f[s] = fe[s] * s_dt;
			break;
		  case WCA_LOWPASS_12DB:
		  case WCA_HIGHPASS_12DB:
		  case WCA_BANDPASS_12DB:
		  case WCA_NOTCH_12DB:
		  case WCA_PEAK_12DB:
			for(s = 0; s < frames; ++s)
			{
				float qlim;
				/*
				 * Here we have some safety limits to keep the
				 * filter from blowing up...
				 */
				if(fe[s] > s_fs * 0.5f)
					fe[s] = s_fs * 0.5f;
				f[s] = 2.0f * sin(M_PI * fe[s] * s_dt * 0.5f);
				q[s] = 1.0f / amp[s];
				if(q[s] > 1.0f)
					q[s] = 1.0f;

				qlim = s_fs / fe[s];
				if(qlim < 5.0f)
				{
					qlim *= qlim * qlim;
					qlim /= 125.0f;
					if(q[s] > qlim)
						q[s] = qlim;
				}
			}
			break;
		}

		/* Perform the actual filtering */
		switch(ft)
		{
		  case WCA_ALLPASS:
		  case WCA_LOWPASS_6DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
			  	d1r += (r[s] - d1r) * f[s];
				r[s] = r[s] * mod1[s] + d1r * (1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
			  	d1l += (l[s] - d1l) * f[s];
				l[s] = l[s] * mod1[s] + d1l * (1.0f - mod1[s]);
			}
			break;
		  case WCA_HIGHPASS_6DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
			  	d1r += (r[s] - d1r) * f[s];
				r[s] = r[s] * mod1[s] + (r[s] - d1r) *
						(1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
			  	d1l += (l[s] - d1l) * f[s];
				l[s] = l[s] * mod1[s] + (l[s] - d1l) *
						(1.0f - mod1[s]);
			}
			break;
		  case WCA_LOWPASS_12DB:
			/*
			 * 2x oversampling - although this quick hack
			 * performs no input interpolation, and just
			 * drops every other output sample.
			 */
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				r[s] = r[s] * mod1[s] + lr * (1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				l[s] = l[s] * mod1[s] + ll * (1.0f - mod1[s]);
			}
			break;
		  case WCA_HIGHPASS_12DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				r[s] = r[s] * mod1[s] + hr * (1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				l[s] = l[s] * mod1[s] + hl * (1.0f - mod1[s]);
			}
			break;
		  case WCA_BANDPASS_12DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				r[s] = r[s] * mod1[s] + br * (1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				l[s] = l[s] * mod1[s] + bl * (1.0f - mod1[s]);
			}
			break;
		  case WCA_NOTCH_12DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				r[s] = r[s] * mod1[s] + (lr + hr) *
						(1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				l[s] = l[s] * mod1[s] + (ll + hl) *
						(1.0f - mod1[s]);
			}
			break;
		  case WCA_PEAK_12DB:
			if(s_stereo) for(s = 0; s < frames; ++s)
			{
				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				lr += f[s]*br;
				hr = r[s] - lr - q[s]*br;
				br += f[s]*hr;

				r[s] = r[s] * mod1[s] + (lr + hr + br) *
						(1.0f - mod1[s]);
			}
			for(s = 0; s < frames; ++s)
			{
				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				ll += f[s]*bl;
				hl = l[s] - ll - q[s]*bl;
				bl += f[s]*hl;

				l[s] = l[s] * mod1[s] + (ll + hl + bl) *
						(1.0f - mod1[s]);
			}
			break;
		}

		write_samples(l, r, frames);
	}
}


void wca_gain(int wid)
{
	unsigned s, frames;
	float a[BLOCK_FRAMES];
	audio_wave_t *wave = audio_wave_get(wid);
	if(!wave)
		return;

	_init_processing(wave);
	_env_start_all();

	switch(wave->format)
	{
	  case AF_MIDI:
		return;
	  case AF_STEREO32:
	  {
		float *d = wave->data.f32;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
			{
				d[s_wpos++] *= a[s];
				d[s_wpos++] *= a[s];
			}
		}
		break;
	  }
	  case AF_MONO32:
	  {
		float *d = wave->data.f32;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
				d[s_wpos++] *= a[s];
		}
		break;
	  }
	  case AF_STEREO16:
	  {
		Sint16 *d = wave->data.si16;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
			{
				float r = (float)d[s_wpos] * a[s];
				if(r > 32767.0)
					d[s_wpos] = 32767;
				else if(r < -32768.0)
					d[s_wpos] = -32768;
				else
					d[s_wpos] = (Sint16)r;
				++s_wpos;

				r = (float)d[s_wpos] * a[s];
				if(r > 32767.0)
					d[s_wpos] = 32767;
				else if(r < -32768.0)
					d[s_wpos] = -32768;
				else
					d[s_wpos] = (Sint16)r;
				++s_wpos;
			}
		}
		break;
	  }
	  case AF_MONO16:
	  {
		Sint16 *d = wave->data.si16;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
			{
				float r = (float)d[s_wpos] * a[s];
				if(r > 32767.0)
					d[s_wpos] = 32767;
				else if(r < -32768.0)
					d[s_wpos] = -32768;
				else
					d[s_wpos] = (Sint16)r;
				++s_wpos;
			}
		}
		break;
	  }
	  case AF_STEREO8:
	  {
		Sint8 *d = wave->data.si8;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
			{
				float r = (float)d[s_wpos] * a[s];
				if(r > 127.0f)
					d[s_wpos] = 127;
				else if(r < -128.0f)
					d[s_wpos] = -128;
				else
					d[s_wpos] = (Sint16)r;
				++s_wpos;

				r = (float)d[s_wpos] * a[s];
				if(r > 127.0f)
					d[s_wpos] = 127;
				else if(r < -128.0f)
					d[s_wpos] = -128;
				else
					d[s_wpos] = (Sint16)r;
				++s_wpos;
			}
		}
		break;
	  }
	  case AF_MONO8:
	  {
		Sint8 *d = wave->data.si8;
		while( (frames = _next_block(s_wpos, BLOCK_FRAMES)) )
		{
			_env_process(&env[WCA_AMPLITUDE], a, frames);
			for(s = 0; s < frames; ++s)
			{
				float r = (float)d[s_wpos] * a[s];
				if(r > 127.0f)
					d[s_wpos] = 127;
				else if(r < -128.0f)
					d[s_wpos] = -128;
				else				
					d[s_wpos] = (Sint16)r;
				++s_wpos;
			}
		}
		break;
	  }
	}
}


#if 0
void wca_mix(int src_wid, int dst_wid)
{
}
#endif


/*
TODO: Output saturation.
*/
void wca_enhance(int wid, int f, float level)
{
	unsigned s, samples;
	int bpl, bpr, a;
	int outl, outr, gain, vu;
	int d1l, d2l, d1r, d2r;
	int ldl, ldr, lf, h;
	int release;
	int sl = 0, sr = 0;
	int samp = 0;
	audio_wave_t *wave = audio_wave_get(wid);
	if(!wave)
		return;

	_init_processing(wave);

	if(AF_MIDI == wave->format)
	{
		log_printf(ELOG, "wca_enhance(): MIDI not supported!\n");
		return;
	}

	a = (int)(level * 32768.0);
	lf = (f * 256 * 2) / wave->rate;
	if(lf > 256)
		lf = 256;
	f = (int)(512.0f * sin(M_PI * (float)f / wave->rate));
	if(f > 256)
		f = 256;
	release = (20 << 16) / wave->rate;
	if(release > 65536)
		release = 65536;
	d1l = d1r = d2l = d2r = 0;
	ldl = ldr = 0;
	gain = 0;
	samples = wave->samples;

	switch (wave->format)
	{
	  case AF_MONO8:
	  case AF_MONO16:
	  case AF_MONO32:
		for(s = 0; s < samples; ++s)
		{
			switch (wave->format)
			{
			  case AF_MONO8:
				samp = wave->data.si8[s] << 8;
				break;
			  case AF_MONO16:
				samp = wave->data.si16[s];
				break;
			  case AF_MONO32:
				samp = (int)(wave->data.f32[s] * 32768.0);
			  default:
				break;
			}

			/* 12 dB LP + BP + HP */
			d2l += f * d1l >> 8;
			h = (samp << 4) - d2l - d1l;
			d1l += f * h >> 8;
			bpl = d1l >> 4;

			/* Octave shift up + 6 dB gain */
			outl = abs(bpl) << 2;

			/* 6 dB HPF on the artificial treble */
			ldl += (outl - ldl) * lf >> 8;
			outl -= ldl;

			/* Use BP level to control artificial treble level 
			 */
			vu = abs(bpl);
			vu = vu * a >> 15;
			if(vu > gain)
			{
				/* Fast attacks! */
				if(vu > 65535)
					gain = 65535;
				else
					gain = vu;
			}
			else
				gain -= gain * release >> 16;

			/* Artificial treble level */
			outl = outl * gain >> 16;

			/* Add in the original signal */
			outl += samp;

			/* Clip + output */
			if(outl > 32767)
				samp = 32767;
			else if(outl < -32768)
				samp = -32768;
			else
				samp = outl;

			switch (wave->format)
			{
			  case AF_MONO8:
				wave->data.si8[s] = samp >> 8;
				break;
			  case AF_MONO16:
				wave->data.si16[s] = (Sint16)samp;
				break;
			  case AF_MONO32:
				wave->data.f32[s] = (float)samp * ONEDIV32K;
			  default:
				break;
			}
		}
		break;
	  case AF_STEREO8:
	  case AF_STEREO16:
	  case AF_STEREO32:
		samples <<= 1;
		for(s = 0; s < samples; s += 2)
		{
			switch (wave->format)
			{
			  case AF_STEREO8:
				sl = wave->data.si8[s] << 8;
				sr = wave->data.si8[s+1] << 8;
				break;
			  case AF_STEREO16:
				sl = wave->data.si16[s];
				sr = wave->data.si16[s+1];
				break;
			  case AF_STEREO32:
				sl = (int)(wave->data.f32[s] * 32768.0);
				sr = (int)(wave->data.f32[s+1] * 32768.0);
			  default:
				break;
			}

			/* 12 dB BP */
			d2l += f * d1l >> 8;
			h = (sl << 4) - d2l - d1l;
			d1l += f * h >> 8;
			bpl = d1l >> 4;

			d2r += f * d1r >> 8;
			h = (sr << 4) - d2r - d1r;
			d1r += f * h >> 8;
			bpr = d1l >> 4;

			/* Octave shift up + 6 dB gain */
			outl = abs(bpl) << 2;
			outr = abs(bpr) << 2;

			/* 6 dB HPF on the artificial treble */
			ldl += (outl - ldl) * lf >> 8;
			ldr += (outr - ldr) * lf >> 8;
			outl -= ldl;
			outr -= ldr;

			/* Use BP level to control artificial treble level 
			 */
			vu = (abs(bpl) + abs(bpr)) >> 1;
			vu = vu * a >> 15;
			if(vu > gain)
			{
				/* Fast attacks! */
				if(vu > 65535)
					gain = 65535;
				else
					gain = vu;
			}
			else
				gain -= gain * release >> 16;

			/* Artificial treble level */
			outl = outl * gain >> 16;
			outr = outr * gain >> 16;

			/* Add in the original signal */
			outl += sl;
			outr += sr;

			/* Clip + output */
			if(outl > 32767)
				sl = 32767;
			else if(outl < -32768)
				sl = -32768;
			else
				sl = outl;
			if(outr > 32767)
				sr = 32767;
			else if(outr < -32768)
				sr = -32768;
			else
				sr = outr;

			switch (wave->format)
			{
			  case AF_STEREO8:
				wave->data.si8[s] = sl >> 8;
				wave->data.si8[s+1] = sr >> 8;
				break;
			  case AF_STEREO16:
				wave->data.si16[s] = (Sint16)sl;
				wave->data.si16[s+1] = (Sint16)sr;
				break;
			  case AF_STEREO32:
				wave->data.f32[s] = (float)sl * ONEDIV32K;
				wave->data.f32[s+1] = (float)sr * ONEDIV32K;
			  default:
				break;
			}
		}
	  case AF_MIDI:
		break;
	}
}


void wca_gate(int wid, int f, float min, float thr, float att)
{
	unsigned s, samples;
	int thresh, min_gain;
	int lpl, lpr, hpl, hpr, h;
	int outl, outr, gain, vu;
	int d1l, d2l, d1r, d2r;
	int attack, release, track, track_level;
	int sl = 0, sr = 0;
	audio_wave_t *wave = audio_wave_get(wid);
	if(!wave)
		return;

	_init_processing(wave);

	if(AF_MIDI == wave->format)
	{
		log_printf(ELOG, "wca_gate(): MIDI not supported!\n");
		return;
	}

	f = (int)(512.0f * sin(M_PI * (float)f / wave->rate));
	if(f > 256)
		f = 256;

	attack = (5000 << 15) / wave->rate;
	if(attack > 32767)
		attack = 32767;

	release = (10 << 15) / wave->rate;
	if(release > 32768)
		release = 32767;

	thresh = (int)(thr * 32767.0);

	min_gain = (int)(min * 32767.0);
	if(min_gain > 32767)
		min_gain = 32767;

	track = (att * 32768.0 * 256.0f) / wave->rate;
	if(track > 32767*256)
		track = 32767*256;
	track_level = 0;

	d1l = d1r = d2l = d2r = 0;
	gain = 0;
	samples = wave->samples;

	switch (wave->format)
	{
	  case AF_MONO8:
	  case AF_MONO16:
	  case AF_MONO32:
		for(s = 0; s < samples; ++s)
		{
			switch (wave->format)
			{
			  case AF_MONO8:
				sl = wave->data.si8[s] << 8;
				break;
			  case AF_MONO16:
				sl = wave->data.si16[s];
				break;
			  case AF_MONO32:
				sl = (int)(wave->data.f32[s] * 32768.0);
			  default:
				break;
			}

			/* 12 dB LP / HP split */
			d2l += f * d1l >> 8;
			h = (sl << 4) - d2l - d1l;
			d1l += f * h >> 8;
			lpl = d2l >> 4;
			hpl = (d1l + h) >> 4;

			/* Auto Threshold Tracking */
			vu = abs(sl);
			if(vu > (thresh>>8))
				track_level += ((vu<<8) - track_level) * track >> 16;
			else
				track_level += ((vu<<8) - track_level) * track >> 16;

			/* Envelope generator */
			vu = abs(hpl);
			if(vu > thresh + (track_level>>8))
				gain += (32767 - gain) * attack >> 14;
			else
			{
				gain -= gain * release >> 16;
				if(gain < min_gain)
					gain = min_gain;
			}

			/* Gate the hp part */
			outl = hpl * gain >> 15;

			/* Add in the LP part */
			outl += lpl;

			/* Clip + output */
			if(outl > 32767)
				sl = 32767;
			else if(outl < -32768)
				sl = -32768;
			else
				sl = outl;

			switch (wave->format)
			{
			  case AF_MONO8:
				wave->data.si8[s] = sl >> 8;
				break;
			  case AF_MONO16:
				wave->data.si16[s] = (Sint16)sl;
				break;
			  case AF_MONO32:
				wave->data.f32[s] = (float)sl * ONEDIV32K;
			  default:
				break;
			}
		}
		break;
	  case AF_STEREO8:
	  case AF_STEREO16:
	  case AF_STEREO32:
		samples <<= 1;
		for(s = 0; s < samples; s += 2)
		{
			switch (wave->format)
			{
			  case AF_STEREO8:
				sl = wave->data.si8[s] << 8;
				sr = wave->data.si8[s+1] << 8;
				break;
			  case AF_STEREO16:
				sl = wave->data.si16[s];
				sr = wave->data.si16[s+1];
				break;
			  case AF_STEREO32:
				sl = (int)(wave->data.f32[s] * 32768.0);
				sr = (int)(wave->data.f32[s+1] * 32768.0);
			  default:
				break;
			}

			/* 12 dB LP / HP split */
			d2l += f * d1l >> 8;
			h = (sl << 4) - d2l - d1l;
			d1l += f * h >> 8;
			lpl = d2l >> 4;
			hpl = (d1l + h) >> 4;

			d2r += f * d1r >> 8;
			h = (sr << 4) - d2r - d1r;
			d1r += f * h >> 8;
			lpr = d2r >> 4;
			hpr = (d1r + h) >> 4;

			/* Auto Threshold Tracking */
			vu = (abs(sl) + abs(sr)) >> 1;
			if(vu > (thresh>>8))
				track_level += ((vu<<8) - track_level) * track >> 16;
			else
				track_level += ((vu<<8) - track_level) * track >> 16;

			/* Envelope generator */
			vu = (abs(hpl) + abs(hpr)) >> 1;
			if(vu > thresh + (track_level>>8))
				gain += (32767 - gain) * attack >> 14;
			else
			{
				gain -= gain * release >> 16;
				if(gain < min_gain)
					gain = min_gain;
			}

			/* Gate the hp part */
			outl = hpl * gain >> 15;
			outr = hpr * gain >> 15;

			/* Add in the LP part */
			outl += lpl;
			outr += lpr;

			/* Clip + output */
			if(outl > 32767)
				sl = 32767;
			else if(outl < -32768)
				sl = -32768;
			else
				sl = outl;
			if(outr > 32767)
				sr = 32767;
			else if(outr < -32768)
				sr = -32768;
			else
				sr = outr;

			switch (wave->format)
			{
			  case AF_STEREO8:
				wave->data.si8[s] = sl >> 8;
				wave->data.si8[s+1] = sr >> 8;
				break;
			  case AF_STEREO16:
				wave->data.si16[s] = (Sint16)sl;
				wave->data.si16[s+1] = (Sint16)sr;
				break;
			  case AF_STEREO32:
				wave->data.f32[s] = (float)sl * ONEDIV32K;
				wave->data.f32[s+1] = (float)sr * ONEDIV32K;
			  default:
				break;
			}
		}
		break;
	  case AF_MIDI:
		break;
	}
}
