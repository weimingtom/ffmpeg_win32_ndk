CC := gcc
CPP := g++
RC := windres
RM = rm -f

CFLAGS := -Wall -pedantic -ansi -std=c99 -I./glut/include -DGLUT_STATIC
CPPFLAGS := -Wall -pedantic -I./glut/include -DGLUT_STATIC
LDFLAGS	:= -L./glut/lib -lglut32 -lglaux -lglu32 -lopengl32 -lwinmm -lgdi32 -lcomdlg32

# objview.a(Texture.o):Texture.cpp:(.text+0x618): undefined reference to `glActiveTexture@4'

OBJS := 
OBJS += TextureLoader.o

# Rendering
LIB_BIN := objview.a
BINS := demo.exe

all: $(BINS)

$(LIB_BIN) : $(OBJS)
	ar r $@ $(OBJS)
	ranlib $@

demo.exe : opengl_sample.o $(LIB_BIN)
	$(CPP) $(CPPFLAGS) -o $@ $^ $(LDFLAGS) 

%.o : %.cpp
	$(CPP) $(CPPFLAGS) -o $@ -c $^

.PHONY: clean test

clean:
	$(RM) $(OBJS) $(LIB_BIN) $(BINS) *.o

test:
	./demo.exe
