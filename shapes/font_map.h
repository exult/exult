/*
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

#ifndef INCL_FONT_MAP_H
#define INCL_FONT_MAP_H

#include <string>

// Translate UTF-8 encoded special characters to font byte positions, in place.
// If use_special_chars is true, multi-byte UTF-8 sequences are mapped to the
// matching single-byte glyph in the original/serif fonts. Otherwise they are
// mapped to ASCII fallback replacements (for the VGA font).
void translate_utf8_to_font_hex(std::string& text, bool use_special_chars);

// Register translate_utf8_to_font_hex() as the Text_msg_file_reader translator
// and trigger the initial load of font_map.txt. Call once early at startup.
void init_font_map();

#endif
