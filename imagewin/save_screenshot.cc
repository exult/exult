/*
Copyright (C) 2000 Willem Jan Palenstijn
Copyright (C) 2000-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
A large part of this code is from the GIMP's PCX plugin:
"This code is based in parts on code by Francisco Bustamante, but the
largest portion of the code has been rewritten and is now maintained
occasionally by Nick Lamb njl195@zepler.org.uk."

It has been partly rewritten to use an SDL surface as input.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ignore_unused_variable_warning.h"

#include <cstdlib>
#include <iostream>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::cout;
using std::endl;

#ifdef HAVE_PNG_H

#	include "png.h"

/*
 * SDL_SavePNG -- libpng-based SDL_Surface writer.
 *
 * This code is free software, available under zlib/libpng license.
 * http://www.libpng.org/pub/png/src/libpng-LICENSE.txt
 */

#	if SDL_BYTEORDER == SDL_LIL_ENDIAN
#		define rmask 0x000000FF
#		define gmask 0x0000FF00
#		define bmask 0x00FF0000
#		define amask 0xFF000000
#	else
#		define rmask 0xFF000000
#		define gmask 0x00FF0000
#		define bmask 0x0000FF00
#		define amask 0x000000FF
#	endif

/* libpng callbacks */
static void png_error_SDL(png_structp ctx, png_const_charp str) {
	ignore_unused_variable_warning(ctx);
	SDL_SetError("libpng: %s\n", str);
}

static void png_write_SDL(
		png_structp png_ptr, png_bytep data, png_size_t length) {
	SDL_IOStream* rw = static_cast<SDL_IOStream*>(png_get_io_ptr(png_ptr));
	SDL_WriteIO(rw, data, sizeof(png_byte), length);
}

static bool save_image(SDL_Surface* surface, SDL_IOStream* dst, int guardband) {
	png_structp  png_ptr;
	png_infop    info_ptr;
	png_colorp   pal_ptr;
	SDL_Palette* pal;
	int          i, colortype;
	png_bytep*   row_pointers;
	const int    width  = surface->w - 2 * guardband;
	const int    height = surface->h - 2 * guardband;
	const int    pitch  = surface->pitch;
	auto*        pixels = static_cast<png_bytep>(surface->pixels) + guardband
				   + pitch * guardband;

	/* err_ptr, err_fn, warn_fn */
	png_ptr = png_create_write_struct(
			PNG_LIBPNG_VER_STRING, nullptr, png_error_SDL, nullptr);
	if (!png_ptr) {
		SDL_SetError(
				"Unable to png_create_write_struct on %s\n",
				PNG_LIBPNG_VER_STRING);
		return false;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		SDL_SetError("Unable to png_create_info_struct\n");
		png_destroy_write_struct(&png_ptr, nullptr);
		return false;
	}
	/* All other errors, see also "png_error_SDL" */
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return false;
	}

	/* Setup our RWops writer */
	/* w_ptr, write_fn, flush_fn */
	png_set_write_fn(png_ptr, dst, png_write_SDL, nullptr);

	/* Prepare chunks */
	colortype = PNG_COLOR_MASK_COLOR;
	if ((surface->format->BytesPerPixel > 0)
		&& (surface->format->BytesPerPixel <= 8)
		&& (pal = surface->format->palette)) {
		colortype |= PNG_COLOR_MASK_PALETTE;
		pal_ptr = static_cast<png_colorp>(
				malloc(pal->ncolors * sizeof(png_color)));
		for (i = 0; i < pal->ncolors; i++) {
			pal_ptr[i].red   = pal->colors[i].r;
			pal_ptr[i].green = pal->colors[i].g;
			pal_ptr[i].blue  = pal->colors[i].b;
		}
		png_set_PLTE(png_ptr, info_ptr, pal_ptr, pal->ncolors);
		free(pal_ptr);
	} else if (
			(surface->format->BytesPerPixel > 3) || (surface->format->Amask)) {
		colortype |= PNG_COLOR_MASK_ALPHA;
	}

	png_set_IHDR(
			png_ptr, info_ptr, width, height, 8, colortype, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	/* Write everything */
	png_write_info(png_ptr, info_ptr);
	row_pointers = static_cast<png_bytep*>(malloc(sizeof(png_bytep) * height));
	for (i = 0; i < height; i++) {
		row_pointers[i] = pixels + i * pitch;
	}
	png_write_image(png_ptr, row_pointers);
	free(row_pointers);
	png_write_end(png_ptr, info_ptr);

	/* Done */
	png_destroy_write_struct(&png_ptr, &info_ptr);
	return true;
}

#else

#	if SDL_BYTEORDER == SDL_LIL_ENDIAN
#		define qtohl(x) (x)
#		define qtohs(x) (x)
#	else
#		define qtohl(x)                                   \
			((Uint32)((((Uint32)(x) & 0x000000ffU) << 24)  \
					  | (((Uint32)(x) & 0x0000ff00U) << 8) \
					  | (((Uint32)(x) & 0x00ff0000U) >> 8) \
					  | (((Uint32)(x) & 0xff000000U) >> 24)))
#		define qtohs(x)                            \
			((Uint16)((((Uint16)(x) & 0x00ff) << 8) \
					  | (((Uint16)(x) & 0xff00) >> 8)))
#	endif
#	define htoql(x) qtohl(x)
#	define htoqs(x) qtohs(x)

struct PCX_Header {
	Uint8  manufacturer;
	Uint8  version;
	Uint8  compression;
	Uint8  bpp;
	Sint16 x1, y1;
	Sint16 x2, y2;
	Sint16 hdpi;
	Sint16 vdpi;
	Uint8  colormap[48];
	Uint8  reserved;
	Uint8  planes;
	Sint16 bytesperline;
	Sint16 color;
	Uint8  filler[58];
};

static void writeline(SDL_IOStream* dst, Uint8* buffer, int bytes) {
	Uint8* finish = buffer + bytes;

	while (buffer < finish) {
		Uint8 value = *(buffer++);
		Uint8 count = 1;

		while (buffer < finish && count < 63 && *buffer == value) {
			count++;
			buffer++;
		}

		if (value < 0xc0 && count == 1) {
			SDL_WriteIO(dst, &value, 1, 1);
		} else {
			Uint8 tmp = count + 0xc0;
			SDL_WriteIO(dst, &tmp, 1, 1);
			SDL_WriteIO(dst, &value, 1, 1);
		}
	}
}

static void save_8(
		SDL_IOStream* dst, int width, int height, int pitch, Uint8* buffer) {
	for (int row = 0; row < height; ++row) {
		writeline(dst, buffer, width);
		buffer += pitch;
	}
}

static void save_24(
		SDL_IOStream* dst, int width, int height, int pitch,
		const Uint8* buffer) {
	auto* line = new Uint8[width];

	for (int y = 0; y < height; ++y) {
		for (int c = 2; c >= 0; --c) {
			for (int x = 0; x < width; ++x) {
				line[x] = buffer[(3 * x) + c];
			}
			writeline(dst, line, width);
		}
		buffer += pitch;
	}
	delete[] line;
}

static bool save_image(SDL_Surface* surface, SDL_IOStream* dst, int guardband) {
	Uint8* cmap   = nullptr;
	int    colors = 0;
	int    width  = surface->w - 2 * guardband;
	int    height = surface->h - 2 * guardband;
	int    pitch  = surface->pitch;
	auto*  pixels = static_cast<Uint8*>(surface->pixels) + guardband
				   + pitch * guardband;

	PCX_Header header;
	header.manufacturer = 0x0a;
	header.version      = 5;
	header.compression  = 1;

	if (surface->format->palette && surface->format->BitsPerPixel == 8) {
		colors = surface->format->palette->ncolors;
		cmap   = new Uint8[3 * colors];
		for (int i = 0; i < colors; i++) {
			cmap[3 * i]     = surface->format->palette->colors[i].r;
			cmap[3 * i + 1] = surface->format->palette->colors[i].g;
			cmap[3 * i + 2] = surface->format->palette->colors[i].b;
		}
		header.bpp          = 8;
		header.bytesperline = htoqs(width);
		header.planes       = 1;
		header.color        = htoqs(1);
	} else if (surface->format->BitsPerPixel == 24) {
		header.bpp          = 8;
		header.bytesperline = htoqs(width);
		header.planes       = 3;
		header.color        = htoqs(1);
	} else {
		return false;
	}

	header.x1 = 0;
	header.y1 = 0;
	header.x2 = htoqs(width - 1);
	header.y2 = htoqs(height - 1);

	header.hdpi     = htoqs(300);
	header.vdpi     = htoqs(300);
	header.reserved = 0;

	/* write header */
	/*  fp_offset = SDL_TellIO(dst);*/
	SDL_WriteIO(dst, &header, sizeof(PCX_Header), 1);

	if (cmap) {
		save_8(dst, width, height, pitch, pixels);

		/* write palette */
		Uint8 tmp = 0x0c;
		SDL_WriteIO(dst, &tmp, 1, 1);
		SDL_WriteIO(dst, cmap, 3, colors);

		/* fill unused colors */
		tmp = 0;
		for (int i = colors; i < 256; i++) {
			SDL_WriteIO(dst, &tmp, 1, 1);
			SDL_WriteIO(dst, &tmp, 1, 1);
			SDL_WriteIO(dst, &tmp, 1, 1);
		}

		delete[] cmap;
	} else {
		save_24(dst, width, height, pitch, pixels);
	}

	return true;
}

#endif    // HAVE_PNG_H

bool SaveIMG_RW(
		SDL_Surface* saveme, SDL_IOStream* dst, bool freedst, int guardband) {
	SDL_Surface* surface;
	bool         found_error = false;

	cout << "Taking screenshot...";

	surface = nullptr;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	constexpr const Uint32 Rmask = 0x00FF0000U;
	constexpr const Uint32 Gmask = 0x0000FF00U;
	constexpr const Uint32 Bmask = 0x000000FFU;
#else
	constexpr const Uint32 Rmask = 0x000000FFU;
	constexpr const Uint32 Gmask = 0x0000FF00U;
	constexpr const Uint32 Bmask = 0x00FF0000U;
#endif
	if (dst) {
		if (saveme->format->palette) {
			if (saveme->format->BitsPerPixel == 8) {
				surface = saveme;
			} else {
				found_error = true;
				cout << saveme->format->BitsPerPixel
					 << "bpp PCX files not supported" << endl;
			}
		} else if (
				(saveme->format->BitsPerPixel == 24)
				&& (saveme->format->Rmask == Rmask)
				&& (saveme->format->Gmask == Gmask)
				&& (saveme->format->Bmask == Bmask)) {
			surface = saveme;
		} else {
			SDL_Rect bounds;

			/* Convert to 24 bits per pixel */
			surface = SDL_CreateRGBSurface(
					SDL_SWSURFACE, saveme->w, saveme->h, 24, Rmask, Gmask,
					Bmask, 0);

			if (surface != nullptr) {
				bounds.x = 0;
				bounds.y = 0;
				bounds.w = saveme->w;
				bounds.h = saveme->h;
				if (SDL_BlitSurfaceUnchecked(saveme, &bounds, surface, &bounds)
					< 0) {
					SDL_DestroySurface(surface);
					cout << "Couldn't convert image to 24 bpp for screenshot";
					found_error = true;
					surface     = nullptr;
				}
			}
		}
	} else {
		/* no valid target */
	}

	if (surface && (SDL_LockSurface(surface) == 0)) {
		found_error |= !save_image(surface, dst, guardband);

		/* Close it up.. */
		SDL_UnlockSurface(surface);
		if (surface != saveme) {
			SDL_DestroySurface(surface);
		}
	}

	if (freedst && dst) {
		SDL_CloseIO(dst);
	}

	if (!found_error) {
		cout << "Done!" << endl;
		return true;
	}

	return false;
}
