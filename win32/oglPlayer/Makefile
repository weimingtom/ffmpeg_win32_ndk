# Makefile to build the SDL library

CC = gcc
CPP = g++
AR	= ar
RANLIB	= ranlib

CFLAGS := -g -O2 
CFLAGS += 
CFLAGS += -I./ffmpeg/include 
CFLAGS += -DPTW32_STATIC_LIB
CFLAGS += -I./glut/include -DGLUT_STATIC

LDFLAGS :=
LDFLAGS	+= -L./glut/lib -lglut32 -lglaux -lglu32 -lopengl32 -lwinmm -lgdi32 -lcomdlg32
# -lavdevice
LDFLAGS += -L./ffmpeg/lib  -lavfilter -lpostproc -lavformat -lavcodec 
LDFLAGS += -lavicap32 -lws2_32 -lmingw32 -lbz2 -lz -lpsapi -ladvapi32 
LDFLAGS += -lshell32 -lwinmm -lgdi32 
LDFLAGS += -lswresample -lswscale -lavutil -lm -lpthreadGC2 -lws2_32

CPPFLAGS := $(CFLAGS)

TARGET := libSDL.a

OBJECTS += SDLMOD_events.o
OBJECTS += SDLMOD_audio.o
OBJECTS += SDLMOD_surface.o
OBJECTS += SDLMOD_yuv.o 
OBJECTS += SDLMOD_timer.o
OBJECTS += TextureLoader.o

all: $(TARGET) main.exe

main.exe : main.o $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET): $(OBJECTS)
	$(AR) cr $@ $^
	$(RANLIB) $@

clean:
	rm -rf $(TARGET) $(OBJECTS) *.exe *.o
