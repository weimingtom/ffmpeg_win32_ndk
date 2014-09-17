#include "SDLMOD_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#define SDL_static_cast(type, expression) ((type)(expression))

#define SDLMOD_memset4(dst, val, len)		\
do {						\
	unsigned _count = (len);		\
	unsigned _n = (_count + 3) / 4;		\
	uint32_t *_p = SDL_static_cast(uint32_t *, dst);	\
	uint32_t _val = (val);			\
	if (len == 0) break;			\
        switch (_count % 4) {			\
        case 0: do {    *_p++ = _val;		\
        case 3:         *_p++ = _val;		\
        case 2:         *_p++ = _val;		\
        case 1:         *_p++ = _val;		\
		} while ( --_n );		\
	}					\
} while(0)

typedef struct {
	uint8_t *s_pixels;
	int s_width;
	int s_height;
	int s_skip;
	uint8_t *d_pixels;
	int d_width;
	int d_height;
	int d_skip;
	void *aux_data;
	SDLMOD_PixelFormat *src;
	uint8_t *table;
	SDLMOD_PixelFormat *dst;
} SDLMOD_BlitInfo;

typedef void (*SDLMOD_loblit)(SDLMOD_BlitInfo *info);

struct private_swaccel {
	SDLMOD_loblit blit;
	void *aux_data;
};

typedef struct SDLMOD_BlitMap {
	SDLMOD_Surface *dst;
	int identity;
	uint8_t *table;
	SDLMOD_blit hw_blit;
	SDLMOD_blit sw_blit;
	struct private_hwaccel *hw_data;
	struct private_swaccel *sw_data;
    unsigned int format_version;
} SDLMOD_BlitMap;

SDLMOD_PixelFormat *SDLMOD_AllocFormat(int bpp,
		uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
SDLMOD_PixelFormat *SDLMOD_ReallocFormat(SDLMOD_Surface *surface, int bpp,
		uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask);
void SDLMOD_FormatChanged(SDLMOD_Surface *surface);
void SDLMOD_FreeFormat(SDLMOD_PixelFormat *format);

SDLMOD_BlitMap *SDLMOD_AllocBlitMap(void);
void SDLMOD_InvalidateMap(SDLMOD_BlitMap *map);
void SDLMOD_FreeBlitMap(SDLMOD_BlitMap *map);

uint16_t SDLMOD_CalculatePitch(SDLMOD_Surface *surface);

SDLMOD_Surface * SDLMOD_CreateRGBSurface (uint32_t flags,
			int width, int height, int depth,
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
	SDLMOD_Surface *screen;
	SDLMOD_Surface *surface;

	if ( width >= 16384 || height >= 65536 ) {
		fprintf(stderr, "Width or height is too large\n");
		return(NULL);
	}

	screen = NULL;
	flags &= ~SDLMOD_HWSURFACE;
	
	surface = (SDLMOD_Surface *)malloc(sizeof(*surface));
	if ( surface == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	surface->flags = SDLMOD_SWSURFACE;
	surface->format = SDLMOD_AllocFormat(depth, Rmask, Gmask, Bmask, Amask);
	if ( surface->format == NULL ) {
		free(surface);
		return(NULL);
	}
	if ( Amask ) {
		surface->flags |= SDLMOD_SRCALPHA;
	}
	surface->w = width;
	surface->h = height;
	surface->pitch = SDLMOD_CalculatePitch(surface);
	surface->pixels = NULL;
	surface->offset = 0;
	surface->hwdata = NULL;
	surface->locked = 0;
	surface->map = NULL;
	surface->unused1 = 0;
	SDLMOD_SetClipRect(surface, NULL);
	SDLMOD_FormatChanged(surface);

	if ( (flags&SDLMOD_HWSURFACE) == SDLMOD_SWSURFACE ) {
		if ( surface->w && surface->h ) {
			surface->pixels = malloc(surface->h*surface->pitch);
			if ( surface->pixels == NULL ) {
				SDLMOD_FreeSurface(surface);
				fprintf(stderr, "Out of memory\n");
				return(NULL);
			}
			memset(surface->pixels, 0, surface->h*surface->pitch);
		}
	}

	surface->map = SDLMOD_AllocBlitMap();
	if ( surface->map == NULL ) {
		SDLMOD_FreeSurface(surface);
		return(NULL);
	}

	surface->refcount = 1;
	return(surface);
}

SDLMOD_Surface * SDLMOD_CreateRGBSurfaceFrom (void *pixels,
			int width, int height, int depth, int pitch,
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
	SDLMOD_Surface *surface;
	surface = SDLMOD_CreateRGBSurface(SDLMOD_SWSURFACE, 0, 0, depth,
	                               Rmask, Gmask, Bmask, Amask);
	if ( surface != NULL ) {
		surface->flags |= SDLMOD_PREALLOC;
		surface->pixels = pixels;
		surface->w = width;
		surface->h = height;
		surface->pitch = pitch;
		SDLMOD_SetClipRect(surface, NULL);
	}
	return(surface);
}

static inline int SDLMOD_IntersectRect(const SDLMOD_Rect *A, const SDLMOD_Rect *B, SDLMOD_Rect *intersection)
{
	int Amin, Amax, Bmin, Bmax;

	Amin = A->x;
	Amax = Amin + A->w;
	Bmin = B->x;
	Bmax = Bmin + B->w;
	if(Bmin > Amin)
		Amin = Bmin;
	intersection->x = Amin;
	if(Bmax < Amax)
		Amax = Bmax;
	intersection->w = Amax - Amin > 0 ? Amax - Amin : 0;

	Amin = A->y;
	Amax = Amin + A->h;
	Bmin = B->y;
	Bmax = Bmin + B->h;
	if (Bmin > Amin)
		Amin = Bmin;
	intersection->y = Amin;
	if (Bmax < Amax)
		Amax = Bmax;
	intersection->h = Amax - Amin > 0 ? Amax - Amin : 0;
	return ((intersection->w && intersection->h) ? 1 : 0);
}

int SDLMOD_SetClipRect(SDLMOD_Surface *surface, const SDLMOD_Rect *rect)
{
	SDLMOD_Rect full_rect;
	if ( ! surface ) {
		return 0;
	}
	full_rect.x = 0;
	full_rect.y = 0;
	full_rect.w = surface->w;
	full_rect.h = surface->h;
	if ( ! rect ) {
		surface->clip_rect = full_rect;
		return 1;
	}
	return SDLMOD_IntersectRect(rect, &full_rect, &surface->clip_rect);
}

void SDLMOD_GetClipRect(SDLMOD_Surface *surface, SDLMOD_Rect *rect)
{
	if ( surface && rect ) {
		*rect = surface->clip_rect;
	}
}

int SDLMOD_LockSurface (SDLMOD_Surface *surface)
{
	if ( ! surface->locked ) {
		surface->pixels = (uint8_t *)surface->pixels + surface->offset;
	}
	++surface->locked;
	return(0);
}

void SDLMOD_UnlockSurface (SDLMOD_Surface *surface)
{
	if ( ! surface->locked || (--surface->locked > 0) ) {
		return;
	}
	surface->pixels = (uint8_t *)surface->pixels - surface->offset;
}

void SDLMOD_FreeSurface (SDLMOD_Surface *surface)
{
	if (surface == NULL) {
		return;
	}
	if ( --surface->refcount > 0 ) {
		return;
	}
	while ( surface->locked > 0 ) {
		SDLMOD_UnlockSurface(surface);
	}
	if ( surface->format ) {
		SDLMOD_FreeFormat(surface->format);
		surface->format = NULL;
	}
	if ( surface->map != NULL ) {
		SDLMOD_FreeBlitMap(surface->map);
		surface->map = NULL;
	}
	if ( surface->pixels &&
	     ((surface->flags & SDLMOD_PREALLOC) != SDLMOD_PREALLOC) ) {
		free(surface->pixels);
	}
	free(surface);
}






















SDLMOD_PixelFormat *SDLMOD_AllocFormat(int bpp,
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
	SDLMOD_PixelFormat *format;
	uint32_t mask;

	format = malloc(sizeof(*format));
	if ( format == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	memset(format, 0, sizeof(*format));
	format->alpha = SDLMOD_ALPHA_OPAQUE;

	format->BitsPerPixel = bpp;
	format->BytesPerPixel = (bpp+7)/8;
	if ( Rmask || Bmask || Gmask ) {
		format->palette = NULL;
		format->Rshift = 0;
		format->Rloss = 8;
		if ( Rmask ) {
			for ( mask = Rmask; !(mask&0x01); mask >>= 1 )
				++format->Rshift;
			for ( ; (mask&0x01); mask >>= 1 )
				--format->Rloss;
		}
		format->Gshift = 0;
		format->Gloss = 8;
		if ( Gmask ) {
			for ( mask = Gmask; !(mask&0x01); mask >>= 1 )
				++format->Gshift;
			for ( ; (mask&0x01); mask >>= 1 )
				--format->Gloss;
		}
		format->Bshift = 0;
		format->Bloss = 8;
		if ( Bmask ) {
			for ( mask = Bmask; !(mask&0x01); mask >>= 1 )
				++format->Bshift;
			for ( ; (mask&0x01); mask >>= 1 )
				--format->Bloss;
		}
		format->Ashift = 0;
		format->Aloss = 8;
		if ( Amask ) {
			for ( mask = Amask; !(mask&0x01); mask >>= 1 )
				++format->Ashift;
			for ( ; (mask&0x01); mask >>= 1 )
				--format->Aloss;
		}
		format->Rmask = Rmask;
		format->Gmask = Gmask;
		format->Bmask = Bmask;
		format->Amask = Amask;
	} else if ( bpp > 8 ) {
		if ( bpp > 24 )
			bpp = 24;
		format->Rloss = 8-(bpp/3);
		format->Gloss = 8-(bpp/3)-(bpp%3);
		format->Bloss = 8-(bpp/3);
		format->Rshift = ((bpp/3)+(bpp%3))+(bpp/3);
		format->Gshift = (bpp/3);
		format->Bshift = 0;
		format->Rmask = ((0xFF>>format->Rloss)<<format->Rshift);
		format->Gmask = ((0xFF>>format->Gloss)<<format->Gshift);
		format->Bmask = ((0xFF>>format->Bloss)<<format->Bshift);
	} else {
		format->Rloss = 8;
		format->Gloss = 8;
		format->Bloss = 8;
		format->Aloss = 8;
		format->Rshift = 0;
		format->Gshift = 0;
		format->Bshift = 0;
		format->Ashift = 0;
		format->Rmask = 0;
		format->Gmask = 0;
		format->Bmask = 0;
		format->Amask = 0;
	}
	if ( bpp <= 8 ) {
		int ncolors = 1<<bpp;
#ifdef DEBUG_PALETTE
		fprintf(stderr,"bpp=%d ncolors=%d\n",bpp,ncolors);
#endif
		format->palette = (SDLMOD_Palette *)malloc(sizeof(SDLMOD_Palette));
		if ( format->palette == NULL ) {
			SDLMOD_FreeFormat(format);
			fprintf(stderr, "Out of memory\n");
			return(NULL);
		}
		(format->palette)->ncolors = ncolors;
		(format->palette)->colors = (SDLMOD_Color *)malloc(
				(format->palette)->ncolors*sizeof(SDLMOD_Color));
		if ( (format->palette)->colors == NULL ) {
			SDLMOD_FreeFormat(format);
			fprintf(stderr, "Out of memory\n");
			return(NULL);
		}
		if ( Rmask || Bmask || Gmask ) {
			int i;
			int Rm=0,Gm=0,Bm=0;
			int Rw=0,Gw=0,Bw=0;
#ifdef ENABLE_PALETTE_ALPHA
			int Am=0,Aw=0;
#endif
			if(Rmask)
			{
				Rw=8-format->Rloss;
				for(i=format->Rloss;i>0;i-=Rw)
					Rm|=1<<i;
			}
#ifdef DEBUG_PALETTE
			fprintf(stderr,"Rw=%d Rm=0x%02X\n",Rw,Rm);
#endif
			if(Gmask)
			{
				Gw=8-format->Gloss;
				for(i=format->Gloss;i>0;i-=Gw)
					Gm|=1<<i;
			}
#ifdef DEBUG_PALETTE
			fprintf(stderr,"Gw=%d Gm=0x%02X\n",Gw,Gm);
#endif
			if(Bmask)
			{
				Bw=8-format->Bloss;
				for(i=format->Bloss;i>0;i-=Bw)
					Bm|=1<<i;
			}
#ifdef DEBUG_PALETTE
			fprintf(stderr,"Bw=%d Bm=0x%02X\n",Bw,Bm);
#endif
#ifdef ENABLE_PALETTE_ALPHA
			if(Amask)
			{
				Aw=8-format->Aloss;
				for(i=format->Aloss;i>0;i-=Aw)
					Am|=1<<i;
			}
# ifdef DEBUG_PALETTE
			fprintf(stderr,"Aw=%d Am=0x%02X\n",Aw,Am);
# endif
#endif
			for(i=0; i < ncolors; ++i) {
				int r,g,b;
				r=(i&Rmask)>>format->Rshift;
				r=(r<<format->Rloss)|((r*Rm)>>Rw);
				format->palette->colors[i].r=r;

				g=(i&Gmask)>>format->Gshift;
				g=(g<<format->Gloss)|((g*Gm)>>Gw);
				format->palette->colors[i].g=g;

				b=(i&Bmask)>>format->Bshift;
				b=(b<<format->Bloss)|((b*Bm)>>Bw);
				format->palette->colors[i].b=b;

#ifdef ENABLE_PALETTE_ALPHA
				a=(i&Amask)>>format->Ashift;
				a=(a<<format->Aloss)|((a*Am)>>Aw);
				format->palette->colors[i].unused=a;
#else
				format->palette->colors[i].unused=0;
#endif
			}
		} else if ( ncolors == 2 ) {
			format->palette->colors[0].r = 0xFF;
			format->palette->colors[0].g = 0xFF;
			format->palette->colors[0].b = 0xFF;
			format->palette->colors[1].r = 0x00;
			format->palette->colors[1].g = 0x00;
			format->palette->colors[1].b = 0x00;
		} else {
			memset((format->palette)->colors, 0,
				(format->palette)->ncolors*sizeof(SDLMOD_Color));
		}
	}
	return(format);
}

SDLMOD_PixelFormat *SDLMOD_ReallocFormat(SDLMOD_Surface *surface, int bpp,
			uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
	if ( surface->format ) {
		SDLMOD_FreeFormat(surface->format);
		SDLMOD_FormatChanged(surface);
	}
	surface->format = SDLMOD_AllocFormat(bpp, Rmask, Gmask, Bmask, Amask);
	return surface->format;
}

void SDLMOD_FormatChanged(SDLMOD_Surface *surface)
{
	static int format_version = 0;
	++format_version;
	if ( format_version < 0 ) {
		format_version = 1;
	}
	surface->format_version = format_version;
	SDLMOD_InvalidateMap(surface->map);
}

void SDLMOD_FreeFormat(SDLMOD_PixelFormat *format)
{
	if ( format ) {
		if ( format->palette ) {
			if ( format->palette->colors ) {
				free(format->palette->colors);
			}
			free(format->palette);
		}
		free(format);
	}
}

uint16_t SDLMOD_CalculatePitch(SDLMOD_Surface *surface)
{
	uint16_t pitch;
	pitch = surface->w*surface->format->BytesPerPixel;
	switch (surface->format->BitsPerPixel) {
	case 1:
		pitch = (pitch+7)/8;
		break;
		
	case 4:
		pitch = (pitch+1)/2;
		break;
		
	default:
		break;
	}
	pitch = (pitch + 3) & ~3;
	return(pitch);
}

void SDLMOD_GetRGBA(uint32_t pixel, const SDLMOD_PixelFormat * const fmt,
		 uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	if ( fmt->palette == NULL ) {
	        unsigned v;
		v = (pixel & fmt->Rmask) >> fmt->Rshift;
		*r = (v << fmt->Rloss) + (v >> (8 - (fmt->Rloss << 1)));
		v = (pixel & fmt->Gmask) >> fmt->Gshift;
		*g = (v << fmt->Gloss) + (v >> (8 - (fmt->Gloss << 1)));
		v = (pixel & fmt->Bmask) >> fmt->Bshift;
		*b = (v << fmt->Bloss) + (v >> (8 - (fmt->Bloss << 1)));
		if(fmt->Amask) {
		        v = (pixel & fmt->Amask) >> fmt->Ashift;
			*a = (v << fmt->Aloss) + (v >> (8 - (fmt->Aloss << 1)));
		} else {
		        *a = SDLMOD_ALPHA_OPAQUE;
                }
	} else {
		*r = fmt->palette->colors[pixel].r;
		*g = fmt->palette->colors[pixel].g;
		*b = fmt->palette->colors[pixel].b;
		*a = SDLMOD_ALPHA_OPAQUE;
	}
}

void SDLMOD_GetRGB(uint32_t pixel, const SDLMOD_PixelFormat * const fmt,
                uint8_t *r,uint8_t *g,uint8_t *b)
{
	if ( fmt->palette == NULL ) {
	    unsigned v;
		v = (pixel & fmt->Rmask) >> fmt->Rshift;
		*r = (v << fmt->Rloss) + (v >> (8 - (fmt->Rloss << 1)));
		v = (pixel & fmt->Gmask) >> fmt->Gshift;
		*g = (v << fmt->Gloss) + (v >> (8 - (fmt->Gloss << 1)));
		v = (pixel & fmt->Bmask) >> fmt->Bshift;
		*b = (v << fmt->Bloss) + (v >> (8 - (fmt->Bloss << 1)));
	} else {
		*r = fmt->palette->colors[pixel].r;
		*g = fmt->palette->colors[pixel].g;
		*b = fmt->palette->colors[pixel].b;
	}
}

SDLMOD_BlitMap *SDLMOD_AllocBlitMap(void)
{
	SDLMOD_BlitMap *map;
	map = (SDLMOD_BlitMap *)malloc(sizeof(*map));
	if ( map == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	memset(map, 0, sizeof(*map));
	map->sw_data = (struct private_swaccel *)malloc(sizeof(*map->sw_data));
	if ( map->sw_data == NULL ) {
		SDLMOD_FreeBlitMap(map);
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	memset(map->sw_data, 0, sizeof(*map->sw_data));
	return(map);
}

void SDLMOD_InvalidateMap(SDLMOD_BlitMap *map)
{
	if ( ! map ) {
		return;
	}
	map->dst = NULL;
	map->format_version = (unsigned int)-1;
	if ( map->table ) {
		free(map->table);
		map->table = NULL;
	}
}

void SDLMOD_FreeBlitMap(SDLMOD_BlitMap *map)
{
	if ( map ) {
		SDLMOD_InvalidateMap(map);
		if ( map->sw_data != NULL ) {
			free(map->sw_data);
		}
		free(map);
	}
}


