/*(LGPL)
---------------------------------------------------------------------------
	a_voice.c - Audio Engine low level mixer voices
---------------------------------------------------------------------------
 * Copyright (C) 2001-2003, David Olofson
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
#include <math.h>

#include "kobolog.h"
#include "a_voice.h"
#include "a_struct.h"
#include "a_globals.h"
#include "a_tools.h"
#include "a_control.h"

#define	LDBG(x)
#define	EVDBG(x)
#define	CHECKPOINTS


/* Random number generator state for randtrig etc */
static int rnd = 16576;
#define	UPDATE_RND	rnd *= 1566083941UL; rnd++; rnd &= 0x7fffffffUL;

/* Last allocated voice (good starting point!) */
static int last_voice = 0;


void voice_kill(audio_voice_t *v)
{
	v->vu = 65535;		/* Newly allocated voices are harder to steal */
	aev_flush(&v->port);
	if(v->channel)
	{
		--v->channel->playing;
		chan_unlink_voice(v);
	}
	v->state = VS_STOPPED;
}


int voice_alloc(audio_channel_t *c)
{
	int lv = 0;
	int i, pri, v, vol;

	/* Pass 1: Look for an unused voice. */
	for(v = 0; v < AUDIO_MAX_VOICES; ++v)
	{
		if(voicetab[v].state != VS_STOPPED)
			continue;	/* Not interesting here... */
		last_voice = v;
		chan_link_voice(c, &voicetab[v]);
		voicetab[v].priority = c->ctl[ACC_PRIORITY];
		voicetab[v].state = VS_RESERVED;
		return v;
	}

	/*
	 * Pass 2: Look for the most silent voice with
	 *         same or lower priority.
	 */
	lv = last_voice;
	vol = 2000000000;
	v = -1;
	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
	{
		int vu;
		if(++lv >= AUDIO_MAX_VOICES)
			lv = 0;
		if(voicetab[lv].priority < c->ctl[ACC_PRIORITY])
			continue;
#ifdef AUDIO_USE_VU
		vu = voicetab[lv].vu;
		vu *= (voicetab[lv].ic[VIC_LVOL].v +
				voicetab[lv].ic[VIC_RVOL].v +
				voicetab[lv].ic[VIC_LSEND].v +
				voicetab[lv].ic[VIC_RSEND].v) >> (RAMP_BITS + 2);
		vu >>= VOL_BITS;
#else
		vu = (voicetab[lv].ic[VIC_LVOL].v +
				voicetab[lv].ic[VIC_RVOL].v +
				voicetab[lv].ic[VIC_LSEND].v +
				voicetab[lv].ic[VIC_RSEND].v) >> (RAMP_BITS + 2);
#endif
		if(vu < vol)
		{
			vol = vu;
			v = lv;
		}
	}
	if(v >= 0)
	{
		voice_kill(&voicetab[v]);
		chan_link_voice(c, &voicetab[v]);
		last_voice = v;
		voicetab[v].priority = c->ctl[ACC_PRIORITY];
		voicetab[v].state = VS_RESERVED;
		return v;
	}

	/* Pass 3: Grab voice with lowest priority. */
	lv = last_voice;
	pri = c->ctl[ACC_PRIORITY];
	v = -1;
	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
	{
		if(++lv >= AUDIO_MAX_VOICES)
			lv = 0;
		if(voicetab[lv].priority > pri)
		{
			pri = voicetab[lv].priority;
			v = lv;
		}
	}
	if(v >= 0)
	{
		voice_kill(&voicetab[v]);
		chan_link_voice(c, &voicetab[v]);
		last_voice = v;
		voicetab[v].priority = c->ctl[ACC_PRIORITY];
		voicetab[v].state = VS_RESERVED;
		return v;
	}

	return -1;
}


void voice_start(audio_voice_t *v, int wid)
{
	int retrig, randtrig;

	v->wave = v->c[VC_WAVE] = wid;
	v->c[VC_LOOP] = wavetab[wid].looped;

	v->position = 0;
	v->position_frac = 0;
	if(!wavetab[wid].data.si8)
	{
		voice_kill(v);
		return;
	}

	/* Set up retrig and looping */
	randtrig = (int)v->c[VC_RANDTRIG];
	retrig = (int)v->c[VC_RETRIG];
	if(randtrig)
	{
		UPDATE_RND
		randtrig = rnd % (randtrig<<1) - randtrig;
		randtrig = retrig * randtrig >> 16;
		retrig += randtrig;
	}

	if(retrig > 0)
	{
		if((unsigned)retrig > wavetab[wid].play_samples)
			v->section_end = wavetab[wid].play_samples;
		else
			v->section_end = (unsigned)retrig;
	}
	else
		v->section_end = wavetab[wid].play_samples;

	/* Start voice! */
	v->state = VS_PLAYING;
}


static inline int __handle_looping(audio_voice_t *v)
{
	unsigned int samples = wavetab[v->c[VC_WAVE]].play_samples;
	int randtrig = v->c[VC_RANDTRIG];
	int retrig = v->c[VC_RETRIG];

	/* Latch (new) waveform index */
	v->wave = v->c[VC_WAVE];

	if(randtrig)
	{
		UPDATE_RND
		randtrig = rnd % (randtrig<<1) - randtrig;
		randtrig = retrig * randtrig >> 16;
		retrig += randtrig;
	}

	if(retrig > 0)
	{
		unsigned int old_se = v->section_end;

		if((unsigned)retrig > samples)
			v->section_end = samples;
		else
			v->section_end = (unsigned)retrig;

		if(old_se > v->position)
		{
			/* Force instant initial retrig */
			v->position = 0;
			v->position_frac = 0;
		}
		else
		{
			/* Wrap loop */
			if(v->position >= old_se)
				v->position = 0;
			else
				v->position -= old_se;
		}
	}
	else
	{
		if(v->c[VC_LOOP])
		{
			v->position -= v->section_end;
			v->section_end = samples;
		}
		else
			return 0;	/* Stop playing! */
	}
	return 1;
}


void voice_check_retrig(audio_voice_t *v)
{
	if(wavetab[v->wave].data.si8)
	{
		int retrig_max = v->c[VC_RETRIG] * v->c[VC_RANDTRIG] >> 16;
		retrig_max += v->c[VC_RETRIG];
		if(v->position > (unsigned)retrig_max)
			__handle_looping(v);
	}
}


/*
 * Macro Mayhem! Create all the mixer variants...
 */

static inline void __mix_m8(audio_voice_t *v, int *out, unsigned frames)
{
#undef	__SEND
#undef	__STEREO
#undef	__16BIT
#include "a_mixers.h"
}

static inline void __mix_s8(audio_voice_t *v, int *out, unsigned frames)
{
#undef	__SEND
#define	__STEREO
#undef	__16BIT
#include "a_mixers.h"
}

static inline void __mix_m16(audio_voice_t *v, int *out, unsigned frames)
{
#undef	__SEND
#undef	__STEREO
#define	__16BIT
#include "a_mixers.h"
}

static inline void __mix_s16(audio_voice_t *v, int *out, unsigned frames)
{
#undef	__SEND
#define	__STEREO
#define	__16BIT
#include "a_mixers.h"
}


static inline void __mix_m8d(audio_voice_t *v, int *out, int *sout, unsigned frames)
{
#define	__SEND
#undef	__STEREO
#undef	__16BIT
#include "a_mixers.h"
}

static inline void __mix_s8d(audio_voice_t *v, int *out, int *sout, unsigned frames)
{
#define	__SEND
#define	__STEREO
#undef	__16BIT
#include "a_mixers.h"
}

static inline void __mix_m16d(audio_voice_t *v, int *out, int *sout, unsigned frames)
{
#define	__SEND
#undef	__STEREO
#define	__16BIT
#include "a_mixers.h"
}

static inline void __mix_s16d(audio_voice_t *v, int *out, int *sout, unsigned frames)
{
#define	__SEND
#define	__STEREO
#define	__16BIT
#include "a_mixers.h"
}

#undef	__SEND
#undef	__STEREO
#undef	__16BIT


/*
 * Calculates resampling input "step", and selects resampling mode.
 */
static inline unsigned int __calc_step(audio_voice_t *v)
{
	audio_resample_t mode = AR_LINEAR;
	/* Resampling factor */
	int pitch = v->c[VC_PITCH];
	unsigned step = (unsigned)fixmul(ptab_convert(pitch),
			wavetab[v->wave].speed);
#if (FREQ_BITS < 16)
	step >>= 16 - FREQ_BITS;
#elif (FREQ_BITS > 16)
	step <<= FREQ_BITS - 16;
#endif

	/*
	 * We must prevent high pithes from locking the mixer in
	 * an infinite loop with short looped waveforms...
	 */
	if(step > MAX_STEP)
	{
#ifdef DEBUG
		log_printf(ELOG, "Too high pitch!\n");
#endif
		while(step > MAX_STEP)
			step >>= 1;
	}

	switch(a_settings.quality)
	{
	  case AQ_VERY_LOW:
		mode = AR_NEAREST;
		break;
	  case AQ_LOW:
		mode = AR_NEAREST_4X;
		break;
	  case AQ_NORMAL:
		mode = AR_LINEAR;
		break;
	  case AQ_HIGH:
		/* Select resampling method based on in/out ratio */
		if(step > (unsigned)(6 << FREQ_BITS))
			mode = AR_LINEAR_8X_R;	/* Above 6:1 */
		else if(step > (unsigned)(3 << FREQ_BITS))
			mode = AR_LINEAR_4X_R;	/* Above 3:1 */
		else
			mode = AR_LINEAR_2X_R;	/* Below 2:1 */
		break;
	  case AQ_VERY_HIGH:
		/* Select resampling method based on in/out ratio */
		if(step > (unsigned)(4 << FREQ_BITS))
			mode = AR_LINEAR_16X_R;	/* Above 4:1 */
		else if(step > (unsigned)(3 << FREQ_BITS))
			mode = AR_LINEAR_8X_R;	/* Above 3:1 */
		else if(step > (unsigned)(2 << FREQ_BITS))
			mode = AR_LINEAR_4X_R;	/* Above 2:1 */
		else if(step > (unsigned)(3 << (FREQ_BITS-1)))
			mode = AR_LINEAR_2X_R;	/* Above 1.5:1 */
		else
			mode = AR_CUBIC_R;	/* Below 1.5:1 */
		break;
	}
	v->c[VC_RESAMPLE] = mode;
	return step;
}


/*
 * Calculates # of output frames to the nearest of 'frames',
 * end of segment and the "fragment span limit".
 */
static inline unsigned int __endframes(audio_voice_t *v, unsigned int frames)
{
#ifdef A_USE_INT64
	Uint64 n, n2;
#else
	double n, n2;
#endif
	if(!v->step)
		return frames;

#ifdef A_USE_INT64
	n = ((Uint64)(v->position) << 32) | (Uint64)(v->position_frac);
	n >>= 32 - FREQ_BITS;
	n = ((Uint64)(v->section_end) << FREQ_BITS) - n + v->step - 1;
	n /= v->step;
#else
	n = (double)(v->position) + (double)(v->position_frac) / 4294967296.0;
	n = (double)(v->section_end) - n;
	n /= (double)v->step / (double)(1<<FREQ_BITS);
	n = ceil(n);
#endif
	if(n > 0xffffffff)
		n = 0xffffffff;
#ifdef A_USE_INT64
	if(n > (Uint64)frames)
		n = (Uint64)frames;
#else
	if(n > (double)frames)
		n = (double)frames;
#endif
	/*
	 * Restrict fragment size to prevent read position overflows.
	 *
	 * (In order to maximize pitch accuracy, voice mixers can only
	 * handle a very limited number of input samples without
	 * recalculating their "base pointers".)
	 */
#ifdef A_USE_INT64
	n2 = (Uint64)MAX_FRAGMENT_SPAN << FREQ_BITS;
	n2 /= (Uint64)v->step * (Uint64)frames;
#else
	n2 = (double)MAX_FRAGMENT_SPAN * (1 << FREQ_BITS);
	n2 /= (double)v->step * (double)frames;
#endif
	if(n > n2)
		n = n2;

#ifdef	CHECKPOINTS
	if(!frames)
	{
		voice_kill(v);
		log_printf(ELOG, "Voice locked up! (Too high pitch "
				"resulted in zero fragment size.)\n");
	}
#endif
	return (unsigned int)n;
}


static inline void __fragment_single(audio_voice_t *v, int *out,
		unsigned int frames)
{
	switch(wavetab[v->wave].format)
	{
	  case AF_MONO8:
		__mix_m8(v, out, frames);
		break;
	  case AF_STEREO8:
		__mix_s8(v, out, frames);
		break;
	  case AF_MONO16:
		__mix_m16(v, out, frames);
		break;
	  case AF_STEREO16:
		__mix_s16(v, out, frames);
		break;
	  case AF_MONO32:
		/*__mix_m32(v, out, frames);*/
		break;
	  case AF_STEREO32:
		/*__mix_s32(v, out, frames);*/
	  case AF_MIDI:	/* warning eliminator */
		break;
	}
}


static inline void __fragment_double(audio_voice_t * v, int *out, int *sout,
			unsigned int frames)
{
	switch(wavetab[v->wave].format)
	{
	  case AF_MONO8:
		__mix_m8d(v, out, sout, frames);
		break;
	  case AF_STEREO8:
		__mix_s8d(v, out, sout, frames);
		break;
	  case AF_MONO16:
		__mix_m16d(v, out, sout, frames);
		break;
	  case AF_STEREO16:
		__mix_s16d(v, out, sout, frames);
		break;
	  case AF_MONO32:
		/*__mix_m32d(v, out, sout, frames);*/
		break;
	  case AF_STEREO32:
		/*__mix_s32d(v, out, sout, frames);*/
	  case AF_MIDI:	/* warning eliminator */
		break;
	}
}

/*
 * Figure out if we should use the "double output" mixers,
 * and where to connect the output(s).
 */
static inline int __setup_output(audio_voice_t *v)
{
	int prim, send;
/*
FIXME: This "automatic routing optimization" isn't needed,
FIXME: and causes trouble elsewhere. Simplify.
*/
	v->fx1 = v->c[VC_PRIM_BUS];
	v->fx2 = v->c[VC_SEND_BUS];
	prim = (v->fx1 >= 0) && (v->fx1 < AUDIO_MAX_BUSSES);
	send = (v->fx2 >= 0) && (v->fx2 < AUDIO_MAX_BUSSES);
	if(!prim && !send)
		return -1;	/* No busses selected! --> */

	if(prim && send)
		v->use_double = (v->fx1 != v->fx2);
	else
	{
		if(send)
			v->fx1 = v->fx2;
		else
			v->fx2 = v->fx1;
		v->use_double = 0;
	}
	return 0;
}


void voice_process_mix(audio_voice_t *v, int *busses[], unsigned frames)
{
	unsigned s, frag_s;

	if((VS_STOPPED == v->state) && (aev_next(&v->port, 0) > frames))
		return;	/* Stopped, and no events for this buffer --> */

	/* Loop until buffer is full, or the voice is "dead". */
	s = 0;
	while(frames)
	{
		unsigned frag_frames;
		while( !(frag_frames = aev_next(&v->port, s)) )
		{
			aev_event_t *ev = aev_read(&v->port);
			switch(ev->type)
			{
			  case VE_START:
				voice_start(v, ev->arg1);
				if(VS_STOPPED == v->state)
				{
					aev_free(ev);
					return;	/* Error! --> */
				}
				/*
				 * NOTE:
				 *	This being checked here means that
				 *	it's not possible to change routing
				 *	during playback. Who would, anyway?
				 */
				if(__setup_output(v) < 0)
				{
					voice_kill(v);
					aev_free(ev);
					return;	/* No sends! --> */
				}
				break;
			  case VE_STOP:
				voice_kill(v);
				aev_free(ev);
				return;	/* Back in the voice pool! --> */
			  case VE_SET:
#ifdef	CHECKPOINTS
				if(ev->index >= VC_COUNT)
				{
					log_printf(ELOG, "BUG! VC index out of range!");
					break;
				}
#endif
				v->c[ev->index] = ev->arg1;
				if(VC_PITCH == ev->index)
					v->step = __calc_step(v);
				break;
			  case VE_IRAMP:
#ifdef	CHECKPOINTS
				if(ev->index >= VIC_COUNT)
				{
					log_printf(ELOG, "BUG! VIC index out of range!");
					break;
				}
#endif
				if(ev->arg2)
				{
					v->ic[ev->index].dv = ev->arg1 << RAMP_BITS;
					v->ic[ev->index].dv -= v->ic[ev->index].v;
					v->ic[ev->index].dv /= ev->arg2 + 1;
				}
				else
					v->ic[ev->index].v = ev->arg1 << RAMP_BITS;
				break;
			}
			aev_free(ev);
		}

		if(frag_frames > frames)
			frag_frames = frames;

		/* Handle fragmentation, end-of-waveform and looping */
		frag_s = (VS_PLAYING == v->state) ? 0 : frag_frames;
		while(frag_s < frag_frames)
		{
			unsigned offs = (s + frag_s) << 1;
			unsigned do_frames = __endframes(v, frag_frames - frag_s);
			if(do_frames)
			{
#ifdef CHECKPOINTS
				if(v->position >= v->section_end)
				{
					log_printf(ELOG, "BUG! position = %u while section_end = %u.",
							v->position, v->section_end);
					log_printf(ELOG, " (step = %u)\n", v->step >> FREQ_BITS);
					v->position = 0;
				}
#endif
				bustab[v->fx1].in_use = 1;
				if(v->use_double)
				{
					bustab[v->fx2].in_use = 1;
					 __fragment_double(v, busses[v->fx1] + offs,
							busses[v->fx2] + offs,
							do_frames);
				}
				else
					__fragment_single(v, busses[v->fx1] + offs,
							do_frames);

				frag_s += do_frames;

				// This is just for that damn oversampling...
				if(v->position >= v->section_end)
					do_frames = 0;
			}
			if(!do_frames && !__handle_looping(v))
			{
				voice_kill(v);
				return;
			}
		}
		s += frag_frames;
		frames -= frag_frames;
	}
}


void voice_process_all(int *bufs[], unsigned frames)
{
	int i;
	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
		voice_process_mix(voicetab + i, bufs, frames);
}


static int _is_open = 0;

void audio_voice_open(void)
{
	int i;
	if(_is_open)
		return;

	memset(voicetab, 0, sizeof(voicetab));
	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
	{
		char *buf = malloc(64);
		snprintf(buf, 64, "Audio Voice %d", i);
		aev_port_init(&voicetab[i].port, buf);
	}
	_is_open = 1;
}


void audio_voice_close(void)
{
	int i;
	if(!_is_open)
		return;

	for(i = 0; i < AUDIO_MAX_VOICES; ++i)
	{
		aev_flush(&voicetab[i].port);
		free((char *)voicetab[i].port.name);
	}
	memset(voicetab, 0, sizeof(voicetab));
	_is_open = 0;
}
