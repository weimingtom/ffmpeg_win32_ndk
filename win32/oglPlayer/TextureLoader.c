#include "TextureLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

unsigned char * loadBMPRaw(const char * filename, 
	unsigned int* outWidth, unsigned int* outHeight, 
	int flipY, int flipToBGR)
{
	unsigned char header[54] = {0};
	unsigned int dataPos;
	unsigned int imageSize;
	unsigned char * data;

	if (!filename || !outWidth || !outHeight)
	{
		fprintf(stderr, "filename or outWidth or outHeight is NULL\n"); 
		return NULL;
	}
	*outWidth = -1;
	*outHeight = -1;
	FILE * file = fopen(filename, "rb");
	if (!file)
	{
		fprintf(stderr, "Open input bmp file error\n");
		return NULL;
	}
	if (fread(header, 1, sizeof(header), file) != 54)
	{ 
		fprintf(stderr, "Read input bmp file header error\n");
		fclose(file);
		return NULL;
	}
	if (header[0] != 'B' || header[1] != 'M')
	{
		fprintf(stderr, "Not a correct BMP file (magic BM)\n");
		fclose(file);
		return NULL;
	}
	if (header[24 + 4] != 24 || header[24 + 5] != 0) //biBitCount
	{
		fprintf(stderr, "Not a correct BMP file (24bpp) \n");
		fclose(file);
		return NULL;
	}
	dataPos = (header[8 + 2]) | (header[8 + 3] << 8) | (header[8 + 4] << 16) | (header[8 + 5] << 24); //bfOffBits
	imageSize = (header[32 + 2]) | (header[32 + 3] << 8) | (header[32 + 4] << 16) | (header[32 + 5] << 24); //biSizeImage
	*outWidth = (header[16 + 2]) | (header[16 + 3] << 8) | (header[16 + 4] << 16) | (header[16 + 5] << 24); //biWidth
	*outHeight = (header[16 + 6]) | (header[16 + 7] << 8) | (header[16 + 8] << 16) | (header[16 + 9] << 24); //biHeight
	if (imageSize == 0)    
	{
		imageSize = (*outWidth) * (*outHeight) * 3;
	}
	if (dataPos == 0)
	{
		dataPos = 54;
	}
	data = (unsigned char *)malloc(imageSize * sizeof(unsigned char));
	if (data == NULL)
	{
		fprintf(stderr, "Out of memory\n");
		fclose(file);
		return NULL;
	}
	if (imageSize != fread(data, 1, imageSize, file))
	{
		fprintf(stderr, "Read input bmp file data error\n");
		fclose(file);
		return NULL;
	}
	fclose(file);

	if (flipY)
	{
		unsigned char * tmpBuffer;
		int size;
		int i;
		size = (*outWidth) * 3;
		tmpBuffer = (unsigned char *)malloc((*outWidth) * 3 * sizeof(unsigned char));
		if (tmpBuffer == NULL)
		{
			fprintf(stderr, "Out of memory\n");
			return NULL;
		}
		for (i = 0; i < (*outHeight) / 2; i++)
		{
			memcpy(tmpBuffer, data + (*outWidth) * 3 * i, size);
			memcpy(data + (*outWidth) * 3 * i, data + (*outWidth) * 3 * ( (*outHeight) - i - 1), size);
			memcpy(data + (*outWidth) * 3 * ((*outHeight) - i - 1), tmpBuffer, size);
		}
		free(tmpBuffer);
	}
	if (flipToBGR)
	{
		int i, j;
		unsigned char * pPixel;
		unsigned char tmpChanel;
		for (i = 0; i < (*outHeight); i++)
		{
			for (j = 0; j < (*outWidth); j++)
			{
				pPixel = data + (*outWidth) * 3 * i + j * 3;
				tmpChanel = *pPixel;
				*pPixel = *(pPixel + 2);
				*(pPixel + 2) = tmpChanel;
			}
		}
	}
	return data;
}

int dumpBMPRaw(const char *filename,
	unsigned char * data, unsigned int width, unsigned int height)
{
	FILE *fp = NULL;
	unsigned int imagesize = 0;
	unsigned int filesize = 0;
	unsigned char header[54] = {0};

	if (!filename)
	{
		fprintf(stderr, "filename is NULL\n"); 
		return 0;
	}
	imagesize = width * height * 3;
	filesize = imagesize + 56;
	header[0] = 0x42; // 'B'
	header[1] = 0x4D; // 'M'
	header[2] = filesize; // bytes num
	header[3] = filesize >> 8; //
	header[4] = filesize >> 16; //
	header[5] = filesize >> 24; //
	//printf("0x%0x, 0x%0x, 0x%0x, 0x%0x\n", header[2], header[3], header[4], header[5]);
	header[8 + 2] = 0x36; // data offset = 54 bfOffBits
	header[8 + 6] = 0x28;
	header[16 + 2] = width;
	header[16 + 3] = width >> 8;
	header[16 + 4] = width >> 16;
	header[16 + 5] = width >> 24;
	header[16 + 6] = height;
	header[16 + 7] = height >> 8;
	header[24 + 0] = height >> 16;
	header[24 + 1] = height >> 24;
	header[24 + 2] = 0x01; // biPlane = 1
	header[24 + 4] = 0x18; // biBitCount = 24
	header[32 + 2] = imagesize; // biSizeImage
	header[32 + 3] = imagesize >> 8; 
	header[32 + 4] = imagesize >> 16;
	header[32 + 5] = imagesize >> 24;
	header[32 + 6] = 0x12; // biXPelsMeter = B12H
	header[32 + 7] = 0x0B;
	header[40 + 2] = 0x12; // biYPelsMeter = B12H
	header[40 + 3] = 0x0B;
	fp = fopen(filename, "wb+");
	if (!fp)
	{
		fprintf(stderr, "Open output bmp file error\n");
		return 1;
	}
	if (sizeof(header) != fwrite(header, 1, sizeof(header), fp))
	{
		fprintf(stderr, "Write output bmp file header error\n");
		fclose(fp);
		return 1;	
	}
	if (imagesize != fwrite(data, 1, imagesize, fp))
	{
		fprintf(stderr, "Write output bmp file pixels error\n");
		fclose(fp);
		return 1;
	}
	fclose(fp);
	return 0;
}
