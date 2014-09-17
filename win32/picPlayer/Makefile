# Makefile to build the SDL library

CC = gcc
CPP = g++
AR	= ar
RANLIB	= ranlib

CFLAGS := -g3 -O0 
CFLAGS += 
CFLAGS += -I./include -I./ffmpeg/include
CFLAGS += -I./glut/include
CFLAGS += -DPTW32_STATIC_LIB
CFLAGS += -DNO_STDIO_REDIRECT

LDFLAGS := -L./ffmpeg/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lavicap32 -lws2_32 -lmingw32 -lbz2 -lz -lpsapi -ladvapi32 -lshell32 -lwinmm -lgdi32 -lswresample -lswscale -lavutil -lm
LDFLAGS += -lpthreadGC2 -lws2_32

CPPFLAGS := $(CFLAGS)



TARGET := libSDL.a

OBJECTS := src/SDL.o src/SDL_error.o src/SDL_fatal.o

OBJECTS += src/audio/SDL_audio.o src/audio/SDL_audiocvt.o src/audio/SDL_audiodev.o
OBJECTS += src/audio/SDL_mixer.o src/audio/SDL_mixer_m68k.o src/audio/SDL_mixer_MMX.o
OBJECTS += src/audio/SDL_mixer_MMX_VC.o src/audio/SDL_wave.o

OBJECTS += src/cdrom/SDL_cdrom.o

OBJECTS += src/cpuinfo/SDL_cpuinfo.o

OBJECTS += src/events/SDL_active.o src/events/SDL_events.o src/events/SDL_expose.o
OBJECTS += src/events/SDL_keyboard.o src/events/SDL_mouse.o
OBJECTS += src/events/SDL_quit.o src/events/SDL_resize.o

OBJECTS += src/file/SDL_rwops.o

OBJECTS += src/joystick/SDL_joystick.o

OBJECTS += src/stdlib/SDL_getenv.o src/stdlib/SDL_iconv.o src/stdlib/SDL_malloc.o
OBJECTS += src/stdlib/SDL_qsort.o src/stdlib/SDL_stdlib.o src/stdlib/SDL_string.o

OBJECTS += src/thread/SDL_thread.o

OBJECTS += src/timer/SDL_timer.o

OBJECTS += src/video/SDL_blit.o src/video/SDL_blit_0.o src/video/SDL_blit_1.o
OBJECTS += src/video/SDL_blit_A.o src/video/SDL_blit_N.o src/video/SDL_bmp.o
OBJECTS += src/video/SDL_cursor.o src/video/SDL_gamma.o src/video/SDL_pixels.o
OBJECTS += src/video/SDL_RLEaccel.o src/video/SDL_stretch.o src/video/SDL_surface.o
OBJECTS += src/video/SDL_video.o src/video/SDL_yuv.o src/video/SDL_yuv_mmx.o
OBJECTS += src/video/SDL_yuv_sw.o

OBJECTS += src/audio/dummy/SDL_dummyaudio.o

OBJECTS += src/audio/windib/SDL_dibaudio.o

OBJECTS += src/audio/disk/SDL_diskaudio.o

OBJECTS += src/video/dummy/SDL_nullevents.o 
OBJECTS += src/video/dummy/SDL_nullmouse.o
OBJECTS += src/video/dummy/SDL_nullvideo.o

OBJECTS += src/video/windib/SDL_dibevents.o
OBJECTS += src/video/windib/SDL_dibvideo.o

OBJECTS += src/video/wincommon/SDL_sysevents.o
OBJECTS += src/video/wincommon/SDL_sysmouse.o
OBJECTS += src/video/wincommon/SDL_syswm.o
OBJECTS += src/video/wincommon/SDL_wingl.o

OBJECTS += src/joystick/win32/SDL_mmjoystick.o

OBJECTS += src/cdrom/win32/SDL_syscdrom.o

OBJECTS += src/thread/win32/SDL_sysmutex.o
OBJECTS += src/thread/win32/SDL_syssem.o
OBJECTS += src/thread/win32/SDL_systhread.o
OBJECTS += src/thread/generic/SDL_syscond.o

OBJECTS += src/timer/win32/SDL_systimer.o

OBJECTS += src/loadso/win32/SDL_sysloadso.o

#MAIN_SRC := main_fullver.c
MAIN_SRC := main.c

all: $(TARGET) main.exe

main.exe : $(MAIN_SRC) TextureLoader.o SDLMOD_timer.o src/main/win32/SDL_win32_main.c $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $^

$(TARGET): $(OBJECTS)
	$(AR) cr $@ $^
	
#$(RANLIB) $@

clean:
	rm -f $(TARGET) $(OBJECTS) *.exe *.o
