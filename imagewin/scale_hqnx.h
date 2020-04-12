/*
 *  Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
 *
 *  Adapted for Exult: 4/7/07 - JSF
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifndef INCL_SCALE_HQNX_H
#define INCL_SCALE_HQNX_H   1

#if defined(USE_HQ2X_SCALER) || defined(USE_HQ3X_SCALER) || defined(USE_HQ4X_SCALER)

#include <cstdlib>

/**
 ** Note: This file should only be included by source files that use the
 ** templates below; the templates will only be instantiated when they
 ** are used anyway.
 **/

/*
 *  Common defines and functions.
 */
const  int   Ymask = 0x00FF0000;
const  int   Umask = 0x0000FF00;
const  int   Vmask = 0x000000FF;
const  int   trY   = 0x00300000;
const  int   trU   = 0x00000700;
const  int   trV   = 0x00000006;

template<class Dest_pixel, class Manip_pixels>
inline void StoreRGB(Dest_pixel *pc, int c, const Manip_pixels &manip) {
	*pc = manip.rgb((c & 0xff0000) >> 16, (c & 0xff00) >> 8, c & 0xff);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp1(Dest_pixel *pc, int c1, int c2, const Manip_pixels &manip) {
	StoreRGB(pc, (c1 * 3 + c2) >> 2, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp2(Dest_pixel *pc, int c1, int c2, int c3,
                    const Manip_pixels &manip) {
	StoreRGB(pc, (c1 * 2 + c2 + c3) >> 2, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp3(Dest_pixel *pc, int c1, int c2,
                    const Manip_pixels &manip) {
	//*((int*)pc) = (c1*7+c2)/8;

	StoreRGB(pc, ((((c1 & 0x00FF00) * 7 + (c2 & 0x00FF00)) & 0x0007F800) +
	              (((c1 & 0xFF00FF) * 7 + (c2 & 0xFF00FF)) & 0x07F807F8)) >> 3,
	         manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp4(Dest_pixel *pc, int c1, int c2, int c3,
                    const Manip_pixels &manip) {
	//*((int*)pc) = (c1*2+(c2+c3)*7)/16;

	StoreRGB(pc, ((((c1 & 0x00FF00) * 2 + ((c2 & 0x00FF00) + (c3 & 0x00FF00)) * 7)
	               & 0x000FF000) +
	              (((c1 & 0xFF00FF) * 2 + ((c2 & 0xFF00FF) + (c3 & 0xFF00FF)) * 7)
	               & 0x0FF00FF0)) >> 4,
	         manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp5(Dest_pixel *pc, int c1, int c2, const Manip_pixels &manip) {
	StoreRGB(pc, (c1 + c2) >> 1, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp6(Dest_pixel *pc, int c1, int c2, int c3,
                    const Manip_pixels &manip) {
	//StoreRGB(pc, (c1*5+c2*2+c3)/8);

	StoreRGB(pc, ((((c1 & 0x00FF00) * 5 + (c2 & 0x00FF00) * 2 +
	                (c3 & 0x00FF00)) & 0x0007F800) +
	              (((c1 & 0xFF00FF) * 5 + (c2 & 0xFF00FF) * 2 +
	                (c3 & 0xFF00FF)) & 0x07F807F8)) >> 3, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp7(Dest_pixel *pc, int c1, int c2, int c3,
                    const Manip_pixels &manip) {
	//StoreRGB(pc, (c1*6+c2+c3)/8);

	StoreRGB(pc, ((((c1 & 0x00FF00) * 6 + (c2 & 0x00FF00) +
	                (c3 & 0x00FF00)) & 0x0007F800) +
	              (((c1 & 0xFF00FF) * 6 + (c2 & 0xFF00FF) +
	                (c3 & 0xFF00FF)) & 0x07F807F8)) >> 3, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp8(Dest_pixel *pc, int c1, int c2,
                    const Manip_pixels &manip) {
	//*((int*)pc) = (c1*5+c2*3)/8;

	StoreRGB(pc, ((((c1 & 0x00FF00) * 5 + (c2 & 0x00FF00) * 3) & 0x0007F800) +
	              (((c1 & 0xFF00FF) * 5 + (c2 & 0xFF00FF) * 3) & 0x07F807F8)) >> 3,
	         manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp9(Dest_pixel *pc, int c1, int c2, int c3,
                    const Manip_pixels &manip) {
	//StoreRGB(pc, (c1*2+(c2+c3)*3)/8);

	StoreRGB(pc, ((((c1 & 0x00FF00) * 2 + ((c2 & 0x00FF00) +
	                                       (c3 & 0x00FF00)) * 3) & 0x0007F800) +
	              (((c1 & 0xFF00FF) * 2 + ((c2 & 0xFF00FF) +
	                                       (c3 & 0xFF00FF)) * 3) & 0x07F807F8)) >> 3, manip);
}

template<class Dest_pixel, class Manip_pixels>
inline void Interp10(Dest_pixel *pc, int c1, int c2, int c3,
                     const Manip_pixels &manip) {
	//StoreRGB(pc, (c1*14+c2+c3)/16);

	StoreRGB(pc, ((((c1 & 0x00FF00) * 14 + (c2 & 0x00FF00) +
	                (c3 & 0x00FF00)) & 0x000FF000) +
	              (((c1 & 0xFF00FF) * 14 + (c2 & 0xFF00FF) +
	                (c3 & 0xFF00FF)) & 0x0FF00FF0)) >> 4, manip);
}

#define PTYPES              Dest_pixel,Manip_pixels

static int RGBtoYUV(unsigned int r, unsigned int g, unsigned int b) {
	int Y;
	int u;
	int v;
	Y = (r + g + b) >> 2;
	u = 128 + ((r - b) >> 2);
	v = 128 + ((-r + 2 * g - b) >> 3);
	return (Y << 16) + (u << 8) + v;
}

inline bool Diff(int YUV1, int YUV2) {
	return (std::abs((YUV1 & Ymask) - (YUV2 & Ymask)) > trY) ||
	       (std::abs((YUV1 & Umask) - (YUV2 & Umask)) > trU) ||
	       (std::abs((YUV1 & Vmask) - (YUV2 & Vmask)) > trV);
}

template<class Manip_pixels>
inline int hqx_init(int *w, int *c, int *yuv, const unsigned char *from,
                    int x, int sline_pixels, int prevline, int nextline,
                    Manip_pixels &manip) {
	int k;

	w[2] = *((from + prevline));
	w[5] = *(from);
	w[8] = *((from + nextline));

	if (x > 0) {
		w[1] = *((from + prevline - 1));
		w[4] = *((from - 1));
		w[7] = *((from + nextline - 1));
	} else {
		w[1] = w[2];
		w[4] = w[5];
		w[7] = w[8];
	}

	if (x < sline_pixels - 1) {
		w[3] = *((from + prevline + 1));
		w[6] = *((from + 1));
		w[9] = *((from + nextline + 1));
	} else {
		w[3] = w[2];
		w[6] = w[5];
		w[9] = w[8];
	}

	for (k = 1; k <= 9; k++) {
		unsigned int r;
		unsigned int g;
		unsigned int b;
		manip.split_source(w[k], r, g, b);
		// The following is so the Interp routines work correctly.
		r &= ~3;
		g &= ~3;
		c[k] = (r << 16) + (g << 8) + b;;
		yuv[k] = RGBtoYUV(r, g, b);
	}

	int pattern = 0;
	int flag = 1;

	int YUV1 = yuv[5];

	for (k = 1; k <= 9; k++) {
		if (k == 5) continue;

		if (w[k] != w[5]) {
			if (Diff(YUV1, yuv[k]))
				pattern |= flag;
		}
		flag <<= 1;
	}
	return pattern;
}

#endif //defined(USE_HQ2X_SCALER) || defined(USE_HQ3X_SCALER) || defined(USE_HQ4X_SCALER)

#endif //INCL_SCALE_HQNX_H
