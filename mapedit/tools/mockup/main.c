/*
 * mockup
 * converts a 8 bpp BMP file into a u7map format based on colour<->u7chunk association found in reference file
 *
 * author: Aurelien Marchand
 * licence: GPL
 * date: 21/10/02
 *
 * syntax: mockup filename.bmp mappings.txt
 * mappings.txt is a simple text file with each line in the form:
 * ffffff 9999
 * where ffffff is the hex value of a colour and 9999 is a the u7chunk number
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>

#include "defs.h"
#include "main.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

Uint32 getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp) {
	case 1:
		return *p;
	default:
		return 0;       /* shouldn't happen, but avoids warnings */
	}
}

int main(int argc, char **argv) {
	if (argc < 3 || argc > 3) {
		printf("Usage: %s <image file> <mapping file>\nYou can name <image file> and <mapping file> the way you want.\n", argv[0]);
		exit(-1);
	}

	// initialise SDL
	if ((SDL_Init(SDL_INIT_VIDEO) == -1)) {
		printf("Couldn't initialise SDL: %s\n", SDL_GetError());
		exit(-1);
	}

	atexit(SDL_Quit);

	// need to read the mockup map
	SDL_Surface *mock_map = IMG_Load(argv[1]);
	if (mock_map == NULL) {
		fprintf(stderr, "Couldn't load %s: %s\n", argv[1], SDL_GetError());
		exit(-1);
	}

	// check if mock_map is in 8bpp format
	SDL_PixelFormat *fmt = mock_map->format;
	if (fmt->BitsPerPixel != 8) {
		printf("The image file is not in 8 bpp. Please convert it.\n");
		exit(-1);
	}

	if (mock_map->w != 192 || mock_map->h != 192) {
		printf("The image file is not 192x192 pixels. Please modify it.\n");
		exit(-1);
	}

	printf("The image file uses %d colours\n", fmt->palette->ncolors);

	FILE *f = fopen(argv[2], "ra");

	// we can now prepare the mapping
	int mapping[MAX_COLOURS];
	for (int i = 0; i < fmt->palette->ncolors; i++) {
		int found = 0;
		Uint8 red;
		Uint8 green;
		Uint8 blue;
		SDL_GetRGB(i, fmt, &red, &green, &blue);
		char buff[7];
		snprintf(buff, sizeof(buff), "%02x%02x%02x", red, green, blue);
		// red, green and blue contains the colour definition. Now we need to enter the u7chunk retrieved for that one

		fseek(f, 0, SEEK_SET); // back to the beginning for each colour
		while ((!feof(f)) && found == 0) {
			unsigned j;
			char cmd[256];
			fscanf(f, "%s %u", cmd, &j);
			//printf("DEBUG: I've read: %s %d and I'm looking for %s\n",cmd,j,buff);
			if (strcasecmp(cmd, buff) == 0) { // chains match
				//printf("DEBUG:  and I've found a match!\n");
				found = 1;
				mapping[i] = j;
			}
		}
		if (!found) {
			printf("Colour in %s (#%02x%02x%02x) not defined in %s!\n", argv[1], red, green, blue, argv[2]);
			exit(-1);
		}
	}
	printf("Colour map is successfully loaded from %s\n", argv[2]);
	fclose(f);

	/*  for(i=0;i<fmt->palette->ncolors;i++){
	  printf("mapping[%d] = %04x\n",i,mapping[i]);
	}
	*/
	u7map mymap;            // a table in which the map is created and is then written to a file
	// need to read all pixels one after the other
	for (int j = 0; j < mock_map->h; j++) {
		for (int i = 0; i < mock_map->w; i++) {
			Uint32 pix = getpixel(mock_map, i, j);

			// calculate offset in u7map based on i,j coordinate of point in map
			long int offset = 256 * sizeof(chunk) * ((i / 16)  + ((j / 16) * 12)) + sizeof(chunk) * ((i % 16) + ((j % 16) * 16));
			//printf("DEBUG: offset = %ld, i=%d, j=%d\n",offset,i,j);

			mymap[offset] = mapping[pix] & 0xFF;
			mymap[offset + 1] = (mapping[pix] >> 8) & 0xFF;
		}
	}

	// write map to a file
	if ((f = fopen("u7map", "wb")) == NULL) {
		perror("Can't open file u7map: ");
		exit(-1);
	};
	for (int i = 0; i < 192 * 192 * 2; i++) {
		fputc(mymap[i], f);
	}
	fclose(f);
	// clean up
	SDL_DestroySurface(mock_map);
	SDL_Quit();
	printf("Done!\n");
	return 0;
}

