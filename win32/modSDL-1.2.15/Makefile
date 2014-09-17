# Makefile to build the SDL library

CC = gcc
CPP = g++
AR	= ar
RANLIB	= ranlib

CFLAGS := -g -O2 
CFLAGS += 
CFLAGS += -I./ffmpeg/include 
CFLAGS += -DPTW32_STATIC_LIB
LDFLAGS :=
LDFLAGS += -L./ffmpeg/lib -lavdevice -lavfilter -lpostproc -lavformat -lavcodec -lavicap32 -lws2_32 -lmingw32 -lbz2 -lz -lpsapi -ladvapi32 -lshell32 -lwinmm -lgdi32 -lswresample -lswscale -lavutil -lm -lpthreadGC2 -lws2_32

CPPFLAGS := $(CFLAGS)

TARGET := libSDL.a

OBJECTS += SDLMOD_events.o
OBJECTS += SDLMOD_audio.o

all: $(TARGET) main.exe

main.exe : main.o $(TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET): $(OBJECTS)
	$(AR) cr $@ $^
	$(RANLIB) $@

clean:
	rm -rf $(TARGET) $(OBJECTS) *.exe *.o
