/**
 ** Fontvga.cc - Handle the 'fonts.vga' file and text rendering.
 **
 ** Written: 4/29/99 - JSF
 **/
/*
Copyright (C) 1999  Jeffrey S. Freedman
Copyright (C) 2000-2022  The Exult Team

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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "fontvga.h"

#include "Configuration.h"
#include "Flex.h"
#include "game.h"
#include "istring.h"

#include <array>

extern Configuration* config;

// using std::string;

/*
 *  Fonts in 'fonts.vga':
 *
 *  0 = Normal yellow.
 *  1 = Large runes.
 *  2 = small black (as in zstats).
 *  3 = runes.
 *  4 = tiny black, used in books.
 *  5 = little white, glowing, for spellbooks.
 *  6 = runes.
 *  7 = normal red.
 *  8 = Serpentine (books)
 *  9 = Serpentine (signs)
 *  10 = Serpentine (gold signs)
 */

/*
 *  Horizontal leads, by fontnum:
 *
 *  This must include the Endgame fonts (currently 32-35)!!
 *      And the MAINSHP font (36)
 *  However, their values are set elsewhere
 */
// +TODO: This shouldn't be hard-coded.
constexpr static const std::array hlead{-2, -1, 0, -1, 0, 0, -1, -2, -1, -1};

/*
 *  Default vertical leads, by fontnum.
 */
static const std::array default_vlead{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*
 *  Initialize.
 */

void Fonts_vga_file::init(
		const File_spec& font_source, const File_spec& font_patch) {
	// The built-in original or the original German/French BG fonts
	// have taller special letters that need tighter line spacing,
	// so we adjust the vertical leads for those fonts.
	auto        vlead = default_vlead;
	std::string font_config;
	config->value("config/gameplay/fonts", font_config, "original");
	Pentagram::tolower(font_config);
	if (font_config == "original"
		|| (font_config == "disabled" && Game::get_game_type() == BLACK_GATE
			&& (Game::get_game_language() == Game_Language::GERMAN
				|| Game::get_game_language() == Game_Language::FRENCH))) {
		vlead[0] = -5;
		vlead[7] = -5;
	}

	FlexFile     sfonts(font_source);
	FlexFile     pfonts(font_patch);
	const size_t sn       = sfonts.number_of_objects();
	const size_t pn       = pfonts.number_of_objects();
	const size_t numfonts = std::max(sn, pn);
	fonts.resize(numfonts);

	for (size_t i = 0; i < numfonts; i++) {
		fonts[i] = std::make_shared<Font>(
				font_source, font_patch, i, i < hlead.size() ? hlead[i] : 0,
				i < vlead.size() ? vlead[i] : 0);
	}
}
