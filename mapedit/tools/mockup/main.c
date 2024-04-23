/*
 * mockup
 * converts a 8 bpp BMP file into a u7map format based on colour<->u7chunk
 * association found in reference file
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

#include "main.h"

#include "defs.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#	define strcasecmp _stricmp
#endif

Uint32 getpixel(SDL_Surface* surface, int x, int y) {
	int bpp = surface->format->bytes_per_pixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp) {
	case 1:
		return *p;
	default:
		return 0; /* shouldn't happen, but avoids warnings */
	}
}

int main(int argc, char** argv) {
	if (argc < 3 || argc > 3) {
		printf("Usage: %s <image file> <mapping file>\nYou can name <image "
			   "file> and <mapping file> the way you want.\n",
			   argv[0]);
		exit(-1);
	}

	// initialise SDL
	if ((SDL_Init(SDL_INIT_VIDEO) == -1)) {
		printf("Couldn't initialise SDL: %s\n", SDL_GetError());
		exit(-1);
	}

	atexit(SDL_Quit);

	// need to read the mockup map
	SDL_Surface* mock_map = IMG_Load(argv[1]);
	if (mock_map == NULL) {
		fprintf(stderr, "Couldn't load %s: %s\n", argv[1], SDL_GetError());
		exit(-1);
	}

	// SDL 3 can return 1, 2, 4 bits per pixel SDL_Surface
	if (mock_map->format->bits_per_pixel < 8) {
		printf("The image file is not in 8 bpp ( reported %d ). Converting it "
			   "to 8 bpp.\n",
			   mock_map->format->bits_per_pixel);
		SDL_PixelFormat* format8
				= SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_INDEX8);
		if (SDL_SetPixelFormatPalette(format8, mock_map->format->palette) < 0) {
			fprintf(stderr, "Couldn't transfer palette %s: %s\n", argv[1],
					SDL_GetError());
			SDL_DestroySurface(mock_map);
			exit(-1);
		}
		SDL_Surface* converted8 = SDL_ConvertSurface(mock_map, format8);
		if (converted8 == NULL) {
			fprintf(stderr, "Couldn't convert %s: %s\n", argv[1],
					SDL_GetError());
			SDL_DestroySurface(mock_map);
			exit(-1);
		}
		SDL_DestroySurface(mock_map);
		mock_map = converted8;
		SDL_FreeFormat(format8);
	}

	// check if mock_map is in 8bpp format
	SDL_PixelFormat* fmt = mock_map->format;
	if (fmt->bits_per_pixel != 8) {
		printf("The image file is not in 8 bpp ( reported %d ). Please convert "
			   "it.\n",
			   mock_map->format->bits_per_pixel);
		exit(-1);
	}

	if (mock_map->w != 192 || mock_map->h != 192) {
		printf("The image file is not 192x192 pixels. Please modify it.\n");
		exit(-1);
	}

	printf("The image file uses %d colours\n", fmt->palette->ncolors);

	FILE* f = fopen(argv[2], "ra");

	// we can now prepare the mapping
	int   mapping[MAX_COLOURS];
	int   found[MAX_COLOURS];
	int   converted[MAX_COLOURS];
	Uint8 origRed[MAX_COLOURS];
	Uint8 origGreen[MAX_COLOURS];
	Uint8 origBlue[MAX_COLOURS];
	for (int i = 0; i < fmt->palette->ncolors; i++) {
		found[i]     = 0;
		mapping[i]   = 0;
		converted[i] = 0;
		SDL_GetRGB(i, fmt, &origRed[i], &origGreen[i], &origBlue[i]);
		char buff[7];
		snprintf(
				buff, sizeof(buff), "%02x%02x%02x", origRed[i], origGreen[i],
				origBlue[i]);
		// red, green and blue contains the colour definition. Now we need to
		// enter the u7chunk retrieved for that one

		fseek(f, 0, SEEK_SET);    // back to the beginning for each colour
		while ((!feof(f)) && found[i] == 0) {
			unsigned j;
			char     cmd[256];
			fscanf(f, "%s %u", cmd, &j);
			if (strcasecmp(cmd, buff) == 0) {    // chains match
				found[i]   = 1;
				mapping[i] = j;
			}
		}
	}
	printf("Colour map is successfully loaded from %s\n", argv[2]);
	fclose(f);

	for (int i = 0; i < fmt->palette->ncolors; i++) {
		if (found[i] == 1) {
			printf("mapping[%3d] = color %02x%02x%02x -> chunk %04x (%5d)\n", i,
				   origRed[i], origGreen[i], origBlue[i], mapping[i],
				   mapping[i]);
		}
	}
	u7map mymap;    // a table in which the map is created and is then written
					// to a file
	// need to read all pixels one after the other
	for (int j = 0; j < mock_map->h; j++) {
		for (int i = 0; i < mock_map->w; i++) {
			Uint32 pix = getpixel(mock_map, i, j);

			// calculate offset in u7map based on i,j coordinate of point in map
			long int offset = 256 * sizeof(chunk) * ((i / 16) + ((j / 16) * 12))
							  + sizeof(chunk) * ((i % 16) + ((j % 16) * 16));
			// printf("DEBUG: offset = %ld, i=%d, j=%d\n",offset,i,j);
			if (found[pix] == 1) {
				converted[pix]    = converted[pix] + 1;
				mymap[offset]     = mapping[pix] & 0xFF;
				mymap[offset + 1] = (mapping[pix] >> 8) & 0xFF;
			} else {
				printf("Picture at %3d x %3d, Not expected pixel is %02x\n", i,
					   j, pix);
			}
		}
	}
	for (int i = 0; i < fmt->palette->ncolors; i++) {
		if (found[i] == 1) {
			printf("mapping[%3d] = color %02x%02x%02x -> chunk %04x (%5d), "
				   "converted %5d times\n",
				   i, origRed[i], origGreen[i], origBlue[i], mapping[i],
				   mapping[i], converted[i]);
		}
	}

	// write map to a file
	if ((f = fopen("u7map", "wb")) == NULL) {
		perror("Can't open file u7map: ");
		exit(-1);
	}
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
