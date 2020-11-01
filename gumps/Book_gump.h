/*
Copyright (C) 2000 The Exult Team

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

#ifndef BOOK_GUMP_H
#define BOOK_GUMP_H

#include "Text_gump.h"

/*
 *  A book shows text side-by-side.
 */
class Book_gump : public Text_gump {

public:
	Book_gump(int fnt = 4, int gump = -1);
	// Paint it and its contents.
	void paint() override;
};

#endif
