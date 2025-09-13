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
#include "ShortcutBar_gump.h"
#include "actors.h"
#include "data/exult_bg_flx.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "miscinf.h"
#include "mouse.h"
#include "touchui.h"
#include "useval.h"

using std::size_t;
using std::string;

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

	Npc_face_info(ShapeID& sid, int num)
			: shape(sid), face_num(num), text_pending(false),
			  no_show_face(false), large_face(false) {}
};

Conversation::~Conversation() {
	delete[] conv_choices;
}

void Conversation::clear_answers() {
	answers.clear();
}

void Conversation::add_answer(const char* str) {
	remove_answer(str);
	const string s(str);
	answers.push_back(s);
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
	auto it = std::find(answers.cbegin(), answers.cend(), str);

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
	num_faces       = 0;
	last_face_shown = -1;
}

void Conversation::set_face_rect(
		Npc_face_info* info, Npc_face_info* prev, int screenw, int screenh) {
	const int text_height = sman->get_text_height(0);
	// Figure starting y-coord.
	// Get character's portrait.
	Shape_frame* face   = info->shape.get_shapenum() >= 0
								  ? info->shape.get_shape()
								  : nullptr;
	int          face_w = 32;
	int          face_h = 32;
	if (face) {
		face_w = face->get_width();
		face_h = face->get_height();
	}
	int startx;
	int extraw;
	if (face_w >= 119) {
		startx           = (screenw - face_w) / 2;
		extraw           = 0;
		info->large_face = true;
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
	info->face_rect = gwin->clip_to_win(
			TileRect(startx, starty, face_w + extraw, face_h + extrah));
	const TileRect& fbox = info->face_rect;
	// This is where NPC text will go.
	info->text_rect = gwin->clip_to_win(TileRect(
			fbox.x + fbox.w + 3, fbox.y + 3, screenw - fbox.x - fbox.w - 6,
			4 * text_height));
	// No room?  (Serpent?)
	if (info->large_face) {
		// Show in lower center.
		const int x     = screenw / 5;
		const int y     = 3 * (screenh / 4);
		info->text_rect = TileRect(x, y, screenw - (2 * x), screenh - y - 4);
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

	// Get screen dims.
	const int      screenw = gwin->get_width();
	const int      screenh = gwin->get_height();
	Npc_face_info* info    = nullptr;
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
		set_face_rect(info, prev, screenw, screenh);
	}
	gwin->get_win()->set_clip(0, 0, screenw, screenh);
	paint_faces();    // Paint all faces.
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	if (Face_stats::Visible()) {
		Face_stats::HideGump();
	}
	if (ShortcutBar_gump::Visible()) {
		ShortcutBar_gump::HideGump();
	}
	gwin->get_win()->clear_clip();
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
	if (info->shape.get_shapenum() < 0
		|| frame > info->shape.get_num_frames()) {
		return;    // Invalid frame.
	}

	if (frame == info->shape.get_framenum()) {
		return;    // We are done here.
	}

	info->shape.set_frame(frame);
	// Get screen dims.
	const int      screenw = gwin->get_width();
	const int      screenh = gwin->get_height();
	Npc_face_info* prev    = slot ? face_info[slot - 1] : nullptr;
	set_face_rect(info, prev, screenw, screenh);

	gwin->get_win()->set_clip(0, 0, screenw, screenh);
	paint_faces();    // Paint all faces.
	gwin->get_win()->clear_clip();
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
}

/*
 *  Show what the NPC had to say.
 */

void Conversation::show_npc_message(const char* msg) {
	if (last_face_shown == -1) {
		return;
	}
	Npc_face_info* info = face_info[last_face_shown];
	const int      font
			= info->large_face ? 7 : 0;    // Use red for Guardian, snake.
	info->cur_text      = "";
	const TileRect& box = info->text_rect;
	//	gwin->paint(box);        // Clear what was there before.
	//	paint_faces();
	gwin->paint();
	int height;    // Break at punctuation.
	/* NOTE:  The original centers text for Guardian, snake.    */
	while ((height = sman->paint_text_box(
					font, msg, box.x, box.y, box.w, box.h, -1, true,
					info->large_face, gwin->get_text_bg()))
		   < 0) {
		// More to do?
		info->cur_text = string(msg, -height);
		int  x;
		int  y;
		char c;
		gwin->paint();    // Paint scenery beneath
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
	// Get screen rectangle.
	const TileRect sbox        = gwin->get_game_rect();
	int            x           = 0;
	int            y           = 0;    // Keep track of coords. in box.
	const int      height      = sman->get_text_height(0);
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
	int            fx   = prev ? prev->face_rect.x + prev->face_rect.w + 4 : 16;
	int            fy;
	if (SI) {
		if (static_cast<unsigned>(num_faces) == face_info.size()) {
			// Remove face #1 if still there.
			remove_slot_face(face_info.size() - 1);
		}
		fy = sbox.h - 2 - face->get_height();
		fx = 8;
	} else if (!prev) {
		fy = sbox.h - face->get_height() - 3 * height;
	} else {
		fy = prev->text_rect.y + prev->last_text_height;
		if (fy < prev->face_rect.y + prev->face_rect.h) {
			fy = prev->face_rect.y + prev->face_rect.h;
		}
		fy += height;
	}
	TileRect mbox(fx, fy, face->get_width(), face->get_height());
	mbox        = mbox.intersect(sbox);
	avatar_face = mbox;    // Repaint entire width.
	// Set to where to draw sentences.
	TileRect tbox(
			mbox.x + mbox.w + 8, mbox.y + 4, sbox.w - mbox.x - mbox.w - 16,
			5 * height);    // Try 5 lines.
	tbox = tbox.intersect(sbox);
	// Draw portrait.
	sman->paint_shape(
			mbox.x + face->get_xleft(), mbox.y + face->get_yabove(), face);
	delete[] conv_choices;    // Set up new list of choices.
	conv_choices = new TileRect[num_choices + 1];
	for (int i = 0; i < num_choices; i++) {
		char text[256];
		text[0] = 127;    // A circle.
		strcpy(&text[1], choices[i]);
		const int width = sman->get_text_width(0, text);
		if (x > 0 && x + width >= tbox.w) {
			// Start a new line.
			x = 0;
			y += height - 1;
		}
		// Store info.
		conv_choices[i] = TileRect(tbox.x + x, tbox.y + y, width, height);
		conv_choices[i] = conv_choices[i].intersect(sbox);
		avatar_face     = avatar_face.add(conv_choices[i]);
		sman->paint_text_box(
				0, text, tbox.x + x, tbox.y + y, width + space_width, height, 0,
				false, false, gwin->get_text_bg());
		x += width + space_width;
	}
	avatar_face.enlarge((3 * c_tilesize) / 4);    // Encloses entire area.
	avatar_face = avatar_face.intersect(sbox);
	// Terminate the list.
	conv_choices[num_choices] = TileRect(0, 0, 0, 0);
	clear_text_pending();
	gwin->set_painted();
}

void Conversation::show_avatar_choices() {
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

void Conversation::clear_avatar_choices() {
	//	gwin->paint(avatar_face);    // Paint over face and answers.
	gwin->add_dirty(avatar_face);
	avatar_face.w = 0;
}

/*
 *  User clicked during a conversation.
 *
 *  Output: Index (0-n) of choice, or -1 if not on a choice.
 */

int Conversation::conversation_choice(int x, int y) {
	int i;
	for (i = 0; conv_choices[i].w != 0 && !conv_choices[i].has_point(x, y); i++)
		;
	if (conv_choices[i].w != 0) {    // Found one?
		return i;
	} else {
		return -1;
	}
}

/*
 *  Repaint everything.
 */

void Conversation::paint() {
	paint_faces(true);
	if (avatar_face.w) {    // Choices?
		show_avatar_choices();
	}
}

/*
 *  Repaint the faces.   Assumes clip has already been set to screen.
 */

void Conversation::paint_faces(bool text    // Show text too.
) {
	if (!num_faces) {
		return;
	}
	for (const Npc_face_info* finfo : face_info) {
		if (!finfo) {
			continue;
		}
		Shape_frame* face
				= finfo->face_num >= 0 ? finfo->shape.get_shape() : nullptr;

		if (face && !finfo->no_show_face) {
			const int face_xleft  = face->get_xleft();
			const int face_yabove = face->get_yabove();
			const int fx          = finfo->face_rect.x + face_xleft;
			const int fy          = finfo->face_rect.y + face_yabove;
			if (finfo->large_face) {
				// Guardian, serpents: fill whole screen with the
				// background pixel.
				const unsigned char px      = face->get_topleft_pix();
				const int           xfstart = 0xff - sman->get_xforms_cnt();
				const int           fw      = finfo->face_rect.w;
				const int           fh      = finfo->face_rect.h;
				Image_window8*      win     = gwin->get_win();
				const int           gw      = win->get_game_width();
				const int           gh      = win->get_game_height();
				// Fill only if (a) not transparent, (b) is a translucent
				// color and (c) the face is not covering the entire screen.
				if (px >= xfstart && px <= 0xfe && (gw > fw || gh > fh)) {
					const Xform_palette& xform = sman->get_xform(px - xfstart);
					const int            gx    = win->get_start_x();
					const int            gy    = win->get_start_y();
					// Another option: 4 fills outside the face area.
					win->fill_translucent8(0, gw, gh, gx, gy, xform);
				}
			}
			// Use translucency.
			sman->paint_shape(fx, fy, face, true);
		}
		if (text) {    // Show text too?
			const TileRect& box = finfo->text_rect;
			// Use red for Guardian, snake.
			const int font = finfo->large_face ? 7 : 0;
			sman->paint_text_box(
					font, finfo->cur_text.c_str(), box.x, box.y, box.w, box.h,
					-1, true, finfo->large_face, gwin->get_text_bg());
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
