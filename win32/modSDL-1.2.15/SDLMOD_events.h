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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
//for NULL
#include <stdlib.h> 
//typedef int8_t		Sint8;
//typedef uint8_t		Uint8;
//typedef int16_t		Sint16;
//typedef uint16_t	Uint16;
//typedef int32_t		Sint32;
//typedef uint32_t	Uint32;

#define SDLMOD_INIT_EVENTTHREAD	0x01000000







typedef enum {
    SDLMOD_NOEVENT = 0,
    SDLMOD_QUIT,
    SDLMOD_USEREVENT = 24,
    SDLMOD_NUMEVENTS = 32
} SDLMOD_EventType;

#define SDLMOD_EVENTMASK(X)	(1<<(X))
typedef enum {
	SDLMOD_QUITMASK		= SDLMOD_EVENTMASK(SDLMOD_QUIT),
} SDLMOD_EventMask ;

#define SDLMOD_ALLEVENTS		0xFFFFFFFF

typedef struct SDLMOD_UserEvent {
	uint8_t type;
	int code;
	void *data1;
	void *data2;
} SDLMOD_UserEvent;

typedef union SDLMOD_Event {
	uint8_t type;
	SDLMOD_UserEvent user;
} SDLMOD_Event;

extern void SDLMOD_PumpEvents(void);

typedef enum {
	SDLMOD_ADDEVENT,
	SDLMOD_PEEKEVENT,
	SDLMOD_GETEVENT
} SDLMOD_eventaction;

extern int SDLMOD_PeepEvents(SDLMOD_Event *events, int numevents, SDLMOD_eventaction action, uint32_t mask);
extern int SDLMOD_PollEvent(SDLMOD_Event *event);
extern int SDLMOD_WaitEvent(SDLMOD_Event *event);
extern int SDLMOD_PushEvent(SDLMOD_Event *event);

typedef int (*SDLMOD_EventFilter)(const SDLMOD_Event *event);
extern void SDLMOD_SetEventFilter(SDLMOD_EventFilter filter);
extern SDLMOD_EventFilter SDLMOD_GetEventFilter(void);

#define SDLMOD_QUERY	-1
#define SDLMOD_IGNORE	 0
#define SDLMOD_DISABLE	 0
#define SDLMOD_ENABLE	 1
extern uint8_t SDLMOD_EventState(uint8_t type, int state);

extern int SDLMOD_StartEventLoop(uint32_t flags);
extern void SDLMOD_StopEventLoop(void);

#ifdef __cplusplus
}
#endif
