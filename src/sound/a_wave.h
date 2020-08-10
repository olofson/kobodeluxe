/*(LGPL)
---------------------------------------------------------------------------
	a_wave.h - Wava Data Manager
---------------------------------------------------------------------------
 * Copyright (C) 2001-2003, 2007 David Olofson
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
 * Note on sample formats and clipping:
 *	* Operations on integer waveforms will saturate if the
 *	  respective integer range is exceeded.
 *
 *	* Operations on floating point waveforms will *not*
 *	  saturate at all, under any circumstances.
 *
 *	* Converting from floating point to integer formats,
 *	  as well as mixing floating point waveforms into
 *	  integer waveforms will saturate if the respective
 *	  integer range is exceeded.
 */

#ifndef _A_WAVE_H_
#define _A_WAVE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "a_types.h"
#include "a_midifile.h"

/*----------------------------------------------------------
	Open/Close (Normally called by the audio engine.)
----------------------------------------------------------*/
void audio_wave_open(void);
void audio_wave_close(void);


typedef enum _audio_free_t
{
	HTF_DONT = 0,
	HTF_FREE
} _audio_free_t;

/*----------------------------------------------------------
	Audio Wave (Raw wave data reference + info)
----------------------------------------------------------*/
typedef struct
{
	/* Format info */
	audio_formats_t	format;
	int		rate;		/* samples/second */
	int		looped;		/* Must be here for preprocessing */

	/* "Real" data */
	union
	{
		Sint8		*si8;
		Sint16		*si16;
		float		*f32;
		midi_file_t	*midi;
	} data;
	unsigned	size;		/* Data size in *bytes* */
	unsigned	xsize;		/* End extension size in *bytes* */

	/* Reference/allocation management */
	_audio_free_t	howtofree;
	unsigned	allocated;

	/* Handy precalculated stuff */
	unsigned	speed;		/* For original sample rate */
	unsigned	samples;	/* Number of *frames*, actually */
	unsigned	play_samples;	/* For the voice mixer... */
} audio_wave_t;

void _audio_wave_init(void);


/*----------------------------------------------------------
	Basic Wave API
----------------------------------------------------------*/

/*
 * Allocate a new wave for later use.
 * If 'id' is -1, a wave id will be allocated automatically.
 *
 * Returns your wave id, or a negative value in the case of
 * failure.
 */
int audio_wave_alloc(int wid);

/*
 * Allocate a range of waves for later use, to prevent the from
 * being stolen by the dynamic allocation system.
 *
 * Returns your first wave id, or a negative value in the case of
 * failure.
 */
int audio_wave_alloc_range(int first_wid, int last_wid);

/*
 * Get the low level struct for waveform 'wid'. (This is
 * mostly meant for low level libraries, not applications.)
 *
 * Returns NULL if the waveform doesn't exist.
 */
audio_wave_t *audio_wave_get(int wid);

/*
 * Set the path to data files.
 *
 * This must be a valid path, terminated by a
 * '/', '\', ':' or whatever "directory separator"
 * the current platform is using, and will be
 * prepended to any external file names used in
 * other calls.
 */
void audio_set_path(const char *path);

/* Returns the current path. */
const char *audio_path(void);

/*
 * Loads a waveform from disk.
 *
 * If 'id' is -1, a new waveform will be allocated automatically.
 *
 * The file name can be the path to an audio file of any supported
 * format.
 *
 * If the file format cannot be identified, the file is assumed to
 * be raw data, and the audio format is assumed to be of the
 * format specified for the waveform. You should set the proper
 * format (using audio_wave_format()) *before* loading a raw audio
 * file, to minimize the risk of extra work for the engine.
 *
 * Returns the id of the new wave, or a negative value in the
 * case of failure.
 */
int audio_wave_load(int wid, const char *name, int looped);

/*
 * Saves a waveform to disk.
 *
FIXME: Temporary kludge:
FIXME: The file will be a raw data file with the first 3 bytes
FIXME: containing ASCII "RAW". The 4th byte contains the format code
FIXME: as defined by the audio_formats_t enum.
 */
int audio_wave_save(int wid, const char *name);

/*
 * "Loads" a waveform from a raw data buffer in memory.
 * The audio engine will *not* free the memory you pass here, but
 * will copy it into a private buffer internally, so your buffer
 * does not have to be static. (The internal copy is often needed
 * anyway due to the need for preprocessing.)
 *
 * The data is assumed to be of the format specified with
 * audio_wave_format().
 *
 * Returns the id of the new wave, or a negative value in the
 * case of failure.
 */
int audio_wave_load_mem(int wid, void *data, unsigned size, int looped);

/*
 * Removes a waveform from memory, and frees the wave id.
 */
void audio_wave_free(int wid);	/* -1 to free all */

/*
 * Specify the data format and sample rate of a waveform.
 *
 * If 'id' is -1, a new waveform will be allocated automatically.
 *
 * Note that this call *will not* convert any data! This call
 * should normally not be used except when loading raw audio
 * files.
 *
 * Returns the id of the new wave, or a negative value in the
 * case of failure. (Yes, it *can* fail, as it may have to
 * reallocate the wave data buffer!)
 */
int audio_wave_format(int wid, audio_formats_t fmt, int fs);

/*
 * Creates a silent wave of the specified size in *samples*.
 *
 * The data will be of the format specified with
 * audio_wave_format().
 *
 * Returns the id of the new wave, or a negative value in the
 * case of failure.
 */
int audio_wave_blank(int wid, unsigned samples, int looped);

/*
 * Convert a waveform from it's current format into the specified
 * format. If there is no data, you just get a new empty wave of
 * the specified format, or if new_vwid == wid, the format is
 * changed into the one you specify.
 *
 * If new_wid is -1, a new wave will be allocated automatically.
 * new_wid may equal wid, for "in-place" conversion.
 *
 * By setting either 'fmt' to -1 or 'fs' to 0, the respective
 * parameter is copied from the original wave. Setting both to
 * -1 respectively 0 is equivalent to calling audio_wave_clone().
 *
 * 'resamp' is the resampling method to use, if resampling is
 * needed.
 *
 * Returns the new wave id, or a negative value in the case of
 * failure.
 */
int audio_wave_convert(int wid, int new_wid, audio_formats_t fmt,
		int fs, audio_resample_t resamp);

/*
 * Copy a waveform.
 *
 * If new_wid is -1, a new wave will be allocated automatically.
 *
 * Returns the clone wave id, or a negative value in the case of
 * failure.
 */
int audio_wave_clone(int wid, int new_wid);

/*
 * Converts a waveform (if needed) and prepares it for playback
 * by the real time synth engine. Passing -1 for 'wid' will
 * prepare *all* waveforms that aren't ready for playback
 * already.
 */
void audio_wave_prepare(int wid);

/*
 * Dump waveform info to log.
 */
void audio_wave_info(int wid);

#ifdef __cplusplus
};
#endif

#endif /*_A_WAVE_H_*/
