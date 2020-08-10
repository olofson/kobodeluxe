/*
------------------------------------------------------------
audiodev.h:	Little silly audio device "class"
------------------------------------------------------------
 *	Copyright (C) David Olofson, 1999, 2001
 *	This code is released under the terms of the LGPL
 */

#include "audiodev.h"
#include "kobolog.h"

#include <stdio.h>

#ifndef HAVE_OSS

void audiodev_init(struct audiodev_t *ad)
{
	log_printf(ELOG, "OSS sound output not compiled in!\n");
}

int audiodev_open(struct audiodev_t *ad)
{
	return -1;
}

void audiodev_close(struct audiodev_t *ad)
{
}

#else	/* HAVE_OSS */

#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifdef OSS_USE_SOUNDCARD_H
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

void audiodev_init(struct audiodev_t *ad)
{
	ad->devicename = "/dev/dsp";
	ad->mode = O_WRONLY;
	ad->rate = 44100;
	ad->fragmentsize = 256;
	ad->fragments = 4;
	ad->infd = ad->outfd = 0;
}

/* Open the audio device */
int audiodev_open(struct audiodev_t *ad)
{
	int r,v;
	struct audio_buf_info abinfo;
	int fragmentshift;

	fragmentshift=(int)floor((log(ad->fragmentsize)/log(2.0))+0.5);
	if(fragmentshift < 4)
		fragmentshift = 4;
	log_printf(DLOG, "  Fragment size: %d bytes\n", 1<<fragmentshift);

	ad->outfd = ad->infd = open(ad->devicename, ad->mode | O_NONBLOCK);
	if(ad->outfd < 0)
	{
		log_printf(ELOG, "Failed to open audio device!\n");
		return -1;
	}

	if(!(ad->mode & O_NONBLOCK))
	{
		r = fcntl(ad->outfd, F_SETFL, ad->mode);
		if(r < 0)
		{
			log_printf(ELOG, "Failed to set blocking mode!\n");
			close(ad->outfd);
			return -2;
		}
	}

	r = ioctl(ad->outfd, SNDCTL_DSP_RESET, NULL);
	if(r < 0)
	{
		log_printf(ELOG, "Failed to reset device!\n");
		close(ad->outfd);
		return -2;
	}

	/* Redundant? */
	v = 16;
	r = ioctl(ad->outfd, SNDCTL_DSP_SAMPLESIZE, &v);
	if(r < 0)
	{
		log_printf(ELOG, "16 bit samples not supported!\n");
		close(ad->outfd);
		return -2;
	}

	v = AFMT_S16_LE;
	r = ioctl(ad->outfd, SNDCTL_DSP_SETFMT, &v);
	if(r < 0)
	{
		log_printf(ELOG, "Failed to set sample format!\n");
		close(ad->outfd);
		return -3;
	}
	else if(v != AFMT_S16_LE)
	{
		log_printf(ELOG, "16 bit stereo LE not supported!\n");
		close(ad->outfd);
		return -4;
	}

	v = 1;
	r = ioctl(ad->outfd, SNDCTL_DSP_STEREO, &v);
	if(r < 0)
	{
		log_printf(ELOG, "Stereo not supported!\n");
		close(ad->outfd);
		return -5;
	}

	v = ad->rate;
	r = ioctl(ad->outfd, SNDCTL_DSP_SPEED, &v);
	if(r < 0)
	{
		log_printf(ELOG, "Failed to set %d Hz sample rate!\n",
				ad->rate);
		close(ad->outfd);
		return -6;
	}

	if(O_RDWR == ad->mode)
	{
		r = ioctl(ad->outfd, SNDCTL_DSP_SETDUPLEX, NULL);
		if(r < 0)
		{
			log_printf(ELOG, "Full duplex not supported!\n");
			close(ad->outfd);
			return -4;
		}
		v = ((ad->fragments+2) << 16) | fragmentshift;
	}
	else
		v = ((ad->fragments) << 16) | fragmentshift;

	r = ioctl(ad->outfd, SNDCTL_DSP_SETFRAGMENT, &v);
	if(r < 0)
	{
		log_printf(ELOG, "Failed to set fragment size/count!\n");
		close(ad->outfd);
		return -5;
	}

 	r = ioctl(ad->outfd, SNDCTL_DSP_GETOSPACE, &abinfo);
	if(r >= 0)
	{
		log_printf(DLOG, "  Driver: fragments = %d, fragsize = %d\n",
			abinfo.fragstotal,
			abinfo.fragsize);
		if( (abinfo.fragstotal != ad->fragments)
				&& (O_RDWR != ad->mode) )
			log_printf(WLOG, "Number of fragments is %d, "
					"not %d as requested.\n",
					abinfo.fragstotal, ad->fragments);
		if(abinfo.fragsize != ad->fragmentsize)
			log_printf(WLOG, "Fragment size is %d, "
					"not %d as requested.\n",
					abinfo.fragsize, ad->fragmentsize);
	}

	return 0;
}

void audiodev_close(struct audiodev_t *ad)
{
	if( (ad->infd != ad->outfd) && ad->infd )
		close(ad->infd);
	if(ad->outfd)
		close(ad->outfd);
	ad->infd = ad->outfd = 0;
}

#endif	/* HAVE_OSS */
