/*
Copyright (C) 2000 Willem Jan Palenstijn

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
#  include <config.h>
#endif

#include <cstdlib>

#include "SDL_video.h"
#include "SDL_endian.h"
#include <iostream>

using std::cout;
using std::endl;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define qtohl(x) (x)
#define qtohs(x) (x)
#else
#define qtohl(x) \
	((Uint32)((((Uint32)(x) & 0x000000ffU) << 24) | \
	          (((Uint32)(x) & 0x0000ff00U) <<  8) | \
	          (((Uint32)(x) & 0x00ff0000U) >>  8) | \
	          (((Uint32)(x) & 0xff000000U) >> 24)))
#define qtohs(x) \
	((Uint16)((((Uint16)(x) & 0x00ff) << 8) | \
	          (((Uint16)(x) & 0xff00) >> 8)))
#endif
#define htoql(x) qtohl(x)
#define htoqs(x) qtohs(x)

struct PCX_Header {
	Uint8 manufacturer;
	Uint8 version;
	Uint8 compression;
	Uint8 bpp;
	Sint16 x1, y1;
	Sint16 x2, y2;
	Sint16 hdpi;
	Sint16 vdpi;
	Uint8 colormap[48];
	Uint8 reserved;
	Uint8 planes;
	Sint16 bytesperline;
	Sint16 color;
	Uint8 filler[58];
};

static void writeline(SDL_RWops *dst, Uint8 *buffer, int bytes) {
	Uint8 value;
	Uint8 count;
	Uint8 tmp;
	Uint8 *finish = buffer + bytes;

	while (buffer < finish) {

		value = *(buffer++);
		count = 1;

		while (buffer < finish && count < 63 && *buffer == value) {
			count++;
			buffer++;
		}

		if (value < 0xc0 && count == 1) {
			SDL_RWwrite(dst, &value, 1, 1);
		} else {
			tmp = count + 0xc0;
			SDL_RWwrite(dst, &tmp, 1, 1);
			SDL_RWwrite(dst, &value, 1, 1);
		}
	}
}


static void save_8(SDL_RWops *dst, int width, int height,
                   int pitch, Uint8 *buffer) {
	int row;

	for (row = 0; row < height; ++row) {
		writeline(dst, buffer, width);
		buffer += pitch;
	}
}


static void save_24(SDL_RWops *dst, int width, int height,
                    int pitch, const Uint8 *buffer) {
	int x;
	int y;
	int c;
	Uint8 *line;

	line = new Uint8[width];

	for (y = 0; y < height; ++y) {
		for (c = 2; c >= 0; --c) {
			for (x = 0; x < width; ++x) {
				line[x] = buffer[(3 * x) + c];
			}
			writeline(dst, line, width);
		}
		buffer += pitch;
	}
	delete [] line;
}

static bool save_image(SDL_Surface *surface, SDL_RWops *dst) {
	Uint8 *cmap = nullptr;
	Uint8 *pixels;
	Uint8 tmp;
	int width;
	int height;
	int pitch;
	int colors = 0;
	int i;
	PCX_Header header;

	width = surface->w;
	height = surface->h;
	pixels = static_cast<Uint8 *>(surface->pixels);
	pitch = surface->pitch;

	header.manufacturer = 0x0a;
	header.version = 5;
	header.compression = 1;

	if (surface->format->palette && surface->format->BitsPerPixel == 8) {
		colors = surface->format->palette->ncolors;
		cmap = new Uint8[3 * colors];
		for (i = 0; i < colors; i++) {
			cmap[3 * i] = surface->format->palette->colors[i].r;
			cmap[3 * i + 1] = surface->format->palette->colors[i].g;
			cmap[3 * i + 2] = surface->format->palette->colors[i].b;
		}
		header.bpp = 8;
		header.bytesperline = htoqs(width);
		header.planes = 1;
		header.color = htoqs(1);
	} else if (surface->format->BitsPerPixel == 24) {
		header.bpp = 8;
		header.bytesperline = htoqs(width);
		header.planes = 3;
		header.color = htoqs(1);
	} else {
		return false;
	}

	header.x1 = 0;
	header.y1 = 0;
	header.x2 = htoqs(width - 1);
	header.y2 = htoqs(height - 1);

	header.hdpi = htoqs(300);
	header.vdpi = htoqs(300);
	header.reserved = 0;

	/* write header */
	/*  fp_offset = SDL_RWtell(dst);*/
	SDL_RWwrite(dst, &header, sizeof(PCX_Header), 1);

	if (cmap) {
		save_8(dst, width, height, pitch, pixels);

		/* write palette */
		tmp = 0x0c;
		SDL_RWwrite(dst, &tmp, 1, 1);
		SDL_RWwrite(dst, cmap, 3, colors);

		/* fill unused colors */
		tmp = 0;
		for (i = colors; i < 256; i++) {
			SDL_RWwrite(dst, &tmp, 1, 1);
			SDL_RWwrite(dst, &tmp, 1, 1);
			SDL_RWwrite(dst, &tmp, 1, 1);
		}

		delete [] cmap;
	} else {
		save_24(dst, width, height, pitch, pixels);
	}

	return true;
}

bool SavePCX_RW(SDL_Surface *saveme, SDL_RWops *dst, bool freedst) {
	SDL_Surface *surface;
	bool found_error = false;

	cout << "Taking screenshot...";

	surface = nullptr;
	if (dst) {
		if (saveme->format->palette) {
			if (saveme->format->BitsPerPixel == 8) {
				surface = saveme;
			} else {
				found_error = true;
				cout << saveme->format->BitsPerPixel
				     << "bpp PCX files not supported" << endl;
			}
		} else if ((saveme->format->BitsPerPixel == 24) &&
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		           (saveme->format->Rmask == 0x00FF0000) &&
		           (saveme->format->Gmask == 0x0000FF00) &&
		           (saveme->format->Bmask == 0x000000FF)
#else
		           (saveme->format->Rmask == 0x000000FF) &&
		           (saveme->format->Gmask == 0x0000FF00) &&
		           (saveme->format->Bmask == 0x00FF0000)
#endif
		          ) {
			surface = saveme;
		} else {
			SDL_Rect bounds;

			/* Convert to 24 bits per pixel */
			surface = SDL_CreateRGBSurface(SDL_SWSURFACE,
			                               saveme->w, saveme->h, 24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
			                               0x00FF0000, 0x0000FF00, 0x000000FF,
#else
			                               0x000000FF, 0x0000FF00, 0x00FF0000,
#endif
			                               0);

			if (surface != nullptr) {
				bounds.x = 0;
				bounds.y = 0;
				bounds.w = saveme->w;
				bounds.h = saveme->h;
				if (SDL_LowerBlit(saveme, &bounds, surface,
				                  &bounds) < 0) {
					SDL_FreeSurface(surface);
					cout << "Couldn't convert image to 24 bpp for screenshot";
					found_error = true;
					surface = nullptr;
				}
			}
		}
	} else {
		/* no valid target */
	}

	if (surface && (SDL_LockSurface(surface) == 0)) {

		found_error |= !save_image(surface, dst);

		/* Close it up.. */
		SDL_UnlockSurface(surface);
		if (surface != saveme) {
			SDL_FreeSurface(surface);
		}
	}


	if (freedst && dst) {
		SDL_RWclose(dst);
	}

	if (!found_error) {
		cout << "Done!" << endl;
		return true;
	}

	return false;
}
