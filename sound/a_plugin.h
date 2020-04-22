/*(LGPL)
---------------------------------------------------------------------------
	a_plugin.h - Audio Engine Plugin API
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

#ifndef _A_PLUGIN_H_
#define _A_PLUGIN_H_

#include "a_types.h"

/*
 * Plugin States
 *	Hosts are allowed to inc/dec the FXC_STATE control
 *	by only 1 unit at a time. If you want to "leap", you
 *	must work through the the states in between, to allow
 *	the plugin to do the required actions for each
 *	transition. The toolkit function audio_plugin_state()
 *	handles this automatically.
 *
 *	This arrangement makes it easier to implement plugins;
 *	just check whether the incoming FXC_STATE argument is
 *	higher or lower than the current value, to select one
 *	of two switch() statements; one with code for entering
 *	states, and another for leaving states.
 *
 * What To Do in the case of a transition from...
 *	CLOSED to OPEN:
 *		Create private instance user data, and set
 *		p->user to point at it.
 *
 *	OPEN to READY:
 *		Allocate larger buffers, and buffers that
 *		depend on system parameter settings.
 *		
 *	READY to PAUSED:
 *		Here you're *not* supposed to do anything
 *		that couldn't be done in "interrupt context"
 *		or similar, as this change may be made from
 *		within the actual real time engine core.
 *		Preferably, any time consuming buffer
 *		clearing and stuff should be done in the
 *		OPEN -> READY and PAUSED -> READY
 *		transitions instead of here.
 *
 *	PAUSED to RUNNING:
 *	RUNNING to PAUSED:
 *		In most cases, you should do absolutely
 *		nothing here. If your plugin cares about the
 *		world outside, these transitions can be used
 *		to manage the real world/engine time slip
 *		when pausing.
 *
 *	PAUSED to READY:
 *		Now you may kill reverb tails and that sort
 *		of things.
 *
 *	READY to OPEN:
 *		Free any system parameter dependent buffers.
 *		Basically, free everything except what you
 *		need to store any incoming system parameter
 *		values.
 *		
 *	OPEN to CLOSED:
 *		Delete any private instance data, and set
 *		p->user to NULL.
 *
 *	The SILENT and RESTING states:
 *		This is just a special case of the RUNNING
 *		state (see below). It's *not* possible to
 *		force a plugin to this state - plugins may,
 *		but do not have to, switch between the
 *		RUNNING and SILENT states by themselves.
 *
 * What To Expect, and How To Act when in the...
 *	CLOSED state:
 *		Well, basically, you don't exist yet! Expect
 *		someone to switch you into the OPEN state,
 *		passing a fresh audio_plugin_t with a NULL
 *		'user' field for you to fill in.
 *
 *	OPEN state:
 *		You must accept calls to control() for setting
 *		system parameters, but you should only store
 *		the values (or whatever you calculate from
 *		them) for later use. Other parameter changes
 *		are not allowed.
 *
 *	READY state:
 *		If system parameters are changed while in the
 *		READY state, they are expected to take effect
 *		immediately. Preferably, work as quickly as
 *		possible, but assume that no one is stupid
 *		enough to tell you to reallocate you buffers
 *		from within a real time thread.
 *		
 *	PAUSED state:
 *		In this state, there will be no calls to
 *		process() or control(). (After a thread has
 *		put a plugin in this state, it's safe to
 *		operate the plugin from another thread, and
 *		then have that thread change the state back
 *		to RUNNING when done.)
 *
 *	RUNNING state:
 *		Same as for the PAUSED state, but of course,
 *		frequent calls to process() are to be expected.
 *
 *		RUNNING, SILENT and RESTING are the *only*
 *		states in which process() may be called.
 *
 *	    IMPORTANT:
 *			Note that process*() may get NULL
 *			input buffer pointers, which is a way
 *			for the host to say that you have no
 *			input.
 *
 *		The easiest way to handle that is just grabbing
 *		the ever present audio_silent_buffer (provided
 *		by the host) and go on.
 *
 *		The in-place process() call will never get
 *		NULL input buffer pointers, as it has no
 *		separate output buffer pointer. This requires
 *		a special case: the process() callback must
 *		*always* do it's work, thus effectively
 *		ignoring the SILENT and RESTING states.
 *
 *		However, the idea is that you should consider
 *		this as a useful hint! Most DSP algorithms,
 *		when given silent input, will eventually
 *		produce silent output. When that point is
 *		reached, a plugin should spontaneously switch
 *		to the SILENT state, to tell everyone that
 *		there will be no output for a while, and thus,
 *		that there's no point in calling process().
 *
 *	SILENT state:
 *		Just handle control() calls. process() will
 *		be called as usual, but you're not required to
 *		do anything with the output buffers. (Or
 *		rather, you *shouldn't*, as it would just be
 *		a waste of CPU cycles.)
 *
 *		As soon as you feel like producing output
 *		again, switch back to the RUNNING state and
 *		do so.
 *
 *		If you know that you will not produce output
 *		again, until you get fresh input, switch to
 *		the RESTING state instead!
 *
 *		Host designers should note that a plugin in
 *		the SILENT state *is* actually still running,
 *		and also that it's most probably *not*
 *		producing valid output! It's also important
 *		that output buffer pointers passed to
 *		process() are valid at all times, as a SILENT
 *		plugin may switch to the RUNNING state at any
 *		time, for no obvious reason whatsoever. Input
 *		is *not* required.)
 *
 *		That is, it will need process() to be called
 *		as usual, *even* if only to pass NULL input
 *		buffers. This is to allow plugins to keep
 *		track of time, and of course, to detect any
 *		input that might - instantaneously or after
 *		some time - make the plugin switch back to
 *		RUNNING mode.
 *
 *	RESTING state:
 *		Do nothing (except the usual handling of
 *		control() calls) until you start getting
 *		input buffers again. You'r *not* allowed to
 *		switch from this state unless you get
 *		non-NULL input buffer pointers.
 *
 *		This state exists to tell the host that a
 *		plugin is *really* done with any tails and
 *		stuff, and that there will be no more output
 *		without fresh input buffers. Hosts may use
 *		this to figure out when to stop recording
 *		and things like that.
 *
 *	    IMPORTANT:	You should *NEVER* return from
 *			process() in the SILENT or RESTING
 *			states unless the buffer you just
 *			generated (or should have generated)
 *			actually *is* garbage, or silent!
 *
 *			ANY OUTPUT WILL BE IGNORED.
 *
 * General Rules for Hosts:
 *	* System Parameter changes must *never* be done from
 *	  real time context, period.
 *
 *	* Plugins should *not* be expected to be thread safe.
 *	  That is, never use a plugin from more than one thread
 *	  at a time, without making sure only one callback is
 *	  running at any time.
 */
typedef enum
{
	FX_STATE_CLOSED = 0,
	FX_STATE_OPEN,
	FX_STATE_READY,
	FX_STATE_PAUSED,
	FX_STATE_RUNNING,
	FX_STATE_SILENT,
	FX_STATE_RESTING
} audio_fxstates_t;

typedef enum
{
	/*
	 * System Parameters.
	 * Will be set before the state is changed.
	 */
	FXC_MAX_FRAMES = 0,	/* ...per process() call. */
	FXC_SAMPLERATE,		/* Buffer sample rate (Hz) */
	FXC_QUALITY,		/* Audio quality setting */

	/* Uniform Parameters */
	FXC_PARAM_1,		/* (fixp) Time/f/Amount 1 */
	FXC_PARAM_2,		/* (fixp) Time/f/Amount 2 */
	FXC_PARAM_3,		/* (fixp) Depth/Level 1 */
	FXC_PARAM_4,		/* (fixp) Depth/Level 2 */
	FXC_PARAM_5,		/* (fixp) Rate/Level */
	FXC_PARAM_6,		/* (int)  Mode/Special */

	FXC_USER,		/* First user defined control */
	FXC_COUNT = FXC_USER

	/* Higher indices are free to use for custom controls. */
} audio_fxcontrols_t;

#ifdef	TODO
typedef enum
{
	/* Timing and dependencies (flags) */
	FXCAP_TIMING_		= 0x0000000f,
	FXCAP_TIMING_SYSCALLS	= 0x00000001,	/* Uses malloc() etc... */
	FXCAP_TIMING_SLOW	= 0x00000002,	/* in relation to process() */

	/* Data type (enumeration) */
	FXCAP_TYPE_		= 0x000000f0,
	FXCAP_TYPE_STROBE	= 0x00000000,	/* Event; value ignored */
	FXCAP_TYPE_BOOLEAN	= 0x00000010,	/* 0 = false, !=0 = true */
	FXCAP_TYPE_INTEGER	= 0x00000020,
	FXCAP_TYPE_FIXED_8	= 0x00000030,	/* 8 fraction bits */
	FXCAP_TYPE_FIXED_16	= 0x00000040,	/* 16 fraction bits */
	FXCAP_TYPE_FIXED_24	= 0x00000050,	/* 24 fraction bits */
	FXCAP_TYPE_FLOAT	= 0x00000060,	/* IEEE 32 bit float */

	/* Access rules (flags) */
	FXCAP_AXS_		= 0x00000f00,
	FXCAP_AXS_READ		= 0x00000100,	/* May be read */
	FXCAP_AXS_WRITE		= 0x00000200,	/* May be written */
	FXCAP_AXS_READ_CALL	= 0x00000400,	/* Must use read_control()! */
	FXCAP_AXS_WRITE_CALL	= 0x00000800,	/* Must use control()! */

	/* Lifetime info - "When can I access this?" (flags)
	 * Note that there's no flag for PAUSED, as it's *never* legal
	 * to use any callback but state() in the PAUSED state.
	 */
	FXCAP_LIFE_		= 0x0000f000,
	FXCAP_LIFE_OPEN		= 0x00001000,
	FXCAP_LIFE_READY	= 0x00002000,
	FXCAP_LIFE_RUNNING	= 0x00004000
} audio_fxcaps_t;
#endif


typedef struct audio_plugin_t
{
	void		*user;		/* Custom instance data */
	int		current_state;	/* DO NOT write in callbacks! */

	unsigned	controls;	/* Number of controls */
	int		*ctl;		/* Array of parameter values */
#ifdef	TODO
	audio_fxcaps_t	*control_caps;
#endif
	/* Plugin state management */
	int (*state)(struct audio_plugin_t *p, audio_fxstates_t new_state);

	/* Parameter Control */
	void (*control)(struct audio_plugin_t *p, unsigned ctl, int arg);

	/* Running the plugin */
	void (*process)(struct audio_plugin_t *p,
			int *buf, unsigned frames);
	void (*process_r)(struct audio_plugin_t *p,
			int *in, int *out, unsigned frames);
	void (*process_m)(struct audio_plugin_t *p,
			int *in, int *out, unsigned frames);
} audio_plugin_t;


/*----------------------------------------------------------
	Handy wrappers & tools
----------------------------------------------------------*/

/*
 * Bring a plugin to the specified state.
 *
 * If anything goes wrong, this call stops and returns
 * the error code from the plugin's state() callback.
 */
int audio_plugin_state(audio_plugin_t *p, audio_fxstates_t new_state);

/*
 * Fill in missing callbacks with standard wrappers where
 * possible.
 *
 * IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT! IMPORTANT!
 *	Note that these wrappers are *not* thread safe
 *	as of this version! Some of them use a single
 *	global mixing buffer when emulating missing
 *	process callback variants. (The alternative
 *	would be to use one buffer per plugin instance,
 *	which would result in more cache thrashing.)
 *
 * Returns a negative error code if it isn't possible to
 * turn the passed struct into a valid plugin.
 */
int audio_plugin_init(audio_plugin_t *p);

/*
 * Initialize a plugin (audio_plugin_init()), bring it
 * plugin into the OPEN state and set up the system
 * parameters.
 *
 * Returns any error code from the plugin.
 */
int audio_plugin_open(audio_plugin_t *p, unsigned max_frames,
		int fs, int quality);

/*
 * Bring a plugin to the CLOSED state.
 */
void audio_plugin_close(audio_plugin_t *p);


/*----------------------------------------------------------
	Tools for plugins (NOT for hosts!)
----------------------------------------------------------*/

/*
 * Set number of controls and allocate a ctl array with
 * sufficient space for them.
 *
 * Note that freeing the array is done by the host. (Using
 * audio_plugin_close() or audio_plugin_state() to close a
 * plugin does that automatically, so only hosts that
 * implement state changing "to the metal" have to worry
 * about this.)
 *
 * Returns the address of the allocated ctl array.
 */
int *audio_plugin_alloc_ctls(audio_plugin_t *p, unsigned count);


/*----------------------------------------------------------
	Host Provided Services
----------------------------------------------------------*/

/* The Silent Buffer */
extern int *audio_silent_buffer;

#endif /*_A_PLUGIN_H_*/
