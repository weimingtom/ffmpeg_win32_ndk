#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDLMOD_ALPHA_OPAQUE 255
#define SDLMOD_ALPHA_TRANSPARENT 0

typedef struct SDLMOD_Rect {
	int16_t x, y;
	uint16_t w, h;
} SDLMOD_Rect;

typedef struct SDLMOD_Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t unused;
} SDLMOD_Color;
#define SDLMOD_Colour SDLMOD_Color

typedef struct SDLMOD_Palette {
	int       ncolors;
	SDLMOD_Color *colors;
} SDLMOD_Palette;

typedef struct SDLMOD_PixelFormat {
	SDLMOD_Palette *palette;
	uint8_t  BitsPerPixel;
	uint8_t  BytesPerPixel;
	uint8_t  Rloss;
	uint8_t  Gloss;
	uint8_t  Bloss;
	uint8_t  Aloss;
	uint8_t  Rshift;
	uint8_t  Gshift;
	uint8_t  Bshift;
	uint8_t  Ashift;
	uint32_t Rmask;
	uint32_t Gmask;
	uint32_t Bmask;
	uint32_t Amask;
	uint32_t colorkey;
	uint8_t  alpha;
} SDLMOD_PixelFormat;

typedef struct SDLMOD_Surface {
	uint32_t flags;
	SDLMOD_PixelFormat *format;
	int w, h;
	uint16_t pitch;
	void *pixels;
	int offset;
	struct private_hwdata *hwdata;
	SDLMOD_Rect clip_rect;
	uint32_t unused1;
	uint32_t locked;
	struct SDLMOD_BlitMap *map;
	unsigned int format_version;
	int refcount;
} SDLMOD_Surface;

#define SDLMOD_SWSURFACE	0x00000000
#define SDLMOD_HWSURFACE	0x00000001
#define SDLMOD_ASYNCBLIT	0x00000004

#define SDLMOD_ANYFORMAT	0x10000000
#define SDLMOD_HWPALETTE	0x20000000
#define SDLMOD_DOUBLEBUF	0x40000000
#define SDLMOD_FULLSCREEN	0x80000000
#define SDLMOD_OPENGL       0x00000002
#define SDLMOD_OPENGLBLIT	0x0000000A
#define SDLMOD_RESIZABLE	0x00000010
#define SDLMOD_NOFRAME	    0x00000020

#define SDLMOD_HWACCEL	    0x00000100
#define SDLMOD_SRCCOLORKEY	0x00001000
#define SDLMOD_RLEACCELOK	0x00002000
#define SDLMOD_RLEACCEL	    0x00004000
#define SDLMOD_SRCALPHA	    0x00010000
#define SDLMOD_PREALLOC	    0x01000000

#define SDLMOD_MUSTLOCK(surface)	\
  (surface->offset ||		\
  ((surface->flags & (SDLMOD_HWSURFACE|SDLMOD_ASYNCBLIT|SDLMOD_RLEACCEL)) != 0))

typedef int (*SDLMOD_blit)(struct SDLMOD_Surface *src, SDLMOD_Rect *srcrect,
			struct SDLMOD_Surface *dst, SDLMOD_Rect *dstrect);

#define SDLMOD_YV12_OVERLAY  0x32315659
#define SDLMOD_IYUV_OVERLAY  0x56555949
#define SDLMOD_YUY2_OVERLAY  0x32595559
#define SDLMOD_UYVY_OVERLAY  0x59565955
#define SDLMOD_YVYU_OVERLAY  0x55595659

typedef struct SDLMOD_Overlay {
	uint32_t format;
	int w, h;
	int planes;
	uint16_t *pitches;
	uint8_t **pixels;
	struct private_yuvhwfuncs *hwfuncs;
	struct private_yuvhwdata *hwdata;
	uint32_t hw_overlay :1;
	uint32_t UnusedBits :31;
} SDLMOD_Overlay;

#define SDLMOD_AllocSurface    SDLMOD_CreateRGBSurface
extern SDLMOD_Surface * SDLMOD_CreateRGBSurface
			(uint32_t flags, int width, int height, int depth, 
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
extern SDLMOD_Surface * SDLMOD_CreateRGBSurfaceFrom(void *pixels,
			int width, int height, int depth, int pitch,
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
extern void SDLMOD_FreeSurface(SDLMOD_Surface *surface);
extern int SDLMOD_LockSurface(SDLMOD_Surface *surface);
extern void SDLMOD_UnlockSurface(SDLMOD_Surface *surface);

extern SDLMOD_Overlay * SDLMOD_CreateYUVOverlay(int width, int height,
				uint32_t format, SDLMOD_Surface *display);
extern int SDLMOD_LockYUVOverlay(SDLMOD_Overlay *overlay);
extern void SDLMOD_UnlockYUVOverlay(SDLMOD_Overlay *overlay);
extern int SDLMOD_DisplayYUVOverlay(SDLMOD_Overlay *overlay, SDLMOD_Rect *dstrect);
extern void SDLMOD_FreeYUVOverlay(SDLMOD_Overlay *overlay);

extern int SDLMOD_SoftStretch(SDLMOD_Surface *src, SDLMOD_Rect *srcrect,
                                    SDLMOD_Surface *dst, SDLMOD_Rect *dstrect);

#ifdef __cplusplus
}
#endif
