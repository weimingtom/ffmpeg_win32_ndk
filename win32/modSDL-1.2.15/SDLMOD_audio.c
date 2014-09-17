#include "SDLMOD_audio.h"

#include <pthread.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#define DEBUG_AUDIO 0
#define DEBUG_BUILD 0
#define SOUND_DEBUG 0

typedef struct SDLMOD_AudioDevice SDLMOD_AudioDevice;

#define _THIS	SDLMOD_AudioDevice *_this
#ifndef _STATUS
#define _STATUS	SDLMOD_status *status
#endif
struct SDLMOD_AudioDevice {
	const char *name;
	const char *desc;
	int  (*OpenAudio)(_THIS, SDLMOD_AudioSpec *spec);
	void (*ThreadInit)(_THIS);
	void (*WaitAudio)(_THIS);
	void (*PlayAudio)(_THIS);
	uint8_t *(*GetAudioBuf)(_THIS);
	void (*WaitDone)(_THIS);
	void (*CloseAudio)(_THIS);
	void (*LockAudio)(_THIS);
	void (*UnlockAudio)(_THIS);
	void (*SetCaption)(_THIS, const char *caption);
	SDLMOD_AudioSpec spec;
	int enabled;
	int paused;
	int opened;
	uint8_t *fake_stream;
	pthread_mutex_t *mixer_lock;
	pthread_t *thread;
	pthread_t threadid;
	uint32_t threadid_valid;
	struct SDLMOD_PrivateAudioData *hidden;
	void (*free)(_THIS);
};
#undef _THIS

typedef struct AudioBootStrap {
	const char *name;
	const char *desc;
	int (*available)(void);
	SDLMOD_AudioDevice *(*create)(int devindex);
} AudioBootStrap;

//AudioBootStrap WAVEOUT_bootstrap;
//SDLMOD_AudioDevice *current_audio;

#define _THIS	SDLMOD_AudioDevice *this

#define NUM_BUFFERS 2

struct SDLMOD_PrivateAudioData {
	HWAVEOUT sound;
	HANDLE audio_sem;
	uint8_t *mixbuf;
	WAVEHDR wavebuf[NUM_BUFFERS];
	int next_buffer;
};

#define sound			(this->hidden->sound)
#define audio_sem 		(this->hidden->audio_sem)
#define mixbuf			(this->hidden->mixbuf)
#define wavebuf			(this->hidden->wavebuf)
#define next_buffer		(this->hidden->next_buffer)



extern uint16_t SDLMOD_FirstAudioFormat(uint16_t format);
extern uint16_t SDLMOD_NextAudioFormat(void);

extern void SDLMOD_CalculateAudioSpec(SDLMOD_AudioSpec *spec);

extern int SDLMOD_RunAudio(void *audiop);

//static AudioBootStrap *bootstrap[] = {
//	&WAVEOUT_bootstrap,
//	NULL
//};
static SDLMOD_AudioDevice *current_audio = NULL;

int SDLMOD_AudioInit(const char *driver_name);
void SDLMOD_AudioQuit(void);








#define SDLMOD_min(x, y)	(((x) < (y)) ? (x) : (y))
#define SDLMOD_max(x, y)	(((x) > (y)) ? (x) : (y))
#define SDLMOD_arraysize(array)	(sizeof(array)/sizeof(array[0]))

size_t SDLMOD_strlcpy(char *dst, const char *src, size_t maxlen)
{
    size_t srclen = strlen(src);
    if ( maxlen > 0 ) {
        size_t len = SDLMOD_min(srclen, maxlen-1);
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return srclen;
}

size_t SDLMOD_strlcat(char *dst, const char *src, size_t maxlen)
{
    size_t dstlen = strlen(dst);
    size_t srclen = strlen(src);
    if ( dstlen < maxlen ) {
        SDLMOD_strlcpy(dst+dstlen, src, maxlen-dstlen);
    }
    return dstlen+srclen;
}










static int DIB_OpenAudio(_THIS, SDLMOD_AudioSpec *spec);
static void DIB_ThreadInit(_THIS);
static void DIB_WaitAudio(_THIS);
static uint8_t *DIB_GetAudioBuf(_THIS);
static void DIB_PlayAudio(_THIS);
static void DIB_WaitDone(_THIS);
static void DIB_CloseAudio(_THIS);

static int Audio_Available(void)
{
	return(1);
}

static void Audio_DeleteDevice(SDLMOD_AudioDevice *device)
{
	free(device->hidden);
	free(device);
}

static SDLMOD_AudioDevice *Audio_CreateDevice(int devindex)
{
	SDLMOD_AudioDevice *this;
	this = (SDLMOD_AudioDevice *)malloc(sizeof(SDLMOD_AudioDevice));
	if ( this ) {
		memset(this, 0, (sizeof *this));
		this->hidden = (struct SDLMOD_PrivateAudioData *)
				malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		fprintf(stderr, "Out of memory\n");
		if ( this ) {
			free(this);
		}
		return(0);
	}
	memset(this->hidden, 0, (sizeof *this->hidden));
	this->OpenAudio = DIB_OpenAudio;
	this->ThreadInit = DIB_ThreadInit;
	this->WaitAudio = DIB_WaitAudio;
	this->PlayAudio = DIB_PlayAudio;
	this->GetAudioBuf = DIB_GetAudioBuf;
	this->WaitDone = DIB_WaitDone;
	this->CloseAudio = DIB_CloseAudio;
	this->free = Audio_DeleteDevice;
	return this;
}

static AudioBootStrap WAVEOUT_bootstrap = {
	"waveout", "Win95/98/NT/2000 WaveOut",
	Audio_Available, Audio_CreateDevice
};

static void CALLBACK FillSound(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
						DWORD dwParam1, DWORD dwParam2)
{
	SDLMOD_AudioDevice *this = (SDLMOD_AudioDevice *)dwInstance;
	if ( uMsg != WOM_DONE )
		return;
	ReleaseSemaphore(audio_sem, 1, NULL);
}

static void SetMMerror(char *function, MMRESULT code)
{
	size_t len;
	char errbuf[MAXERRORLENGTH];
	snprintf(errbuf, SDLMOD_arraysize(errbuf), "%s: ", function);
	len = strlen(errbuf);
	waveOutGetErrorText(code, errbuf+len, (UINT)(MAXERRORLENGTH-len));
	fprintf(stderr, "%s\n", errbuf);
}

static void DIB_ThreadInit(_THIS)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

void DIB_WaitAudio(_THIS)
{
	WaitForSingleObject(audio_sem, INFINITE);
}

uint8_t *DIB_GetAudioBuf(_THIS)
{
    uint8_t *retval;
	retval = (uint8_t *)(wavebuf[next_buffer].lpData);
	return retval;
}

void DIB_PlayAudio(_THIS)
{
#if DEBUG_BUILD
        printf("[DIB_PlayAudio] : begin\n");
#endif
	waveOutWrite(sound, &wavebuf[next_buffer], sizeof(wavebuf[0]));
	next_buffer = (next_buffer+1)%NUM_BUFFERS;
#if DEBUG_BUILD
        printf("[DIB_PlayAudio] : end\n");
#endif
}

void DIB_WaitDone(_THIS)
{
	int i, left;
	do {
		left = NUM_BUFFERS;
		for ( i=0; i<NUM_BUFFERS; ++i ) {
			if ( wavebuf[i].dwFlags & WHDR_DONE ) {
				--left;
			}
		}
		if ( left > 0 ) {
			SDLMOD_Delay(100);
		}
	} while ( left > 0 );
}

void DIB_CloseAudio(_THIS)
{
	int i;
	if ( audio_sem ) {
		CloseHandle(audio_sem);
	}
	if ( sound ) {
		waveOutClose(sound);
	}
	for ( i=0; i<NUM_BUFFERS; ++i ) {
		if ( wavebuf[i].dwUser != 0xFFFF ) {
			waveOutUnprepareHeader(sound, &wavebuf[i],
						sizeof(wavebuf[i]));
			wavebuf[i].dwUser = 0xFFFF;
		}
	}
	if ( mixbuf != NULL ) {
		free(mixbuf);
		mixbuf = NULL;
	}
}

int DIB_OpenAudio(_THIS, SDLMOD_AudioSpec *spec)
{
	MMRESULT result;
	int i;
	WAVEFORMATEX waveformat;
	sound = NULL;
	audio_sem = NULL;
	for ( i = 0; i < NUM_BUFFERS; ++i )
		wavebuf[i].dwUser = 0xFFFF;
	mixbuf = NULL;
	memset(&waveformat, 0, sizeof(waveformat));
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	switch ( spec->format & 0xFF ) {
	case 8:
		spec->format = AUDIO_U8;
		waveformat.wBitsPerSample = 8;
		break;
		
	case 16:
		spec->format = AUDIO_S16;
		waveformat.wBitsPerSample = 16;
		break;
		
	default:
		fprintf(stderr, "Unsupported audio format\n");
		return(-1);
	}
	waveformat.nChannels = spec->channels;
	waveformat.nSamplesPerSec = spec->freq;
	waveformat.nBlockAlign =
		waveformat.nChannels * (waveformat.wBitsPerSample/8);
	waveformat.nAvgBytesPerSec = 
		waveformat.nSamplesPerSec * waveformat.nBlockAlign;
	if ( spec->samples < (spec->freq/4) )
		spec->samples = ((spec->freq/4)+3)&~3;
	SDLMOD_CalculateAudioSpec(spec);
	result = waveOutOpen(&sound, WAVE_MAPPER, &waveformat,
			(DWORD_PTR)FillSound, (DWORD_PTR)this, CALLBACK_FUNCTION);
	if ( result != MMSYSERR_NOERROR ) {
		SetMMerror("waveOutOpen()", result);
		return(-1);
	}
#if SOUND_DEBUG
	{
		WAVEOUTCAPS caps;
		result = waveOutGetDevCaps((UINT)sound, &caps, sizeof(caps));
		if ( result != MMSYSERR_NOERROR ) {
			SetMMerror("waveOutGetDevCaps()", result);
			return(-1);
		}
		printf("Audio device: %s\n", caps.szPname);
	}
#endif
	audio_sem = CreateSemaphore(NULL, NUM_BUFFERS-1, NUM_BUFFERS, NULL);
	if ( audio_sem == NULL ) {
		fprintf(stderr, "Couldn't create semaphore\n");
		return(-1);
	}
	mixbuf = (uint8_t *)malloc(NUM_BUFFERS*spec->size);
	if ( mixbuf == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(-1);
	}
	for ( i = 0; i < NUM_BUFFERS; ++i ) {
		memset(&wavebuf[i], 0, sizeof(wavebuf[i]));
		wavebuf[i].lpData = (LPSTR) &mixbuf[i*spec->size];
		wavebuf[i].dwBufferLength = spec->size;
		wavebuf[i].dwFlags = WHDR_DONE;
		result = waveOutPrepareHeader(sound, &wavebuf[i],
							sizeof(wavebuf[i]));
		if ( result != MMSYSERR_NOERROR ) {
			SetMMerror("waveOutPrepareHeader()", result);
			return(-1);
		}
	}
	next_buffer = 0;
	return(0);
}


















static AudioBootStrap *bootstrap[] = {
	&WAVEOUT_bootstrap,
	NULL
};




int SDLMOD_RunAudio(void *audiop)
{
	SDLMOD_AudioDevice *audio = (SDLMOD_AudioDevice *)audiop;
	uint8_t *stream;
	int    stream_len;
	void  *udata;
	void (*fill)(void *userdata,uint8_t *stream, int len);
	int    silence;
	
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task start.....1\n");
#endif
	if ( audio->ThreadInit ) {
		audio->ThreadInit(audio);
	}
	audio->threadid = pthread_self();
	audio->threadid_valid = 1;
	fill  = audio->spec.callback;
	udata = audio->spec.userdata;
	silence = audio->spec.silence;
	stream_len = audio->spec.size;
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task start.....2\n");
#endif
	while ( audio->enabled ) {
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task loop.....1\n");
#endif
		stream = audio->GetAudioBuf(audio);
		if ( stream == NULL ) {
			stream = audio->fake_stream;
		}
		memset(stream, silence, stream_len);
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task loop.....2, audio->paused == %d \n", audio->paused);
#endif
		if ( ! audio->paused ) {
			pthread_mutex_lock(audio->mixer_lock);
			(*fill)(udata, stream, stream_len);
			pthread_mutex_unlock(audio->mixer_lock);
		}
		if ( stream != audio->fake_stream ) {
			audio->PlayAudio(audio);
		}
		if ( stream == audio->fake_stream ) {
			SDLMOD_Delay((audio->spec.samples*1000)/audio->spec.freq);
		} else {
			audio->WaitAudio(audio);
		}
	}
	if ( audio->WaitDone ) {
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task wait done.....\n");
#endif
		audio->WaitDone(audio);
	}
#if DEBUG_BUILD
        printf("[SDLMOD_RunAudio] : Task exiting.\n");
#endif
	return(0);
}

static void SDLMOD_LockAudio_Default(SDLMOD_AudioDevice *audio)
{
	if ( audio->thread && (audio->threadid_valid && !pthread_equal(pthread_self(), audio->threadid)) ) {
		return;
	}
	pthread_mutex_lock(audio->mixer_lock);
}

static void SDLMOD_UnlockAudio_Default(SDLMOD_AudioDevice *audio)
{
	if ( audio->thread && (audio->threadid_valid && !pthread_equal(pthread_self(), audio->threadid)) ) {
		return;
	}
	pthread_mutex_unlock(audio->mixer_lock);
}

static uint16_t SDLMOD_ParseAudioFormat(const char *string)
{
	uint16_t format = 0;
	switch (*string) {
	    case 'U':
		++string;
		format |= 0x0000;
		break;
	    case 'S':
		++string;
		format |= 0x8000;
		break;
	    default:
		return 0;
	}
	switch (SDLMOD_atoi(string)) {
	    case 8:
		string += 1;
		format |= 8;
		break;
	    case 16:
		string += 2;
		format |= 16;
		if ( SDLMOD_strcmp(string, "LSB") == 0
#if SDLMOD_BYTEORDER == SDLMOD_LIL_ENDIAN
		     || SDLMOD_strcmp(string, "SYS") == 0
#endif
		    ) {
			format |= 0x0000;
		}
		if ( SDLMOD_strcmp(string, "MSB") == 0
#if SDLMOD_BYTEORDER == SDLMOD_BIG_ENDIAN
		     || SDLMOD_strcmp(string, "SYS") == 0
#endif
		    ) {
			format |= 0x1000;
		}
		break;
	    default:
		return 0;
	}
	return format;
}

int SDLMOD_AudioInit(const char *driver_name)
{
	SDLMOD_AudioDevice *audio;
	int i = 0, idx;
	if ( current_audio != NULL ) {
		SDLMOD_AudioQuit();
	}
	audio = NULL;
	idx = 0;
	if ( audio == NULL ) {
		if ( driver_name != NULL ) {
			for ( i=0; bootstrap[i]; ++i ) {
				if (strcasecmp(bootstrap[i]->name, driver_name) == 0) {
					if ( bootstrap[i]->available() ) {
						audio=bootstrap[i]->create(idx);
						break;
					}
				}
			}
		} else {
			for ( i=0; bootstrap[i]; ++i ) {
				if ( bootstrap[i]->available() ) {
					audio = bootstrap[i]->create(idx);
					if ( audio != NULL ) {
						break;
					}
				}
			}
		}
		if ( audio == NULL ) {
			fprintf(stderr, "No available audio device\n");
		}
	}
	current_audio = audio;
	if ( current_audio ) {
		current_audio->name = bootstrap[i]->name;
		if ( !current_audio->LockAudio && !current_audio->UnlockAudio ) {
			current_audio->LockAudio = SDLMOD_LockAudio_Default;
			current_audio->UnlockAudio = SDLMOD_UnlockAudio_Default;
		}
	}
	return(0);
}

char *SDLMOD_AudioDriverName(char *namebuf, int maxlen)
{
	if ( current_audio != NULL ) {
		SDLMOD_strlcpy(namebuf, current_audio->name, maxlen);
		return(namebuf);
	}
	return(NULL);
}


static void *run_audio(void *data)
{
	SDLMOD_RunAudio(data);
	return NULL;
}
int SDLMOD_OpenAudio(SDLMOD_AudioSpec *desired, SDLMOD_AudioSpec *obtained)
{
	SDLMOD_AudioDevice *audio;
	const char *env;
	int ret;
	if ( ! current_audio ) {
		if ( (SDLMOD_AudioInit(NULL) < 0) ||
		     (current_audio == NULL) ) {
			return(-1);
		}
	}
	audio = current_audio;
	if (audio->opened) {
		fprintf(stderr, "Audio device is already opened\n");
		return(-1);
	}
	if ( desired->freq == 0 ) {
		desired->freq = 22050;
	}
	if ( desired->format == 0 ) {
		desired->format = AUDIO_S16;
	}
	if ( desired->channels == 0 ) {
		desired->channels = 2;
	}
	switch ( desired->channels ) {
	case 1:
	case 2:
	case 4:
	case 6:
		break;
		
	default:
		fprintf(stderr, "1 (mono) and 2 (stereo) channels supported\n");
		return(-1);
	}
	if ( desired->samples == 0 ) {
		int samples = (desired->freq / 1000) * 46;
		int power2 = 1;
		while ( power2 < samples ) {
			power2 *= 2;
		}
		desired->samples = power2;
	}
	if ( desired->callback == NULL ) {
		fprintf(stderr, "SDLMOD_OpenAudio() passed a NULL callback\n");
		return(-1);
	}

#if SDLMOD_THREADS_DISABLED

#else
	audio->mixer_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	ret = pthread_mutex_init(audio->mixer_lock, NULL);
	if ( ret != 0 ) {
		free(audio->mixer_lock);
		fprintf(stderr, "Couldn't create mixer lock\n");
		SDLMOD_CloseAudio();
		return(-1);
	}
#endif
	SDLMOD_CalculateAudioSpec(desired);
	memcpy(&audio->spec, desired, sizeof(audio->spec));
	audio->enabled = 1;
	audio->paused  = 1;
	audio->opened = audio->OpenAudio(audio, &audio->spec)+1;
	if ( ! audio->opened ) {
		SDLMOD_CloseAudio();
		return(-1);
	}
	if ( audio->spec.samples != desired->samples ) {
		desired->samples = audio->spec.samples;
		SDLMOD_CalculateAudioSpec(desired);
	}
	audio->fake_stream = malloc(audio->spec.size);
	if ( audio->fake_stream == NULL ) {
		SDLMOD_CloseAudio();
		fprintf(stderr, "Out of memory\n");
		return(-1);
	}
	if ( obtained != NULL ) {
		memcpy(obtained, &audio->spec, sizeof(audio->spec));
	} else if ( desired->freq != audio->spec.freq ||
                    desired->format != audio->spec.format ||
	            desired->channels != audio->spec.channels ) {
#if DEBUG_BUILD
	printf("SDLMOD_OpenAudio : audio is not desired...\n");
#endif
			SDLMOD_CloseAudio();
			return(-1);
	}
	switch (audio->opened) {
	case 1:
		//audio->thread = SDLMOD_CreateThread(SDLMOD_RunAudio, audio);
		//if ( audio->thread == NULL ) {
		//	SDLMOD_CloseAudio();
		//	fprintf(stderr, "Couldn't create audio thread\n");
		//	return(-1);
		//}
		{
			int err;
			audio->thread = (pthread_t *)malloc(sizeof(pthread_t));
			err = pthread_create(audio->thread, NULL, run_audio, audio);
			if (err!=0)
			{
				free(audio->thread);
				audio->thread = NULL;
				SDLMOD_CloseAudio();
				fprintf(stderr, "Couldn't create audio thread\n");
				return(-1);
			}
		}
		break;
		
	default:
		break;
	}
	return(0);
}

SDLMOD_audiostatus SDLMOD_GetAudioStatus(void)
{
	SDLMOD_AudioDevice *audio = current_audio;
	SDLMOD_audiostatus status;
	status = SDLMOD_AUDIO_STOPPED;
	if ( audio && audio->enabled ) {
		if ( audio->paused ) {
			status = SDLMOD_AUDIO_PAUSED;
		} else {
			status = SDLMOD_AUDIO_PLAYING;
		}
	}
	return(status);
}

void SDLMOD_PauseAudio (int pause_on)
{
	SDLMOD_AudioDevice *audio = current_audio;
	if ( audio ) {
		audio->paused = pause_on;
	}
}

void SDLMOD_LockAudio (void)
{
	SDLMOD_AudioDevice *audio = current_audio;
	if ( audio && audio->LockAudio ) {
		audio->LockAudio(audio);
	}
}

void SDLMOD_UnlockAudio (void)
{
	SDLMOD_AudioDevice *audio = current_audio;
	if ( audio && audio->UnlockAudio ) {
		audio->UnlockAudio(audio);
	}
}

void SDLMOD_CloseAudio (void)
{
	SDLMOD_AudioQuit();
}

void SDLMOD_AudioQuit(void)
{
	SDLMOD_AudioDevice *audio = current_audio;
	if ( audio ) {
		audio->enabled = 0;
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 1...\n");
#endif
		if ( audio->thread != NULL ) {
			//SDLMOD_WaitThread(audio->thread, NULL);
			pthread_join(*(audio->thread), NULL);
			free(audio->thread);
			audio->thread = NULL;
		}
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 2...\n");
#endif
		if ( audio->mixer_lock != NULL ) {
			pthread_mutex_destroy(audio->mixer_lock);
			free(audio->mixer_lock);
			audio->mixer_lock = NULL;
		}
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 3...\n");
#endif
		if ( audio->fake_stream != NULL ) {
			free(audio->fake_stream);
		}
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 4...\n");
#endif
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 5...\n");
#endif
		if ( audio->opened ) {
			audio->CloseAudio(audio);
			audio->opened = 0;
		}
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 6...\n");
#endif
		audio->free(audio);
		current_audio = NULL;
#if DEBUG_AUDIO
	printf("SDLMOD_AudioQuit() 7...\n");
#endif
	}
}

#define NUM_FORMATS	6
static int format_idx;
static int format_idx_sub;
static uint16_t format_list[NUM_FORMATS][NUM_FORMATS] = {
 { AUDIO_U8, AUDIO_S8, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB },
 { AUDIO_S8, AUDIO_U8, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB },
 { AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_U8, AUDIO_S8 },
 { AUDIO_S16MSB, AUDIO_S16LSB, AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_U8, AUDIO_S8 },
 { AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U8, AUDIO_S8 },
 { AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_S16MSB, AUDIO_S16LSB, AUDIO_U8, AUDIO_S8 },
};

uint16_t SDLMOD_FirstAudioFormat(uint16_t format)
{
	for ( format_idx=0; format_idx < NUM_FORMATS; ++format_idx ) {
		if ( format_list[format_idx][0] == format ) {
			break;
		}
	}
	format_idx_sub = 0;
	return(SDLMOD_NextAudioFormat());
}

uint16_t SDLMOD_NextAudioFormat(void)
{
	if ( (format_idx == NUM_FORMATS) || (format_idx_sub == NUM_FORMATS) ) {
		return(0);
	}
	return(format_list[format_idx][format_idx_sub++]);
}

void SDLMOD_CalculateAudioSpec(SDLMOD_AudioSpec *spec)
{
	switch (spec->format) {
	case AUDIO_U8:
		spec->silence = 0x80;
		break;
		
	default:
		spec->silence = 0x00;
		break;
	}
	spec->size = (spec->format&0xFF)/8;
	spec->size *= spec->channels;
	spec->size *= spec->samples;
}

void SDLMOD_Audio_SetCaption(const char *caption)
{
	if ((current_audio) && (current_audio->SetCaption)) {
		current_audio->SetCaption(current_audio, caption);
	}
}




