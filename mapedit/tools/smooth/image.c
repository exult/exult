/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for dealing with images, pixels and all the other graphical stuff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_image.h"

#include "globals.h"

int img_read(char *filein) {

	SDL_PixelFormat *fmt;   // we need that to determine which BPP
	SDL_RWops *rw;

	if (g_statics.debug) {
		printf("Reading from %s\n", filein);
		fflush(stdout);
	}

	if (!strcmp(filein, "-")) { // stdin as input. Shouldn't work but we try anyways
		rw = SDL_RWFromFP(stdin, 0);
	} else { // a regular file name
		rw = SDL_RWFromFile(filein, "rb");
	}

	g_statics.image_in = IMG_Load_RW(rw, 0);
	if (g_statics.image_in == NULL) {
		fprintf(stderr, "ERROR: %s\n", SDL_GetError());
		SDL_FreeRW(rw);
		return -1;
	}

	// check if image is in 8bpp format
	fmt = g_statics.image_in->format;
	if (fmt->BitsPerPixel != 8) {
		fprintf(stderr, "ERROR: the image file is not in 8 bpp. Please convert it.\n");
		SDL_FreeSurface(g_statics.image_in);
		SDL_FreeRW(rw);
		return -1;
	}

	if (g_statics.image_in->w != 192 || g_statics.image_in->h != 192) {
		fprintf(stderr, "ERROR: The image file is not 192x192 pixels. Please modify it.\n");
		SDL_FreeSurface(g_statics.image_in);
		SDL_FreeRW(rw);
		return -1;
	}

	if (g_statics.debug > 1) {
		printf("The image file uses %d colours\n", fmt->palette->ncolors);
		fflush(stdout);
	}

	if ((g_variables.image_out = SDL_CreateRGBSurfaceFrom(g_statics.image_in->pixels, g_statics.image_in->w, g_statics.image_in->h, g_statics.image_in->pitch, g_statics.image_in->format->BitsPerPixel, 0, 0, 0, 0)) == NULL) {
		fprintf(stderr, "ERROR: %s", SDL_GetError());
		return -1;
	}

	// need to convert the image using proper values for format, since there is a strong likelyhood to have more colours in the palette than for image_in.
	// algo: create proper palette, retrieve number of colours
	//       update image_in->format->palette with new info
	//       launch SDL_ConvertSurface

	g_variables.image_out = SDL_ConvertSurface(g_variables.image_out, g_statics.image_in->format, SDL_SWSURFACE);

	// a bit of clean up
	SDL_FreeRW(rw);
	return 0;
}

int img_write(char *img_out) {

	if (g_statics.debug) {
		printf("Writing to %s\n", img_out);
		fflush(stdout);
	}
	if (!strcmp(img_out, "-")) { // img_out is set to be stdout
		fprintf(stderr, "ERROR: Can't write output to stdout.\n");
		return -1;
	}
	if (strncasecmp(img_out + strlen(img_out) - 4, ".bmp", 4)) { // img_out does not end in .bmp
		fprintf(stderr, "WARNING: it seems the output file does not end with .bmp. Creating a BMP anyways.\n");
	}

	if (SDL_SaveBMP(g_variables.image_out, img_out) < 0) {
		fprintf(stderr, "ERROR: %s", SDL_GetError());
		return -1;
	}
	return 0;
}

Uint8 getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	/* Here p is the address to the pixel we want to retrieve */

	switch (bpp) {
	case 1:
		return *p;
	default:
		return 0;       /* shouldn't happen, but avoids warnings */
	}
}

void putpixel(SDL_Surface *surface, int x, int y, Uint8 pixel) {
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 1;
	*p = pixel;
}

char *transform(int index) {
	char *ret, *tmp;
	node *cursor;
	pfnPluginApply tmp_func;

	ret = (char *)malloc(7 * sizeof(char));

	sprintf(ret, "%02x%02x%02x", \
	        g_variables.image_out->format->palette->colors[index].r, \
	        g_variables.image_out->format->palette->colors[index].g, \
	        g_variables.image_out->format->palette->colors[index].b\
	       );

	cursor = action_table[index];
	if (cursor != NULL) {
		// there is some apply functions to take care of
		do {
			tmp_func = cursor->plugin_apply;
			tmp = (*tmp_func)(ret, &g_variables);
			cursor = cursor->next;
		} while (cursor != NULL);
		return tmp;
	} else {
		return ret;
	}
}


Uint8 palette_rw(char *col) {
	// this function returns the colour number from image_out palette for the colour [rgb]
	// or if it doesn't exist in the palette, it simply adds it!
	unsigned r, g, b;
	int ncol = g_variables.image_out->format->palette->ncolors;
	int idx;

	sscanf(col, "%02x%02x%02x", &r, &g, &b);
	// get index of col from palette
	idx = SDL_MapRGB(g_variables.image_out->format, r, g, b);

	if (g_variables.image_out->format->palette->colors[idx].r != r || \
	        g_variables.image_out->format->palette->colors[idx].g != g || \
	        g_variables.image_out->format->palette->colors[idx].b != b) {
		// not an exact match! Gotta add the color
		g_variables.image_out->format->palette->colors[ncol].r = r;
		g_variables.image_out->format->palette->colors[ncol].g = g;
		g_variables.image_out->format->palette->colors[ncol].b = b;
		g_variables.image_out->format->palette->ncolors++;
		return ncol;
	} else {
		return idx;
	}
}

int process_image() {
	// returns < 0 if pb
	// that's where the meat of the program is
	// algo:
	// for each pixel of image_in at coord (x,y):
	//    write the converted pixel at coord (x,y) in image_out

	Uint8 idx;
	for (g_variables.global_y = 0; g_variables.global_y < 192; g_variables.global_y++) {
		for (g_variables.global_x = 0; g_variables.global_x < 192; g_variables.global_x++) {
			idx = getpixel(g_statics.image_in, g_variables.global_x, g_variables.global_y);
			SDL_LockSurface(g_variables.image_out);
			putpixel(g_variables.image_out, g_variables.global_x, g_variables.global_y, palette_rw(transform(idx)));
			SDL_UnlockSurface(g_variables.image_out);
		}
	}
	return 1;
}

