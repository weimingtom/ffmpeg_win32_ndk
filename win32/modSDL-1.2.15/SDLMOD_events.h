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
