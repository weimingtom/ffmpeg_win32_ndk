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

