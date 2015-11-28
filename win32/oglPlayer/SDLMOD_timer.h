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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDLMOD_TIMESLICE 10
#define TIMER_RESOLUTION 10

extern uint32_t SDLMOD_GetTicks(void);
extern void SDLMOD_Delay(uint32_t ms);
typedef uint32_t (*SDLMOD_TimerCallback)(uint32_t interval);
extern int SDLMOD_SetTimer(uint32_t interval, SDLMOD_TimerCallback callback);
typedef uint32_t (*SDLMOD_NewTimerCallback)(uint32_t interval, void *param);
typedef struct _SDLMOD_TimerID *SDLMOD_TimerID;
extern SDLMOD_TimerID SDLMOD_AddTimer(uint32_t interval, SDLMOD_NewTimerCallback callback, void *param);
extern int SDLMOD_RemoveTimer(SDLMOD_TimerID t);

extern int SDLMOD_TimerInit(void);
extern void SDLMOD_TimerQuit(void);

#ifdef __cplusplus
}
#endif

