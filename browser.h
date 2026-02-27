/*
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BROWSER_H
#define BROWSER_H

#include "imagewin.h"
#include "vgafile.h"

class ShapeBrowser {
	Vga_file* shapes;
	int       current_shape;
	int       current_frame;
	int       current_file;
	int       current_palette;
	int       current_xform;
	int       num_shapes;
	int       num_frames;
	int       num_files;
	int       num_palettes;
	int       num_xforms;

public:
	ShapeBrowser();
	~ShapeBrowser();
	void browse_shapes();
	bool get_shape(int& shape, int& frame);
};

#endif
