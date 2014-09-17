#pragma once

extern unsigned char * loadBMPRaw(const char * filename, 
	unsigned int* outWidth, unsigned int* outHeight, 
	int flipY, int flipToBGR);

extern int dumpBMPRaw(const char *filename,
	unsigned char * data, unsigned int width, unsigned int height);
