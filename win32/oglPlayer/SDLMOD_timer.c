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

#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <malloc.h>

#define USE_SELECT 1

#if USE_SELECT
#ifdef __MINGW32__
//#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#endif
#endif

#include "SDLMOD_timer.h"

#define ROUND_RESOLUTION(X)	\
	(((X+TIMER_RESOLUTION-1)/TIMER_RESOLUTION)*TIMER_RESOLUTION)

extern int SDLMOD_timer_started;
extern int SDLMOD_timer_running;

extern uint32_t SDLMOD_alarm_interval;
extern SDLMOD_TimerCallback SDLMOD_alarm_callback;
extern int SDLMOD_SetTimerThreaded(int value);
extern void SDLMOD_ThreadedTimerCheck(void);

static struct timeval start;










int SDLMOD_timer_started = 0;
int SDLMOD_timer_running = 0;

uint32_t SDLMOD_alarm_interval = 0;
SDLMOD_TimerCallback SDLMOD_alarm_callback;

static int SDLMOD_timer_threaded = 0;

struct _SDLMOD_TimerID {
	uint32_t interval;
	SDLMOD_NewTimerCallback cb;
	void *param;
	uint32_t last_alarm;
	struct _SDLMOD_TimerID *next;
};

static SDLMOD_TimerID SDLMOD_timers = NULL;
static pthread_mutex_t *SDLMOD_timer_mutex;
static volatile int list_changed = 0;







void SDLMOD_StartTicks(void)
{
	gettimeofday(&start, NULL);
}

uint32_t SDLMOD_GetTicks(void)
{
	uint32_t ticks;
	struct timeval now;
	gettimeofday(&now, NULL);
	ticks=(now.tv_sec-start.tv_sec)*1000+(now.tv_usec-start.tv_usec)/1000;
	return(ticks);
}

void SDLMOD_Delay(uint32_t ms)
{
#if USE_SELECT
	int was_error;
	struct timeval tv;
	uint32_t then, now, elapsed;

	then = SDLMOD_GetTicks();
	do {
		errno = 0;
		now = SDLMOD_GetTicks();
		elapsed = (now-then);
		then = now;
		if ( elapsed >= ms ) {
			break;
		}
		ms -= elapsed;
		tv.tv_sec = ms/1000;
		tv.tv_usec = (ms%1000)*1000;
		was_error = select(0, NULL, NULL, NULL, &tv);
	} while ( was_error && (errno == EINTR) );
#else
	Sleep(ms);
#endif
}

static int timer_alive = 0;
static pthread_t *timer = NULL;

static int RunTimer(void *unused)
{
	while ( timer_alive ) {
		if ( SDLMOD_timer_running ) {
			SDLMOD_ThreadedTimerCheck();
		}
		SDLMOD_Delay(1);
	}
	return(0);
}

void *run_timer(void *data)
{
	RunTimer(data);
	return NULL;
}

int SDLMOD_SYS_TimerInit(void)
{
	timer_alive = 1;
    {
		pthread_t tid;
		int err;
		timer = (pthread_t *)malloc(sizeof(pthread_t));
		err = pthread_create(timer, NULL, run_timer, NULL);
		if (err!=0)
		{
			free(timer);
			timer = NULL;
			printf("can't create thread: %s\n", strerror(err));
		}
	}
	if (timer == NULL)
	{
		return(-1);
	}
	return(SDLMOD_SetTimerThreaded(1));
}

void SDLMOD_SYS_TimerQuit(void)
{
	timer_alive = 0;
	if (timer) 
	{
		pthread_join(*timer, NULL);
		free(timer);
		timer = NULL;
	}
}

int SDLMOD_SYS_StartTimer(void)
{
	fprintf(stderr, "Internal logic error: Linux uses threaded timer\n");
	return(-1);
}

void SDLMOD_SYS_StopTimer(void)
{
	return;
}
















int SDLMOD_SetTimerThreaded(int value)
{
	int retval;
	if ( SDLMOD_timer_started ) {
		fprintf(stderr, "Timer already initialized\n");
		retval = -1;
	} else {
		retval = 0;
		SDLMOD_timer_threaded = value;
	}
	return retval;
}

int SDLMOD_TimerInit(void)
{
	int retval;
	retval = 0;

#if USE_SELECT
#ifdef __MINGW32__
	{
		WSADATA wsaData;
		int iResult;
		iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		if (iResult != 0) {
			fprintf(stderr, "WSAStartup failed: %d\n", iResult);
			return 1;
		}
	}
#endif
#endif

	if (SDLMOD_timer_started) {
		SDLMOD_TimerQuit();
	}
	if (!SDLMOD_timer_threaded) {
		retval = SDLMOD_SYS_TimerInit();
	}
	if (SDLMOD_timer_threaded) {
		SDLMOD_timer_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(SDLMOD_timer_mutex, NULL);
	}
	if (retval == 0) {
		SDLMOD_timer_started = 1;
	}
	return(retval);
}

void SDLMOD_TimerQuit(void)
{
	SDLMOD_SetTimer(0, NULL);
	if (SDLMOD_timer_threaded < 2) {
		SDLMOD_SYS_TimerQuit();
	}
	if (SDLMOD_timer_threaded) {
		pthread_mutex_destroy(SDLMOD_timer_mutex);
		free(SDLMOD_timer_mutex);
		SDLMOD_timer_mutex = NULL;
	}
	SDLMOD_timer_started = 0;
	SDLMOD_timer_threaded = 0;
}

void SDLMOD_ThreadedTimerCheck(void)
{
	uint32_t now, ms;
	SDLMOD_TimerID t, prev, next;
	int removed;

	pthread_mutex_lock(SDLMOD_timer_mutex);
	list_changed = 0;
	now = SDLMOD_GetTicks();
	for ( prev = NULL, t = SDLMOD_timers; t; t = next ) {
		removed = 0;
		ms = t->interval - SDLMOD_TIMESLICE;
		next = t->next;
		if ( (int)(now - t->last_alarm) > (int)ms ) {
			struct _SDLMOD_TimerID timer;

			if ( (now - t->last_alarm) < t->interval ) {
				t->last_alarm += t->interval;
			} else {
				t->last_alarm = now;
			}
#ifdef DEBUG_TIMERS
			printf("Executing timer %p\n", t);
#endif
			timer = *t;
			pthread_mutex_unlock(SDLMOD_timer_mutex);
			ms = timer.cb(timer.interval, timer.param);
			pthread_mutex_lock(SDLMOD_timer_mutex);
			if ( list_changed ) {
				break;
			}
			if ( ms != t->interval ) {
				if ( ms ) {
					t->interval = ROUND_RESOLUTION(ms);
				} else {
#ifdef DEBUG_TIMERS
					printf("SDL: Removing timer %p\n", t);
#endif
					if ( prev ) {
						prev->next = next;
					} else {
						SDLMOD_timers = next;
					}
					free(t);
					--SDLMOD_timer_running;
					removed = 1;
				}
			}
		}
		if ( ! removed ) {
			prev = t;
		}
	}
	pthread_mutex_unlock(SDLMOD_timer_mutex);
}

static SDLMOD_TimerID SDLMOD_AddTimerInternal(uint32_t interval, SDLMOD_NewTimerCallback callback, void *param)
{
	SDLMOD_TimerID t;
	t = (SDLMOD_TimerID) malloc(sizeof(struct _SDLMOD_TimerID));
	if ( t ) {
		t->interval = ROUND_RESOLUTION(interval);
		t->cb = callback;
		t->param = param;
		t->last_alarm = SDLMOD_GetTicks();
		t->next = SDLMOD_timers;
		SDLMOD_timers = t;
		++SDLMOD_timer_running;
		list_changed = 1;
	}
#ifdef DEBUG_TIMERS
	printf("SDLMOD_AddTimer(%d) = %08x num_timers = %d\n", interval, (uint32_t)t, SDLMOD_timer_running);
#endif
	return t;
}

SDLMOD_TimerID SDLMOD_AddTimer(uint32_t interval, SDLMOD_NewTimerCallback callback, void *param)
{
	SDLMOD_TimerID t;
	if ( ! SDLMOD_timer_mutex ) {
		if ( SDLMOD_timer_started ) {
			fprintf(stderr, "This platform doesn't support multiple timers\n");
		} else {
			fprintf(stderr, "You must call SDLMOD_Init(SDLMOD_INIT_TIMER) first\n");
		}
		return NULL;
	}
	if ( ! SDLMOD_timer_threaded ) {
		fprintf(stderr, "Multiple timers require threaded events!\n");
		return NULL;
	}
	pthread_mutex_lock(SDLMOD_timer_mutex);
	t = SDLMOD_AddTimerInternal(interval, callback, param);
	pthread_mutex_unlock(SDLMOD_timer_mutex);
	return t;
}

int SDLMOD_RemoveTimer(SDLMOD_TimerID id)
{
	SDLMOD_TimerID t, prev = NULL;
	int removed;

	removed = 0;
	pthread_mutex_lock(SDLMOD_timer_mutex);
	/* Look for id in the linked list of timers */
	for (t = SDLMOD_timers; t; prev=t, t = t->next ) {
		if ( t == id ) {
			if(prev) {
				prev->next = t->next;
			} else {
				SDLMOD_timers = t->next;
			}
			free(t);
			--SDLMOD_timer_running;
			removed = 1;
			list_changed = 1;
			break;
		}
	}
#ifdef DEBUG_TIMERS
	printf("SDLMOD_RemoveTimer(%08x) = %d num_timers = %d\n", (uint32_t)id, removed, SDLMOD_timer_running);
#endif
	pthread_mutex_unlock(SDLMOD_timer_mutex);
	return removed;
}

/* Old style callback functions are wrapped through this */
static uint32_t callback_wrapper(uint32_t ms, void *param)
{
	SDLMOD_TimerCallback func = (SDLMOD_TimerCallback) param;
	return (*func)(ms);
}

int SDLMOD_SetTimer(uint32_t ms, SDLMOD_TimerCallback callback)
{
	int retval;

#ifdef DEBUG_TIMERS
	printf("SDLMOD_SetTimer(%d)\n", ms);
#endif
	retval = 0;

	if ( SDLMOD_timer_threaded ) {
		pthread_mutex_lock(SDLMOD_timer_mutex);
	}
	if ( SDLMOD_timer_running ) 
	{
		if ( SDLMOD_timer_threaded ) {
			while ( SDLMOD_timers ) {
				SDLMOD_TimerID freeme = SDLMOD_timers;
				SDLMOD_timers = SDLMOD_timers->next;
				free(freeme);
			}
			SDLMOD_timer_running = 0;
			list_changed = 1;
		} else {
			SDLMOD_SYS_StopTimer();
			SDLMOD_timer_running = 0;
		}
	}
	if (ms) {
		if (SDLMOD_timer_threaded) {
			if (SDLMOD_AddTimerInternal(ms, callback_wrapper, (void *)callback) == NULL) {
				retval = -1;
			}
		} else {
			SDLMOD_timer_running = 1;
			SDLMOD_alarm_interval = ms;
			SDLMOD_alarm_callback = callback;
			retval = SDLMOD_SYS_StartTimer();
		}
	}
	if (SDLMOD_timer_threaded) {
		pthread_mutex_unlock(SDLMOD_timer_mutex);
	}
	return retval;
}
