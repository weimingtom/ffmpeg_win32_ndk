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

#define SDLMOD_LIL_ENDIAN	1234
#define SDLMOD_BIG_ENDIAN	4321
#define SDLMOD_BYTEORDER 1234

#include <stdint.h>
//for NULL
#include <stdlib.h> 
//typedef int8_t		Sint8;
//typedef uint8_t		Uint8;
//typedef int16_t		Sint16;
//typedef uint16_t	Uint16;
//typedef int32_t		Sint32;
//typedef uint32_t	Uint32;

typedef struct SDLMOD_AudioSpec {
	int freq;
	uint16_t format;
	uint8_t  channels;
	uint8_t  silence;
	uint16_t samples;
	uint16_t padding;
	uint32_t size;
	void (*callback)(void *userdata, uint8_t *stream, int len);
	void  *userdata;
} SDLMOD_AudioSpec;

#define AUDIO_U8	0x0008
#define AUDIO_S8	0x8008
#define AUDIO_U16LSB	0x0010
#define AUDIO_S16LSB	0x8010
#define AUDIO_U16MSB	0x1010
#define AUDIO_S16MSB	0x9010
#define AUDIO_U16	AUDIO_U16LSB
#define AUDIO_S16	AUDIO_S16LSB

#if SDLMOD_BYTEORDER == SDLMOD_LIL_ENDIAN
#define AUDIO_U16SYS	AUDIO_U16LSB
#define AUDIO_S16SYS	AUDIO_S16LSB
#else
#define AUDIO_U16SYS	AUDIO_U16MSB
#define AUDIO_S16SYS	AUDIO_S16MSB
#endif

extern int SDLMOD_AudioInit(const char *driver_name);
extern void SDLMOD_AudioQuit(void);
extern char * SDLMOD_AudioDriverName(char *namebuf, int maxlen);
extern int SDLMOD_OpenAudio(SDLMOD_AudioSpec *desired, SDLMOD_AudioSpec *obtained);
typedef enum {
	SDLMOD_AUDIO_STOPPED = 0,
	SDLMOD_AUDIO_PLAYING,
	SDLMOD_AUDIO_PAUSED
} SDLMOD_audiostatus;
extern SDLMOD_audiostatus SDLMOD_GetAudioStatus(void);
extern void SDLMOD_PauseAudio(int pause_on);

extern void SDLMOD_LockAudio(void);
extern void SDLMOD_UnlockAudio(void);
extern void SDLMOD_CloseAudio(void);

#ifdef __cplusplus
}
#endif
