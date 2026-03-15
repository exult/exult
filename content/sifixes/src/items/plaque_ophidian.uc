/*
 *
 *  Copyright (C) 2026  The Exult Team
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

/*  
 *  2026-03-15 One of the Ophidian plaques in the original shows the spelling
 *  error "CHOAS AND ORDER". This changes just that one plaque.
 */

void shapeOphidianPlaque shape#(0x2C5) ()
{
	var quality;
	var show_text;

	quality = UI_get_item_quality(item);

	// Error in the release: "CHOAS AND ORDER"
	if (quality == 149)
	{
		show_text = ["PLACE HERE", "THE CUBES", "OF BOTH", "CHAOS AND ORDER"];
		UI_display_runes(49, show_text);
	}

	else {
		shapeOphidianPlaque.original();	}
}
