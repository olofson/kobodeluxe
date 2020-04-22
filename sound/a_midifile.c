/*(LGPL)
------------------------------------------------------------
	a_midifile.c - MIDI file loader and player
------------------------------------------------------------
 * Stolen from the AdPlug player, stripped down,
 * translated into C, and adapted for the Audio Engine
 * by David Olofson, 2002.
 *
 * Below is the original copyright. Of course, the LGPL
 * license still applies.
------------------------------------------------------------
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999, 2000, 2001 Simon Peter, <dn.tlp@gmx.net>, et al.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * MIDI & MIDI-like file player - Last Update: 8/16/2000
 *                  by Phil Hassey - www.imitationpickles.org
 *                                   philhassey@hotmail.com
 *
 * Can play the following
 *      .LAA - a raw save of a Lucas Arts Adlib music
 *             or
 *             a raw save of a LucasFilm Adlib music
 *      .MID - a "midi" save of a Lucas Arts Adlib music
 *           - or general MIDI files
 *      .CMF - Creative Music Format
 *      .SCI - the sierra "midi" format.
 *             Files must be in the form
 *             xxxNAME.sci
 *             So that the loader can load the right patch file:
 *             xxxPATCH.003  (patch.003 must be saved from the
 *                            sierra resource from each game.)
 *
 * 6/2/2000:  v1.0 relased by phil hassey
 *      Status:  LAA is almost perfect
 *                      - some volumes are a bit off (intrument too quiet)
 *               MID is fine (who wants to listen to MIDI vid adlib anyway)
 *               CMF is okay (still needs the adlib rythm mode implemented
 *                            for real)
 * 6/6/2000:
 *      Status:  SCI:  there are two SCI formats, orginal and advanced.
 *                    original:  (Found in SCI/EGA Sierra Adventures)
 *                               played almost perfectly, I believe
 *                               there is one mistake in the instrument
 *                               loader that causes some sounds to
 *                               not be quite right.  Most sounds are fine.
 *                    advanced:  (Found in SCI/VGA Sierra Adventures)
 *                               These are multi-track files.  (Thus the
 *                               player had to be modified to work with
 *                               them.)  This works fine.
 *                               There are also multiple tunes in each file.
 *                               I think some of them are supposed to be
 *                               played at the same time, but I'm not sure
 *                               when.
 *  8/16/200:
 *      Status:  LAA: now EGA and VGA lucas games work pretty well
 *
 * Other acknowledgements:
 *  Allegro - for the midi instruments and the midi volume table
 *  SCUMM Revisited - for getting the .LAA / .MIDs out of those
 *                    LucasArts files.
 *  FreeSCI - for some information on the sci music files
 *  SD - the SCI Decoder (to get all .sci out of the Sierra files)
 */

/*
TODO:
	* Number of subsongs and other stuff should be checked
	  in mf_open(), rather than in mp_rewind().
*/

#undef	TESTING
#undef	TESTING2

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "kobolog.h"
#include "a_midifile.h"


#ifdef TESTING
#define midiprintf log_printf
#else
static inline void midiprintf(int level, const char *format, ...)
{
}
#endif

#ifdef TESTING2
#define midiprintf2 log_printf
#else
static inline void midiprintf2(int level, const char *format, ...)
{
}
#endif

static inline unsigned char datalook(midi_player_t *mp, unsigned pos)
{
	if(pos < 0 || pos > mp->mf->flen)
		return 0;
	return mp->mf->data[pos];
}


static inline unsigned getnext(midi_player_t *mp, unsigned num)
{
	unsigned v = 0;
	unsigned i;
	for(i = 0; i < num; i++)
	{
		v <<= 8;
		v += datalook(mp, mp->pos);
		mp->pos++;
	}
	return v;
}


static inline unsigned getval(midi_player_t *mp)
{
	unsigned v = 0;
	unsigned char b;

	b = (unsigned char)getnext(mp, 1);
	v = b & 0x7f;
	while((b & 0x80) != 0)
	{
		b = (unsigned char)getnext(mp, 1);
		v = (v << 7) + (b & 0x7F);
	}
	return v;
}


static inline void set_tempo(midi_player_t *mp, unsigned ppqn, unsigned usqtr)
{
	mp->c.ppqn = ppqn;
	mp->c.usqtr = usqtr;
	mp->c.spulse = (float)usqtr / (float)ppqn / 1000000.0;
	midiprintf(D2LOG, "ppqn = %u  ", ppqn);
	midiprintf(D2LOG, "µsqtr = %u  ", usqtr);
	midiprintf(D2LOG, "spulse = %f  ", mp->c.spulse);
}


static int matchnext(midi_player_t *mp, const char *s, unsigned len)
{
	while(len--)
		if(getnext(mp, 1) != (unsigned)(((int)*s++) & 0xff))
			return 0;
	return 1;
}


static int readheader(midi_player_t *mp, const char *s)
{
	if(!matchnext(mp, s, 4))
		return -1;

	return (int)getnext(mp, 4);
}


static int printnext(midi_player_t *mp, unsigned len)
{
#ifdef TESTING
	midiprintf(D2LOG, "\"");
	while(len--)
	{
		unsigned int ch = getnext(mp, 1);
		if(ch < 32)
			midiprintf(D2LOG, ".");
		else
			midiprintf(D2LOG, "%c", (char)ch);
	}
	midiprintf(D2LOG, "\"");
#else
	while(len--)
		getnext(mp, 1);
#endif
	return 1;
}

/* FIXME: This won't work if there are signature changes... */
static void mp_print_time(midi_player_t *mp, unsigned pulses)
{
#ifdef TESTING
	unsigned beatval, beat, tick, bar;
	if(mp->c.timesig.value)
		beatval = mp->c.ppqn * 4 / mp->c.timesig.value;
	else
		beatval = 120;
	beat = pulses / beatval;
	tick = pulses % beatval;
	if(mp->c.timesig.beats)
	{
		bar = beat / mp->c.timesig.beats;
		beat = beat % mp->c.timesig.beats;
	}
	else
	{
		bar = beat / 4;
		beat = beat % 4;
	}
	midiprintf(D2LOG, "(%u:%u:%u)", bar+1, beat+1, tick);
#endif
}


static long filelength(FILE *f)
{
	long buf, size;
	buf = ftell(f);
	if(buf < 0)
		return buf;
	size = fseek(f, 0, SEEK_END);
	if(size < 0)
		return size;
	size = ftell(f);
	if(size < 0)
		return size;
	fseek(f, buf, SEEK_SET);
	return size;
}


midi_file_t *mf_open(const char *name)
{
	FILE *f;
	unsigned char s[6];
	midi_file_t *mf = calloc(1, sizeof(midi_file_t));
	if(!mf)
		return NULL;

	mf->title = mf->author = "<Unknown>";
	mf->remarks = "<None>";

	f = fopen(name, "rb");
	if(!f)
	{
		free(mf);
		return NULL;
	}

	if(fread(s, 6, 1, f) != 1)
	{
		fclose(f);
		free(mf);
		return NULL;
	}

	if(!memcmp(s, "MThd", 4))
		mf->subsongs = 1;
	else
	{
		mf->subsongs = 0;
		fclose(f);
		free(mf);
		return NULL;
	}

	fseek(f, 0, SEEK_SET);
	mf->flen = (unsigned)filelength(f);
	mf->data = malloc(mf->flen);
	if(!mf->data)
	{
		fclose(f);
		free(mf);
		return NULL;
	}

	if(fread(mf->data, mf->flen, 1, f) != 1)
	{
		fclose(f);
		free(mf->data);
		free(mf);
		return NULL;
	}

	fclose(f);
	return mf;
}


void mf_close(midi_file_t *mf)
{
	if(!mf)
		return;
	free(mf->data);
	free(mf);
}


int mp_select(midi_player_t *mp, midi_file_t *midifile)
{
	mp_stop(mp);
	mp->mf = midifile;
	mp_rewind(mp, 0);
	return 0;
}


static void mp_mark_loop(midi_player_t *mp)
{
	int i;
	for(i = 0; i < 16; i++)
		if(mp->track[i].enabled)
			mp->track[i].loop_start = mp->track[i].c;
	mp->loop_start = mp->c;
	mp_print_time(mp, mp->c.time);
	midiprintf(D2LOG, ": mp_mark_loop\n");
}


static void mp_loop(midi_player_t *mp)
{
	int i;
	for(i = 0; i < 16; i++)
		if(mp->track[i].enabled)
			mp->track[i].c = mp->track[i].loop_start;
	mp_print_time(mp, mp->c.time);
	midiprintf(D2LOG, ": mp_loop\n");
	mp->c = mp->loop_start;
}


static int mp_parse_command(midi_player_t *mp, unsigned len)
{
	char buf[128];
	unsigned i;
	for(i = 0; i < len; i++)
	{
		if(i < 127)
			buf[i] = (char)datalook(mp, mp->pos);
		mp->pos++;
	}
	buf[len < 127 ? len : 127] = 0;

	if('#' != buf[0])
	{
		midiprintf(D2LOG, "Marker: %s", buf);
		return 0;
	}

	if(!memcmp(buf + 1, "LOOP", 4))
	{
		if(!memcmp(buf + 5, "_START", 6))
			mp_mark_loop(mp);
		else if(!memcmp(buf + 5, "_END", 4))
		{
			mp_loop(mp);
			return 1;
		}
	}
	return 0;
}


static void mp_read_time_sig(midi_player_t *mp)
{
	mp_timesig_t *ts = &mp->c.timesig;
	ts->beats = getnext(mp, 1);
	ts->value = 1 << getnext(mp, 1);
	ts->clock = getnext(mp, 1);
	ts->qn = getnext(mp, 1);
	midiprintf(D2LOG, "Time Sig: %d/%d;", ts->beats, ts->value);
	midiprintf(D2LOG, " %d clock %d qn", ts->clock, ts->qn);
}


#define	FOR_TRACKS_ON	for(trk = 0; trk < 16; trk++)	\
				if(mp->track[trk].c.on)
int mp_update(midi_player_t *mp)
{
	unsigned note, vel, ctrl, x, l;
	unsigned i, j, c;
	int trk, ret;

	if(mp->doing == 1)
	{
		/* just get the first wait and ignore it :> */
		midiprintf(D2LOG, "Initial wait: ");
		FOR_TRACKS_ON
		{
			mp_track_t *mt = mp->track + trk;
			mp->pos = mt->c.pos;
			mt->c.iwait += getval(mp);
			midiprintf(D2LOG, "%d:%u  ", trk, mt->c.iwait);
			mt->c.pos = mp->pos;
		}
		mp->doing = 0;
		midiprintf(D2LOG, "\n\n");
	}

	do
	{
		ret = 0;
		FOR_TRACKS_ON
		{
			mp_track_t *mt = mp->track + trk;
			if(mt->c.iwait)
				continue;
			if(mt->c.pos >= mt->tend)
			{
				midiprintf(D2LOG, "(%d) End Of Track\n", trk);
				mt->c.on = 0;
				continue;
			}
			ret = 1;

			midiprintf2(D2LOG, "<%u>: ", mt->c.pos);

			mp->pos = mt->c.pos;
			x = getnext(mp, 1);

			/* This is for MIDI "running status" */
			if(x < 0x80)
			{
				x = mt->c.pv;
				mp->pos--;
			}
			mt->c.pv = (unsigned char)x;

			c = x & 0x0f;
			midiprintf2(D2LOG, "(%d)[%2X]", trk, x);
			switch (x & 0xf0)
			{
			  case 0x80:	/* note off */
				note = getnext(mp, 1) + (mp->pitch >> 16);
				if(note > 127)
					note = 127;
				else if(note < 0)
					note = 0;
				vel = getnext(mp, 1);
				if(mp->sock->note_off)
					mp->sock->note_off(c, note, vel);
				break;
			  case 0x90:	/* note on */
				/* doing=0; */
				note = getnext(mp, 1) + (mp->pitch >> 16);
				if(note > 127)
					note = 127;
				else if(note < 0)
					note = 0;
				vel = getnext(mp, 1);
				if(mp->sock->note_on)
					mp->sock->note_on(c, note, vel);
				break;
			  case 0xa0:	/* key aftertouch */
				note = getnext(mp, 1) + (mp->pitch >> 16);
				if(note > 127)
					note = 127;
				else if(note < 0)
					note = 0;
				vel = getnext(mp, 1);
				if(mp->sock->poly_pressure)
					mp->sock->poly_pressure(c, note,
							vel);
				break;
			  case 0xb0:	/* control change */
				ctrl = getnext(mp, 1);
				vel = getnext(mp, 1);
				if(mp->sock->control_change)
					mp->sock->control_change(c, ctrl,
							vel);
				break;
			  case 0xc0:	/* patch change */
				x = getnext(mp, 1);
				if(mp->sock->program_change)
					mp->sock->program_change(c, x);
				break;
			  case 0xd0:	/* channel aftertouch */
				x = getnext(mp, 1);
				if(mp->sock->channel_pressure)
					mp->sock->channel_pressure(c, x);
				break;
			  case 0xe0:	/* pitch wheel */
				x = getnext(mp, 1);
				x |= getnext(mp, 1) << 7;
				if(mp->sock->pitch_bend)
					mp->sock->pitch_bend(c, (int)x - 8192);
				break;
			  case 0xf0:
				switch (x)
				{
				  case 0xf0:
				  case 0xf7:	/* sysex */
					l = getval(mp);
					i = (datalook(mp, mp->pos + l) ==
							0xf7);
					midiprintf2(D2LOG, "{%ld}", l);
					midiprintf2(D2LOG, "\n");
					for(j = 0; j < l; j++)
						midiprintf2(D2LOG, "%2lX ", getnext(mp, 1));
					midiprintf2(D2LOG, "\n");
					if(i)
						getnext(mp, 1);
					break;
				  case 0xf1:
					break;
				  case 0xf2:
					getnext(mp, 2);
					break;
				  case 0xf3:
					getnext(mp, 1);
					break;
				  case 0xf4:
					break;
				  case 0xf5:
					break;
				  case 0xf6:	/* something */
				  case 0xf8:
				  case 0xfa:
				  case 0xfb:
				  case 0xfc:
					break;
				  case 0xfe:
					break;
				  case 0xfd:
					break;
				  case 0xff:
					x = getnext(mp, 1);
					l = getval(mp);
					midiprintf(D2LOG, " {%X (%X): ", x, l);
					switch(x)
					{
					  case 1:
						midiprintf(D2LOG, "Text: ");
						printnext(mp, l);
						break;
					  case 2:
						midiprintf(D2LOG, "Copyright: ");
						printnext(mp, l);
						break;
					  case 3:
						midiprintf(D2LOG, "Track Name: ");
						printnext(mp, l);
						break;
					  case 4:
						midiprintf(D2LOG, "Instrument: ");
						printnext(mp, l);
						break;
					  case 5:
						midiprintf(D2LOG, "Lyric: ");
						printnext(mp, l);
						break;
					  case 6:
						if(mp_parse_command(mp, l))
						{
							trk = -1;
							continue;
						}
						break;
					  case 7:
						midiprintf(D2LOG, "Cue Point: ");
						printnext(mp, l);
						break;
					  case 8:
					  case 9:
					  case 10:
					  case 11:
					  case 12:
					  case 13:
					  case 14:
					  case 15:
						midiprintf(D2LOG, "Text event: ");
						printnext(mp, l);
						break;
					  case 0x21:	/* MIDI Port # */
						x = getnext(mp, 1);
						midiprintf(D2LOG, "Port %u", x);
						break;
					  case 0x2f:	/* End Of Track */
						mt->c.on = 0;
						midiprintf(D2LOG, "End Of Track");
						break;
					  case 0x51:	/* Set Tempo */
						x = getnext(mp, l);
						set_tempo(mp, mp->c.ppqn, x);
						midiprintf(D2LOG, "(µsqtr:%u ==> BPM:%f)",
								mp->c.usqtr,
								60.0*1000000.0 /
								(float)mp->c.usqtr);
						break;
					  case 0x58:	/* Time signature */
						mp_read_time_sig(mp);
						break;
					  default:
						for(i = 0; i < l; i++)
							midiprintf2(D2LOG, "%2lX ", getnext(mp, 1));
						break;
					}
					midiprintf(D2LOG, "}\n");
					break;
				}
				break;
			  default:
				/* if we get down here, an error occurred */
				log_printf(ELOG, "a_midifile.c: INTERNAL ERROR;"
						" illegal status byte!\n");
				break;
			}

			if(mt->c.on)
				mt->c.iwait = getval(mp);

			mt->c.pos = mp->pos;

			midiprintf2(D2LOG, "\n");
		}
	} while(ret);

	/* Find first track to play */
	mp->c.iwait = 1000000000;	/* Bigger than any wait can be! */
	FOR_TRACKS_ON
		if(mp->track[trk].c.iwait < mp->c.iwait)
			mp->c.iwait = mp->track[trk].c.iwait;

	if(1000000000 == mp->c.iwait)
	{
		/* No tracks playing! */
		mp->c.iwait = 0;
		mp->c.fwait = 1000;
		return 0;
	}

	/* Calculate time to next event */
	mp->c.fwait = mp->c.iwait * mp->c.spulse;
	midiprintf2(D2LOG, "fwait = %f\n", mp->c.fwait);

	/* Update "track timers" and check if any tracks are playing */
	ret = 0;
	midiprintf2(D2LOG, "Wait:  ");
	FOR_TRACKS_ON
	{
		ret = 1;
		mp->track[trk].c.iwait -= mp->c.iwait;
		midiprintf2(D2LOG, "%d:%ld  ", trk, mp->track[trk].c.iwait);
	}
	midiprintf2(D2LOG, "\n");

	/* Update song time */
	mp->c.time += mp->c.iwait;

	midiprintf2(D2LOG, "----------------------------\n");
	return ret;
}
#undef	FOR_TRACKS_ON


int mp_play(midi_player_t *mp, float dt)
{
	if(!mp->mf)
		return 0;
	while(dt > 0.0)
	{
		if(dt < mp->c.fwait)
		{
			mp->c.fwait -= dt;
			return 1;
		}
		dt -= mp->c.fwait;
		if(!mp_update(mp))
			return 0;
	}
	return 1;
}


void mp_stop(midi_player_t *mp)
{
	unsigned c;
	if(!mp)
		return;
	if(!mp->sock->control_change)
		return;
	for(c = 0; c < 16; ++c)
	{
		mp->sock->control_change(c, 120, 1);
		mp->sock->control_change(c, 121, 1);
		mp->sock->control_change(c, 123, 1);
	}
	mp->mf = NULL;
}


void mp_rewind(midi_player_t *mp, unsigned subsong)
{
	unsigned i;
	int len;

	if(!mp->mf)
		return;

	mp->pos = 0;
	set_tempo(mp, 250, (unsigned)(1000.0 / (120.0/60.0)));
	mp->c.time = 0;
	mp->c.fwait = 0;
	mp->c.iwait = 0;
	mp->mf->subsongs = 1;

	for(i = 0; i < 16; i++)
	{
		mp->track[i].tend = 0;
		mp->track[i].spos = 0;
		mp->track[i].c.pos = 0;
		mp->track[i].c.iwait = 0;
		mp->track[i].c.on = 0;
		mp->track[i].c.pv = 0;
	}

	len = readheader(mp, "MThd");
	if(len < 0)
	{
		log_printf(ELOG, "mp_rewind(): Is this a MIDI file...?\n");
		mp->mf = NULL;
		return;
	}
	midiprintf(D2LOG, "header length:%d\n", len);
	if(len >= 6)
	{
		mp->mf->format = getnext(mp, 2);
		mp->mf->tracks = getnext(mp, 2);
		set_tempo(mp, getnext(mp, 2), mp->c.usqtr);
		midiprintf(D2LOG, "format:%u\n", mp->mf->format);
		midiprintf(D2LOG, "tracks:%u\n", mp->mf->tracks);
		midiprintf(D2LOG, "  ppqn:%u\n", mp->c.ppqn);
		if(0 == mp->mf->format)
			mp->mf->tracks = 1;
	}
	else
	{
		log_printf(ELOG, "mp_rewind(): WARNING: Short header!\n");
		mp->mf->format = 0;
		mp->mf->tracks = 1;
	}
	if(len > 6)
		getnext(mp, (unsigned)(len - 6)); /* Skip rest of header */
	if(mp->mf->tracks > 16)
	{
		log_printf(ELOG, "mp_rewind(): WARNING: Too many tracks!\n");
		mp->mf->tracks = 16;
	}
	for(i = 0; i < mp->mf->tracks; ++i)
	{
		len = readheader(mp, "MTrk");
		if(len < 0)
		{
			log_printf(ELOG, "mp_rewind(): Bad MIDI file!\n");
			mp->mf = NULL;
			return;
		}
		mp->track[i].enabled = 1;
		mp->track[i].c.on = 1;
		mp->track[i].spos = mp->pos;
		mp->track[i].tend = mp->pos + len;
		midiprintf(D2LOG, "track %u; start:%u length:%d\n",
				i, mp->track[i].spos, len);
		getnext(mp, (unsigned)len);
	}

	for(i = 0; i < 16; i++)
		if(mp->track[i].enabled)
		{
			mp->track[i].c.pos = mp->track[i].spos;
			mp->track[i].c.pv = 0;
			mp->track[i].c.iwait = 0;
			mp->track[i].loop_start = mp->track[i].c;
		}

	mp->doing = 1;
}


midi_player_t *mp_open(midisock_t *ms)
{
	midi_player_t *mp = calloc(1, sizeof(midi_player_t));
	if(!mp)
		return NULL;
	mp->sock = ms;
	mp->mf = NULL;
	return mp;
}


void mp_close(midi_player_t *mp)
{
	mp_stop(mp);
	free(mp);
}
