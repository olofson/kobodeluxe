/*(LGPL)
---------------------------------------------------------------------------
	a_midi.c - midi_socket_t MIDI Input Driver for OSS or ALSA
---------------------------------------------------------------------------
 * Copyright (C) 2001, 2003, David Olofson
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
 * NOTE: This code will build for ALSA *or* OSS, in that order of preference.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include "kobolog.h"
#include "a_midi.h"

#ifdef HAVE_ALSA
# include <sys/asoundlib.h>
# define USE_MIDI
#elif defined(HAVE_OSS)
# include <fcntl.h>
# include <unistd.h>
# define USE_MIDI
#else
# undef USE_MIDI
#endif

#define	MDBG(x)
#define	MDBG2(x)


#ifdef USE_MIDI

/* MIDI open/close */
#ifdef HAVE_ALSA
static snd_rawmidi_t *m_in_fd = NULL;
static snd_rawmidi_t *m_out_fd = NULL;
#elif defined(HAVE_OSS)
static int m_fd = 0;
#endif


/* MIDI I/O */
#ifdef HAVE_ALSA
# define	midi_read(b,s)	snd_rawmidi_read(m_in_fd, b, s)
# define	midi_write(b,s)	snd_rawmidi_write(m_out_fd, b, s)
#elif defined(HAVE_OSS)
# define	midi_read(b,s)	read(m_fd, b, s)
# define	midi_write(b,s)	write(m_fd, b, s)
#endif


/*
 * Basic real time MIDI parsing
 */

static midisock_t *ms;
static midisock_t *mon;

/* Global state stuff - *not* per channel! */
static int m_channel = 0;	/* current channel */
static int m_datacount = 0;	/* current data byte */
static char m_status = 0;	/* current (last seen) status */
static char m_buf[3];		/* space for data bytes */
static int m_expect = -1;	/* expected message data length */

static inline void decode_midi_message(void)
{
	/* Seems like we've got a complete message! */
	int val;
	switch(m_status & 0xf0)
	{
	  case 0x80:		/* Note off */
		MDBG2(mon->note_off(m_channel, m_buf[0], m_buf[1]);)
		ms->note_off(m_channel, m_buf[0], m_buf[1]);
		break;
	  case 0x90:		/* Note on */
		MDBG2(mon->note_on(m_channel, m_buf[0], m_buf[1]);)
		ms->note_on(m_channel, m_buf[0], m_buf[1]);
		break;
	  case 0xa0:		/* Poly pressure */
		MDBG2(mon->poly_pressure(m_channel, m_buf[0], m_buf[1]);)
		ms->poly_pressure(m_channel, m_buf[0], m_buf[1]);
		break;
	  case 0xb0:		/* Control change */
		MDBG2(mon->control_change(m_channel, m_buf[0], m_buf[1]);)
		ms->control_change(m_channel, m_buf[0], m_buf[1]);
		break;
	  case 0xc0:		/* Program change */
		MDBG2(mon->program_change(m_channel, m_buf[0]);)
		ms->program_change(m_channel, m_buf[0]);
		break;
	  case 0xd0:		/* Channel pressure */
		MDBG2(mon->channel_pressure(m_channel, m_buf[0]);)
		ms->channel_pressure(m_channel, m_buf[0]);
		break;
	  case 0xe0:		/* Pitch bend */
		val = ((m_buf[1] << 7) | m_buf[0]) - 8192;
		MDBG2(mon->pitch_bend(m_channel, val);)
		ms->pitch_bend(m_channel, val);
		break;
	  case 0xf0:		/* System message */
		break;
	}
	m_datacount = 0;
}

static const int full_xtab[8] = {
	2,	/* 0x80/0: Note off, */
	2,	/* 0x90/1: Note on */
	2,	/* 0xa0/2: Poly pressure */
	2,	/* 0xb0/3: Control change */
	1,	/* 0xc0/4: Program change */
	1,	/* 0xd0/5: Channel pressure */
	2,	/* 0xe0/6: Pitch bend */
	-1	/* 0xf0/7: System message (unsupported) */
};

static int xtab[8];

static inline void decode_midi_byte(unsigned char b)
{
	if(b & 0x80)		/* New status! */
	{
		MDBG(log_printf(D3LOG, "\n%2.2X", b & 0xff));
		m_status = b;
		m_datacount = 0;
		m_channel = b & 0x0f;
		m_expect = xtab[(m_status >> 4) & 0x07];
		return;
	}

	MDBG(log_printf(D3LOG, " %2.2X", b));
	if(m_expect < 0)
		return;		/* Ignore --> */

	m_buf[m_datacount++] = b;

	if(m_datacount >= m_expect)
		decode_midi_message();
}

#endif /*USE_MIDI*/


int midi_open(midisock_t *output)
{
#ifdef USE_MIDI
	if(!output)
		output = &dummy_midisock;
	ms = output;

	/*
	 * Set up the message length table to ignore messages
	 * that aren't supported by the output midisock.
	 */
	memset(xtab, -1, sizeof(xtab));
	if(output->note_off)
		xtab[0] = full_xtab[0];
	if(output->note_on)
		xtab[1] = full_xtab[1];
	if(output->poly_pressure)
		xtab[2] = full_xtab[2];
	if(output->control_change)
		xtab[3] = full_xtab[3];
	if(output->program_change)
		xtab[4] = full_xtab[4];
	if(output->channel_pressure)
		xtab[5] = full_xtab[5];
	if(output->pitch_bend)
		xtab[6] = full_xtab[6];

	mon = &monitor_midisock;
#ifdef HAVE_ALSA
	if(snd_rawmidi_open(&m_in_fd, 0, 0, SND_RAWMIDI_OPEN_INPUT |
			SND_RAWMIDI_OPEN_NONBLOCK))
	{
		log_printf(ELOG, "Failed to open MIDI input device!\n");
		return -1;
	}
# ifdef MIDI_THRU
	if(snd_rawmidi_open(&m_out_fd, 0, 0, SND_RAWMIDI_OPEN_OUTPUT))
	{
		log_printf(ELOG, "Failed to open MIDI output device!\n");
		return -2;
	}
# endif
	return 0;
#elif defined(HAVE_OSS)
	{
# ifdef MIDI_THRU
		int flags = O_RDWR | O_NONBLOCK;
# else
		int flags = O_RDONLY | O_NONBLOCK;
# endif
		m_fd = open("/dev/midi00", flags);
		if(m_fd < 0)
		{
			log_printf(ELOG, "Failed to open MIDI device!\n");
			return -1;
		}
		return 0;
	}
#endif /*HAVE_OSS*/
#else /*USE_MIDI*/
	log_printf(ELOG, "MIDI support not compiled in!\n");
	return -3;
#endif /*USE_MIDI*/
}


void midi_close(void)
{
#ifdef USE_MIDI
#ifdef HAVE_ALSA
	if(m_in_fd)
	{
		snd_rawmidi_input_flush(m_in_fd);
		snd_rawmidi_close(m_in_fd);
	}
	if(m_out_fd)
	{
		snd_rawmidi_output_flush(m_out_fd);
		snd_rawmidi_close(m_out_fd);
	}
	m_in_fd = m_out_fd = NULL;
#elif defined(HAVE_OSS)
	if(m_fd)
		close(m_fd);
	m_fd = 0;
#endif
#endif /*USE_MIDI*/
}


void midi_process(void)
{
#ifdef USE_MIDI
	int bytes;
	static char buf[MIDIMAXBYTES];
	if( (bytes = midi_read(buf, MIDIMAXBYTES)) > 0 )
	{
		char *bufp = buf;
#ifdef MIDI_THRU
		midi_write(buf, bytes);	/* MIDI Thru */
#endif
		while(bytes--)
			decode_midi_byte(*bufp++);
	}
#endif /*USE_MIDI*/
}
