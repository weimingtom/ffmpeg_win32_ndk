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

#include "SDLMOD_video.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

struct private_yuvhwfuncs {
	int (*Lock)(void *unused, SDLMOD_Overlay *overlay);
	void (*Unlock)(void *unused, SDLMOD_Overlay *overlay);
	int (*Display)(void *unused, SDLMOD_Overlay *overlay, SDLMOD_Rect *src, SDLMOD_Rect *dst);
	void (*FreeHW)(void *unused, SDLMOD_Overlay *overlay);
};

struct private_yuvhwdata {
	SDLMOD_Surface *stretch;
	SDLMOD_Surface *display;
	uint8_t *pixels;
	int *colortab;
	uint32_t *rgb_2_pix;
	void (*Display1X)(int *colortab, uint32_t *rgb_2_pix,
                          unsigned char *lum, unsigned char *cr,
                          unsigned char *cb, unsigned char *out,
                          int rows, int cols, int mod );
	void (*Display2X)(int *colortab, uint32_t *rgb_2_pix,
	                  unsigned char *lum, unsigned char *cr,
                          unsigned char *cb, unsigned char *out,
                          int rows, int cols, int mod );
	uint16_t pitches[3];
	uint8_t *planes[3];
};

extern SDLMOD_Overlay *SDLMOD_CreateYUV_SW(void *unused, int width, int height, uint32_t format, SDLMOD_Surface *display);
extern int SDLMOD_LockYUV_SW(void *unused, SDLMOD_Overlay *overlay);
extern void SDLMOD_UnlockYUV_SW(void *unused, SDLMOD_Overlay *overlay);
extern int SDLMOD_DisplayYUV_SW(void *unused, SDLMOD_Overlay *overlay, SDLMOD_Rect *src, SDLMOD_Rect *dst);
extern void SDLMOD_FreeYUV_SW(void *unused, SDLMOD_Overlay *overlay);

static struct private_yuvhwfuncs sw_yuvfuncs = {
	SDLMOD_LockYUV_SW,
	SDLMOD_UnlockYUV_SW,
	SDLMOD_DisplayYUV_SW,
	SDLMOD_FreeYUV_SW
};

SDLMOD_Overlay *SDLMOD_CreateYUVOverlay(int w, int h, uint32_t format,
                                  SDLMOD_Surface *display)
{
	SDLMOD_Overlay *overlay;
	if ( (display->flags & SDLMOD_OPENGL) == SDLMOD_OPENGL ) {
		fprintf(stderr, "YUV overlays are not supported in OpenGL mode\n");
		return NULL;
	}
	overlay = SDLMOD_CreateYUV_SW(NULL, w, h, format, display);
	return overlay;
}

int SDLMOD_LockYUVOverlay(SDLMOD_Overlay *overlay)
{
	if ( overlay == NULL ) {
		fprintf(stderr, "Passed NULL overlay\n");
		return -1;
	}
	return overlay->hwfuncs->Lock(NULL, overlay);
}

void SDLMOD_UnlockYUVOverlay(SDLMOD_Overlay *overlay)
{
	if ( overlay == NULL ) {
		return;
	}
	overlay->hwfuncs->Unlock(NULL, overlay);
}

int SDLMOD_DisplayYUVOverlay(SDLMOD_Overlay *overlay, SDLMOD_Rect *dstrect)
{
	SDLMOD_Rect src, dst;
	int srcx, srcy, srcw, srch;
	int dstx, dsty, dstw, dsth;
	
	if ( overlay == NULL || dstrect == NULL ) {
		fprintf(stderr, "Passed NULL overlay or dstrect\n");
		return -1;
	}
	srcx = 0;
	srcy = 0;
	srcw = overlay->w;
	srch = overlay->h;
	dstx = dstrect->x;
	dsty = dstrect->y;
	dstw = dstrect->w;
	dsth = dstrect->h;
	if ( dstx < 0 ) {
		srcw += (dstx * overlay->w) / dstrect->w;
		dstw += dstx;
		srcx -= (dstx * overlay->w) / dstrect->w;
		dstx = 0;
	}
	if ( dsty < 0 ) {
		srch += (dsty * overlay->h) / dstrect->h;
		dsth += dsty;
		srcy -= (dsty * overlay->h) / dstrect->h;
		dsty = 0;
	}
	if ( srcw <= 0 || srch <= 0 ||
	     srch <= 0 || dsth <= 0 ) {
		return 0;
	}
	src.x = srcx;
	src.y = srcy;
	src.w = srcw;
	src.h = srch;
	dst.x = dstx;
	dst.y = dsty;
	dst.w = dstw;
	dst.h = dsth;
	return overlay->hwfuncs->Display(NULL, overlay, &src, &dst);
}

void SDLMOD_FreeYUVOverlay(SDLMOD_Overlay *overlay)
{
	if ( overlay == NULL ) {
		return;
	}
	if ( overlay->hwfuncs ) {
		overlay->hwfuncs->FreeHW(NULL, overlay);
	}
	free(overlay);
}


static void Color16DitherYV12Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned short* row1;
    unsigned short* row2;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row1 = (unsigned short*) out;
    row2 = row1 + cols + mod;
    lum2 = lum + cols;

    mod += cols + mod;

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;

            L = *lum++;
            *row1++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                       rgb_2_pix[ L + crb_g ] |
                                       rgb_2_pix[ L + cb_b ]);

            L = *lum++;
            *row1++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                       rgb_2_pix[ L + crb_g ] |
                                       rgb_2_pix[ L + cb_b ]);


            /* Now, do second row.  */

            L = *lum2++;
            *row2++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                       rgb_2_pix[ L + crb_g ] |
                                       rgb_2_pix[ L + cb_b ]);

            L = *lum2++;
            *row2++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                       rgb_2_pix[ L + crb_g ] |
                                       rgb_2_pix[ L + cb_b ]);
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color24DitherYV12Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int value;
    unsigned char* row1;
    unsigned char* row2;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row1 = out;
    row2 = row1 + cols*3 + mod*3;
    lum2 = lum + cols;

    mod += cols + mod;
    mod *= 3;

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;

            L = *lum++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row1++ = (value      ) & 0xFF;
            *row1++ = (value >>  8) & 0xFF;
            *row1++ = (value >> 16) & 0xFF;

            L = *lum++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row1++ = (value      ) & 0xFF;
            *row1++ = (value >>  8) & 0xFF;
            *row1++ = (value >> 16) & 0xFF;
            
            L = *lum2++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row2++ = (value      ) & 0xFF;
            *row2++ = (value >>  8) & 0xFF;
            *row2++ = (value >> 16) & 0xFF;

            L = *lum2++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row2++ = (value      ) & 0xFF;
            *row2++ = (value >>  8) & 0xFF;
            *row2++ = (value >> 16) & 0xFF;
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color32DitherYV12Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row1;
    unsigned int* row2;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row1 = (unsigned int*) out;
    row2 = row1 + cols + mod;
    lum2 = lum + cols;

    mod += cols + mod;

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;
            
            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;
            
            L = *lum++;
            *row1++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);
                       
            L = *lum++;
            *row1++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);
                       
            L = *lum2++;
            *row2++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);

            L = *lum2++;
            *row2++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color16DitherYV12Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row1 = (unsigned int*) out;
    const int next_row = cols+(mod/2);
    unsigned int* row2 = row1 + 2*next_row;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    lum2 = lum + cols;

    mod = (next_row * 3) + (mod/2);

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;

            L = *lum++;
            row1[0] = row1[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row1++;

            L = *lum++;
            row1[0] = row1[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row1++;
            
            L = *lum2++;
            row2[0] = row2[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row2++;

            L = *lum2++;
            row2[0] = row2[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row2++;
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color24DitherYV12Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int value;
    unsigned char* row1 = out;
    const int next_row = (cols*2 + mod) * 3;
    unsigned char* row2 = row1 + 2*next_row;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    lum2 = lum + cols;

    mod = next_row*3 + mod*3;

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;

            L = *lum++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row1[0+0] = row1[3+0] = row1[next_row+0] = row1[next_row+3+0] =
                     (value      ) & 0xFF;
            row1[0+1] = row1[3+1] = row1[next_row+1] = row1[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row1[0+2] = row1[3+2] = row1[next_row+2] = row1[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row1 += 2*3;

            L = *lum++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row1[0+0] = row1[3+0] = row1[next_row+0] = row1[next_row+3+0] =
                     (value      ) & 0xFF;
            row1[0+1] = row1[3+1] = row1[next_row+1] = row1[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row1[0+2] = row1[3+2] = row1[next_row+2] = row1[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row1 += 2*3;
            
            L = *lum2++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row2[0+0] = row2[3+0] = row2[next_row+0] = row2[next_row+3+0] =
                     (value      ) & 0xFF;
            row2[0+1] = row2[3+1] = row2[next_row+1] = row2[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row2[0+2] = row2[3+2] = row2[next_row+2] = row2[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row2 += 2*3;

            L = *lum2++;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row2[0+0] = row2[3+0] = row2[next_row+0] = row2[next_row+3+0] =
                     (value      ) & 0xFF;
            row2[0+1] = row2[3+1] = row2[next_row+1] = row2[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row2[0+2] = row2[3+2] = row2[next_row+2] = row2[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row2 += 2*3;
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color32DitherYV12Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row1 = (unsigned int*) out;
    const int next_row = cols*2+mod;
    unsigned int* row2 = row1 + 2*next_row;
    unsigned char* lum2;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    lum2 = lum + cols;

    mod = (next_row * 3) + mod;

    y = rows / 2;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            ++cr; ++cb;

            L = *lum++;
            row1[0] = row1[1] = row1[next_row] = row1[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row1 += 2;

            L = *lum++;
            row1[0] = row1[1] = row1[next_row] = row1[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row1 += 2;
			
            L = *lum2++;
            row2[0] = row2[1] = row2[next_row] = row2[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row2 += 2;

            L = *lum2++;
            row2[0] = row2[1] = row2[next_row] = row2[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row2 += 2;
        }
        lum  += cols;
        lum2 += cols;
        row1 += mod;
        row2 += mod;
    }
}

static void Color16DitherYUY2Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned short* row;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row = (unsigned short*) out;

    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            *row++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                      rgb_2_pix[ L + crb_g ] |
                                      rgb_2_pix[ L + cb_b ]);

            L = *lum; lum += 2;
            *row++ = (unsigned short)(rgb_2_pix[ L + cr_r ] |
                                      rgb_2_pix[ L + crb_g ] |
                                      rgb_2_pix[ L + cb_b ]);
        }
        row += mod;
    }
}

static void Color24DitherYUY2Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int value;
    unsigned char* row;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row = (unsigned char*) out;
    mod *= 3;
    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row++ = (value      ) & 0xFF;
            *row++ = (value >>  8) & 0xFF;
            *row++ = (value >> 16) & 0xFF;

            L = *lum; lum += 2;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            *row++ = (value      ) & 0xFF;
            *row++ = (value >>  8) & 0xFF;
            *row++ = (value >> 16) & 0xFF;
        }
        row += mod;
    }
}

static void Color32DitherYUY2Mod1X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    row = (unsigned int*) out;
    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            *row++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);

            L = *lum; lum += 2;
            *row++ = (rgb_2_pix[ L + cr_r ] |
                       rgb_2_pix[ L + crb_g ] |
                       rgb_2_pix[ L + cb_b ]);
        }
        row += mod;
    }
}

static void Color16DitherYUY2Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row = (unsigned int*) out;
    const int next_row = cols+(mod/2);
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;

    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            row[0] = row[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row++;

            L = *lum; lum += 2;
            row[0] = row[next_row] = (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row++;
        }
        row += next_row;
    }
}

static void Color24DitherYUY2Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int value;
    unsigned char* row = out;
    const int next_row = (cols*2 + mod) * 3;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;
    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row[0+0] = row[3+0] = row[next_row+0] = row[next_row+3+0] =
                     (value      ) & 0xFF;
            row[0+1] = row[3+1] = row[next_row+1] = row[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row[0+2] = row[3+2] = row[next_row+2] = row[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row += 2*3;

            L = *lum; lum += 2;
            value = (rgb_2_pix[ L + cr_r ] |
                     rgb_2_pix[ L + crb_g ] |
                     rgb_2_pix[ L + cb_b ]);
            row[0+0] = row[3+0] = row[next_row+0] = row[next_row+3+0] =
                     (value      ) & 0xFF;
            row[0+1] = row[3+1] = row[next_row+1] = row[next_row+3+1] =
                     (value >>  8) & 0xFF;
            row[0+2] = row[3+2] = row[next_row+2] = row[next_row+3+2] =
                     (value >> 16) & 0xFF;
            row += 2*3;
        }
        row += next_row;
    }
}

static void Color32DitherYUY2Mod2X( int *colortab, uint32_t *rgb_2_pix,
                                    unsigned char *lum, unsigned char *cr,
                                    unsigned char *cb, unsigned char *out,
                                    int rows, int cols, int mod )
{
    unsigned int* row = (unsigned int*) out;
    const int next_row = cols*2+mod;
    int x, y;
    int cr_r;
    int crb_g;
    int cb_b;
    int cols_2 = cols / 2;
    mod+=mod;
    y = rows;
    while( y-- )
    {
        x = cols_2;
        while( x-- )
        {
            register int L;

            cr_r   = 0*768+256 + colortab[ *cr + 0*256 ];
            crb_g  = 1*768+256 + colortab[ *cr + 1*256 ]
                               + colortab[ *cb + 2*256 ];
            cb_b   = 2*768+256 + colortab[ *cb + 3*256 ];
            cr += 4; cb += 4;

            L = *lum; lum += 2;
            row[0] = row[1] = row[next_row] = row[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row += 2;

            L = *lum; lum += 2;
            row[0] = row[1] = row[next_row] = row[next_row+1] =
                                       (rgb_2_pix[ L + cr_r ] |
                                        rgb_2_pix[ L + crb_g ] |
                                        rgb_2_pix[ L + cb_b ]);
            row += 2;


        }

        row += next_row;
    }
}

static int number_of_bits_set( uint32_t a )
{
    if(!a) return 0;
    if(a & 1) return 1 + number_of_bits_set(a >> 1);
    return(number_of_bits_set(a >> 1));
}

static int free_bits_at_bottom( uint32_t a )
{
    if(!a) return sizeof(uint32_t) * 8;
    if(((int32_t)a) & 1l) return 0;
    return 1 + free_bits_at_bottom ( a >> 1);
}

SDLMOD_Overlay *SDLMOD_CreateYUV_SW(void *unused/*_THIS*/, int width, int height, uint32_t format, SDLMOD_Surface *display)
{
	SDLMOD_Overlay *overlay;
	struct private_yuvhwdata *swdata;
	int *Cr_r_tab;
	int *Cr_g_tab;
	int *Cb_g_tab;
	int *Cb_b_tab;
	uint32_t *r_2_pix_alloc;
	uint32_t *g_2_pix_alloc;
	uint32_t *b_2_pix_alloc;
	int i;
	int CR, CB;
	uint32_t Rmask, Gmask, Bmask;
	
	if ( (display->format->BytesPerPixel != 2) &&
	     (display->format->BytesPerPixel != 3) &&
	     (display->format->BytesPerPixel != 4) ) {
		fprintf(stderr, "Can't use YUV data on non 16/24/32 bit surfaces\n");
		return(NULL);
	}

	switch (format) {
	case SDLMOD_YV12_OVERLAY:
	case SDLMOD_IYUV_OVERLAY:
	case SDLMOD_YUY2_OVERLAY:
	case SDLMOD_UYVY_OVERLAY:
	case SDLMOD_YVYU_OVERLAY:
		break;
		
	default:
		fprintf(stderr, "Unsupported YUV format\n");
		return(NULL);
	}

	overlay = (SDLMOD_Overlay *)malloc(sizeof *overlay);
	if ( overlay == NULL ) {
		fprintf(stderr, "Out of memory\n");
		return(NULL);
	}
	memset(overlay, 0, (sizeof *overlay));
	overlay->format = format;
	overlay->w = width;
	overlay->h = height;
	overlay->hwfuncs = &sw_yuvfuncs;

	swdata = (struct private_yuvhwdata *)malloc(sizeof *swdata);
	overlay->hwdata = swdata;
	if ( swdata == NULL ) {
		fprintf(stderr, "Out of memory\n");
		SDLMOD_FreeYUVOverlay(overlay);
		return(NULL);
	}
	swdata->stretch = NULL;
	swdata->display = display;
	swdata->pixels = (uint8_t *) malloc(width*height*2);
	swdata->colortab = (int *)malloc(4*256*sizeof(int));
	Cr_r_tab = &swdata->colortab[0*256];
	Cr_g_tab = &swdata->colortab[1*256];
	Cb_g_tab = &swdata->colortab[2*256];
	Cb_b_tab = &swdata->colortab[3*256];
	swdata->rgb_2_pix = (uint32_t *)malloc(3*768*sizeof(uint32_t));
	r_2_pix_alloc = &swdata->rgb_2_pix[0*768];
	g_2_pix_alloc = &swdata->rgb_2_pix[1*768];
	b_2_pix_alloc = &swdata->rgb_2_pix[2*768];
	if ( ! swdata->pixels || ! swdata->colortab || ! swdata->rgb_2_pix ) {
		fprintf(stderr, "Out of memory\n");
		SDLMOD_FreeYUVOverlay(overlay);
		return(NULL);
	}

	for (i=0; i<256; i++) {
		CB = CR = (i-128);
		Cr_r_tab[i] = (int) ( (0.419/0.299) * CR);
		Cr_g_tab[i] = (int) (-(0.299/0.419) * CR);
		Cb_g_tab[i] = (int) (-(0.114/0.331) * CB); 
		Cb_b_tab[i] = (int) ( (0.587/0.331) * CB);
	}
	Rmask = display->format->Rmask;
	Gmask = display->format->Gmask;
	Bmask = display->format->Bmask;
	for ( i=0; i<256; ++i ) {
		r_2_pix_alloc[i+256] = i >> (8 - number_of_bits_set(Rmask));
		r_2_pix_alloc[i+256] <<= free_bits_at_bottom(Rmask);
		g_2_pix_alloc[i+256] = i >> (8 - number_of_bits_set(Gmask));
		g_2_pix_alloc[i+256] <<= free_bits_at_bottom(Gmask);
		b_2_pix_alloc[i+256] = i >> (8 - number_of_bits_set(Bmask));
		b_2_pix_alloc[i+256] <<= free_bits_at_bottom(Bmask);
	}
	if( display->format->BytesPerPixel == 2 ) {
		for ( i=0; i<256; ++i ) {
			r_2_pix_alloc[i+256] |= (r_2_pix_alloc[i+256]) << 16;
			g_2_pix_alloc[i+256] |= (g_2_pix_alloc[i+256]) << 16;
			b_2_pix_alloc[i+256] |= (b_2_pix_alloc[i+256]) << 16;
		}
	}
	for ( i=0; i<256; ++i ) {
		r_2_pix_alloc[i] = r_2_pix_alloc[256];
		r_2_pix_alloc[i+512] = r_2_pix_alloc[511];
		g_2_pix_alloc[i] = g_2_pix_alloc[256];
		g_2_pix_alloc[i+512] = g_2_pix_alloc[511];
		b_2_pix_alloc[i] = b_2_pix_alloc[256];
		b_2_pix_alloc[i+512] = b_2_pix_alloc[511];
	}

	switch (format) {
	    case SDLMOD_YV12_OVERLAY:
	    case SDLMOD_IYUV_OVERLAY:
		if ( display->format->BytesPerPixel == 2 ) {
			swdata->Display1X = Color16DitherYV12Mod1X;
			swdata->Display2X = Color16DitherYV12Mod2X;
		}
		if ( display->format->BytesPerPixel == 3 ) {
			swdata->Display1X = Color24DitherYV12Mod1X;
			swdata->Display2X = Color24DitherYV12Mod2X;
		}
		if ( display->format->BytesPerPixel == 4 ) {
			swdata->Display1X = Color32DitherYV12Mod1X;
			swdata->Display2X = Color32DitherYV12Mod2X;
		}
		break;
	    case SDLMOD_YUY2_OVERLAY:
	    case SDLMOD_UYVY_OVERLAY:
	    case SDLMOD_YVYU_OVERLAY:
		if ( display->format->BytesPerPixel == 2 ) {
			swdata->Display1X = Color16DitherYUY2Mod1X;
			swdata->Display2X = Color16DitherYUY2Mod2X;
		}
		if ( display->format->BytesPerPixel == 3 ) {
			swdata->Display1X = Color24DitherYUY2Mod1X;
			swdata->Display2X = Color24DitherYUY2Mod2X;
		}
		if ( display->format->BytesPerPixel == 4 ) {
			swdata->Display1X = Color32DitherYUY2Mod1X;
			swdata->Display2X = Color32DitherYUY2Mod2X;
		}
		break;
	    default:
		/* We should never get here (caught above) */
		break;
	}
	overlay->pitches = swdata->pitches;
	overlay->pixels = swdata->planes;
	switch (format) {
	case SDLMOD_YV12_OVERLAY:
	case SDLMOD_IYUV_OVERLAY:
		overlay->pitches[0] = overlay->w;
		overlay->pitches[1] = overlay->pitches[0] / 2;
		overlay->pitches[2] = overlay->pitches[0] / 2;
			overlay->pixels[0] = swdata->pixels;
			overlay->pixels[1] = overlay->pixels[0] +
							 overlay->pitches[0] * overlay->h;
			overlay->pixels[2] = overlay->pixels[1] +
							 overlay->pitches[1] * overlay->h / 2;
		overlay->planes = 3;
		break;
		
	case SDLMOD_YUY2_OVERLAY:
	case SDLMOD_UYVY_OVERLAY:
	case SDLMOD_YVYU_OVERLAY:
		overlay->pitches[0] = overlay->w*2;
			overlay->pixels[0] = swdata->pixels;
		overlay->planes = 1;
		break;
		
	default:
		break;
	}
	return(overlay);
}

int SDLMOD_LockYUV_SW(void *unused/*_THIS*/, SDLMOD_Overlay *overlay)
{
	return(0);
}

void SDLMOD_UnlockYUV_SW(void *unused/*_THIS*/, SDLMOD_Overlay *overlay)
{
	return;
}

int SDLMOD_DisplayYUV_SW(void *unused, SDLMOD_Overlay *overlay, SDLMOD_Rect *src, SDLMOD_Rect *dst)
{
	struct private_yuvhwdata *swdata;
	int stretch;
	int scale_2x;
	SDLMOD_Surface *display;
	uint8_t *lum, *Cr, *Cb;
	uint8_t *dstp;
	int mod;

	swdata = overlay->hwdata;
	stretch = 0;
	scale_2x = 0;
	if ( src->x || src->y || src->w < overlay->w || src->h < overlay->h ) {
		stretch = 1;
	} else if ( (src->w != dst->w) || (src->h != dst->h) ) {
		if ( (dst->w == 2*src->w) &&
		     (dst->h == 2*src->h) ) {
			scale_2x = 1;
		} else {
			stretch = 1;
		}
	}
	if ( stretch ) {
		if ( ! swdata->stretch ) {
			display = swdata->display;
			swdata->stretch = SDLMOD_CreateRGBSurface(
				SDLMOD_SWSURFACE,
				overlay->w, overlay->h,
				display->format->BitsPerPixel,
				display->format->Rmask,
				display->format->Gmask,
				display->format->Bmask, 0);
			if ( ! swdata->stretch ) {
				return(-1);
			}
		}
		display = swdata->stretch;
	} else {
		display = swdata->display;
	}
	switch (overlay->format) {
	case SDLMOD_YV12_OVERLAY:
		lum = overlay->pixels[0];
		Cr =  overlay->pixels[1];
		Cb =  overlay->pixels[2];
		break;
		
	case SDLMOD_IYUV_OVERLAY:
		lum = overlay->pixels[0];
		Cr =  overlay->pixels[2];
		Cb =  overlay->pixels[1];
		break;
		
	case SDLMOD_YUY2_OVERLAY:
		lum = overlay->pixels[0];
		Cr = lum + 3;
		Cb = lum + 1;
		break;
		
	case SDLMOD_UYVY_OVERLAY:
		lum = overlay->pixels[0]+1;
		Cr = lum + 1;
		Cb = lum - 1;
		break;
		
	case SDLMOD_YVYU_OVERLAY:
		lum = overlay->pixels[0];
		Cr = lum + 1;
		Cb = lum + 3;
		break;
		
	default:
		fprintf(stderr, "Unsupported YUV format in blit\n");
		return(-1);
	}
	if ( SDLMOD_MUSTLOCK(display) ) {
        	if ( SDLMOD_LockSurface(display) < 0 ) {
			return(-1);
		}
	}
	if ( stretch ) {
		dstp = (uint8_t *)swdata->stretch->pixels;
	} else {
		dstp = (uint8_t *)display->pixels
			+ dst->x * display->format->BytesPerPixel
			+ dst->y * display->pitch;
	}
	mod = (display->pitch / display->format->BytesPerPixel);

	if ( scale_2x ) {
		mod -= (overlay->w * 2);
		swdata->Display2X(swdata->colortab, swdata->rgb_2_pix,
		                  lum, Cr, Cb, dstp, overlay->h, overlay->w, mod);
	} else {
		mod -= overlay->w;
		swdata->Display1X(swdata->colortab, swdata->rgb_2_pix,
		                  lum, Cr, Cb, dstp, overlay->h, overlay->w, mod);
	}
	if ( SDLMOD_MUSTLOCK(display) ) {
		SDLMOD_UnlockSurface(display);
	}
	if ( stretch ) {
		display = swdata->display;
		SDLMOD_SoftStretch(swdata->stretch, src, display, dst);
	}
	return(0);
}

void SDLMOD_FreeYUV_SW(void *unused, SDLMOD_Overlay *overlay)
{
	struct private_yuvhwdata *swdata;

	swdata = overlay->hwdata;
	if ( swdata ) {
		if ( swdata->stretch ) {
			SDLMOD_FreeSurface(swdata->stretch);
		}
		if ( swdata->pixels ) {
			free(swdata->pixels);
		}
		if ( swdata->colortab ) {
			free(swdata->colortab);
		}
		if ( swdata->rgb_2_pix ) {
			free(swdata->rgb_2_pix);
		}
		free(swdata);
		overlay->hwdata = NULL;
	}
}















#define DEFINE_COPY_ROW(name, type)			\
static void name(type *src, int src_w, type *dst, int dst_w)	\
{							\
	int i;						\
	int pos, inc;					\
	type pixel = 0;					\
							\
	pos = 0x10000;					\
	inc = (src_w << 16) / dst_w;			\
	for ( i=dst_w; i>0; --i ) {			\
		while ( pos >= 0x10000L ) {		\
			pixel = *src++;			\
			pos -= 0x10000L;		\
		}					\
		*dst++ = pixel;				\
		pos += inc;				\
	}						\
}
DEFINE_COPY_ROW(copy_row1, uint8_t)
DEFINE_COPY_ROW(copy_row2, uint16_t)
DEFINE_COPY_ROW(copy_row4, uint32_t)

void copy_row3(uint8_t *src, int src_w, uint8_t *dst, int dst_w)
{
	int i;
	int pos, inc;
	uint8_t pixel[3] = { 0, 0, 0 };
	pos = 0x10000;
	inc = (src_w << 16) / dst_w;
	for ( i=dst_w; i>0; --i ) {
		while ( pos >= 0x10000L ) {
			pixel[0] = *src++;
			pixel[1] = *src++;
			pixel[2] = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel[0];
		*dst++ = pixel[1];
		*dst++ = pixel[2];
		pos += inc;
	}
}

int SDLMOD_SoftStretch(SDLMOD_Surface *src, SDLMOD_Rect *srcrect,
                    SDLMOD_Surface *dst, SDLMOD_Rect *dstrect)
{
	int src_locked;
	int dst_locked;
	int pos, inc;
	int dst_maxrow;
	int src_row, dst_row;
	uint8_t *srcp = NULL;
	uint8_t *dstp;
	SDLMOD_Rect full_src;
	SDLMOD_Rect full_dst;
	const int bpp = dst->format->BytesPerPixel;

	if ( src->format->BitsPerPixel != dst->format->BitsPerPixel ) {
		fprintf(stderr, "Only works with same format surfaces\n");
		return(-1);
	}
	if ( srcrect ) {
		if ( (srcrect->x < 0) || (srcrect->y < 0) ||
		     ((srcrect->x+srcrect->w) > src->w) ||
		     ((srcrect->y+srcrect->h) > src->h) ) {
			fprintf(stderr, "Invalid source blit rectangle\n");
			return(-1);
		}
	} else {
		full_src.x = 0;
		full_src.y = 0;
		full_src.w = src->w;
		full_src.h = src->h;
		srcrect = &full_src;
	}
	if ( dstrect ) {
		if ( (dstrect->x < 0) || (dstrect->y < 0) ||
		     ((dstrect->x+dstrect->w) > dst->w) ||
		     ((dstrect->y+dstrect->h) > dst->h) ) {
			fprintf(stderr, "Invalid destination blit rectangle\n");
			return(-1);
		}
	} else {
		full_dst.x = 0;
		full_dst.y = 0;
		full_dst.w = dst->w;
		full_dst.h = dst->h;
		dstrect = &full_dst;
	}
	dst_locked = 0;
	if ( SDLMOD_MUSTLOCK(dst) ) {
		if ( SDLMOD_LockSurface(dst) < 0 ) {
			fprintf(stderr, "Unable to lock destination surface\n");
			return(-1);
		}
		dst_locked = 1;
	}
	src_locked = 0;
	if ( SDLMOD_MUSTLOCK(src) ) {
		if ( SDLMOD_LockSurface(src) < 0 ) {
			if ( dst_locked ) {
				SDLMOD_UnlockSurface(dst);
			}
			fprintf(stderr, "Unable to lock source surface\n");
			return(-1);
		}
		src_locked = 1;
	}
	pos = 0x10000;
	inc = (srcrect->h << 16) / dstrect->h;
	src_row = srcrect->y;
	dst_row = dstrect->y;
	for ( dst_maxrow = dst_row+dstrect->h; dst_row<dst_maxrow; ++dst_row ) {
		dstp = (uint8_t *)dst->pixels + (dst_row*dst->pitch)
		                            + (dstrect->x*bpp);
		while ( pos >= 0x10000L ) {
			srcp = (uint8_t *)src->pixels + (src_row*src->pitch)
			                            + (srcrect->x*bpp);
			++src_row;
			pos -= 0x10000L;
		}
		switch (bpp) {
		    case 1:
				copy_row1(srcp, srcrect->w, dstp, dstrect->w);
				break;
		    
		    case 2:
				copy_row2((uint16_t *)srcp, srcrect->w,
						  (uint16_t *)dstp, dstrect->w);
				break;
				
		    case 3:
				copy_row3(srcp, srcrect->w, dstp, dstrect->w);
				break;
				
		    case 4:
				copy_row4((uint32_t *)srcp, srcrect->w,
						  (uint32_t *)dstp, dstrect->w);
				break;
		}
		pos += inc;
	}
	if ( dst_locked ) {
		SDLMOD_UnlockSurface(dst);
	}
	if ( src_locked ) {
		SDLMOD_UnlockSurface(src);
	}
	return(0);
}
