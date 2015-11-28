/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#define DEBUG_EVENTS 0
#define DEBUG_PEEP_EVENTS 0
#define USE_LOCK 1
//SDLMOD_SYSWMEVENT

#if USE_LOCK
#include <pthread.h>
#endif

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "SDLMOD_events.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
SDLMOD_EventFilter SDLMOD_EventOK = NULL;
uint8_t SDLMOD_ProcessEvents[SDLMOD_NUMEVENTS];
static uint32_t SDLMOD_eventstate = 0;

#define MAXEVENTS	128
static struct {
#if USE_LOCK
	pthread_mutex_t *lock;
#endif
	int active;
	int head;
	int tail;
	SDLMOD_Event event[MAXEVENTS];
} SDLMOD_EventQ;

#if USE_LOCK
static struct {
	pthread_mutex_t *lock;
	int safe;
} SDLMOD_EventLock;

static pthread_t *SDLMOD_EventThread = NULL;
static pthread_t event_thread;
static uint32_t event_thread_valid = 0;

#endif

static void SDLMOD_Delay(uint32_t ms)
{
	Sleep(ms);
}

void SDLMOD_Lock_EventThread(void)
{
#if USE_LOCK
	if ( SDLMOD_EventThread && (event_thread_valid && !pthread_equal(pthread_self(), event_thread)) ) {
		pthread_mutex_lock(SDLMOD_EventLock.lock);
		while ( ! SDLMOD_EventLock.safe ) {
			SDLMOD_Delay(1);
		}
	}
#endif
}
void SDLMOD_Unlock_EventThread(void)
{
#if USE_LOCK
	if ( SDLMOD_EventThread && (event_thread_valid && !pthread_equal(pthread_self(), event_thread)) ) {
		pthread_mutex_unlock(SDLMOD_EventLock.lock);
	}
#endif
}

static int SDLMOD_GobbleEvents(void *unused)
{
#if USE_LOCK
	event_thread = pthread_self();
	event_thread_valid = 1;
#endif
	while ( SDLMOD_EventQ.active ) {
#if USE_LOCK
		SDLMOD_EventLock.safe = 1;
#endif
		SDLMOD_Delay(1);
#if USE_LOCK
		pthread_mutex_lock(SDLMOD_EventLock.lock);
		SDLMOD_EventLock.safe = 0;
		pthread_mutex_unlock(SDLMOD_EventLock.lock);
#endif
	}
#if USE_LOCK
	event_thread_valid = 0;
#endif
	return(0);
}

static void *gobble_events(void *data)
{
	SDLMOD_GobbleEvents(data);
	return NULL;
}

static int SDLMOD_StartEventThread(uint32_t flags)
{
#if USE_LOCK
	int ret;
	SDLMOD_EventThread = NULL;
	memset(&SDLMOD_EventLock, 0, sizeof(SDLMOD_EventLock));
	SDLMOD_EventQ.lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	ret = pthread_mutex_init(SDLMOD_EventQ.lock, NULL);
	if ( ret != 0 ) {
		free(SDLMOD_EventQ.lock);
		SDLMOD_EventQ.lock = NULL;
		return(-1);
	}
#endif
	SDLMOD_EventQ.active = 1;
#if USE_LOCK
	if ( (flags&SDLMOD_INIT_EVENTTHREAD) == SDLMOD_INIT_EVENTTHREAD ) {
		//SDLMOD_EventLock.lock = SDLMOD_CreateMutex();
		SDLMOD_EventLock.lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		ret = pthread_mutex_init(SDLMOD_EventLock.lock, NULL);
		if ( ret != 0 ) {
			free(SDLMOD_EventLock.lock);
			SDLMOD_EventLock.lock = NULL;
			return(-1);
		}
		SDLMOD_EventLock.safe = 0;
		//SDLMOD_EventThread = SDLMOD_CreateThread(SDLMOD_GobbleEvents, NULL);
		//if ( SDLMOD_EventThread == NULL ) {
		//	return(-1);
		//}
		{
			int err;
			SDLMOD_EventThread = (pthread_t *)malloc(sizeof(pthread_t));
			err = pthread_create(SDLMOD_EventThread, NULL, gobble_events, NULL);
			if (err!=0)
			{
				free(SDLMOD_EventThread);
				SDLMOD_EventThread = NULL;
				return(-1);
			}
		}
	} else {
		event_thread_valid = 0;
	}
#endif
	return(0);
}

static void SDLMOD_StopEventThread(void)
{
	SDLMOD_EventQ.active = 0;
#if USE_LOCK
	if ( SDLMOD_EventThread ) {
		//SDLMOD_WaitThread(SDLMOD_EventThread, NULL);
		pthread_join(*SDLMOD_EventThread, NULL);
		free(SDLMOD_EventThread);
		SDLMOD_EventThread = NULL;
		if (SDLMOD_EventLock.lock)
		{
			pthread_mutex_destroy(SDLMOD_EventLock.lock);
			free(SDLMOD_EventLock.lock);
		}
		SDLMOD_EventLock.lock = NULL;
	}
	if (SDLMOD_EventQ.lock)
	{
		pthread_mutex_destroy(SDLMOD_EventQ.lock);
		free(SDLMOD_EventQ.lock);
	}
	SDLMOD_EventQ.lock = NULL;
#endif
}

void SDLMOD_StopEventLoop(void)
{
	SDLMOD_StopEventThread();
	SDLMOD_EventQ.head = 0;
	SDLMOD_EventQ.tail = 0;
}

int SDLMOD_StartEventLoop(uint32_t flags)
{
	int retcode;
#if DEBUG_EVENTS
	printf("SDLMOD_StartEventLoop() begin...\n");
#endif
#if USE_LOCK
	SDLMOD_EventThread = NULL;
	SDLMOD_EventQ.lock = NULL;
#endif
	SDLMOD_StopEventLoop();
	SDLMOD_EventOK = NULL;
	memset(SDLMOD_ProcessEvents,SDLMOD_ENABLE,sizeof(SDLMOD_ProcessEvents));
	SDLMOD_eventstate = ~0;
	retcode = 0;
	if ( retcode < 0 ) {
		return(-1);
	}
	if ( SDLMOD_StartEventThread(flags) < 0 ) {
		SDLMOD_StopEventLoop();
		return(-1);
	}
	return(0);
}

static int SDLMOD_AddEvent(SDLMOD_Event *event)
{
	int tail, added;
#if DEBUG_EVENTS
	printf("SDLMOD_PeepEvents->SDLMOD_AddEvent addevent...\n");
#endif
	tail = (SDLMOD_EventQ.tail+1)%MAXEVENTS;
	if ( tail == SDLMOD_EventQ.head ) {
		added = 0;
	} else {
		SDLMOD_EventQ.event[SDLMOD_EventQ.tail] = *event;
		SDLMOD_EventQ.tail = tail;
		added = 1;
	}
	return(added);
}

static int SDLMOD_CutEvent(int spot)
{
	if ( spot == SDLMOD_EventQ.head ) {
		SDLMOD_EventQ.head = (SDLMOD_EventQ.head+1)%MAXEVENTS;
		return(SDLMOD_EventQ.head);
	} 
	else if ( (spot+1)%MAXEVENTS == SDLMOD_EventQ.tail ) {
		SDLMOD_EventQ.tail = spot;
		return(SDLMOD_EventQ.tail);
	} 
	else {
		int here, next;
		if ( --SDLMOD_EventQ.tail < 0 ) {
			SDLMOD_EventQ.tail = MAXEVENTS-1;
		}
		for ( here=spot; here != SDLMOD_EventQ.tail; here = next ) {
			next = (here+1)%MAXEVENTS;
			SDLMOD_EventQ.event[here] = SDLMOD_EventQ.event[next];
		}
		return(spot);
	}
}

int SDLMOD_PeepEvents(SDLMOD_Event *events, int numevents, SDLMOD_eventaction action, uint32_t mask)
{
	int i, used;
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_PeepEvents : begin...\n");
#endif
	if ( ! SDLMOD_EventQ.active ) {
		return(-1);
	}
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_PeepEvents : SDLMOD_EventQ is active...\n");
#endif
	used = 0;
#if USE_LOCK
	if ( pthread_mutex_lock(SDLMOD_EventQ.lock) == 0 ) {
#endif
		if ( action == SDLMOD_ADDEVENT ) {
			for ( i=0; i<numevents; ++i ) {
				used += SDLMOD_AddEvent(&events[i]);
			}
		} else {
			SDLMOD_Event tmpevent;
			int spot;
			if ( events == NULL ) {
				action = SDLMOD_PEEKEVENT;
				numevents = 1;
				events = &tmpevent;
			}
			spot = SDLMOD_EventQ.head;
			while ((used < numevents)&&(spot != SDLMOD_EventQ.tail)) {
				if ( mask & SDLMOD_EVENTMASK(SDLMOD_EventQ.event[spot].type) ) {
					events[used++] = SDLMOD_EventQ.event[spot];
					if ( action == SDLMOD_GETEVENT ) {
						spot = SDLMOD_CutEvent(spot);
					} else {
						spot = (spot+1)%MAXEVENTS;
					}
				} else {
					spot = (spot+1)%MAXEVENTS;
				}
			}
		}
#if USE_LOCK
		pthread_mutex_unlock(SDLMOD_EventQ.lock);
	} else {
		fprintf(stderr, "Couldn't lock event queue");
		used = -1;
	}
#endif
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_PeepEvents : used == %d...\n", used);
#endif
	return(used);
}

void SDLMOD_PumpEvents(void)
{
#if USE_LOCK
	if ( !SDLMOD_EventThread ) {
		
	}
#endif
}

int SDLMOD_PollEvent (SDLMOD_Event *event)
{
	SDLMOD_PumpEvents();
	if ( SDLMOD_PeepEvents(event, 1, SDLMOD_GETEVENT, SDLMOD_ALLEVENTS) <= 0 )
		return 0;
	return 1;
}

int SDLMOD_WaitEvent (SDLMOD_Event *event)
{
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent begin...\n");
#endif
	while ( 1 ) {
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent SDLMOD_PumpEvents begin...\n");
#endif
		SDLMOD_PumpEvents();
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent SDLMOD_PumpEvents end...\n");
#endif
		switch (SDLMOD_PeepEvents(event, 1, SDLMOD_GETEVENT, SDLMOD_ALLEVENTS)) {
		case -1: 
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent return 0...\n");
#endif
			return 0;
			
		case 1: 
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent return 1...\n");
#endif
			return 1;
			
		case 0: 
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent delay...\n");
#endif
			SDLMOD_Delay(10);
		}
	}
#if DEBUG_PEEP_EVENTS
	printf("SDLMOD_WaitEvent bottom...\n");
#endif
}

int SDLMOD_PushEvent(SDLMOD_Event *event)
{
#if DEBUG_EVENTS
	printf("SDLMOD_PushEvent...\n");
#endif
	if ( SDLMOD_PeepEvents(event, 1, SDLMOD_ADDEVENT, 0) <= 0 )
		return -1;
	return 0;
}

void SDLMOD_SetEventFilter (SDLMOD_EventFilter filter)
{
	SDLMOD_Event bitbucket;
	SDLMOD_EventOK = filter;
	while ( SDLMOD_PollEvent(&bitbucket) > 0 )
		;
}

SDLMOD_EventFilter SDLMOD_GetEventFilter(void)
{
	return(SDLMOD_EventOK);
}

uint8_t SDLMOD_EventState (uint8_t type, int state)
{
	SDLMOD_Event bitbucket;
	uint8_t current_state;
	if ( type == 0xFF ) {
		current_state = SDLMOD_IGNORE;
		for ( type=0; type<SDLMOD_NUMEVENTS; ++type ) {
			if ( SDLMOD_ProcessEvents[type] != SDLMOD_IGNORE ) {
				current_state = SDLMOD_ENABLE;
			}
			SDLMOD_ProcessEvents[type] = state;
			if ( state == SDLMOD_ENABLE ) {
				SDLMOD_eventstate |= (0x00000001 << (type));
			} else {
				SDLMOD_eventstate &= ~(0x00000001 << (type));
			}
		}
		while ( SDLMOD_PollEvent(&bitbucket) > 0 )
			;
		return(current_state);
	}
	current_state = SDLMOD_ProcessEvents[type];
	switch (state) {
		case SDLMOD_IGNORE:
		case SDLMOD_ENABLE:
			/* Set state and discard pending events */
			SDLMOD_ProcessEvents[type] = state;
			if ( state == SDLMOD_ENABLE ) {
				SDLMOD_eventstate |= (0x00000001 << (type));
			} else {
				SDLMOD_eventstate &= ~(0x00000001 << (type));
			}
			while ( SDLMOD_PollEvent(&bitbucket) > 0 )
				;
			break;
		
		default:
			break;
	}
	return(current_state);
}
