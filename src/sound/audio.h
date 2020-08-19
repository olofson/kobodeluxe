/*(GPL)
---------------------------------------------------------------------------
	audio.h - Public Audio Engine Interface
---------------------------------------------------------------------------
 * Copyright (C) 19??, Masanao Izumo
 * Copyright (C) 2001-2003, 2007 David Olofson
 *
 * Written for SGI DMedia API by Masanao Izumo <mo@goice.co.jp>
 * Mostly rewritten by David Olofson <do@reologica.se>, 2001
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "config.h"
#include "a_types.h"
#include "a_commands.h"
#include "a_wave.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define this to get range checking in the API entry calls. */
#define	AUDIO_SAFE

/* Limits - exceed these and things may blow up! */
#define AUDIO_MIN_OUTPUT_RATE	8000
#define AUDIO_MAX_OUTPUT_RATE	48000
#define AUDIO_MAX_MIX_RATE	128000
#define	AUDIO_MAX_OVERSAMPLING	16

/* Sounds */
#define AUDIO_MAX_WAVES		128
#define AUDIO_MAX_PATCHES	128

/* Control */
/* Grous and channels; virtually no per-unit overhead. */
#define AUDIO_MAX_GROUPS	8
#define AUDIO_MAX_CHANNELS	32

/*
 * Changes (David):
 *------------------
 * play_stop(-1) feature removed.
 *	Silent channels are now released automatically.
 *
 * get_audio_volume() removed.
 *	It cannot safely be used the way XKobo used it
 *	due to the asynchronous nature of the modified
 *	audio engine. (Background music attenuation is
 *	now done in a more reliable way.)
 *
 * play_loop() removed.
 *	Not needed with a callback or thread driven
 *	audio engine.
 *
 * API cleaned up; all function names now begin with
 * 'audio_'.
 *
 * audio_control() API added.
 *
 *
 * Changes in 0.4-pre6:
 *----------------------
 * audio_volume() removed, as it was redundant.
 *
 * AC_SEND_# dry/fx level controls added.
 *
 * AC_GROUP added.
 *
 * The term "channel" is replaced with "voice".
 *
 * The new term "channel" is different from "voice".
 *	Just like before, channel indices are used as
 *	addresses when starting/stopping sounds and
 *	sending control commands. However, the "new"
 *	channels are internally separated from the
 *	engine's mixing voices, which means that it's
 *	possible to implement polyphonic channels and
 *	dynamic channel allocation.
 *
 * New term "group" introduced.
 *	Channels are organized in groups. A group is
 *	physically a structure that holds "modifications"
 *	that are applied as control commands are passed
 *	from channels belonging to the group to the
 *	voices controlled by the respective channels.
 *	What this means is basically that a group can
 *	be used as a central master control for all
 *	channels that belong to it.
 *
 * Volume/send format changed to 16:16 fixed point.
 *
 * Float API added, where 1.0 corresponds to
 * 0x0001.0000.
 *
 * Master volume and reverb API now uses float values.
 *
 * AC_PLAYRESET renamed AC_DETACH.
 *	The new version works in a slightly different way
 *	to AC_PLAYRESET.
 *
 *	Normally (AC_DETACH == 0), the engine behaves
 *	like a MIDI synth; all control commands to a
 *	channel instantly affects *all* voices started
 *	from that channel. Voices are not disconnected
 *	until they stop playing. Group default control
 *	values are never used after initialization.
 *
 *	With AC_DETACH enabled, audio_play() disconnects
 *	voices, so that they are not affected by any
 *	further control commands. That is, control
 *	commands to a new voice have to be sent *before*
 *	the voice is actually started! After a voice has
 *	been started and detached, all channel controls
 *	are reset to the defaults for the group which
 *	the channel belongs to.
 *
 *	Note that audio_stop() *always* stops all voices
 *	started from a channel.
 *
 *	Note that audio_group_control() is affected by
 *	the AC_DETACH feature as well. As it's
 *	impossible to update detached voices properly,
 *	audio_group_control() never touches AC_DETACH
 *	mode channels directly. New voices are still
 *	affected when they're launched, as that's when
 *	AC_DETACH mode channels check the controls of
 *	their parent groups.
 *
 * Removed the hardcoded 8 kHz default AC_SPEED.
 *
 * *Lots* of other changes, like:
 *	* Switched to channel->bus->master architecture.
 *	* Added mono8, mono16 and stereo16 data support.
 *	* Added high quality voice mixers.
 *	* Added uniform plugin API.
 *	* Cleaned up engine and API namespace.
 *
 * Changes in 0.4pre7:
 *---------------------
 * More than I can keep track of... Some of it is in
 * ChangeLog, I hope! :-)
 *
 * Changes in 0.4pre8:
 *---------------------
 * (See Kobo Deluxe 0.4pre8 ChangeLog.)
 */

/*----------------------------------------------------------
	Init/close
----------------------------------------------------------*/
/*
 * Initialize audio engine. Will do nothing if called when
 * the engine is already open.
 *
 * Returns 0 on success, or a negative value in the case
 * of failure.
 */
int audio_open(void);

/*
 * Start audio engine. Can also be used for restarting
 * the engine with new parameters. Will open the engine
 * first if required. Any loaded sounds and patches (if
 * the engine is open, that is) are unaffected.
 *
 * Returns 0 on success, or a negative value in the case
 * of failure.
 */
int audio_start(int rate, int latency, int use_oss, int use_midi, int pollaudio);

/*
 * "Driver" call for engine low priority housekeeping
 * work. Call this "frequently" - at least ten times per
 * second if possible.
 *
 *	Note that while this call may not do anything
 *	at all on some platforms, it could provide
 *	CRITICAL SERVICES to the engine on others!
 *
 * (Mac OS Classic would be an example of the latter;
 * since the engine is running in interrupt context,
 * plugins must be instantiated and destroyed in the
 * context of audio_run() instead.)
 *
 * With "pollaudio" enabled:
 *	Fills up the driver's audio buffer, calling the
 *	engine callback as needed to generate data. In
 *	this mode, audio_run() must be called rather
 *	frequently to avoid drop-outs. (The exact
 *	requirement depends on the configured latency.)
 */
void audio_run(void);

/* Returns the estimated time of the next callback */
int audio_next_callback(void);

/*
 * Stop the audio engine. Does not unload sounds or
 * patches.
 */
void audio_stop(void);

/*
 * Stops the audio engine if running, and then unloads
 * all waves & patches.
 */
void audio_close(void);

void audio_quality(audio_quality_t quality);
void audio_set_limiter(float thres, float rels);

/*
 * Patch Construction (low level)
 */
void audio_patch_param(int pid, int param, int arg);
void audio_patch_paramf(int pid, int param, float arg);
void audio_patch_control(int pid, int layer, int ctl, int arg);
void audio_patch_controlf(int pid, int layer, int ctl, float arg);

#ifdef __cplusplus
};
#endif

#endif /* _AUDIO_H_ */
