/*(LGPL)
---------------------------------------------------------------------------
	a_events.c - Internal Event System
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

#define	EVDBG(x)

#include <stdlib.h>

#include "kobolog.h"
#include "a_events.h"

#define	EVENTS_PER_BLOCK	256

/*----------------------------------------------------------
	The global event timestamp time base
----------------------------------------------------------*/
Uint16 aev_timer = 0;


/*----------------------------------------------------------
	Event pool management
----------------------------------------------------------*/

aev_event_t		*aev_event_pool = NULL;

#ifdef	AEV_TRACKING
int aev_event_counter = 0;
int aev_event_counter_max = 0;
const char *aev_current_client = "Unknown";
#endif


typedef struct evblock_t
{
	struct evblock_t	*next;
	aev_event_t		events[EVENTS_PER_BLOCK];
} evblock_t;

static evblock_t *blocks = NULL;


int _aev_refill_pool(void)
{
	/*
	 * We can't refill in real time context on some
	 * platforms - and it would be a bad idea anyway...
	 */
	log_printf(ELOG, "Audio event pool exhausted!\n");
	return -1;
}


/*
 * A job for a future butler thread. For now, we only use
 * this during initialization.
 */
static int refill_pool(void)
{
	int i;
	evblock_t *eb = calloc(1, sizeof(evblock_t));
	if(!eb)
		return -1;

	/* Add to block list */
	eb->next = blocks;
	blocks = eb;

	/* Add the events to the pool */
	for(i = 0; i < EVENTS_PER_BLOCK; ++i)
	{
		eb->events[i].type = AEV_ET_FREE;
		aev_free(&eb->events[i]);
	}

	return 0;
}


/*----------------------------------------------------------
	Event Input Port
----------------------------------------------------------*/
void aev_insert(aev_port_t *evp, aev_event_t *ev)
{
EVDBG(log_printf(D2LOG, "aev_insert:\n");)
	if(!evp->first)
	{
EVDBG(log_printf(D2LOG, "  empty list\n");)
		ev->next = NULL;
		evp->first = evp->last = ev;
	}
	else if((Sint16)(evp->last->frame - ev->frame) <= 0)
	{
EVDBG(log_printf(D2LOG, "  last\n");)
		ev->next = NULL;
		evp->last->next = ev;
		evp->last = ev;
	}
	else
	{
		aev_event_t *e_prev = NULL;
		aev_event_t *e = evp->first;
EVDBG(log_printf(D2LOG, " scanning...\n");)
		while((Sint16)(e->frame - ev->frame) <= 0)
		{
EVDBG(log_printf(D2LOG, "e->frame = %d, ev->frame = %d\n", e->frame, ev->frame);)
			e_prev = e;
			e = e->next;
		}
		ev->next = e;
		if(e_prev)
		{
EVDBG(log_printf(D2LOG, "  insert\n");)
			e_prev->next = ev;
		}
		else
		{
EVDBG(log_printf(D2LOG, "  first\n");)
			evp->first = ev;
		}
	}
#ifdef AEV_TRACKING
	ev->port = evp;
#endif
}


/*----------------------------------------------------------
	Open/Close
----------------------------------------------------------*/

int aev_open(int pool_size)
{
	while(pool_size > 0)
	{
		if(refill_pool() < 0)
		{
			aev_close();
			return -1;	/* OOM! */
		}
		pool_size -= EVENTS_PER_BLOCK;
	}
#if 0
	{
		aev_evport_t port;
		int i;
		aev_evport_init(&port, "testport");
		aev_timer = 0;
		aev_event_client(42);
		for(i = 0; i < 5; ++i)
			aev_put0(&port, 20+i*20, 42);
		for(i = 0; i < 5; ++i)
			aev_put0(&port, 100-i*20, 43);
		i = 0;
		aev_timer = 0;
		while(i < 200)
		{
			unsigned int frames;
			log_printf(D2LOG, "aev_timer = %d\n", aev_timer);
			while(!(frames = aev_next(&port, 0)))
			{
				aev_event_t *ev = aev_read(&port);
				log_printf(D2LOG, "  event.frame = %d, type = %d\n",
						ev->frame, ev->type);
				aev_free(ev);
			}
			if(frames > 5)
				frames = 5;
			aev_timer += frames;
			i += frames;
		}
	}
#endif
#ifdef	AEV_TRACKING
	aev_event_counter = 0;
	aev_event_counter_max = 0;
#endif
	return 0;
}


void aev_close(void)
{
	log_printf(DLOG, "aev_close(): max events used: %d\n",
				aev_event_counter_max);
#ifdef	AEV_TRACKING
	if(aev_event_counter)
	{
		evblock_t *eb = blocks;
		log_printf(ELOG, "INTERNAL ERROR: Closing audio event system"
				" system with %d events in use!"
				" (Memory leaked.)\n",
				aev_event_counter);
		while(eb)
		{
			int i;
			for(i = 0; i < EVENTS_PER_BLOCK; ++i)
			{
				if(eb->events[i].type == AEV_ET_FREE)
					continue;
				log_printf(ELOG, "  Event %p, "
						"sent from client \"%s\", "
						"to port \"%s\".\n",
						eb->events + i,
						eb->events[i].client,
						eb->events[i].port->name );
			}
			eb = eb->next;
		}
	}
	else
#endif
		while(blocks)
		{
			evblock_t *eb = blocks;
			blocks = eb->next;
			free(eb);
		}

	blocks = NULL;
	aev_event_pool = NULL;
#ifdef	AEV_TRACKING
	aev_event_counter = 0;
	aev_event_counter_max = 0;
#endif
}
