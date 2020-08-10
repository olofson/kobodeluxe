/*
------------------------------------------------------------
audiodev.h:	Little silly audio device "class"
------------------------------------------------------------
 *	Copyright (C) David Olofson, 1999-2001
 *	This code is released under the terms of the LGPL
 */

#ifndef _AUDIODEV_H_
#define _AUDIODEV_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audiodev_t
{
	const char *devicename;
	int outfd, infd;
	int mode;
	int rate;
	int fragmentsize;
	int fragments;
} audiodev_t;
void audiodev_init(struct audiodev_t *ad);
int audiodev_open(struct audiodev_t *ad);
void audiodev_close(struct audiodev_t *ad);

#ifdef __cplusplus
};
#endif
#endif
