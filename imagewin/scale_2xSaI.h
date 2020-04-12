/*
 *  Copyright (C) 2009 Exult Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 */

#ifndef INCL_SCALE_2XSAI_H
#define INCL_SCALE_2XSAI_H  1

/**
 ** Note: This file should only be included by source files that use the
 ** templates below; the templates will only be instantiated when they
 ** are used anyway.
 **/

/**
 ** 2xSaI scaling filter source code adapted for Exult
 ** August 29 2000, originally written in May 1999
 ** by Derek Liauw Kie Fa (DerekL666@yahoo.com/D.A.K.L.LiauwKieFa@student.tudelft.nl)
 ** This source is made available under the terms of the GNU GPL
 ** I'd appreciate it I am given credit in the program or documentation
 **/


template <class Source_pixel, class Dest_pixel, class Manip_pixels>
inline Dest_pixel Interpolate_2xSaI(Source_pixel colorA, Source_pixel colorB, const Manip_pixels &manip) {
	unsigned int r0;
	unsigned int r1;
	unsigned int g0;
	unsigned int g1;
	unsigned int b0;
	unsigned int b1;
	manip.split_source(colorA, r0, g0, b0);
	manip.split_source(colorB, r1, g1, b1);
	int r = (r0 + r1) >> 1;
	int g = (g0 + g1) >> 1;
	int b = (b0 + b1) >> 1;
	return manip.rgb(r, g, b);
}

template <class Source_pixel, class Dest_pixel, class Manip_pixels>
inline Dest_pixel OInterpolate_2xSaI(Source_pixel colorA, Source_pixel colorB, Source_pixel colorC, const Manip_pixels &manip) {
	unsigned int r0;
	unsigned int r1;
	unsigned int g0;
	unsigned int g1;
	unsigned int b0;
	unsigned int b1;
	unsigned int r2;
	unsigned int g2;
	unsigned int b2;
	manip.split_source(colorA, r0, g0, b0);
	manip.split_source(colorB, r1, g1, b1);
	manip.split_source(colorC, r2, g2, b2);
	unsigned int r = ((r0 << 2) + (r0 << 1) + r1 + r2) >> 3;
	unsigned int g = ((g0 << 2) + (g0 << 1) + g1 + g2) >> 3;
	unsigned int b = ((b0 << 2) + (b0 << 1) + b1 + b2) >> 3;
	return manip.rgb(r, g, b);
}

template <class Source_pixel, class Dest_pixel, class Manip_pixels>
inline Dest_pixel QInterpolate_2xSaI(Source_pixel colorA, Source_pixel colorB, Source_pixel colorC, Source_pixel colorD, const Manip_pixels &manip) {
	unsigned int r0;
	unsigned int r1;
	unsigned int g0;
	unsigned int g1;
	unsigned int b0;
	unsigned int b1;
	unsigned int r2;
	unsigned int r3;
	unsigned int g2;
	unsigned int g3;
	unsigned int b2;
	unsigned int b3;
	manip.split_source(colorA, r0, g0, b0);
	manip.split_source(colorB, r1, g1, b1);
	manip.split_source(colorC, r2, g2, b2);
	manip.split_source(colorD, r3, g3, b3);
	unsigned int r = (r0 + r1 + r2 + r3) >> 2;
	unsigned int g = (g0 + g1 + g2 + g3) >> 2;
	unsigned int b = (b0 + b1 + b2 + b3) >> 2;
	return manip.rgb(r, g, b);
}

template <class Source_pixel>
inline int GetResult1(Source_pixel A, Source_pixel B, Source_pixel C, Source_pixel D) {
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C) x += 1;
	else if (B == C) y += 1;
	if (A == D) x += 1;
	else if (B == D) y += 1;
	if (x <= 1) r += 1;
	if (y <= 1) r -= 1;
	return r;
}

template <class Source_pixel>
inline int GetResult2(Source_pixel A, Source_pixel B, Source_pixel C, Source_pixel D) {
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C) x += 1;
	else if (B == C) y += 1;
	if (A == D) x += 1;
	else if (B == D) y += 1;
	if (x <= 1) r -= 1;
	if (y <= 1) r += 1;
	return r;
}


// 2xSaI scaler
template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_2xSaI(
    Source_pixel *source,   // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {
	Source_pixel *srcPtr = source + (srcx + srcy * sline_pixels);
	Dest_pixel *dstPtr = dest + (2 * srcy * dline_pixels + 2 * srcx);

	if (srcx + srcw >= sline_pixels) {
		srcw = sline_pixels - srcx;
	}
	// Init offset to prev. line, next 2.
	int prev1_yoff = srcy ? sline_pixels : 0;
	int next1_yoff = sline_pixels;
	int next2_yoff = 2 * sline_pixels;
	// Figure threshholds for counters.
	int ybeforelast = sheight - 2 - srcy;
	int xbeforelast = sline_pixels - 2 - srcx;
	for (int y = 0; y < srch; y++, prev1_yoff = sline_pixels) {
		if (y >= ybeforelast) { // Last/next-to-last row?
			if (y == ybeforelast)
				next2_yoff = sline_pixels;
			else        // Very last line?
				next2_yoff = next1_yoff = 0;
		}

		Source_pixel *bP = srcPtr;
		Dest_pixel *dP = dstPtr;
		int prev1_xoff = srcx ? 1 : 0;
		int next1_xoff = 1;
		int next2_xoff = 2;

		for (int x = 0; x < srcw; x++) {
			Source_pixel colorA;
			Source_pixel colorB;
			Source_pixel colorC;
			Source_pixel colorD;
			Source_pixel colorE;
			Source_pixel colorF;
			Source_pixel colorG;
			Source_pixel colorH;
			Source_pixel colorI;
			Source_pixel colorJ;
			Source_pixel colorK;
			Source_pixel colorL;
			Source_pixel colorM;
			Source_pixel colorN;
			Source_pixel colorO/*, colorP*/;
			Dest_pixel product;
			Dest_pixel product1;
			Dest_pixel product2;
			Dest_pixel orig;

			// Last/next-to-last row?
			if (x >= xbeforelast) {
				if (x == xbeforelast)
					next2_xoff = 1;
				else
					next2_xoff = next1_xoff = 0;
			}

			//---------------------------------------
			// Map of the pixels:                    I|E F|J
			//                                       G|A B|K
			//                                       H|C D|L
			//                                       M|N O|P
			colorI = *(bP - prev1_yoff - prev1_xoff);
			colorE = *(bP - prev1_yoff);
			colorF = *(bP - prev1_yoff + next1_xoff);
			colorJ = *(bP - prev1_yoff + next2_xoff);

			colorG = *(bP - prev1_xoff);
			colorA = *(bP);
			colorB = *(bP + next1_xoff);
			colorK = *(bP + next2_xoff);

			colorH = *(bP + next1_yoff - prev1_xoff);
			colorC = *(bP + next1_yoff);
			colorD = *(bP + next1_yoff + next1_xoff);
			colorL = *(bP + next1_yoff + next2_xoff);

			colorM = *(bP + next2_yoff - prev1_xoff);
			colorN = *(bP + next2_yoff);
			colorO = *(bP + next2_yoff + next1_xoff);
			//colorP = *(bP + next2_yoff + next2_xoff);

			if ((colorA == colorD) && (colorB != colorC)) {
				if (((colorA == colorE) && (colorB == colorL)) ||
				        ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ))) {
					//product = colorA;
					manip.copy(product, colorA);
				} else {
					//product = INTERPOLATE(colorA, colorB);
					product = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, manip);
				}

				if (((colorA == colorG) && (colorC == colorO)) ||
				        ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM))) {
					//product1 = colorA;
					manip.copy(product1, colorA);
				} else {
					//product1 = INTERPOLATE(colorA, colorC);
					product1 = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorC, manip);
				}
				//product2 = colorA;
				manip.copy(product2, colorA);
			} else if ((colorB == colorC) && (colorA != colorD)) {
				if (((colorB == colorF) && (colorA == colorH)) ||
				        ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))) {
					//product = colorB;
					manip.copy(product, colorB);
				} else {
					//product = INTERPOLATE(colorA, colorB);
					product = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, manip);
				}

				if (((colorC == colorH) && (colorA == colorF)) ||
				        ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI))) {
					//product1 = colorC;
					manip.copy(product1, colorC);
				} else {
					//product1 = INTERPOLATE(colorA, colorC);
					product1 = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorC, manip);
				}
				//product2 = colorB;
				manip.copy(product2, colorB);
			} else if ((colorA == colorD) && (colorB == colorC)) {
				if (colorA == colorB) {
					//product = colorA;
					manip.copy(product, colorA);
					//product1 = colorA;
					manip.copy(product1, colorA);
					//product2 = colorA;
					manip.copy(product2, colorA);
				} else {
					int r = 0;
					//product1 = INTERPOLATE(colorA, colorC);
					product1 = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorC, manip);
					//product = INTERPOLATE(colorA, colorB);
					product = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, manip);

					r += GetResult1 <Source_pixel>(colorA, colorB, colorG, colorE);
					r += GetResult2 <Source_pixel>(colorB, colorA, colorK, colorF);
					r += GetResult2 <Source_pixel>(colorB, colorA, colorH, colorN);
					r += GetResult1 <Source_pixel>(colorA, colorB, colorL, colorO);

					if (r > 0)
						//product2 = colorA;
						manip.copy(product2, colorA);
					else if (r < 0)
						//product2 = colorB;
						manip.copy(product2, colorB);
					else {
						//product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);
						product2 = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, colorC, colorD, manip);
					}
				}
			} else {
				//product2 = Q_INTERPOLATE(colorA, colorB, colorC, colorD);
				product2 = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, colorC, colorD, manip);

				if ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ)) {
					//product = colorA;
					manip.copy(product, colorA);
				} else if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI)) {
					//product = colorB;
					manip.copy(product, colorB);
				} else {
					//product = INTERPOLATE(colorA, colorB);
					product = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorB, manip);
				}

				if ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM)) {
					//product1 = colorA;
					manip.copy(product1, colorA);
				} else if ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI)) {
					//product1 = colorC;
					manip.copy(product1, colorC);
				} else {
					//product1 = INTERPOLATE(colorA, colorC);
					product1 = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(colorA, colorC, manip);
				}
			}


			//product = colorA | (product << 16);
			//product1 = product1 | (product2 << 16);
			manip.copy(orig, colorA);
			*dP = orig;
			*(dP + 1) = product;
			*(dP + dline_pixels) = product1;
			*(dP + dline_pixels + 1) = product2;

			bP += 1;
			dP += 2;
			prev1_xoff = 1;
		}//end of for ( finish= width etc..)

		srcPtr += sline_pixels;
		dstPtr += 2 * dline_pixels;
		prev1_yoff = 1;
	};
}



template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_Super2xSaI(
    Source_pixel *source,   // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {

	Source_pixel *srcPtr = source + (srcx + srcy * sline_pixels);
	Dest_pixel *dstPtr = dest + (2 * srcy * dline_pixels + 2 * srcx);

	if (srcx + srcw >= sline_pixels) {
		srcw = sline_pixels - srcx;
	}

	int ybeforelast1 = sheight - 1 - srcy;
	int ybeforelast2 = sheight - 2 - srcy;
	int xbeforelast1 = sline_pixels - 1 - srcx;
	int xbeforelast2 = sline_pixels - 2 - srcx;

	for (int y = 0; y < srch; y++) {
		Source_pixel *bP = srcPtr;
		Dest_pixel *dP = dstPtr;

		for (int x = 0; x < srcw; x++) {
			Source_pixel color4;
			Source_pixel color5;
			Source_pixel color6;
			Source_pixel color1;
			Source_pixel color2;
			Source_pixel color3;
			Source_pixel colorA0;
			Source_pixel colorA1;
			Source_pixel colorA2;
			Source_pixel colorA3;
			Source_pixel colorB0;
			Source_pixel colorB1;
			Source_pixel colorB2;
			Source_pixel colorB3;
			Source_pixel colorS1;
			Source_pixel colorS2;
			Dest_pixel product1a;
			Dest_pixel product1b;
			Dest_pixel product2a;
			Dest_pixel product2b;

			//---------------------------------------  B0 B1 B2 B3
			//                                         4  5  6  S2
			//                                         1  2  3  S1
			//                                         A0 A1 A2 A3
			//--------------------------------------
			int add1;
			int add2;
			int sub1;
			int nextl1;
			int nextl2;
			int prevl1;

			if (x == 0)
				sub1 = 0;
			else
				sub1 = 1;

			if (x >= xbeforelast2)
				add2 = 0;
			else add2 = 1;

			if (x >= xbeforelast1)
				add1 = 0;
			else add1 = 1;

			if (y == 0)
				prevl1 = 0;
			else
				prevl1 = sline_pixels;

			if (y >= ybeforelast2)
				nextl2 = 0;
			else nextl2 = sline_pixels;

			if (y >= ybeforelast1)
				nextl1 = 0;
			else nextl1 = sline_pixels;


			colorB0 = *(bP - prevl1 - sub1);
			colorB1 = *(bP - prevl1);
			colorB2 = *(bP - prevl1 + add1);
			colorB3 = *(bP - prevl1 + add1 + add2);

			color4 = *(bP - sub1);
			color5 = *(bP);
			color6 = *(bP + add1);
			colorS2 = *(bP + add1 + add2);

			color1 = *(bP + nextl1 - sub1);
			color2 = *(bP + nextl1);
			color3 = *(bP + nextl1 + add1);
			colorS1 = *(bP + nextl1 + add1 + add2);

			colorA0 = *(bP + nextl1 + nextl2 - sub1);
			colorA1 = *(bP + nextl1 + nextl2);
			colorA2 = *(bP + nextl1 + nextl2 + add1);
			colorA3 = *(bP + nextl1 + nextl2 + add1 + add2);

			if (color2 == color6 && color5 != color3) {
				//product2b = product1b = color2;
				manip.copy(product2b, color2);
				product1b = product2b;
			} else if (color5 == color3 && color2 != color6) {
				//product2b = product1b = color5;
				manip.copy(product2b, color5);
				product1b = product2b;
			} else if (color5 == color3 && color2 == color6) {
				int r = 0;

				//r += GetResult (color6, color5, color1, colorA1);
				//r += GetResult (color6, color5, color4, colorB1);
				//r += GetResult (color6, color5, colorA2, colorS1);
				//r += GetResult (color6, color5, colorB2, colorS2);
				r += GetResult1 <Source_pixel>(color5, color6, color4, colorB1);
				r += GetResult2 <Source_pixel>(color6, color5, colorA2, colorS1);
				r += GetResult2 <Source_pixel>(color6, color5, color1, colorA1);
				r += GetResult1 <Source_pixel>(color5, color6, colorB2, colorS2);

				if (r > 0) {
					//product2b = product1b = color6;
					manip.copy(product2b, color6);
					product1b = product2b;
				} else if (r < 0) {
					//product2b = product1b = color5;
					manip.copy(product2b, color5);
					product1b = product2b;
				} else {
					//product2b = product1b = INTERPOLATE (color5, color6);
					product1b = product2b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, manip);
				}

			} else {

				if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
					//product2b = Q_INTERPOLATE (color3, color3, color3, color2);
					product2b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color3, color3, color3, color2, manip);
				else if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
					//product2b = Q_INTERPOLATE (color2, color2, color2, color3);
					product2b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color3, color2, color2, color2, manip);
				else
					//product2b = INTERPOLATE (color2, color3);
					product2b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color3, manip);


				if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
					//product1b = Q_INTERPOLATE (color6, color6, color6, color5);
					product1b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, color6, color6, manip);
				else if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
					//product1b = Q_INTERPOLATE (color6, color5, color5, color5);
					product1b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color6, color5, color5, color5, manip);
				else
					//product1b = INTERPOLATE (color5, color6);
					product1b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, manip);

			}

			if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
				//product2a = INTERPOLATE (color2, color5);
				product2a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color2, manip);
			else if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
				//product2a = INTERPOLATE(color2, color5);
				product2a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color2, manip);
			else
				//product2a = color2;
				manip.copy(product2a, color2);


			if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
				//product1a = INTERPOLATE (color2, color5);
				product1a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color2, manip);
			else if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
				//product1a = INTERPOLATE(color2, color5);
				product1a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color2, manip);
			else
				//product1a = color5;
				manip.copy(product1a, color5);


			*dP = product1a;
			*(dP + 1) = product1b;
			*(dP + dline_pixels) = product2a;
			*(dP + dline_pixels + 1) = product2b;

			bP += 1;
			dP += 2;

		}
		srcPtr += sline_pixels;
		dstPtr += 2 * dline_pixels;
	};
}



template <class Source_pixel, class Dest_pixel, class Manip_pixels>
void Scale_SuperEagle(
    Source_pixel *source,   // ->source pixels.
    int srcx, int srcy,     // Start of rectangle within src.
    int srcw, int srch,     // Dims. of rectangle.
    int sline_pixels,       // Pixels (words)/line for source.
    int sheight,            // Source height.
    Dest_pixel *dest,       // ->dest pixels.
    int dline_pixels,       // Pixels (words)/line for dest.
    const Manip_pixels &manip   // Manipulator methods.
) {

	// Need to ensure that the update is alligned to 4 pixels - Colourless
	// The idea was to prevent artifacts from appearing, but it doesn't seem
	// to help
	/*
	{
	    int sx = ((srcx-4)/4)*4;
	    int ex = ((srcx+srcw+7)/4)*4;
	    int sy = ((srcy-4)/4)*4;
	    int ey = ((srcy+srch+7)/4)*4;

	    if (sx < 0) sx = 0;
	    if (sy < 0) sy = 0;
	    if (ex > sline_pixels) ex = sline_pixels;
	    if (ey > sheight) ey = sheight;

	    srcx = sx;
	    srcy = sy;
	    srcw = ex - sx;
	    srch = ey - sy;
	}
	*/

	Source_pixel *srcPtr = source + (srcx + srcy * sline_pixels);
	Dest_pixel *dstPtr = dest + (2 * srcy * dline_pixels + 2 * srcx);

	if (srcx + srcw >= sline_pixels) {
		srcw = sline_pixels - srcx;
	}

	int ybeforelast1 = sheight - 1 - srcy;
	int ybeforelast2 = sheight - 2 - srcy;
	int xbeforelast1 = sline_pixels - 1 - srcx;
	int xbeforelast2 = sline_pixels - 2 - srcx;

	for (int y = 0; y < srch; y++) {
		Source_pixel *bP = srcPtr;
		Dest_pixel *dP = dstPtr;

		for (int x = 0; x < srcw; x++) {
			Source_pixel color4;
			Source_pixel color5;
			Source_pixel color6;
			Source_pixel color1;
			Source_pixel color2;
			Source_pixel color3;
			Source_pixel /*colorA0,*/ colorA1;
			Source_pixel /*colorA0,*/ colorA2;
			Source_pixel /*colorA0,*/ /*colorA3,*/
			             /*colorB0,*/ colorB1;
			Source_pixel /*colorA0,*/ colorB2;
			Source_pixel /*colorA0,*/ /*colorB3,*/
			             colorS1;
			Source_pixel /*colorA0,*/ colorS2;
			Dest_pixel product1a;
			Dest_pixel product1b;
			Dest_pixel product2a;
			Dest_pixel product2b;

			//---------------------------------------  B0 B1 B2 B3
			//                                         4  5  6  S2
			//                                         1  2  3  S1
			//                                         A0 A1 A2 A3
			//--------------------------------------
			int add1;
			int add2;
			int sub1;
			int nextl1;
			int nextl2;
			int prevl1;

			if (x == 0)
				sub1 = 0;
			else
				sub1 = 1;

			if (x >= xbeforelast2)
				add2 = 0;
			else add2 = 1;

			if (x >= xbeforelast1)
				add1 = 0;
			else add1 = 1;

			if (y == 0)
				prevl1 = 0;
			else
				prevl1 = sline_pixels;

			if (y >= ybeforelast2)
				nextl2 = 0;
			else nextl2 = sline_pixels;

			if (y >= ybeforelast1)
				nextl1 = 0;
			else nextl1 = sline_pixels;


			//colorB0 = *(bP- prevl1 - sub1);
			colorB1 = *(bP - prevl1);
			colorB2 = *(bP - prevl1 + add1);
			//colorB3 = *(bP- prevl1 + add1 + add2);

			color4 = *(bP - sub1);
			color5 = *(bP);
			color6 = *(bP + add1);
			colorS2 = *(bP + add1 + add2);

			color1 = *(bP + nextl1 - sub1);
			color2 = *(bP + nextl1);
			color3 = *(bP + nextl1 + add1);
			colorS1 = *(bP + nextl1 + add1 + add2);

			//colorA0 = *(bP + nextl1 + nextl2 - sub1);
			colorA1 = *(bP + nextl1 + nextl2);
			colorA2 = *(bP + nextl1 + nextl2 + add1);
			//colorA3 = *(bP + nextl1 + nextl2 + add1 + add2);


			if (color2 == color6 && color5 != color3) {
				//product1b = product2a = color2;
				manip.copy(product2a, color2);
				product1b = product2a;


				if ((color1 == color2) || (color6 == colorB2)) {
					//product1a = INTERPOLATE (color2, color5);
					//product1a = INTERPOLATE (color2, product1a);
					product1a = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color2, color2, color5, manip);

				} else {
					//product1a = INTERPOLATE (color5, color6);
					product1a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color6, color5, manip);
				}

				if ((color6 == colorS2) || (color2 == colorA1)) {
					//product2b = INTERPOLATE (color2, color3);
					//product2b = INTERPOLATE (color2, product2b);
					product2b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color2, color2, color3, manip);

				} else {
					//product2b = INTERPOLATE (color2, color3);
					product2b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color3, manip);
				}
			} else if (color5 == color3 && color2 != color6) {
				//product2b = product1a = color5;
				manip.copy(product1a, color5);
				product2b = product1a;


				if ((colorB1 == color5) || (color3 == colorS1)) {
					//product1b = INTERPOLATE (color5, color6);
					//product1b = INTERPOLATE (color5, product1b);
					product1b = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color5, color5, color6, manip);
				} else {
					//product1b = INTERPOLATE (color5, color6);
					product1b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, manip);
				}

				if ((color3 == colorA2) || (color4 == color5)) {
					//product2a = INTERPOLATE (color5, color2);
					//product2a = INTERPOLATE (color5, product2a);
					product2a = QInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color5, color5, color5, manip);
				} else {
					//product2a = INTERPOLATE (color2, color3);
					product2a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color3, color2, manip);
				}

			} else if (color5 == color3 && color2 == color6) {
				int r = 0;

				//r += GetResult (color6, color5, color1, colorA1);
				//r += GetResult (color6, color5, color4, colorB1);
				//r += GetResult (color6, color5, colorA2, colorS1);
				//r += GetResult (color6, color5, colorB2, colorS2);
				r += GetResult1 <Source_pixel>(color5, color6, color4, colorB1);
				r += GetResult2 <Source_pixel>(color6, color5, colorA2, colorS1);
				r += GetResult2 <Source_pixel>(color6, color5, color1, colorA1);
				r += GetResult1 <Source_pixel>(color5, color6, colorB2, colorS2);

				if (r > 0) {
					//product1b = product2a = color2;
					manip.copy(product2a, color2);
					product1b = product2a;
					//product1a = product2b = INTERPOLATE (color5, color6);
					product1a = product2b = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, manip);
				} else if (r < 0) {
					//product2b = product1a = color5;
					manip.copy(product1a, color5);
					product2b = product1a;
					//product1b = product2a = INTERPOLATE (color5, color6);
					product1b = product2a = Interpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, manip);
				} else {
					//product2b = product1a = color5;
					manip.copy(product1a, color5);
					product2b = product1a;
					//product1b = product2a = color2;
					manip.copy(product2a, color2);
					product1b = product2a;

				}
			} else {
				//product2b = product1a = INTERPOLATE (color2, color6);
				//product2b = Q_INTERPOLATE (color3, color3, color3, product2b);
				//product1a = Q_INTERPOLATE (color5, color5, color5, product1a);
				product2b = OInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color3, color2, color6, manip);
				product1a = OInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color5, color6, color2, manip);

				//product2a = product1b = INTERPOLATE (color5, color3);
				//product2a = Q_INTERPOLATE (color2, color2, color2, product2a);
				//product1b = Q_INTERPOLATE (color6, color6, color6, product1b);
				product2a = OInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color2, color5, color3, manip);
				product1b = OInterpolate_2xSaI< Source_pixel,  Dest_pixel,  Manip_pixels>(color6, color5, color3, manip);
			}

			*dP = product1a;
			*(dP + 1) = product1b;
			*(dP + dline_pixels) = product2a;
			*(dP + dline_pixels + 1) = product2b;

			bP += 1;
			dP += 2;

		}
		srcPtr += sline_pixels;
		dstPtr += 2 * dline_pixels;
	};
}



/**
 ** End of 2xSaI code
 **/

#endif

