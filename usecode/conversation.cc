/*
 *  Copyright (C) 2001-2025  The Exult Team
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

#include "conversation.h"

#include "Face_stats.h"
#include "Gump_manager.h"
#include "ShortcutBar_gump.h"
#include "actors.h"
#include "data/exult_bg_flx.h"
#include "effects.h"
#include "exult.h"
#include "font_map.h"
#include "game.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "miscinf.h"
#include "mouse.h"
#include "touchui.h"
#include "tqueue.h"
#include "useval.h"

using std::size_t;
using std::string;

// The conversation faces and text are drawn into an overlay layer that is
// composited on top of the scene. The layer is always laid out at a fixed
// 320x200 (the classic conversation size); that whole layout is then scaled
// onto an on-screen rectangle whose size comes from config/video/ui/size, so
// the faces, text and rectangle all scale together.
namespace {
	constexpr unsigned char conv_transparent = 255;    // See-through palette index.
	constexpr int           conv_width       = 320;    // Fixed layout resolution.
	constexpr int           conv_height      = 200;
	constexpr unsigned char conv_bg_alpha    = 176;    // Text background translucency.

	// On a transparent layer, translucent fills need a non-transparent base
	// pixel to transform; seed with 0 first, then apply the selected xform.
	void paint_conv_text_bg(int x, int y, int w, int h, int shading) {
		if (shading < 0 || w <= 0 || h <= 0) {
			return;
		}
		Game_window*   local_gwin = Game_window::get_instance();
		Shape_manager* local_sman = Shape_manager::get_instance();
		if (!local_gwin || !local_sman) {
			return;
		}
		Image_window8* win = local_gwin->get_win();
		if (!win || !win->get_ib8()) {
			return;
		}
		win->get_ib8()->fill8(0, w, h, x, y);
		win->get_ib8()->fill_translucent8(0, w, h, x, y, local_sman->get_xform(shading));
	}
}    // namespace

// TODO: show_face & show_avatar_choices seem to share code?
// TODO: show_avatar_choices shouldn't first convert to char**, probably

bool Conversation::noface = false;

/*
 *  Store information about an NPC's face and text on the screen during
 *  a conversation:
 */
class Npc_face_info {
public:
	ShapeID shape;
	int     face_num;    // NPC's face shape #.
	// int frame;
	bool text_pending;    // Text has been written, but user
	//   has not yet been prompted.
	bool     no_show_face;        // Whether this specific face should be hidden
	TileRect face_rect;           // Rectangle where face is shown.
	TileRect text_rect;           // Rectangle NPC statement is shown in.
	bool     large_face;          // Guardian, snake.
	int      last_text_height;    // Height of last text painted.
	string   cur_text;            // Current text being shown.

	Npc_face_info(ShapeID& sid, int num) : shape(sid), face_num(num), text_pending(false), no_show_face(false), large_face(false) {}
};

Conversation::~Conversation() {
	if (conv_layer >= 0) {
		gwin->destroy_layer(conv_layer);
		conv_layer = -1;
	}
	if (conv_bg_layer >= 0) {
		gwin->destroy_layer(conv_bg_layer);
		conv_bg_layer = -1;
	}
	delete[] conv_choices;
}

void Conversation::clear_answers() {
	answers.clear();
}

void Conversation::add_answer(const char* str) {
	remove_answer(str);
	string s(str);
	translate_usecode_text(s);
	answers.push_back(std::move(s));
}

/*
 *  Add an answer to the list.
 */

void Conversation::add_answer(Usecode_value& val) {
	const char* str;
	const int   size = val.get_array_size();
	if (size) {    // An array?
		for (int i = 0; i < size; i++) {
			add_answer(val.get_elem(i));
		}
	} else if ((str = val.get_str_value()) != nullptr) {
		add_answer(str);
	}
}

void Conversation::remove_answer(const char* str) {
	string s(str);
	translate_usecode_text(s);
	auto it = std::find(answers.cbegin(), answers.cend(), s);

	if (it != answers.cend()) {
		answers.erase(it);
	}
}

/*
 *  Remove an answer from the list.
 */

void Conversation::remove_answer(Usecode_value& val) {
	const char* str;
	if (val.is_array()) {
		const int size = val.get_array_size();
		for (int i = 0; i < size; i++) {
			str = val.get_elem(i).get_str_value();
			if (str) {
				remove_answer(str);
			}
		}
	} else {
		str = val.get_str_value();
		remove_answer(str);
	}
}

/*
 *  Initialize face list.
 */

void Conversation::init_faces() {
	for (Npc_face_info*& finfo : face_info) {
		delete finfo;
		finfo = nullptr;
		if (!gwin->main_actor_dont_move()) {
			if (touchui != nullptr && !gumpman->gump_mode()) {
				touchui->showGameControls();
			}
			if (!Face_stats::Visible()) {
				Face_stats::ShowGump();
			}
			if (!ShortcutBar_gump::Visible()) {
				ShortcutBar_gump::ShowGump();
			}
		}
	}
	num_faces       = 0;
	last_face_shown = -1;
	choices_active  = false;
	if (conv_layer >= 0) {    // Hide any leftover faces/text.
		gwin->layer_set_visible(conv_layer, false);
		gwin->layer_set_dirty(conv_layer);
	}
	if (conv_bg_layer >= 0) {
		gwin->layer_set_visible(conv_bg_layer, false);
		gwin->layer_set_dirty(conv_bg_layer);
	}
}

/*
 * Get the rectangle within the overlay layer used for conversation faces
 * and text. The layer is a fixed 320x200 with its own coordinate space
 * starting at (0, 0).
 */
TileRect Conversation::get_conv_rect() const {
	return TileRect(0, 0, conv_width, conv_height);
}

/*
 *  Create the fixed 320x200 overlay layer that holds the conversation
 *  faces and text.  Returns the layer handle, or -1 if it could not be
 *  created.
 */
int Conversation::get_conv_layer() {
	if (conv_layer < 0) {
		conv_layer = gwin->create_layer(conv_width, conv_height, conv_transparent);
		if (conv_layer >= 0) {
			gwin->layer_set_ui_kind(conv_layer, Image_window::UiLayerConversations);
			// Let the layer render translucent (Guardian/serpent) pixels with
			// real texture alpha. The original engine applied this
			// translucency twice for large faces - a full-screen tint plus the
			// face drawn on top - so the face body appeared stronger. There
			// is no full-screen tint here; drawing once looks too transparent
			// but the full double application looks too strong, so use the
			// midpoint between them.
			const uint32* base = sman->get_translucency_argb();
			if (base) {
				uint32 boosted[256];
				for (int i = 0; i < 256; i++) {
					const uint32 argb = base[i];
					if (argb == 0) {
						boosted[i] = 0;
						continue;
					}
					const uint32 a = (argb >> 24) & 0xff;
					// a2 = opacity if the slot were applied twice.
					const uint32 a2 = 255 - ((255 - a) * (255 - a)) / 255;
					const uint32 af = (a + a2) / 2;    // Halfway boost.
					boosted[i]      = (af << 24) | (argb & 0x00ffffffu);
				}
				gwin->layer_set_index_argb(conv_layer, boosted);
			}
		}
	}
	return conv_layer;
}

int Conversation::get_conv_bg_layer() {
	if (conv_bg_layer < 0) {
		conv_bg_layer = gwin->create_layer(conv_width, conv_height, conv_transparent, 0, -1);
		if (conv_bg_layer >= 0) {
			gwin->layer_set_ui_kind(conv_bg_layer, Image_window::UiLayerConversations);
			gwin->layer_set_alpha(conv_bg_layer, conv_bg_alpha);
		}
	}
	return conv_bg_layer;
}

/*
 *  Place the conversation layer on screen. The fixed 320x200 layout is shaped
 *  by the UI fill mode and scaled by the UI size (see compute_ui_layer_dest).
 */
void Conversation::position_conv_layer(int layer) {
	SDL_FRect fr;
	gwin->get_win()->compute_ui_layer_dest(conv_width, conv_height, fr, Image_window::UiLayerConversations);
	gwin->layer_set_dest(layer, static_cast<int>(fr.x), static_cast<int>(fr.y), static_cast<int>(fr.w), static_cast<int>(fr.h));
}

/*
 *  Repaint the conversation into the overlay layer.
 */
void Conversation::repaint_conversation() {
	render_conv_layer();
}

/*
 *  Rebuild the overlay layer from the current faces, text and (if active)
 *  Avatar choices. The layer is composited on top of the scene, so this
 *  does not need to repaint the world.
 */
void Conversation::render_conv_layer() {
	const int cl = get_conv_layer();
	if (cl < 0) {
		return;
	}
	const int      bgl  = get_conv_bg_layer();
	Image_buffer8* lbuf = gwin->get_layer_ibuf(cl);
	if (!lbuf) {
		return;
	}
	position_conv_layer(cl);
	if (bgl >= 0) {
		position_conv_layer(bgl);
		if (Image_buffer8* bbuf = gwin->get_layer_ibuf(bgl)) {
			Image_buffer8* prev_bg = gwin->push_render_target(bbuf);
			bbuf->set_clip(0, 0, static_cast<int>(bbuf->get_width()), static_cast<int>(bbuf->get_height()));
			bbuf->fill8(conv_transparent);
			if (gwin->get_text_bg() >= 0) {
				for (const Npc_face_info* finfo : face_info) {
					if (!finfo || finfo->cur_text.empty()) {
						continue;
					}
					const TileRect& box = finfo->text_rect;
					paint_conv_text_bg(box.x, box.y, box.w, box.h, gwin->get_text_bg());
				}
			}
			bbuf->clear_clip();
			gwin->pop_render_target(prev_bg);
			gwin->layer_set_dirty(bgl);
			gwin->layer_set_visible(bgl, (num_faces > 0 || choices_active) && gwin->get_text_bg() >= 0);
		}
	}
	Image_buffer8* prev = gwin->push_render_target(lbuf);
	lbuf->set_clip(0, 0, static_cast<int>(lbuf->get_width()), static_cast<int>(lbuf->get_height()));
	lbuf->fill8(conv_transparent);    // Clear to fully transparent.
	paint_faces(true);
	if (choices_active) {
		build_avatar_choices();
	}
	lbuf->clear_clip();
	gwin->pop_render_target(prev);
	gwin->layer_set_dirty(cl);
	gwin->layer_set_visible(cl, num_faces > 0 || choices_active);
}

/*
 *  Compute where the given face and its text go on the overlay layer.
 */

void Conversation::set_face_rect(Npc_face_info* info, Npc_face_info* prev) {
	const int text_height = sman->get_text_line_height(0);
	// Figure starting y-coord.
	// Get character's portrait.
	Shape_frame* face   = info->shape.get_shapenum() >= 0 ? info->shape.get_shape() : nullptr;
	int          face_w = 32;
	int          face_h = 32;
	if (face) {
		face_w = face->get_width();
		face_h = face->get_height();
	}
	info->large_face = face_w >= 119;
	// Faces are laid out in the overlay layer's own coordinate space (starting
	// at 0,0), a fixed 320x200. Large (Guardian/serpent) faces render their
	// translucent pixels with real texture alpha, so they can stay on the
	// layer too.
	const int screenw = conv_width;
	const int screenh = conv_height;
	int       startx;
	int       extraw;
	if (info->large_face) {
		startx = (screenw - face_w) / 2;
		extraw = 0;
	} else {
		startx = 8;
		extraw = 4;
	}
	int starty;
	int extrah;
	if (face_h >= 142) {
		starty = (screenh - face_h) / 2;
		extrah = 0;
	} else if (prev) {
		starty = prev->text_rect.y + prev->last_text_height;
		if (starty < prev->face_rect.y + prev->face_rect.h) {
			starty = prev->face_rect.y + prev->face_rect.h;
		}
		starty += 2 * text_height;
		if (starty + face_h > screenh - 1) {
			starty = screenh - face_h - 1;
		}
		extrah = 4;
	} else {
		starty = 1;
		extrah = 4;
	}
	info->face_rect      = gwin->clip_to_win(TileRect(startx, starty, face_w + extraw, face_h + extrah));
	const TileRect& fbox = info->face_rect;
	// This is where NPC text will go.
	info->text_rect = gwin->clip_to_win(TileRect(fbox.x + fbox.w + 3, fbox.y + 3, screenw - fbox.x - fbox.w - 6, 4 * text_height));
	// No room?  (Serpent?)
	if (info->large_face) {
		// Show in lower center.
		const int lx    = screenw / 5;
		const int ly    = 3 * (screenh / 4);
		info->text_rect = TileRect(lx, ly, screenw - (2 * lx), screenh - ly - 4);
	}
	info->last_text_height = info->text_rect.h;
}

/*
 *  Show a "face" on the screen.  Npc_text_rect is also set.
 *  If shape < 0, an empty space is shown.
 */

void Conversation::show_face(int shape, int frame, int slot) {
	ShapeID face_sid(shape, frame, SF_FACES_VGA);

	// Make sure mode is set right.
	Palette* pal = gwin->get_pal();    // Watch for weirdness (lightning).
	if (pal->get_brightness() >= 300) {
		pal->set(-1, 100);
	}

	Npc_face_info* info = nullptr;
	// See if already on screen.
	for (size_t i = 0; i < face_info.size(); i++) {
		if (face_info[i] && face_info[i]->face_num == shape) {
			info            = face_info[i];
			last_face_shown = i;
			break;
		}
	}
	if (!info) {    // New one?
		if (static_cast<unsigned>(num_faces) == face_info.size()) {
			// None free?  Steal last one.
			remove_slot_face(face_info.size() - 1);
		}
		info = new Npc_face_info(face_sid, shape);
		if (noface) {
			info->no_show_face = true;
		}
		if (slot == -1) {    // Want next one?
			slot = num_faces;
		}
		// Get last one shown.
		Npc_face_info* prev = slot ? face_info[slot - 1] : nullptr;
		last_face_shown     = slot;
		if (!face_info[slot]) {
			num_faces++;    // We're adding one (not replacing).
		} else {
			delete face_info[slot];
		}
		face_info[slot] = info;
		set_face_rect(info, prev);
	}
	repaint_conversation();    // Paint all faces.
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	if (Face_stats::Visible()) {
		Face_stats::HideGump();
	}
	if (ShortcutBar_gump::Visible()) {
		ShortcutBar_gump::HideGump();
	}
}

/*
 *  Change the frame of the face on given slot.
 */

void Conversation::change_face_frame(int frame, int slot) {
	// Make sure mode is set right.
	Palette* pal = gwin->get_pal();    // Watch for weirdness (lightning).
	if (pal->get_brightness() >= 300) {
		pal->set(-1, 100);
	}

	if (static_cast<unsigned>(slot) >= face_info.size() || !face_info[slot]) {
		return;    // Invalid slot.
	}

	last_face_shown     = slot;
	Npc_face_info* info = face_info[slot];
	// These are needed in case conversation is done.
	if (info->shape.get_shapenum() < 0 || frame > info->shape.get_num_frames()) {
		return;    // Invalid frame.
	}

	if (frame == info->shape.get_framenum()) {
		return;    // We are done here.
	}

	info->shape.set_frame(frame);
	Npc_face_info* prev = slot ? face_info[slot - 1] : nullptr;
	set_face_rect(info, prev);
	repaint_conversation();    // Repaint all faces.
}

/*
 *  Remove face from screen.
 */

void Conversation::remove_face(int shape) {
	for (size_t i = 0; i < face_info.size(); i++) {
		if (face_info[i] && face_info[i]->face_num == shape) {
			remove_slot_face(i);
			return;
		}
	}
}

/*
 *  Remove face from indicated slot (SI).
 */

void Conversation::remove_slot_face(int slot) {
	if (static_cast<unsigned>(slot) >= face_info.size() || !face_info[slot]) {
		return;    // Invalid.
	}
	Npc_face_info* info = face_info[slot];
	// These are needed in case conversation is done.
	if (info->large_face) {
		gwin->set_all_dirty();
	} else {
		gwin->add_dirty(info->face_rect);
		gwin->add_dirty(info->text_rect);
	}
	delete face_info[slot];
	face_info[slot] = nullptr;
	num_faces--;
	if (last_face_shown == slot) {    // Just in case.
		size_t j;
		for (j = face_info.size(); j > 0; j--) {
			if (face_info[j - 1]) {
				break;
			}
		}
		last_face_shown = j - 1;
		if (!gwin->main_actor_dont_move() && num_faces == 0) {
			if (touchui != nullptr) {
				touchui->showGameControls();
			}
			if (!Face_stats::Visible()) {
				Face_stats::ShowGump();
			}
			if (!ShortcutBar_gump::Visible()) {
				ShortcutBar_gump::ShowGump();
			}
		}
	}
	// Repaint the remaining faces, or hide the overlay layer if empty.
	if (num_faces > 0 || choices_active) {
		repaint_conversation();
	} else if (conv_layer >= 0) {
		gwin->layer_set_visible(conv_layer, false);
		gwin->layer_set_dirty(conv_layer);
		if (conv_bg_layer >= 0) {
			gwin->layer_set_visible(conv_bg_layer, false);
			gwin->layer_set_dirty(conv_bg_layer);
		}
	}
}

/*
 *  Show what the NPC had to say.
 */

void Conversation::show_npc_message(const char* msg) {
	if (last_face_shown <= -1 || size_t(last_face_shown) >= face_info.size() || !face_info[last_face_shown]) {
		return;
	}
	string translated(msg);
	translate_usecode_text(translated);
	msg = translated.c_str();
	// Wait for any sprite effects to finish before showing text.
	Effects_manager* eman = gwin->get_effects();
	if (eman->has_active_sprites()) {
		// Pause the queue so only 'always' entries fire (no usecode).
		const uint32 now = SDL_GetTicks();
		gwin->get_tqueue()->pause(now);
		eman->set_sprites_always(true);
		while (eman->has_active_sprites()) {
			Delay();
			const uint32 ticks = SDL_GetTicks();
			Game::set_ticks(ticks);
			gwin->get_tqueue()->activate(ticks);
			gwin->paint();
			gwin->show();
		}
		eman->set_sprites_always(false);
		gwin->get_tqueue()->resume(SDL_GetTicks());
	}
	Npc_face_info* info    = face_info[last_face_shown];
	const int      font    = info->large_face ? 7 : 0;    // Use red for Guardian, snake.
	const int      text_bg = gwin->get_text_bg();
	info->cur_text         = "";
	const TileRect& box    = info->text_rect;
	/* NOTE:  The original centers text for Guardian, snake.    */
	// Draw the text into the overlay layer (with paging).
	gwin->paint();    // Repaint world beneath (the layer composites on top).
	const int      cl   = get_conv_layer();
	const int      bgl  = get_conv_bg_layer();
	Image_buffer8* lbuf = cl >= 0 ? gwin->get_layer_ibuf(cl) : nullptr;
	Image_buffer8* bbuf = bgl >= 0 ? gwin->get_layer_ibuf(bgl) : nullptr;
	if (bgl >= 0) {
		position_conv_layer(bgl);
	}
	if (cl >= 0) {
		position_conv_layer(cl);
	}
	int height;    // Break at punctuation.
	for (;;) {
		// Draw the faces and this page of text into the overlay layer.  The
		// current face's text is (re)drawn here, so keep its stored text
		// empty while paint_faces() runs to avoid drawing it twice.
		info->cur_text = "";
		if (bbuf) {
			Image_buffer8* prev_bg = gwin->push_render_target(bbuf);
			bbuf->set_clip(0, 0, static_cast<int>(bbuf->get_width()), static_cast<int>(bbuf->get_height()));
			bbuf->fill8(conv_transparent);
			if (text_bg >= 0) {
				for (const Npc_face_info* finfo : face_info) {
					if (!finfo || finfo == info || finfo->cur_text.empty()) {
						continue;
					}
					const TileRect& fbox = finfo->text_rect;
					paint_conv_text_bg(fbox.x, fbox.y, fbox.w, fbox.h, text_bg);
				}
				paint_conv_text_bg(box.x, box.y, box.w, box.h, text_bg);
			}
			bbuf->clear_clip();
			gwin->pop_render_target(prev_bg);
			gwin->layer_set_dirty(bgl);
			gwin->layer_set_visible(bgl, text_bg >= 0);
		}
		Image_buffer8* prev = lbuf ? gwin->push_render_target(lbuf) : nullptr;
		if (lbuf) {
			lbuf->set_clip(0, 0, static_cast<int>(lbuf->get_width()), static_cast<int>(lbuf->get_height()));
			lbuf->fill8(conv_transparent);
			paint_faces(true);
		}
		height = sman->paint_text_box(font, msg, box.x, box.y, box.w, box.h, -1, true, info->large_face, -1);
		if (lbuf) {
			lbuf->clear_clip();
			gwin->pop_render_target(prev);
			gwin->layer_set_dirty(cl);
			gwin->layer_set_visible(cl, true);
		}
		if (height >= 0) {
			break;    // All of the text fit.
		}
		// More to show: display this page, wait for a click, then continue.
		info->cur_text = string(msg, -height);
		int  x;
		int  y;
		char c;
		gwin->paint();
		gwin->set_painted();
		Get_click(x, y, Mouse::hand, &c, false, this, true);
		gwin->paint();
		msg += -height;
	}
	// All fit?  Store height painted.
	info->last_text_height = height;
	info->cur_text         = msg;
	info->text_pending     = true;
	gwin->set_painted();
	//	gwin->show();
}

/*
 *  Is there NPC text that the user hasn't had a chance to read?
 */

bool Conversation::is_npc_text_pending() {
	for (const Npc_face_info* finfo : face_info) {
		if (finfo && finfo->text_pending) {
			return true;
		}
	}
	return false;
}

/*
 *  Clear text-pending flags.
 */

void Conversation::clear_text_pending() {
	for (Npc_face_info* finfo : face_info) {    // Clear 'pending' flags.
		if (finfo) {
			finfo->text_pending = false;
		}
	}
}

/*
 *  Show the Avatar's conversation choices (and face).
 */

void Conversation::show_avatar_choices(int num_choices, char** choices) {
	const bool  SI         = Game::get_game_type() == SERPENT_ISLE;
	Main_actor* main_actor = gwin->get_main_actor();
	// Everything is laid out in the overlay layer's own coordinate space.
	const TileRect sbox        = get_conv_rect();
	int            x           = 0;
	int            y           = 0;    // Keep track of coords. in box.
	const int      line_height = sman->get_text_line_height(0);
	const int      space_width = sman->get_text_width(0, " ");

	// Get main actor's portrait, checking for Petra flag.
	int shape = Shapeinfo_lookup::GetFaceReplacement(0);
	int frame = 0;

	if (shape == 0) {
		Skin_data* skin = Shapeinfo_lookup::GetSkinInfoSafe(main_actor);
		if (main_actor->get_flag(Obj_flags::tattooed)) {
			shape = skin->alter_face_shape;
			frame = skin->alter_face_frame;
		} else {
			shape = skin->face_shape;
			frame = skin->face_frame;
		}
	}

	const ShapeID face_sid(shape, frame, SF_FACES_VGA);
	Shape_frame*  face = face_sid.get_shape();
	size_t        empty;    // Find face prev. to 1st empty slot.
	for (empty = 0; empty < face_info.size(); empty++) {
		if (!face_info[empty]) {
			break;
		}
	}
	// Get last one shown.
	Npc_face_info* prev = empty ? face_info[empty - 1] : nullptr;
	int            fx   = prev ? prev->face_rect.x + prev->face_rect.w + 4 : sbox.x + 16;
	int            fy;
	if (SI) {
		if (static_cast<unsigned>(num_faces) == face_info.size()) {
			// Remove face #1 if still there.
			remove_slot_face(face_info.size() - 1);
		}
		fy = sbox.y + sbox.h - 2 - face->get_height();
		fx = sbox.x + 8;
	} else if (!prev) {
		fy = sbox.y + sbox.h - face->get_height() - 3 * line_height;
	} else {
		fy = prev->text_rect.y + prev->last_text_height;
		if (fy < prev->face_rect.y + prev->face_rect.h) {
			fy = prev->face_rect.y + prev->face_rect.h;
		}
		fy += line_height;
	}
	TileRect mbox(fx, fy, face->get_width(), face->get_height());
	mbox        = mbox.intersect(sbox);
	avatar_face = mbox;    // Repaint entire width.
	// Set to where to draw sentences.
	TileRect tbox(mbox.x + mbox.w + 8, mbox.y + 4, sbox.x + sbox.w - mbox.x - mbox.w - 16,
				  5 * line_height);    // Try 5 lines.
	tbox = tbox.intersect(sbox);
	// Draw portrait.
	sman->paint_shape(mbox.x + face->get_xleft(), mbox.y + face->get_yabove(), face);
	delete[] conv_choices;    // Set up new list of choices.
	conv_choices             = new TileRect[num_choices + 1];
	const int      text_bg   = gwin->get_text_bg();
	const int      bg_offset = (sman->get_text_height(0) - line_height) / 2;
	Image_buffer8* prev_bg   = nullptr;
	if (text_bg >= 0) {
		const int bgl = get_conv_bg_layer();
		if (bgl >= 0) {
			if (Image_buffer8* bbuf = gwin->get_layer_ibuf(bgl)) {
				prev_bg = gwin->push_render_target(bbuf);
				bbuf->set_clip(0, 0, static_cast<int>(bbuf->get_width()), static_cast<int>(bbuf->get_height()));
			}
		}
	}
	// First pass: determine positions and draw all backgrounds.
	for (int i = 0; i < num_choices; i++) {
		char text[256];
		text[0] = 127;    // A circle.
		strcpy(&text[1], choices[i]);
		const int width = sman->get_text_width(0, text);
		if (x > 0 && x + width >= tbox.w) {
			// Start a new line.
			x = 0;
			y += line_height - 1;
		}
		// Store info.
		conv_choices[i] = TileRect(tbox.x + x, tbox.y + y, width, line_height);
		conv_choices[i] = conv_choices[i].intersect(sbox);
		avatar_face     = avatar_face.add(conv_choices[i]);
		// Draw shading with line_height, shifted down to align with text.
		if (text_bg >= 0) {
			paint_conv_text_bg(tbox.x + x, tbox.y + y + bg_offset, width + space_width, line_height, text_bg);
		}
		x += width + space_width;
	}
	if (prev_bg) {
		const int bgl = get_conv_bg_layer();
		if (bgl >= 0) {
			if (Image_buffer8* bbuf = gwin->get_layer_ibuf(bgl)) {
				bbuf->clear_clip();
			}
			gwin->pop_render_target(prev_bg);
			gwin->layer_set_dirty(bgl);
			gwin->layer_set_visible(bgl, true);
		}
	}
	// Second pass: draw all text on top of backgrounds.
	for (int i = 0; i < num_choices; i++) {
		char text[256];
		text[0] = 127;    // A circle.
		strcpy(&text[1], choices[i]);
		sman->paint_text(0, text, conv_choices[i].x, conv_choices[i].y);
	}
	avatar_face.enlarge((3 * c_tilesize) / 4);    // Encloses entire area.
	avatar_face = avatar_face.intersect(sbox);
	// Terminate the list.
	conv_choices[num_choices] = TileRect(0, 0, 0, 0);
	clear_text_pending();
	gwin->set_painted();
}

void Conversation::build_avatar_choices() {
	char** result;
	size_t i;    // Blame MSVC

	result = new char*[answers.size()];
	for (i = 0; i < answers.size(); i++) {
		result[i] = new char[answers[i].size() + 1];
		strcpy(result[i], answers[i].c_str());
	}
	show_avatar_choices(answers.size(), result);
	for (i = 0; i < answers.size(); i++) {
		delete[] result[i];
	}
	delete[] result;
}

/*
 *  Show the Avatar's conversation choices.
 */

void Conversation::show_avatar_choices() {
	choices_active = true;
	repaint_conversation();
}

void Conversation::clear_avatar_choices() {
	choices_active = false;
	avatar_face.w  = 0;
	repaint_conversation();    // Repaint the faces without the choices.
}

/*
 *  User clicked during a conversation.
 *
 *  Input: x, y are in game-buffer coordinates.
 *  Output: Index (0-n) of choice, or -1 if not on a choice.
 */

int Conversation::conversation_choice(int x, int y) {
	if (conv_layer < 0) {
		return -1;
	}
	// The choices live on the overlay layer, which is scaled independently of
	// the world, so map the click through the layer's placement.
	int sx;
	int sy;
	gwin->get_win()->game_to_screen(x, y, false, sx, sy);
	int lx;
	int ly;
	if (!gwin->screen_to_layer(conv_layer, sx, sy, lx, ly)) {
		return -1;
	}
	for (int i = 0; conv_choices[i].w != 0; i++) {
		if (conv_choices[i].has_point(lx, ly)) {
			return i;
		}
	}
	return -1;
}

/*
 *  Repaint everything.
 */

void Conversation::paint() {
	repaint_conversation();
}

/*
 *  Repaint the faces into the overlay layer.  Faces are drawn without the
 *  8-bit translucency blend so translucent (Guardian/serpent) pixels keep
 *  their palette index and become real texture alpha on upload.
 */

void Conversation::paint_faces(bool text) {
	if (!num_faces) {
		return;
	}
	for (const Npc_face_info* finfo : face_info) {
		if (!finfo) {
			continue;
		}
		Shape_frame* face = finfo->face_num >= 0 ? finfo->shape.get_shape() : nullptr;

		if (face && !finfo->no_show_face) {
			const int face_xleft  = face->get_xleft();
			const int face_yabove = face->get_yabove();
			const int fx          = finfo->face_rect.x + face_xleft;
			const int fy          = finfo->face_rect.y + face_yabove;
			// Draw the raw shape (no 8-bit translucency blend): translucent
			// pixels keep their palette index and are turned into real
			// texture alpha when the layer is uploaded, so Guardian/serpent
			// faces blend correctly with the scene at a constant size.
			sman->paint_shape(fx, fy, face, false);
		}
		if (text) {    // Show text too?
			const TileRect& box = finfo->text_rect;
			// Use red for Guardian, snake.
			const int font = finfo->large_face ? 7 : 0;
			sman->paint_text_box(font, finfo->cur_text.c_str(), box.x, box.y, box.w, box.h, -1, true, finfo->large_face, -1);
		}
	}
}

/*
 *  return nr. of conversation option 'str'. -1 if not found
 */

int Conversation::locate_answer(const char* str) {
	int num = 0;
	for (auto& answer : answers) {
		if (answer == str) {
			return num;
		}
		num++;
	}

	return -1;
}

void Conversation::push_answers() {
	answer_stack.push_front(answers);
	answers.clear();
}

void Conversation::pop_answers() {
	answers = answer_stack.front();
	answer_stack.pop_front();
	gwin->paint();    // Really just need to figure tbox.
}
