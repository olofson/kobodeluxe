/*(LGPL)
---------------------------------------------------------------------------
	a_wcaosc.h - Oscillators for the Wave Construction API
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

/*
 * Not a "real" header - it just looks like one to great extent. ;-)
 * It is best seen as a huge macro.
 */

static unsigned int rnd = 16576;

//Resets the noise generator
static void noise_reset(void)
{
	rnd = 16576;
}

//Returns a pseudo random number in the range [-1.0, 1.0]
static inline float noise(void)
{
	int out;
	rnd *= 1566083941UL;
	rnd++;
	rnd &= 0xffffffffUL;	/* NOP on 32 bit machines */
	out = (int)(rnd * (rnd >> 16) >> 16);
	return (float)(out - 32767) * ONEDIV32K;
}


#if 0
typedef struct soscillator_t
{
	float a;	/* amplitude */
	float w;	/* angular position */
	float dwr;	/* relative angular velocity */
} soscillator_t;

static int oscillators = 0;
static soscillator_t osc[MAX_SPECTRUM_OSCILLATORS];

static inline float osc_process(float dw0)
{
	int i;
	float acc = 0f;
	for(i = 0; i < oscillators; ++i)
	{
		acc += sin(osc[i].w) * osc[i].a;
		osc[i].w += dw0 * osc[i].dwr;
	}
	return acc;
}
#endif


static inline float rolloff(float f, float limit)
{
	float a = f / limit;
	return 1.0f - (.45 + .3*a)*a;
}


static double osc_w;		/* Ohmega for most oscillators */
static float noise_out;		/* S&H accumulator for noise */
static float osc_yit;		/* State for recursive oscillators */

static inline void _osc_sine(char *sync, float *f,
		float *mod1,
		float *out, unsigned frames)
{
	const float onediv8 = 1.0f / 8.0f;
	unsigned s, os;
	float dt = s_dt * onediv8;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float dw = f[s] * dt;
		if(sync[s])
			osc_w = 0.0f;
		if(mod1[s])
			for(os = 8; os; --os)
			{
				float mod = sin(M_PI * 2.0f * osc_w) * mod1[s];
				acc += sin(M_PI * 2.0f * (osc_w + mod));
				osc_w += dw;
			}
		else
			for(os = 8; os; --os)
			{
				acc += sin(M_PI * 2.0f * osc_w);
				osc_w += dw;
			}
		out[s] = acc * onediv8;
	}
}


static inline void _osc_halfsine(char *sync, float *f,
		float *mod1,
		float *out, unsigned frames)
{
	const float onediv2 = 1.0f / 2.0f;
	unsigned s, os;
	float dt = s_dt * onediv2;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float dw = f[s] * dt;
		if(sync[s])
			osc_w = 0.0f;
		for(os = 2; os; --os)
		{
			float v = sin(M_PI * 2.0f * osc_w);
			if(v < mod1[s])
				v = mod1[s];
			v -= 0.5f + mod1[s] * 0.5f;
			if(mod1[s] < 1.0f)
				v *= 2.0f / (1.0f - mod1[s]);
			else
				v = 1.0f;
			acc += v;
			osc_w += dw;
		}
		out[s] = acc * onediv2;
	}
}


static inline void _osc_rectsine(char *sync, float *f,
		float *mod1,
		float *out, unsigned frames)
{
	const float onediv4 = 1.0f / 4.0f;
	unsigned s, os;
	float dt = s_dt * onediv4;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float dw = f[s] * dt;
		if(sync[s])
			osc_w = 0.0f;
		for(os = 4; os; --os)
		{
			float v = fabs(sin(M_PI * 2.0f * osc_w) + mod1[s]);
			v -= fabs(mod1[s] * 0.5f) + 0.5f;
			v *= 2.0f - 2.0f * fabs(mod1[s]);
			acc += v;
			osc_w += dw;
		}
		out[s] = acc * onediv4;
	}
}


static inline void _osc_pulse(char *sync, float *f,
		float *mod1,
		float *out, unsigned frames)
{
	const float onediv8 = 1.0f / 8.0f;
	unsigned s, os;
	float dt = s_dt * onediv8;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float dw = f[s] * dt;
		if(sync[s])
			osc_w = 0.0f;
		for(os = 8; os; --os)
		{
			float saw = osc_w - floor(osc_w);
			acc += saw > mod1[s] ? 1.0f : -1.0f;
			osc_w += dw;
		}
		out[s] = acc * onediv8;
	}
}


static inline void _osc_triangle(char *sync, float *f,
		float *mod1,
		float *out, unsigned frames)
{
	const float onediv4 = 1.0f / 4.0f;
	unsigned s, os;
	float dt = s_dt * onediv4;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float dw = f[s] * dt;
		if(sync[s])
			osc_w = 0.0f;
		if(0.0f == mod1[s])
			for(os = 4; os; --os)
			{
				acc += (osc_w - floor(osc_w)) * 2.0f - 1.0f;
				osc_w += dw;
			}
		else
			for(os = 4; os; --os)
			{
				float v = osc_w - floor(osc_w);
				if(v < mod1[s])
					v = v / mod1[s];
				else
					v = (1.0f - v) / (1.0f - mod1[s]);
				v *= 2.0f;
				v -= 1.0f;
				acc += v;
				osc_w += dw;
			}
		out[s] = acc * onediv4;
	}
}


static inline void _osc_sinemorph(char *sync, float *f,
		float *mod1, float *mod2, float *limit,
		float *out, unsigned frames)
{
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float m1, m2;
		if(sync[s])
		{
			osc_w = 0.0f;
			osc_yit = 0.0f;
		}
		if(f[s] > limit[s] * 0.5f)
			m1 = m2 = 0.0f;
		else
		{
			float scale = 1.0f - f[s] / (limit[s] * 0.5f);
			if(mod1[s] + mod2[s] > 1.0f)
				scale *= 1.0f / (mod1[s] + mod2[s]);
			m1 = mod1[s] * scale;
			m2 = mod2[s] * scale;
		}
		osc_yit = sin(M_PI*2.0f*osc_w + m1 * osc_yit +
				m2 * osc_yit*osc_yit);
		out[s] = osc_yit;
		osc_w += f[s] * s_dt;
	}
}


static inline void _osc_blmorph(char *sync, float *f,
		float *mod1, float *mod2, float *mod3, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float hlimit1, hlimit2, hlimit3;
		float ha;
		float n = 2.0f;
		int running = 1;
		float m1 = mod1[s]*mod1[s];
		float m2 = mod2[s]*mod2[s];
		float m3 = mod3[s]*mod3[s];
		int count = 0;

		if(sync[s])
			osc_w = 0.0f;

		/* Fundamental */
		out[s] = sin(M_PI * 2.0f * osc_w * f[s]);

		while(running)
		{
			running = 0;

			/* Even harmonics (sawtooth) */
			hlimit1 = f[s] * (1.0f - m1) + (limit[s] * m1);
			if(f[s] * n <= hlimit1)
			{
				out[s] += sin(M_PI * 2.0f * osc_w * f[s] * n) *
						(1.0f / n) *
						rolloff(f[s] * n, hlimit1);
				running = 1;
			}
			n += 1.0f;
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;

			/* Odd harmonics (sawtooth, square & triangle) */
			hlimit1 = f[s] * (1.0f - m1) + (limit[s] * m1);
			if(f[s] * n <= hlimit1)
				ha = 1.0f / n * rolloff(f[s] * n, hlimit1);
			else
				ha = 0;
			hlimit2 = f[s] * (1.0f - m2) + (limit[s] * m2);
			if(f[s] * n <= hlimit2)
				ha += 1.0f / n * rolloff(f[s] * n, hlimit2);
			hlimit3 = f[s] * (1.0f-m3) + (limit[s] * m3);
			if(f[s] * n <= hlimit3)
				ha += -1.0f / (n*n) *
						rolloff(f[s] * n, hlimit3);
			if(ha != 0)
			{
				out[s] += sin(M_PI * 2.0f * osc_w * f[s] * n)
						* ha;
				running = 1;
			}
			n += 1.0f;
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		osc_w += s_dt;
	}
}


static inline void _osc_blcross(char *sync, float *f,
		float *mod1, float *mod2, float *mod3, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float n = 2.0f;
		float ha;
		int count = 0;

		if(sync[s])
			osc_w = 0.0f;

		/* Fundamental */
		out[s] = sin(M_PI * 2.0f * osc_w * f[s]);

		while(1)
		{
			/* Even harmonics (sawtooth) */
			if(f[s] * n > limit[s])
				break;

			ha = mod1[s] / n;
			out[s] += sin(M_PI * 2.0f * osc_w * f[s] * n) * ha *
					rolloff(f[s] * n, limit[s]);
			n += 1.0f;
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;

			/* Odd harmonics (sawtooth, square & triangle) */
			if(f[s] * n > limit[s])
				break;

			ha = (mod1[s] + mod2[s]) / n - mod3[s] / (n*n);
			out[s] += sin(M_PI * 2.0f * osc_w * f[s] * n) * ha *
					rolloff(f[s] * n, limit[s]);
			n += 1.0f;
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		osc_w += s_dt;
	}
}


static inline void _osc_noise(char *sync, float *f,
		float *out, unsigned frames)
{
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		unsigned os;
		float acc = 0.0f;
		float dt = (2.0f * 0.25f) * f[s] * s_dt;
		if(dt > 0.25f)
			dt = 0.25f;

		if(sync[s])
			osc_w = 1.0f;

		for(os = 4; os; --os)
		{
			if(osc_w >= 1.0f) /* new value every half period */
			{
				osc_w -= 1.0f;
				noise_out = noise();
			}
			osc_w += dt;
			acc += noise_out;
		}
		out[s] = 0.25f * acc;
	}
}


static inline void _osc_spectrum(char *sync, float *f,
		float *mod1, float *mod2, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float sa = 1.0f;
		float sf = 1.0f;
		float m1 = mod1[s];
		float lim;
		int count = 0;

		if(sync[s])
			osc_w = 0.0f;

		if(m1 <= 1.0f)
		{
			m1 = 10.0f;
			lim = f[s];
		}
		else
			lim = limit[s];

		while(f[s] * sf <= lim)
		{
			acc += sin(M_PI * 2.0f * f[s] * osc_w * sf) * sa *
					rolloff(sf, lim);
			sf *= m1;
			sa *= mod2[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		out[s] = acc;
		osc_w += s_dt;
	}
}


static inline void _osc_aspectrum(char *sync, float *f,
		float *mod1, float *mod2, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float acc = 0.0f;
		float sa = 1.0f;
		float sf = 1.0f;
		float m1 = mod1[s];
		float lim;
		int count = 0;

		if(sync[s])
			osc_w = 0.0f;

		if(m1 <= 1.0f)
		{
			m1 = 10.0f;
			lim = f[s];
		}
		else
			lim = limit[s];

		while(f[s] * sf <= lim)
		{
			acc += sin(M_PI * 2.0f * f[s] * osc_w * sf) * sa *
					rolloff(sf, lim);
			sf += m1;
			sa *= mod2[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		out[s] = acc;
		osc_w += s_dt;
	}
}


static inline void _osc_hspectrum(char *sync, float *f,
		float *mod1, float *mod2, float *mod3, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float sao = 1.0f;
		float sae = mod2[s];
		float n = 1.0f;
		unsigned count = 0;
		float acc = 0.0f;

		if(sync[s])
			osc_w = 0.0f;

		while(1)
		{
			/* Odd overtones */
			if(f[s] * n > limit[s])
				break;
			acc += sin(M_PI * 2.0f * osc_w * f[s] * n) * sao *
					rolloff(f[s] * n, limit[s]);
			n *= mod1[s];
			sao *= mod2[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;

			/* Even overtones */
			if(f[s] * n > limit[s])
				break;
			acc += sin(M_PI * 2.0f * osc_w * f[s] * n) * sae *
					rolloff(f[s] * n, limit[s]);
			n *= mod1[s];
			sae *= mod3[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		out[s] = acc;
		osc_w += s_dt;
	}
}


static inline void _osc_ahspectrum(char *sync, float *f,
		float *mod1, float *mod2, float *mod3, float *limit,
		float *out, unsigned frames)
{
/*
FIXME: Frequency sweeping broken!
*/
	unsigned s;
	for(s = 0; s < frames; ++s)
	{
		float sao = 1.0f;
		float sae = mod2[s];
		float n = 1.0f;
		unsigned count = 0;
		float acc = 0.0f;

		if(sync[s])
			osc_w = 0.0f;

		while(1)
		{
			/* Odd overtones */
			if(f[s] * n > limit[s])
				break;
			acc += sin(M_PI * 2.0f * osc_w * f[s] * n) * sao *
					rolloff(f[s] * n, limit[s]);
			n += mod1[s];
			sao *= mod2[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;

			/* Even overtones */
			if(f[s] * n > limit[s])
				break;
			acc += sin(M_PI * 2.0f * osc_w * f[s] * n) * sae *
					rolloff(f[s] * n, limit[s]);
			n += mod1[s];
			sae *= mod3[s];
			if(++count > MAX_SPECTRUM_OSCILLATORS)
				break;
		}
		out[s] = acc;
		osc_w += s_dt;
	}
}
