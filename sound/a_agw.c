/*(LGPL)
---------------------------------------------------------------------------
	a_agw.c - "Algorithmically Generated Waveform" file support
---------------------------------------------------------------------------
 * Copyright (C) 2002, 2003, David Olofson
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

#define	DBG(x)

#include <stdio.h>

#include "kobolog.h"
#include "config.h"
#include "eel.h"
#include "a_types.h"
#include "a_wca.h"
#include "a_control.h"


/* Enum class handles */

static int enum_aformat, enum_amodtarget, enum_awaveform, enum_afiltertype;
static int enum_amixmode, enum_aresample;

static int enum_pparam, enum_pdriver;


/*----------------------------------------------------------
	AGW Command Callbacks
----------------------------------------------------------*/

static int op_w_reset(int argc, struct eel_data_t *argv)
{
	DBG(log_printf(D2LOG, "w_reset;\n");)
	wca_reset();
	return 1;
}


static int op_w_format(int argc, struct eel_data_t *argv)
{
	int target;
	double fs;
	int format = enum_aformat;
#if 0
	/* Alternative, more verbose method. */
	eel_symbol_t *fmt;
	if(eel_get_args("iEr", &target, &fmt, &fs) != 3)
		return -1;
	if(fmt->token != enum_aformat)
	{
		eel_error("Wrong enum class!");
		return -1;
	}
	DBG(log_printf(D2LOG, "w_format %d, %s, %f;\n", target, eel_s_stringrep(fmt), fs);)
	format = fmt->data.value.i;
#else
	if(eel_get_args("ier", &target, &format, &fs) != 3)
		return -1;
#endif
	DBG(log_printf(D2LOG, "w_format %d, %d, %f;\n", target, format, fs);)
	audio_wave_format(target, format, (int)fs);
	return 1;
}


static int op_w_gain(int argc, struct eel_data_t *argv)
{
	int target;
	if(eel_get_args("i", &target) != 1)
		return -1;
	DBG(log_printf(D2LOG, "w_gain %d;\n", target);)
	wca_gain(target);
	return 1;
}


static int op_w_convert(int argc, struct eel_data_t *argv)
{
	int source, target;
	double fs;
	int format = enum_aformat;
	int interpol = enum_aresample;
	argc = eel_get_args("iie[re]",
			&source, &target, &format,
			&fs, &interpol);
	if(argc < 3)
		return -1;
	if(argc < 4)
		fs = 0;
	if(argc < 5)
		interpol = AR_BEST;
	DBG(log_printf(D2LOG, "w_convert %d, %d, %d, %d, %d;\n", source, target,
			format, (int)fs, interpol);)
	audio_wave_convert(source, target, format, (int)fs, interpol);
	return 1;
}


static int op_w_enhance(int argc, struct eel_data_t *argv)
{
	int target;
	double f, level;
	if(eel_get_args("irr", &target, &f, &level) != 3)
		return -1;
	DBG(log_printf(D2LOG, "w_enhance %d, %d, %f;\n", target, (int)f, level);)
	wca_enhance(target, (int)f, level);
	return 1;
}


static int op_w_gate(int argc, struct eel_data_t *argv)
{
	int target;
	double f, min, thres, att;
	if(eel_get_args("irrrr",
			&target, &f, &min, &thres, &att) != 5)
		return -1;
	DBG(log_printf(D2LOG, "w_gate %d, %d, %f, %f, %f;\n",
			target, (int)f, min, thres, att);)
	wca_gate(target, (int)f, min, thres, att);
	return 1;
}


static int op_w_blank(int argc, struct eel_data_t *argv)
{
	int target, loop;
	unsigned samples;
	argc = eel_get_args("ii[i]", &target, &samples, &loop);
	if(argc < 2)
		return -1;
	if(argc < 3)
		loop = 0;
	DBG(log_printf(D2LOG, "w_blank %d, %d, %d;\n", target, samples, loop);)
	audio_wave_blank(target, samples, loop);
	return 1;
}


static int op_w_load(int argc, struct eel_data_t *argv)
{
	int target, loop;
	const char *fn;
	argc = eel_get_args("is[i]", &target, &fn, &loop);
	if(argc < 2)
		return -1;
	if(argc < 3)
		loop = 0;
	DBG(log_printf(D2LOG, "w_load %d, \"%s\", %d;\n", target, fn, loop);)
	if(audio_wave_load(target, fn, loop) < 0)
		return -1;
	return 1;
}


static int op_w_save(int argc, struct eel_data_t *argv)
{
	int target;
	const char *fn;
	if(eel_get_args("is", &target, &fn) != 2)
		return -1;
	DBG(log_printf(D2LOG, "w_save %d, \"%s\";\n", target, fn);)
	if(audio_wave_save(target, fn) < 0)
		return -1;
	return 1;
}


static int op_w_prepare(int argc, struct eel_data_t *argv)
{
	int target;
	if(eel_get_args("i", &target) < 1)
		return -1;
	DBG(log_printf(D2LOG, "w_prepare %d;\n", target);)
	audio_wave_prepare(target);
	return 1;
}


static int op_w_env(int argc, struct eel_data_t *argv)
{
	int i;
	int modtarget = enum_amodtarget;
	double dur[WCA_MAX_ENV_STEPS];
	double val[WCA_MAX_ENV_STEPS];

	if(argc > 1 + 2*WCA_MAX_ENV_STEPS)
	{
		eel_error("Too many envelope steps!");
		return -1;
	}

	if(eel_get_args("e[<rr>]", &modtarget, &dur, &val) != argc)
		return -1;

	DBG(log_printf(D2LOG, "w_env %d", modtarget);)
	wca_mod_reset(modtarget);
	if(2 == argc)
	{
		/*
		 * Note that dur[0] actually becomes a *value*
		 * rather than a time in this case!
		 */
		DBG(log_printf(D2LOG, ", %f;\n", dur[0]);)
		wca_val(modtarget, dur[0]);
		return 1;
	}
	for(i = 1; i < argc; i += 2)
	{
		int sec = (i-1)/2;
		DBG(log_printf(D2LOG, ", <%f", dur[sec]);)		
		if(i+1 >= argc)
		{
			eel_error("<duration, value> argruments must"
					" come in pairs!");
			return -1;
		}
		DBG(else
			log_printf(D2LOG, ", %f", val[sec]);)
		DBG(log_printf(D2LOG, ">");)
		wca_env(modtarget, dur[sec], val[sec]);
	}
	DBG(log_printf(D2LOG, ";\n");)

	return 1;
}


static int op_w_mod(int argc, struct eel_data_t *argv)
{
	int mt;
	double f, a, d;
	mt = enum_amodtarget;
	if(eel_get_args("errr", &mt, &f, &a, &d) != 4)
		return -1;
	DBG(log_printf(D2LOG, "w_mod %d, %f, %f, %f;\n", mt, f, a, d);)
	wca_mod(mt, f, a, d);
	return 1;
}


static int op_w_osc(int argc, struct eel_data_t *argv)
{
	int target, wf, mm;
	wf = enum_awaveform;
	mm = enum_amixmode;
	switch(eel_get_args("ie[e]", &target, &wf, &mm))
	{
	  case 2:
		mm = WCA_ADD;
	  case 3:
		break;
	  default:
		return -1;
	}
	DBG(log_printf(D2LOG, "w_osc %d, %d, %d;\n", target, wf, mm);)
	wca_osc(target, wf, mm);
	return 1;
}


static int op_w_filter(int argc, struct eel_data_t *argv)
{
	int target, ft;
	ft = enum_afiltertype;
	if(eel_get_args("ie", &target, &ft) != 2)
		return -1;
	DBG(log_printf(D2LOG, "w_filter %d, %d;\n", target, ft);)
	wca_filter(target, ft);
	return 1;
}



/*----------------------------------------------------------
	Patch Command Callbacks
----------------------------------------------------------*/

static int op_p_param(int argc, struct eel_data_t *argv)
{
	int patch, param;
	double value;
	param = enum_pparam;
	if(eel_get_args("ier", &patch, &param, &value) != 3)
		return -1;
	switch(param)
	{
	  case APP_DRIVER:
	  case APP_WAVE:
	  case APP_ENV_SKIP:
	  case APP_LFO_SHAPE:
		audio_patch_param(patch, param, (int)value);
		break;
	  default:
		audio_patch_param(patch, param, (int)(value * 65536.0));
		break;
	}
	DBG(log_printf(D2LOG, "p_param %d, %d, %f;\n", patch, param, value);)
	return 1;
}


static int op_p_driver(int argc, struct eel_data_t *argv)
{
	int patch;
	eel_symbol_t *callback;
	if(eel_get_args("if", &patch, &callback) != 2)
		return -1;
	DBG(log_printf(D2LOG, "p_driver %d, %s;\n", patch, eel_s_stringrep(callback));)
	return 1;
}


/*----------------------------------------------------------
	Init code
----------------------------------------------------------*/

static void load_keywords(void)
{
	/* WCA calls */
	eel_register_operator("w_reset", op_w_reset, 100, 0 ,0);
	eel_register_operator("w_format", op_w_format, 100, 0 ,0);
	eel_register_operator("w_blank", op_w_blank, 100, 0 ,0);
	eel_register_operator("w_load", op_w_load, 100, 0 ,0);
	eel_register_operator("w_save", op_w_save, 100, 0 ,0);
	eel_register_operator("w_prepare", op_w_prepare, 100, 0 ,0);
	eel_register_operator("w_env", op_w_env, 100, 0 ,0);
	eel_register_operator("w_mod", op_w_mod, 100, 0 ,0);
	eel_register_operator("w_osc", op_w_osc, 100, 0 ,0);
	eel_register_operator("w_filter", op_w_filter, 100, 0 ,0);
	eel_register_operator("w_gain", op_w_gain, 100, 0 ,0);
	eel_register_operator("w_convert", op_w_convert, 100, 0 ,0);
	eel_register_operator("w_enhance", op_w_enhance, 100, 0 ,0);
	eel_register_operator("w_gate", op_w_gate, 100, 0 ,0);

	/* w_convert resampling modes */
	enum_aresample = eel_register_enum_class();
	eel_register_enum(enum_aresample, "NEAREST", AR_NEAREST);
	eel_register_enum(enum_aresample, "NEAREST4X", AR_NEAREST_4X);
	eel_register_enum(enum_aresample, "LINEAR", AR_LINEAR);
	eel_register_enum(enum_aresample, "LINEAR2X", AR_LINEAR_2X_R);
	eel_register_enum(enum_aresample, "LINEAR4X", AR_LINEAR_4X_R);
	eel_register_enum(enum_aresample, "LINEAR8X", AR_LINEAR_8X_R);
	eel_register_enum(enum_aresample, "LINEAR16X", AR_LINEAR_16X_R);
	eel_register_enum(enum_aresample, "CUBIC", AR_CUBIC_R);

	eel_register_enum(enum_aresample, "WORST", AR_WORST);
	eel_register_enum(enum_aresample, "MEDIUM", AR_MEDIUM);
	eel_register_enum(enum_aresample, "BEST", AR_BEST);

	/* Sample formats */
	enum_aformat = eel_register_enum_class();
	eel_register_enum(enum_aformat, "MONO8", AF_MONO8);
	eel_register_enum(enum_aformat, "STEREO8", AF_STEREO8);
	eel_register_enum(enum_aformat, "MONO16", AF_MONO16);
	eel_register_enum(enum_aformat, "STEREO16", AF_STEREO16);
	eel_register_enum(enum_aformat, "MONO32", AF_MONO32);
	eel_register_enum(enum_aformat, "STEREO32", AF_STEREO32);

	/* Modulation targets */
	enum_amodtarget = eel_register_enum_class();
	eel_register_enum(enum_amodtarget, "AMPLITUDE", WCA_AMPLITUDE);
	eel_register_enum(enum_amodtarget, "BALANCE", WCA_BALANCE);
	eel_register_enum(enum_amodtarget, "FREQUENCY", WCA_FREQUENCY);
	eel_register_enum(enum_amodtarget, "LIMIT", WCA_LIMIT);
	eel_register_enum(enum_amodtarget, "MOD1", WCA_MOD1);
	eel_register_enum(enum_amodtarget, "MOD2", WCA_MOD2);
	eel_register_enum(enum_amodtarget, "MOD3", WCA_MOD3);

	/* Oscillator output modes */
	enum_amixmode = eel_register_enum_class();
	eel_register_enum(enum_amixmode, "ADD", WCA_ADD);
	eel_register_enum(enum_amixmode, "MUL", WCA_MUL);
	eel_register_enum(enum_amixmode, "FM", WCA_FM);
	eel_register_enum(enum_amixmode, "FM_ADD", WCA_FM_ADD);
	eel_register_enum(enum_amixmode, "SYNC", WCA_SYNC);
	eel_register_enum(enum_amixmode, "SYNC_ADD", WCA_SYNC_ADD);

	/* Oscillator waveforms */
	enum_awaveform = eel_register_enum_class();
	eel_register_enum(enum_awaveform, "DC", WCA_DC);
	eel_register_enum(enum_awaveform, "SINE", WCA_SINE);
	eel_register_enum(enum_awaveform, "HALFSINE", WCA_HALFSINE);
	eel_register_enum(enum_awaveform, "RECTSINE", WCA_RECTSINE);
	eel_register_enum(enum_awaveform, "SINEMORPH", WCA_SINEMORPH);
	eel_register_enum(enum_awaveform, "BLMORPH", WCA_BLMORPH);
	eel_register_enum(enum_awaveform, "BLCROSS", WCA_BLCROSS);
	eel_register_enum(enum_awaveform, "PULSE", WCA_PULSE);
	eel_register_enum(enum_awaveform, "TRIANGLE", WCA_TRIANGLE);
	eel_register_enum(enum_awaveform, "NOISE", WCA_NOISE);
	eel_register_enum(enum_awaveform, "SPECTRUM", WCA_SPECTRUM);
	eel_register_enum(enum_awaveform, "ASPECTRUM", WCA_ASPECTRUM);
	eel_register_enum(enum_awaveform, "HSPECTRUM", WCA_HSPECTRUM);
	eel_register_enum(enum_awaveform, "AHSPECTRUM", WCA_AHSPECTRUM);

	/* Filter types */
	enum_afiltertype = eel_register_enum_class();
	eel_register_enum(enum_afiltertype, "ALLPASS", WCA_ALLPASS);
	eel_register_enum(enum_afiltertype, "LOWPASS_6", WCA_LOWPASS_6DB);
	eel_register_enum(enum_afiltertype, "HIGHPASS_6", WCA_HIGHPASS_6DB);
	eel_register_enum(enum_afiltertype, "LOWPASS_12", WCA_LOWPASS_12DB);
	eel_register_enum(enum_afiltertype, "HIGHPASS_12", WCA_HIGHPASS_12DB);
	eel_register_enum(enum_afiltertype, "BANDPASS_12", WCA_BANDPASS_12DB);
	eel_register_enum(enum_afiltertype, "NOTCH_12", WCA_NOTCH_12DB);
	eel_register_enum(enum_afiltertype, "PEAK_12", WCA_PEAK_12DB);

	/* Patch calls */
	eel_register_operator("p_param", op_p_param, 100, 0 ,0);
	eel_register_operator("p_driver", op_p_driver, 100, 0 ,0);

	/* Patch Parameters */
	enum_pparam = eel_register_enum_class();
	eel_register_enum(enum_pparam, "DRIVER", APP_DRIVER);
	eel_register_enum(enum_pparam, "WAVE", APP_WAVE);
	eel_register_enum(enum_pparam, "RANDPITCH", APP_RANDPITCH);
	eel_register_enum(enum_pparam, "RANDVOL", APP_RANDVOL);

	eel_register_enum(enum_pparam, "ENV_SKIP", APP_ENV_SKIP);
	eel_register_enum(enum_pparam, "ENV_L0", APP_ENV_L0);
	eel_register_enum(enum_pparam, "ENV_DELAY", APP_ENV_DELAY);
	eel_register_enum(enum_pparam, "ENV_T1", APP_ENV_T1);
	eel_register_enum(enum_pparam, "ENV_L1", APP_ENV_L1);
	eel_register_enum(enum_pparam, "ENV_HOLD", APP_ENV_HOLD);
	eel_register_enum(enum_pparam, "ENV_T2", APP_ENV_T2);
	eel_register_enum(enum_pparam, "ENV_L2", APP_ENV_L2);
	eel_register_enum(enum_pparam, "ENV_T3", APP_ENV_T3);
	eel_register_enum(enum_pparam, "ENV_T4", APP_ENV_T4);

	/* Patch Drivers */
	enum_pdriver = eel_register_enum_class();
	eel_register_enum(enum_pdriver, "MONO", PD_MONO);
	eel_register_enum(enum_pdriver, "POLY", PD_POLY);
	eel_register_enum(enum_pdriver, "MIDI", PD_MIDI);
	eel_register_enum(enum_pdriver, "EEL", PD_EEL);
}


/*----------------------------------------------------------
	API code
----------------------------------------------------------*/

static int _agw_initialized = 0;


int agw_open(void)
{
	if(_agw_initialized)
		return 0;

	eel_open();

	eel_push_scope();	/* begin AGW extensions scope */

	load_keywords();

	/* Built-in variables */
	(void)eel_set_integer("target", -1);

	_agw_initialized = 1;
	return 0;
}

void agw_close(void)
{
	if(!_agw_initialized)
		return;

	eel_pop_scope();	/* end AGW extensions scope */

	eel_close();

	_agw_initialized = 0;
}

#define	CHECKINIT	if(!_agw_initialized) agw_open();

int agw_load(int wid, const char *name)
{
	int script, res;

	CHECKINIT

	if(wid < 0)
	{
		wid = audio_wave_alloc(wid);
		if(wid < 0)
			return wid;
	}

	script = eel_load(name);
	if(script < 0)
		return -1;

	eel_push_scope();	/* begin script scope */

	/* Initialize "argument" variables */
	(void)eel_set_integer("target", wid);

	/* Reset the waveform construction engine */
	wca_reset();

	res = eel_run(script);
	eel_free(script);

	eel_pop_scope();	/* end script scope */

	if(res < 0)
		return -1;

	audio_wave_prepare(wid);

	return wid;
}
