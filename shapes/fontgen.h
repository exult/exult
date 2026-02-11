/**
 ** Create RLE font shapes from a given font.
 **
 ** Written: 4/8/2002 - JSF
 **/

/*
Copyright (C) 2002-2022  The Exult Team

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

#ifndef INCL_FONTGEN
#define INCL_FONTGEN
#ifdef HAVE_FREETYPE2

class Shape;

bool Gen_font_shape(
		Shape* shape, const char* fontfile, int nframes, int pixels_ht, unsigned char fg, unsigned char bg, int shadow = -1);

#endif /* HAVE_FREETYPE2 */
#endif /* INCL_FONTGEN */
