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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "shapeid.h"

#include "Configuration.h"
#include "Flex.h"
#include "U7file.h"
#include "U7fileman.h"
#include "data/exult_bg_flx.h"
#include "data/exult_si_flx.h"
#include "exceptions.h"
#include "fnames.h"
#include "fontvga.h"
#include "game.h"
#include "gamewin.h"
#include "miscinf.h"
#include "u7drag.h"
#include "utils.h"
#include "vgafile.h"

#include <fstream>
#include <memory>
#include <utility>
#include <vector>

using std::cerr;
using std::endl;
using std::make_unique;
using std::pair;
using std::string;
using std::unique_ptr;
using std::vector;

Shape_manager* Shape_manager::instance = nullptr;

/*
 *  Singletons:
 */
Game_window*     Game_singletons::gwin      = nullptr;
Game_map*        Game_singletons::gmap      = nullptr;
Effects_manager* Game_singletons::eman      = nullptr;
Shape_manager*   Game_singletons::sman      = nullptr;
Usecode_machine* Game_singletons::ucmachine = nullptr;
Game_clock*      Game_singletons::gclock    = nullptr;
Palette*         Game_singletons::pal       = nullptr;
Gump_manager*    Game_singletons::gumpman   = nullptr;
Party_manager*   Game_singletons::partyman  = nullptr;

void Game_singletons::init(Game_window* g) {
	gwin      = g;
	gmap      = g->get_map();
	eman      = g->get_effects();
	sman      = Shape_manager::get_instance();
	ucmachine = g->get_usecode();
	gclock    = g->get_clock();
	pal       = g->get_pal();
	gumpman   = g->get_gump_man();
	partyman  = g->get_party_man();
}

/*
 *  Create shape manager.
 */
Shape_manager::Shape_manager() {
	assert(instance == nullptr);
	instance = this;
}

/*
 *  Read in shape-file info.
 */

void Shape_manager::read_shape_info() {
	// Want space for extra shapes if BG multiracial enabled.
	shapes.init();
	// Read in shape information.
	shapes.read_info(Game::get_game_type(), Game::is_editing());
	// Fixup Avatar shapes (1024-1035 in default SI).
	const Shape_info& male
			= shapes.get_info(Shapeinfo_lookup::GetMaleAvShape());
	const Shape_info& female
			= shapes.get_info(Shapeinfo_lookup::GetFemaleAvShape());

	vector<Skin_data>* skins = Shapeinfo_lookup::GetSkinList();
	for (auto& skin : *skins) {
		if (skin.copy_info) {
			shapes.copy_info(skin.shape_num, skin.is_female ? female : male);
			shapes.copy_info(skin.naked_shape, skin.is_female ? female : male);
		}
	}
}

/*
 *  Load files.
 */

void Shape_manager::load() {
	// Reset all caches, just in case.
	for (auto& cache : shape_cache) {
		cache.clear();
	}

	shapes.reset_imports();

	// Determine some colors based on the default palette
	Palette pal;
	// could throw!
	pal.load(PALETTES_FLX, PATCH_PALETTES, 0);
	// Get a bright green.
	special_pixels[POISON_PIXEL] = pal.find_color(4, 63, 4);
	// Get a light gray.
	special_pixels[PROTECT_PIXEL] = pal.find_color(62, 62, 55);
	// Yellow for cursed.
	special_pixels[CURSED_PIXEL] = pal.find_color(62, 62, 5);
	// Light blue for charmed.
	special_pixels[CHARMED_PIXEL] = pal.find_color(30, 40, 63);
	// Red for hit in battle.
	special_pixels[HIT_PIXEL] = pal.find_color(63, 4, 4);
	// Purple for paralyze.
	special_pixels[PARALYZE_PIXEL] = pal.find_color(49, 27, 49);
	// Black for ShortcutBar_gump
	special_pixels[BLACK_PIXEL] = pal.find_color(0, 0, 0);

	files[SF_GUMPS_VGA].load(GUMPS_VGA, PATCH_GUMPS, true);

	if (!files[SF_PAPERDOL_VGA].load(
				*Shapeinfo_lookup::GetPaperdollSources())) {
		if (GAME_SI) {
			gwin->abort("Can't open 'paperdol.vga' file.");
		} else if (GAME_BG) {    // NOT for devel. games.
			std::cerr << "Couldn't open SI 'paperdol.vga'." << std::endl
					  << "Support for SI Paperdolls in BG is disabled."
					  << std::endl;
		}
		can_have_paperdolls = false;
	} else {
		can_have_paperdolls = true;
	}

	if (GAME_SI) {
		got_si_shapes = true;
	} else if (GAME_BG) {
		// Source for importing SI data.
		pair<string, int> source;

		vector<pair<int, int>>* imports;
		if (can_have_paperdolls) {    // Do this only if SI paperdol.vga was
									  // found.
			source = pair<string, int>(
					string("<SERPENT_STATIC>/gumps.vga"), -1);
			// Gump shapes to import from SI.
			imports = Shapeinfo_lookup::GetImportedGumpShapes();

			if (!imports->empty()) {
				can_have_paperdolls
						= files[SF_GUMPS_VGA].import_shapes(source, *imports);
			} else {
				can_have_paperdolls = false;
			}

			if (can_have_paperdolls) {
				std::cout << "Support for SI Paperdolls is enabled."
						  << std::endl;
			} else {
				std::cerr << "Couldn't open SI 'gumps.vga'." << std::endl
						  << "Support for SI Paperdolls in BG is disabled."
						  << std::endl;
			}
		}

		source = pair<string, int>(string("<SERPENT_STATIC>/shapes.vga"), -1);
		// Skin shapes to import from SI.
		imports = Shapeinfo_lookup::GetImportedSkins();
		if (!imports->empty()) {
			got_si_shapes = shapes.import_shapes(source, *imports);
		} else {
			got_si_shapes = false;
		}

		if (got_si_shapes) {
			std::cout << "Support for SI Multiracial Avatars is enabled."
					  << std::endl;
		} else {
			std::cerr << "Couldn't open SI 'shapes.vga'." << std::endl
					  << "Support for SI Multiracial Avatars is disabled."
					  << std::endl;
		}
	}

	files[SF_SPRITES_VGA].load(SPRITES_VGA, PATCH_SPRITES);
	if (GAME_SIB) {
		// Lets try to import shape 0 of sprites.vga from BG or SI.
		const pair<string, int> sourcebg(
				string("<ULTIMA7_STATIC>/sprites.vga"), -1);
		const pair<string, int> sourcesi(
				string("<SERPENT_STATIC>/sprites.vga"), -1);

		vector<pair<int, int>> imports;
		imports.emplace_back(0, 0);
		if (U7exists(sourcesi.first.c_str())) {
			files[SF_SPRITES_VGA].import_shapes(sourcesi, imports);
		} else if (U7exists(sourcebg.first.c_str())) {
			files[SF_SPRITES_VGA].import_shapes(sourcebg, imports);
		} else {
			// Create lots of pixel frames.
			Shape*        shp      = files[SF_SPRITES_VGA].new_shape(0);
			unsigned char whitepix = 118;
			for (int ii = 0; ii < 28; ii++) {
				shp->add_frame(
						std::make_unique<Shape_frame>(
								&whitepix, 1, 1, 0, 0, false),
						ii);
			}
		}
	}

	vector<pair<string, int>> source;
	source.emplace_back(FACES_VGA, -1);
	if (GAME_BG) {
		// Multiracial faces.
		const str_int_pair& resource = game->get_resource("files/mrfacesvga");
		source.emplace_back(resource.str, resource.num);
	}
	source.emplace_back(PATCH_FACES, -1);
	files[SF_FACES_VGA].load(source);

	files[SF_EXULT_FLX].load(BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX));

	const char* gamedata = game->get_resource("files/gameflx").str;
	std::cout << "Loading " << gamedata << "..." << std::endl;
	files[SF_GAME_FLX].load(gamedata);

	read_shape_info();

	fonts = make_unique<Fonts_vga_file>();
	fonts->init();

	// Get translucency tables.
	unique_ptr<unsigned char[]>
			ptr;    // We will delete THIS at the end, not blends!
	// ++++TODO: Make this file editable in ES.
	{
		const auto& blendsflexspec
				= GAME_BG ? File_spec(
									BUNDLE_CHECK(
											BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX),
									EXULT_BG_FLX_BLENDS_DAT)
						  : File_spec(
									BUNDLE_CHECK(
											BUNDLE_EXULT_SI_FLX, EXULT_SI_FLX),
									EXULT_SI_FLX_BLENDS_DAT);
		const U7multiobject in(BLENDS, blendsflexspec, PATCH_BLENDS, 0);
		size_t              len;
		ptr = in.retrieve(len);
	}
	unsigned char* blends;
	size_t         nblends;
	if (!ptr) {
		// All else failed.
		// Note: the files bundled in exult_XX.flx contain these values.
		// They are "good" enough, but there is probably room for
		// improvement.
		static unsigned char hard_blends[4 * 17]
				= {208, 216, 224, 192, 136, 44,  148, 198, 248, 252, 80,  211,
				   144, 148, 252, 247, 64,  216, 64,  201, 204, 60,  84,  140,
				   144, 40,  192, 128, 96,  40,  16,  128, 100, 108, 116, 192,
				   68,  132, 28,  128, 255, 208, 48,  64,  28,  52,  255, 128,
				   8,   68,  0,   128, 255, 8,   8,   118, 255, 244, 248, 128,
				   56,  40,  32,  128, 228, 224, 214, 82};
		blends  = hard_blends;
		nblends = 17;
	} else {
		blends  = ptr.get();
		nblends = *blends++;
	}
	xforms.resize(nblends);
	const size_t nxforms = nblends;
	// ++++TODO: Make this file editable in ES.
	if (U7exists(XFORMTBL) || U7exists(PATCH_XFORMS)) {
		// Read in translucency tables.
		U7multifile  xformfile(XFORMTBL, PATCH_XFORMS);
		const size_t nobjs = std::min(
				xformfile.number_of_objects(), nblends);    // Limit by blends.
		for (size_t i = 0; i < nobjs; i++) {
			auto ds = xformfile.retrieve(i);
			if (!ds.good()) {
				// No XForm data at all. Make this XForm into an
				// identity transformation.
				for (size_t j = 0; j < sizeof(xforms[0].colors); j++) {
					xforms[nxforms - 1 - i].colors[j] = static_cast<uint8>(j);
				}
			} else {
				ds.read(xforms[nxforms - 1 - i].colors,
						sizeof(xforms[0].colors));
			}
		}
	} else {    // Create algorithmically.
		gwin->get_pal()->load(PALETTES_FLX, PATCH_PALETTES, 0);
		for (size_t i = 0; i < nxforms; i++) {
			gwin->get_pal()->create_trans_table(
					blends[4 * i + 0] / 4, blends[4 * i + 1] / 4,
					blends[4 * i + 2] / 4, blends[4 * i + 3], xforms[i].colors);
		}
	}

	invis_xform = &xforms[nxforms - 1 - 0];    // ->entry 0.
}

// Read in files needed to display gumps.
bool Shape_manager::load_gumps_minimal() {
	U7FileManager::get_ptr()->reset();    // Cache no longer valid.
	shape_cache[SF_GUMPS_VGA].clear();
	shape_cache[SF_EXULT_FLX].clear();
	try {
		if (!files[SF_GUMPS_VGA].load(GUMPS_VGA, PATCH_GUMPS, true)) {
			std::cerr << "Couldn't open 'gumps.vga'." << std::endl;
			return false;
		}
	} catch (exult_exception& ex) {
		std::cerr << ex.what() << std::endl;
		return false;
	}

	try {
		if (!files[SF_EXULT_FLX].load(
					BUNDLE_CHECK(BUNDLE_EXULT_FLX, EXULT_FLX))) {
			std::cerr << "Couldn't open 'exult.flx'." << std::endl;
			return false;
		}
	} catch (exult_exception& ex) {
		std::cerr << ex.what() << std::endl;
		return false;
	}

	// if (fonts) delete fonts;
	// fonts = new Fonts_vga_file();
	// fonts->init();

	return true;
}

/*
 *  Reload one of the shape files (msg. from ExultStudio).
 */

void Shape_manager::reload_shapes(int shape_kind    // Type from u7drag.h.
) {
	U7FileManager::get_ptr()->reset();    // Cache no longer valid.
	shape_cache[shape_kind].clear();
	switch (shape_kind) {
	case U7_SHAPE_SHAPES:
		read_shape_info();
		// ++++Reread text?
		break;
	case U7_SHAPE_GUMPS:
		files[SF_GUMPS_VGA].load(GUMPS_VGA, PATCH_GUMPS);
		break;
	case U7_SHAPE_FONTS:
		fonts->init();
		break;
	case U7_SHAPE_FACES:
		files[SF_FACES_VGA].load(FACES_VGA, PATCH_FACES);
		break;
	case U7_SHAPE_SPRITES:
		files[SF_SPRITES_VGA].load(SPRITES_VGA, PATCH_SPRITES);
		break;
	case U7_SHAPE_PAPERDOL:
		files[SF_PAPERDOL_VGA].load(PAPERDOL, PATCH_PAPERDOL);
		break;
	default:
		cerr << "Type not supported:  " << shape_kind << endl;
		break;
	}
}

/*
 *  Just reload info. files.
 */

void Shape_manager::reload_shape_info() {
	shapes.reload_info(Game::get_game_type());
}

/*
 *  Clean up.
 */
Shape_manager::~Shape_manager() {
	assert(this == instance);
	instance = nullptr;
}

/*
 *  Text-drawing methods:
 */
int Shape_manager::paint_text_box(
		int fontnum, const char* text, int x, int y, int w, int h,
		int vert_lead, bool pbreak, bool center, int shading,
		Cursor_info* cursor) {
	if (shading >= 0) {
		gwin->get_win()->fill_translucent8(0, w, h, x, y, xforms[shading]);
	}
	return fonts->paint_text_box(
			gwin->get_win()->get_ib8(), fontnum, text, x, y, w, h, vert_lead,
			pbreak, center, cursor);
}

int Shape_manager::paint_text(
		int fontnum, const char* text, int xoff, int yoff) {
	return fonts->paint_text(
			gwin->get_win()->get_ib8(), fontnum, text, xoff, yoff);
}

int Shape_manager::paint_text(
		int fontnum, const char* text, int textlen, int xoff, int yoff) {
	return fonts->paint_text(
			gwin->get_win()->get_ib8(), fontnum, text, textlen, xoff, yoff);
}

int Shape_manager::paint_text(
		std::shared_ptr<Font> font, const char* text, int xoff, int yoff) {
	return font->paint_text(gwin->get_win()->get_ib8(), text, xoff, yoff);
}

int Shape_manager::paint_text(
		std::shared_ptr<Font> font, const char* text, int textlen, int xoff,
		int yoff) {
	return font->paint_text(
			gwin->get_win()->get_ib8(), text, textlen, xoff, yoff);
}

int Shape_manager::get_text_width(int fontnum, const char* text) {
	return fonts->get_text_width(fontnum, text);
}

int Shape_manager::get_text_width(int fontnum, const char* text, int textlen) {
	return fonts->get_text_width(fontnum, text, textlen);
}

int Shape_manager::get_text_height(int fontnum) {
	return fonts->get_text_height(fontnum);
}

int Shape_manager::get_text_baseline(int fontnum) {
	return fonts->get_text_baseline(fontnum);
}

int Shape_manager::find_cursor(
		int fontnum, const char* text, int x, int y, int w, int h, int cx,
		int cy, int vert_lead) {
	return fonts->find_cursor(fontnum, text, x, y, w, h, cx, cy, vert_lead);
}

std::shared_ptr<Font> Shape_manager::get_font(int fontnum) {
	return fonts->get_font(fontnum);
}

Shape_manager::Cached_shape Shape_manager::cache_shape(
		int shape_kind, int shapenum, int framenum) {
	Cached_shape cache{nullptr, false};
	if (framenum == -1) {
		return cache;
	}
	using cache_key = std::pair<int, int>;
	auto iter = shape_cache[shape_kind].find(cache_key(shapenum, framenum));
	if (iter != shape_cache[shape_kind].cend()) {
		return iter->second;
	}
	if (shape_kind == SF_SHAPES_VGA) {
		cache.shape     = sman->shapes.get_shape(shapenum, framenum);
		cache.has_trans = sman->shapes.get_info(shapenum).has_translucency();
	} else if (shape_kind < SF_OTHER) {
		cache.shape = sman->files[shape_kind].get_shape(shapenum, framenum);
		if (shape_kind == SF_SPRITES_VGA) {
			cache.has_trans = true;
		}
	} else {
		std::cerr << "Error! Wrong ShapeFile!" << std::endl;
		return cache;
	}
	shape_cache[shape_kind][cache_key(shapenum, framenum)] = cache;
	return cache;
}

/*
 *  Read in shape.
 */
Shape_manager::Cached_shape ShapeID::cache_shape() const {
	return sman->cache_shape(shapefile, shapenum, framenum);
}

int ShapeID::get_num_frames() const {
	if (!shapefile) {
		return sman->shapes.get_num_frames(shapenum);
	} else if (shapefile < SF_OTHER) {
		if (!sman->files[static_cast<int>(shapefile)].is_good()) {
			return 0;
		}
		return sman->files[static_cast<int>(shapefile)].get_num_frames(
				shapenum);
	}
	std::cerr << "Error! Wrong ShapeFile!" << std::endl;
	return 0;
}

bool ShapeID::is_frame_empty() const {
	auto* const shp = get_shape();
	return shp == nullptr || shp->is_empty();
}

uint8* ShapeID::Get_palette_transform_table(uint8 table[256]) const {
	uint16 xform = palette_transform & PT_xForm;
	uint16 index = palette_transform & 0xff;
	if (palette_transform & PT_RampRemap) {
		uint16 to   = palette_transform & 31;
		uint16 from = ((palette_transform & ~32768) >> 5);

		int remaps[32];

		if (from > std::size(remaps) - 1 && from != 255) {
			return nullptr;
		}
		for (unsigned int i = 0; i < std::size(remaps); i++) {
			if (from == 255) {
				remaps[i] = to;
			} else if (i == from) {
				remaps[i]     = to;
				remaps[i + 1] = -1;
				break;
			} else {
				remaps[i] = i;
			}
		}
		remaps[31] = -1;

		gwin->get_pal()->Generate_remap_xformtable(table, remaps);
	} else if (xform == 0) {
		for (int i = 0; i < 256; i++) {
			table[i] = (i + index) & 0xff;
		}
		// keep index 0 and 255 as themselves as this is more useful
		table[0]   = 0;
		table[255] = 255;
		// Bound check xform table
	} else if (xform && index < sman->get_xforms_cnt()) {
		return sman->get_xform(index).colors;
	}
	return table;
}

ImageBufferPaintable::ImageBufferPaintable() : x(0), y(0) {
	auto gwin = Game_window::get_instance();
	auto iwin = gwin->get_win();
	buffer    = iwin->create_buffer(
            iwin->get_full_width(), iwin->get_full_height());
	iwin->get(buffer.get(), x, y);
}

void ImageBufferPaintable::paint() {
	auto gwin = Game_window::get_instance();
	auto iwin = gwin->get_win();
	iwin->put(buffer.get(), x, y);
}
