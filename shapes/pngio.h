/**
 ** Import/export .PNG files.
 **
 ** Written: 6/9/99 - JSF
 **/

/*
Copyright (C) 2002  The Exult Team
Copyright (C) 1999  Jeffrey S. Freedman

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

#ifndef INCL_PNGIO
#define INCL_PNGIO
#ifdef HAVE_PNG_H

int Import_png8(const char *pngname, int transp_index, int &width, int &height,
                int &rowbytes, int &xoff, int &yoff, unsigned char  *&pixels,
                unsigned char  *&palette, int &pal_size);
int Export_png8(const char *pngname, int transp_index, int width, int height,
                int rowbytes, int xoff, int yoff, unsigned char *pixels,
                const unsigned char *palette, int pal_size,
                bool transp_to_0 = false);
int Import_png32(const char *pngname, int &width, int &height,
                 int &rowbytes, int &xoff, int &yoff, unsigned char  *&pixels,
                 bool bottom_first = false);
int Export_png32(const char *pngname, int width, int height,
                 int rowbytes, int xoff, int yoff, unsigned char *pixels);

#endif
#endif
