/*
 *
 *  Copyright (C) 2006  The Exult Team
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

enum party_members
{                                              //FE9B
	PARTY				= -357,	//Used by several intrinsics (e.g. UI_count_objects) that would otherwise take a single NPC
								//Not supported by several other intrinsics that you'd really like it to (e.g. UI_get_cont_items)
	AVATAR				= -356 //FE9C
};
