/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for dealing with images, pixels and all the other graphical
 * stuff
 */

#include "globals.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3_image/SDL_image.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int img_read(const char* filein) {
	if (g_statics.debug) {
		printf("Reading from %s\n", filein);
		fflush(stdout);
	}

	SDL_IOStream* rw;
	if (!strcmp(filein,
				"-")) {    // stdin as input. Shouldn't work but we try anyways
		// rw = SDL_IOFromFP(stdin, false);
		return -1;    // Not supported in SDL3
	} else {          // a regular file name
		rw = SDL_IOFromFile(filein, "rb");
	}

	g_statics.image_in = IMG_Load_IO(rw, 0);
	if (g_statics.image_in == NULL) {
		fprintf(stderr, "ERROR: %s\n", SDL_GetError());
		SDL_CloseIO(rw);
		return -1;
	}

	// SDL 3 can return 1, 2, 4 bits per pixel SDL_Surface
	g_statics.image_in_format
			= SDL_GetPixelFormatDetails(g_statics.image_in->format);
	g_statics.image_in_palette = SDL_GetSurfacePalette(g_statics.image_in);
	g_statics.image_in_ncolors = g_statics.image_in_palette->ncolors;
	if (g_statics.image_in_format->bits_per_pixel < 8) {
		fprintf(stderr,
				"WARNING: The image file is not in 8 bpp (reported %d). "
				"Converting it to 8 bpp.\n",
				g_statics.image_in_format->bits_per_pixel);
		SDL_Surface* converted8 = SDL_ConvertSurface(
				g_statics.image_in, SDL_PIXELFORMAT_INDEX8);
		if (converted8 == NULL) {
			fprintf(stderr, "ERROR: Conversion : %s\n", SDL_GetError());
			SDL_DestroySurface(g_statics.image_in);
			SDL_CloseIO(rw);
			return -1;
		}
		SDL_Palette* converted8_palette = SDL_CreateSurfacePalette(converted8);
		if (converted8_palette == NULL
			|| converted8_palette->ncolors < g_statics.image_in_ncolors) {
			fprintf(stderr, "ERROR: Palette build : %s\n", SDL_GetError());
			SDL_DestroySurface(converted8);
			SDL_DestroySurface(g_statics.image_in);
			return -1;
		}
		if (!SDL_SetPaletteColors(
					converted8_palette, g_statics.image_in_palette->colors, 0,
					g_statics.image_in_ncolors)) {
			fprintf(stderr, "ERROR: Palette transfer : %s\n", SDL_GetError());
			SDL_DestroySurface(converted8);
			SDL_DestroySurface(g_statics.image_in);
			return -1;
		}
		SDL_DestroySurface(g_statics.image_in);
		g_statics.image_in = converted8;
		g_statics.image_in_format
				= SDL_GetPixelFormatDetails(g_statics.image_in->format);
		g_statics.image_in_palette = SDL_GetSurfacePalette(g_statics.image_in);
	}

	// check if image is in 8bpp format
	if (g_statics.image_in_format->bits_per_pixel != 8) {
		fprintf(stderr,
				"ERROR: The image file is not in 8 bpp (reported %d). "
				"Please convert it.\n",
				g_statics.image_in_format->bits_per_pixel);
		SDL_DestroySurface(g_statics.image_in);
		SDL_CloseIO(rw);
		return -1;
	}

	if (g_statics.image_in->w != 192 || g_statics.image_in->h != 192) {
		fprintf(stderr, "ERROR: The image file is not 192x192 pixels. "
						"Please modify it.\n");
		SDL_DestroySurface(g_statics.image_in);
		SDL_CloseIO(rw);
		return -1;
	}

	if (g_statics.debug > 1) {
		printf("The image file uses %d colours\n", g_statics.image_in_ncolors);
		fflush(stdout);
	}

	if ((g_variables.image_out = SDL_DuplicateSurface(g_statics.image_in))
		== NULL) {
		fprintf(stderr, "ERROR: %s\n", SDL_GetError());
		SDL_DestroySurface(g_statics.image_in);
		SDL_CloseIO(rw);
		return -1;
	}
	g_variables.image_out_format
			= SDL_GetPixelFormatDetails(g_variables.image_out->format);
	if ((g_variables.image_out_palette
		 = SDL_CreateSurfacePalette(g_variables.image_out))
				== NULL
		|| g_variables.image_out_palette->ncolors
				   < g_statics.image_in_ncolors) {
		fprintf(stderr, "ERROR: Palette build : %s\n", SDL_GetError());
		SDL_DestroySurface(g_variables.image_out);
		SDL_DestroySurface(g_statics.image_in);
		SDL_CloseIO(rw);
		return -1;
	}
	if (!SDL_SetPaletteColors(
				g_variables.image_out_palette,
				g_statics.image_in_palette->colors, 0,
				g_statics.image_in_ncolors)) {
		fprintf(stderr, "ERROR: Palette transfer : %s\n", SDL_GetError());
		SDL_DestroySurface(g_variables.image_out);
		SDL_DestroySurface(g_statics.image_in);
		SDL_CloseIO(rw);
		return -1;
	}
	g_variables.image_out_ncolors = g_statics.image_in_ncolors;
	// a bit of clean up
	SDL_CloseIO(rw);
	return 0;
}

int img_write(const char* img_out) {
	if (g_statics.debug) {
		printf("Writing to %s\n", img_out);
		fflush(stdout);
	}
	if (!strcmp(img_out, "-")) {    // img_out is set to be stdout
		fprintf(stderr, "ERROR: Can't write output to stdout.\n");
		return -1;
	}
	if (strncasecmp(img_out + strlen(img_out) - 4, ".bmp", 4)
		!= 0) {    // img_out does not end in .bmp
		fprintf(stderr, "WARNING: it seems the output file does not end with "
						".bmp. Creating a BMP anyway.\n");
	}

	if (g_statics.debug > 1) {
		printf("The smoothed image file uses %d of %d colours\n",
			   g_variables.image_out_ncolors,
			   g_variables.image_out_palette->ncolors);
		fflush(stdout);
	}
	if (g_variables.image_out_palette->ncolors
		> g_variables.image_out_ncolors) {
		g_variables.image_out_palette->ncolors = g_variables.image_out_ncolors;
	}
	if (!SDL_SaveBMP(g_variables.image_out, img_out)) {
		fprintf(stderr, "ERROR: %s\n", SDL_GetError());
		return -1;
	}
	return 0;
}

Uint8 getpixel(
		SDL_Surface* surface, const SDL_PixelFormatDetails* surface_format,
		int x, int y) {
	int    bpp = surface_format->bytes_per_pixel;
	Uint8* p   = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
	/* Here p is the address to the pixel we want to retrieve */

	switch (bpp) {
	case 1:
		return *p;
	default:
		return 0; /* shouldn't happen, but avoids warnings */
	}
}

void putpixel(SDL_Surface* surface, int x, int y, Uint8 pixel) {
	Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * 1;
	*p       = pixel;
}

colour_hex transform(int index) {
	SDL_Color* colors = g_variables.image_out_palette->colors;
	colour_hex ret
			= ((((colour_hex)(colors[index].r)) << 16)
			   | (((colour_hex)(colors[index].g)) << 8)
			   | (((colour_hex)(colors[index].b))));

	node* cursor = action_table[index];
	if (cursor != NULL) {
		// there is some apply functions to take care of
		colour_hex tmp;
		do {
			pfnPluginApply tmp_func = cursor->plugin_apply;
			tmp                     = (*tmp_func)(ret, &g_variables);
			cursor                  = cursor->next;
		} while (cursor != NULL);
		return tmp;
	} else {
		return ret;
	}
}

Uint8 palette_rw(colour_hex col) {
	// this function returns the colour number from image_out palette for the
	// colour [rgb] or if it doesn't exist in the palette, it simply adds it!
	unsigned r = (col >> 16) & 0xffu;
	unsigned g = (col >> 8) & 0xffu;
	unsigned b = (col) & 0xffu;
	// get index of col from palette
	int idx = SDL_MapRGB(
			g_variables.image_out_format, g_variables.image_out_palette, r, g,
			b);

	SDL_Color* colors = g_variables.image_out_palette->colors;
	if (colors[idx].r != r || colors[idx].g != g || colors[idx].b != b) {
		// not an exact match! Gotta add the color
		int ncol = g_variables.image_out_ncolors;
		if (ncol < g_variables.image_out_palette->ncolors) {
			colors[ncol].r = r;
			colors[ncol].g = g;
			colors[ncol].b = b;
			g_variables.image_out_ncolors++;
			return ncol;
		} else {
			return idx;
		}
	} else {
		return idx;
	}
}

int process_image(void) {
	// returns < 0 if pb
	// that's where the meat of the program is
	// algo:
	// for each pixel of image_in at coord (x,y):
	//    write the converted pixel at coord (x,y) in image_out

	for (g_variables.global_y = 0; g_variables.global_y < 192;
		 g_variables.global_y++) {
		for (g_variables.global_x = 0; g_variables.global_x < 192;
			 g_variables.global_x++) {
			Uint8 idx = getpixel(
					g_statics.image_in, g_statics.image_in_format,
					g_variables.global_x, g_variables.global_y);
			SDL_LockSurface(g_variables.image_out);
			putpixel(
					g_variables.image_out, g_variables.global_x,
					g_variables.global_y, palette_rw(transform(idx)));
			SDL_UnlockSurface(g_variables.image_out);
		}
	}
	return 1;
}
