/*(LGPL)
---------------------------------------------------------------------------
	a_events.h - Internal Event System
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

#ifndef _A_EVENTS_H_
#define _A_EVENTS_H_

#define	AEV_TRACKING

#include "a_globals.h"
#include "a_types.h"

/*----------------------------------------------------------
	Open/Close
----------------------------------------------------------*/
int aev_open(int pool_size);
void aev_close(void);


/*----------------------------------------------------------
	The global event timestamp time base
----------------------------------------------------------*/
#define	AEV_TIMESTAMP_MASK	0xffff
typedef Uint16 aev_timestamp_t;

extern aev_timestamp_t aev_timer;

static inline void aev_reset_timer(void)
{
	aev_timer = 0;
}

static inline void aev_advance_timer(unsigned frames)
{
	aev_timer += (aev_timestamp_t)frames;
}


/*----------------------------------------------------------
	Event
----------------------------------------------------------*/
#define	AEV_ET_FREE		0xff
#define	AEV_ET_ALLOCATED	0xfe

struct aev_port_t;
typedef struct aev_event_t
{
	struct aev_event_t	*next;
	aev_timestamp_t		frame;		/* When */
	Uint8			type;		/* What */
	Uint8			index;		/* Qualifier */
	Sint32			arg1;		/* Argument 1 */
	Sint32			arg2;		/* Argument 2 */
	Sint32			arg3;		/* Argument 3 */
#ifdef AEV_TRACKING
	const char		*client;	/* Client name */
	struct aev_port_t	*port;		/* Receiver port */
#endif
} aev_event_t;


/* Macro for interpreting event timestamps. */
#define	AEV_TIME(frame, offset)					\
		((unsigned)(frame - aev_timer - offset) &	\
		AEV_TIMESTAMP_MASK)

/* The global event pool. */
extern aev_event_t		*aev_event_pool;

#ifdef AEV_TRACKING
extern int aev_event_counter;			//Current number of events in use
extern int aev_event_counter_max;		//Peak use tracker
extern const char *aev_current_client;	//Name of current client
#endif

#ifdef AEV_TRACKING
#define	aev_client(x)	aev_current_client = x;
#else
#define	aev_client(x)
#endif

/* Refill event pool. */
int _aev_refill_pool(void);

/*
 * Allocate an event.
 *
 * Will try to refill the event pool if required.
 * Returns the event, or NULL, if the pool was empty
 * and could not be refilled.
 */
static inline aev_event_t *aev_new(void)
{
	aev_event_t *ev;
	if(!aev_event_pool)
		if(_aev_refill_pool() < 0)
			return NULL;

	ev = aev_event_pool;
	aev_event_pool = ev->next;
#ifdef AEV_TRACKING
	ev->type = AEV_ET_ALLOCATED;
	ev->client = aev_current_client;
	++aev_event_counter;
	if(aev_event_counter > aev_event_counter_max)
		aev_event_counter_max = aev_event_counter;
#endif
	return ev;
}


/* Free an event. */
static inline void aev_free(aev_event_t *ev)
{
	ev->next = aev_event_pool;
	aev_event_pool = ev;
#ifdef AEV_TRACKING
	ev->type = AEV_ET_FREE;
	--aev_event_counter;
#endif
}


/*----------------------------------------------------------
	Event Input Port
----------------------------------------------------------*/
typedef struct aev_port_t
{
	/*
	 * Note that when 'first' is NULL, 'last' is undefined!
	 * (If there's no first event, there can't be a last
	 * event either - makes sense, eh? :-)
	 */
	aev_event_t		*first;
	aev_event_t		*last;
	const char		*name;
} aev_port_t;


/*
 * Initialize a port.
 *****************************************
 *	DO NOT FORGET TO USE THIS!
 *****************************************
 */
static inline void aev_port_init(aev_port_t *evp, const char *name)
{
	evp->first = evp->last = NULL;
	evp->name = name;
}


/*
 * Write one event to a port. The event will belong to the
 * owner of the port, who is responsible for freeing or
 * reusing the event after reading it.
 *
 * NOTE: Events must be queued in increasing timestamp order!
 */
static inline void aev_write(aev_port_t *evp, aev_event_t *ev)
{
	ev->next = NULL;
	if(!evp->first)
		evp->first = evp->last = ev;	/* Oops, empty port! */
	else
	{
		evp->last->next = ev;
		evp->last = ev;
	}
#ifdef AEV_TRACKING
	ev->port = evp;
#endif
}


/*
 * Insert an event at the right position in the queue of
 * the specified port. The event will be placed *after*
 * any other events with the same timestamp.
 *
 * The event will belong to the owner of the port, who is
 * responsible for freeing or reusing the event after
 * reading it.
 */
void aev_insert(aev_port_t *evp, aev_event_t *ev);


/*
 * Read one event from a port. The event will be removed
 * from the port, and is effectively detached from the
 * event system until you reuse it by sending it somewhere,
 * or free it.
 *
 *	WARNING: Events *MUST* be either freed or reused!
 *
 * Event leaks will result in severe timing issues, as the
 * pool will need to be refilled regularly, which is *not*
 * a real time compatible operation.
 *
 * Returns the event, or NULL, if no event was queued.
 */
static inline aev_event_t *aev_read(aev_port_t *evp)
{
	aev_event_t *ev = evp->first;
	if(!ev)
		return NULL;

	evp->first = ev->next;
	return ev;
}

/*
 * Return number of frames from the start of the current
 * buffer (current value of 'aev_timer') + 'offset' to
 * the next event queued on this port.
 *
 * If there's no event enqueued, the maximum possible
 * number of frames is returned.
 */
static inline unsigned aev_next(aev_port_t *evp, unsigned offset)
{
	aev_event_t *ev = evp->first;
	if(ev)
		return AEV_TIME(ev->frame, offset);
	else
		return AEV_TIMESTAMP_MASK;
}


/*
 * Remove and free all events on the specified port.
 */
static inline void aev_flush(aev_port_t *evp)
{
#ifdef AEV_TRACKING
	while(1)
	{
		aev_event_t *ev = evp->first;
		if(!ev)
			return;

		evp->first = ev->next;
		aev_free(ev);
	}
#else
	/*
	 * Silly me! I've pulled this simple trick
	 * before, but just forgot somehow... :-)
	 */
	if(!evp->first)
		return;

	evp->last->next = aev_event_pool;
	aev_event_pool = evp->first;
	evp->first = NULL;
#endif
}


/*
 * Short and handy "send" functions.
 *
 * Allocates an event, fills it in, and sends it to the
 * specified port. The address of the event used is
 * returned - which means that you can detect an error
 * by checking that the return is not NULL.
 */
static inline aev_event_t *aev_send0(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	aev_write(evp, ev);
	return ev;
}

static inline aev_event_t *aev_send1(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type, Sint32 arg1)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	aev_write(evp, ev);
	return ev;
}

static inline aev_event_t *aev_send2(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type, Sint32 arg1, Sint32 arg2)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	ev->arg2 = arg2;
	aev_write(evp, ev);
	return ev;
}

static inline aev_event_t *aev_sendi0(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type, Uint8 index)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->index = index;
	ev->frame = delay + aev_timer;
	aev_write(evp, ev);
	return ev;
}

static inline aev_event_t *aev_sendi1(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type,
		Uint8 index, Sint32 arg1)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->index = index;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	aev_write(evp, ev);
	return ev;
}

static inline aev_event_t *aev_sendi2(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type,
		Uint8 index, Sint32 arg1, Sint32 arg2)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->index = index;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	ev->arg2 = arg2;
	aev_write(evp, ev);
	return ev;
}

/*
 * Like the above, but using aev_insert().
 */
static inline aev_event_t *aev_insert0(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	aev_insert(evp, ev);
	return ev;
}

static inline aev_event_t *aev_insert1(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type, Sint32 arg1)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	aev_insert(evp, ev);
	return ev;
}

static inline aev_event_t *aev_insert2(aev_port_t *evp,
		aev_timestamp_t delay, Uint8 type,
		Sint32 arg1, Sint32 arg2)
{
	aev_event_t *ev = aev_new();
	if(!ev)
		return NULL;
	ev->type = type;
	ev->frame = delay + aev_timer;
	ev->arg1 = arg1;
	ev->arg2 = arg2;
	aev_insert(evp, ev);
	return ev;
}

#endif /*_A_EVENTS_H_*/
