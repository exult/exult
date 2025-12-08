/*
 *  Copyright (C) 2000-2024  The Exult Team
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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Audio.h"
#include "Configuration.h"
#include "File_gump.h"
#include "Gump_button.h"
#include "Yesno_gump.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "items.h"
#include "mouse.h"

#include <array>
#include <cctype>
#include <cstring>

using std::cout;
using std::endl;
using std::string;
using std::strncpy;

/*
 *  Statics:
 */
std::array<short, 2> File_gump::btn_rows{143, 156};
std::array<short, 3> File_gump::btn_cols{94, 163, 232};
short                File_gump::textx = 237;
short                File_gump::texty = 14;
short                File_gump::texth = 13;

/*
 *  Load or save button.
 */
class Load_save_button : public Gump_button {
public:
	Load_save_button(Gump* par, int px, int py, int shapenum)
			: Gump_button(par, shapenum, px, py) {}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Quit button.
 */
class Quit_button : public Gump_button {
public:
	Quit_button(Gump* par, int px, int py)
			: Gump_button(par, game->get_shape("gumps/quitbtn"), px, py) {}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Sound 'toggle' buttons.
 */
class Sound_button : public Gump_button {
public:
	Sound_button(Gump* par, int px, int py, int shapenum, bool enabled)
			: Gump_button(par, shapenum, px, py) {
		set_pushed(enabled);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Clicked a 'load' or 'save' button.
 */

bool Load_save_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}

	if (get_shapenum() == game->get_shape("gumps/loadbtn")) {
		static_cast<File_gump*>(parent)->load();
	} else {
		static_cast<File_gump*>(parent)->save();
	}

	return true;
}

/*
 *  Clicked on 'quit'.
 */

bool Quit_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	static_cast<File_gump*>(parent)->quit();
	return true;
}

/*
 *  Clicked on one of the sound options.
 */

bool Sound_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	set_pushed(static_cast<File_gump*>(parent)->toggle_option(this) != 0);
	parent->paint();
	return true;
}

/*
 *  An editable text field:
 */
class Gump_text : public Gump_widget {
	char* text;            // Holds text, 0-delimited.
	int   max_size;        // Size (max) of text.
	int   length;          // Current # chars.
	int   textx, texty;    // Where to show text rel. to parent.
	int   cursor;          // Index of char. cursor is before.
public:
	Gump_text(Gump* par, int shnum, int px, int py, int maxsz, int tx, int ty)
			: Gump_widget(par, shnum, px, py), text(new char[maxsz + 1]),
			  max_size(maxsz), length(0), textx(x + tx), texty(y + ty),
			  cursor(0) {
		text[0] = text[maxsz] = 0;
		Shape_frame* shape    = ShapeID(shnum, 0, SF_GUMPS_VGA).get_shape();
		// Want text coords. rel. to parent.
		textx -= shape->get_xleft();
		texty -= shape->get_yabove();
	}

	~Gump_text() override {
		delete[] text;
	}

	int get_length() {
		return length;
	}

	char* get_text() {
		return text;
	}

	void set_text(const char* newtxt) {    // Set text.
		strncpy(text, newtxt ? newtxt : "", max_size);
		length = strlen(text);
	}

	int get_cursor() {
		return cursor;
	}

	void set_cursor(int pos) {    // Set cursor (safely).
		if (pos >= 0 && pos <= length) {
			cursor = pos;
			refresh();
		}
	}

	void paint() override;    // Paint.
	// Handle mouse click.
	int  mouse_clicked(int mx, int my);
	void insert(int chr);    // Insert a character.
	int  delete_left();      // Delete char. to left of cursor.
	int  delete_right();     // Delete char. to right of cursor.
	void lose_focus();

protected:
	void refresh() {
		paint();
	}
};

/*
 *  Paint text field.
 */

void Gump_text::paint() {
	paint_shape(parent->get_x() + x, parent->get_y() + y);
	// Show text.
	sman->paint_text(2, text, parent->get_x() + textx, parent->get_y() + texty);
	if (get_framenum()) {    // Focused?  Show cursor.
		gwin->get_win()->fill8(
				0, 1, sman->get_text_height(2),
				parent->get_x() + textx + sman->get_text_width(2, text, cursor),
				parent->get_y() + texty + 1);
	}
	gwin->set_painted();
}

/*
 *  Handle click on text object.
 *
 *  Output: 1 if point is within text object, else 0.
 */

int Gump_text::mouse_clicked(
		int mx, int my    // Mouse position on screen.
) {
	if (!on_widget(mx, my)) {    // Not in our area?
		return 0;
	}
	mx -= textx + parent->get_x();    // Get pt. rel. to text area.
	if (!get_framenum()) {            // Gaining focus?
		set_frame(1);                 // We have focus now.
		cursor = 0;                   // Put cursor at start.
	} else {
		for (cursor = 0; cursor <= length; cursor++) {
			if (sman->get_text_width(2, text, cursor) > mx) {
				if (cursor > 0) {
					cursor--;
				}
				break;
			}
		}
		if (cursor > length) {
			cursor--;    // Passed the end.
		}
	}
	return 1;
}

/*
 *  Insert a character at the cursor.
 */

void Gump_text::insert(int chr) {
	if (!get_framenum() || length == max_size) {
		return;    // Can't.
	}
	if (cursor < length) {    // Open up space.
		memmove(text + cursor + 1, text + cursor, length - cursor);
	}
	text[cursor++] = chr;    // Store, and increment cursor.
	length++;
	text[length] = 0;
	refresh();
}

/*
 *  Delete the character to the left of the cursor.
 *
 *  Output: 1 if successful.
 */

int Gump_text::delete_left() {
	if (!get_framenum() || !cursor) {    // Can't do it.
		return 0;
	}
	if (cursor < length) {    // Shift text left.
		memmove(text + cursor - 1, text + cursor, length - cursor);
	}
	text[--length] = 0;    // 0-delimit.
	cursor--;
	refresh();
	return 1;
}

/*
 *  Delete char. to right of cursor.
 *
 *  Output: 1 if successful.
 */

int Gump_text::delete_right() {
	if (!get_framenum() || cursor == length) {
		return 0;    // Past end of text.
	}
	cursor++;                // Move right.
	return delete_left();    // Delete what was passed.
}

/*
 *  Lose focus.
 */

void Gump_text::lose_focus() {
	set_frame(0);
	refresh();
}

/*
 *  Create the load/save box.
 */

File_gump::File_gump() : Modal_gump(nullptr, game->get_shape("gumps/fileio")) {
	set_object_area(TileRect(0, 0, 0, 0), 24, 138);

	int ty = texty;
	for (size_t i = 0; i < names.size(); i++, ty += texth) {
		names[i] = new Gump_text(
				this, game->get_shape("gumps/fntext"), textx, ty, 30, 12, 2);
		names[i]->set_text(gwin->get_save_name(i).c_str());
	}
	// First row of buttons:
	buttons[0] = buttons[1] = nullptr;    // No load/save until name chosen.
	buttons[2]              = new Quit_button(this, btn_cols[2], btn_rows[0]);
	// 2nd row.
	buttons[3] = new Sound_button(
			this, btn_cols[0], btn_rows[1], game->get_shape("gumps/musicbtn"),
			Audio::get_ptr()->is_music_enabled());
	buttons[4] = new Sound_button(
			this, btn_cols[1], btn_rows[1], game->get_shape("gumps/speechbtn"),
			Audio::get_ptr()->is_speech_enabled());
	buttons[5] = new Sound_button(
			this, btn_cols[2], btn_rows[1], game->get_shape("gumps/soundbtn"),
			Audio::get_ptr()->are_effects_enabled());
}

/*
 *  Delete the load/save box.
 */

File_gump::~File_gump() {
	for (auto& name : names) {
		delete name;
	}
	for (auto& button : buttons) {
		delete button;
	}
}

/*
 *  Get the index of one of the text fields (savegame #).
 *
 *  Output: Index, or -1 if not found.
 */

int File_gump::get_save_index(Gump_text* txt) {
	for (size_t i = 0; i < names.size(); i++) {
		if (names[i] == txt) {
			return i;
		}
	}
	return -1;
}

/*
 *  Remove text focus.
 */

void File_gump::remove_focus() {
	if (!focus) {
		return;
	}
	focus->lose_focus();
	focus = nullptr;
	delete buttons[0];    // Remove load/save buttons.
	buttons[0] = nullptr;
	delete buttons[1];
	buttons[1] = nullptr;
	paint();
}

/*
 *  'Load' clicked.
 */

void File_gump::load() {
	if (!focus ||    // This would contain the name.
		!focus->get_length()) {
		return;
	}
	const int num = get_save_index(focus);    // Which one is it?
	if (num == -1) {
		return;    // Shouldn't ever happen.
	}
	if (!Yesno_gump::ask("Okay to load over your current game?")) {
		return;
	}
	gwin->restore_gamedat(num);    // Aborts if unsuccessful.
	gwin->read();                  // And read the files in.
	done     = true;
	restored = 1;
}

/*
 *  'Save' clicked.
 */

void File_gump::save() {
	if (!focus ||    // This would contain the name.
		!focus->get_length()) {
		return;
	}
	const int num = get_save_index(focus);    // Which one is it?
	if (num == -1) {
		return;    // Shouldn't ever happen.
	}
	if (!gwin->get_save_name(num).empty()) {    // Already a game in this slot?
		if (!Yesno_gump::ask("Okay to write over existing saved game?")) {
			return;
		}
	}
	gwin->write();    // First flush to 'gamedat'.
	gwin->save_gamedat(num, focus->get_text());
	cout << "Saved game #" << num << " successfully." << endl;
	remove_focus();
	gwin->got_bad_feeling(4);
}

/*
 *  'Quit' clicked.
 */

void File_gump::quit() {
	if (!Yesno_gump::ask(GumpStrings::Doyoureallywanttoquit_())) {
		return;
	}
	quitting_time = QUIT_TIME_YES;
	done          = true;
}

/*
 *  One of the option toggle buttons was pressed.
 *
 *  Output: New state of option (0 or 1).
 */

int File_gump::toggle_option(Gump_button* btn    // Button that was clicked.
) {
	if (btn == buttons[3]) {    // Music?
		const bool music = !Audio::get_ptr()->is_music_enabled();
		Audio::get_ptr()->set_music_enabled(music);
		if (!music) {    // Stop what's playing.
			Audio::get_ptr()->stop_music();
		}
		const string s = music ? "yes" : "no";
		// Write option out.
		config->set("config/audio/midi/enabled", s, true);
		return music ? 1 : 0;
	}
	if (btn == buttons[4]) {    // Speech?
		const bool speech = !Audio::get_ptr()->is_speech_enabled();
		Audio::get_ptr()->set_speech_enabled(speech);
		const string s = speech ? "yes" : "no";
		// Write option out.
		config->set("config/audio/speech/enabled", s, true);
		return speech ? 1 : 0;
	}
	if (btn == buttons[5]) {    // Sound effects?
		const bool effects = !Audio::get_ptr()->are_effects_enabled();
		Audio::get_ptr()->set_effects_enabled(effects);
		if (!effects) {    // Off?  Stop what's playing.
			Audio::get_ptr()->stop_sound_effects();
		}
		const string s = effects ? "yes" : "no";
		// Write option out.
		config->set("config/audio/effects/enabled", s, true);
		return effects ? 1 : 0;
	}
	return false;    // Shouldn't get here.
}

/*
 *  Paint on screen.
 */

void File_gump::paint() {
	Modal_gump::paint();    // Paint background
	// Paint text objects.
	for (auto& name : names) {
		if (name) {
			name->paint();
		}
	}
	for (auto& button : buttons) {
		if (button) {
			button->paint();
		}
	}
}

/*
 *  Handle mouse-down events.
 */

bool File_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	pushed      = nullptr;
	pushed_text = nullptr;
	// First try checkmark.
	Gump_button* btn = Gump::on_button(mx, my);
	if (btn) {
		pushed = btn;
	} else {    // Try buttons at bottom.
		for (auto& button : buttons) {
			if (button && button->on_button(mx, my)) {
				pushed = button;
				break;
			}
		}
	}
	if (pushed) {    // On a button?
		pushed->push(button);
		return true;
	}
	// See if on text field.
	for (auto& name : names) {
		if (name->on_widget(mx, my)) {
			pushed_text = name;
			return true;
		}
	}

	return Modal_gump::mouse_down(mx, my, button);
}

/*
 *  Handle mouse-up events.
 */

bool File_gump::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	if (pushed) {    // Pushing a button?
		pushed->unpush(button);
		if (pushed->on_button(mx, my)) {
			pushed->activate(button);
		}
		pushed = nullptr;
	}
	if (!pushed_text) {
		return Modal_gump::mouse_up(mx, my, button);
	}
	// Let text field handle it.
	if (!pushed_text->mouse_clicked(mx, my)
		|| pushed_text == focus) {    // Same field already selected?
		pushed_text->paint();
		pushed_text = nullptr;
		return true;
	}
	if (focus) {    // Another had focus.
		focus->set_text(gwin->get_save_name(get_save_index(focus)).c_str());
		focus->lose_focus();
	}
	focus       = pushed_text;    // Switch focus to new field.
	pushed_text = nullptr;
	if (focus->get_length()) {    // Need load/save buttons?
		if (!buttons[0]) {
			buttons[0] = new Load_save_button(
					this, btn_cols[0], btn_rows[0],
					game->get_shape("gumps/loadbtn"));
		}
		if (!buttons[1]) {
			buttons[1] = new Load_save_button(
					this, btn_cols[1], btn_rows[0],
					game->get_shape("gumps/savebtn"));
		}
	} else if (!focus->get_length()) {
		// No name yet.
		delete buttons[0];
		delete buttons[1];
		buttons[0] = buttons[1] = nullptr;
	}
	paint();    // Repaint.
	gwin->set_painted();

	return true;
}

/*
 *  Handle character that was typed.
 */

bool File_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	if (!focus) {    // Text field?
		return false;
	}
	switch (chr) {
	case SDLK_RETURN:    // If only 'Save', do it.
		if (!buttons[0] && buttons[1]) {
			if (buttons[1]->push(MouseButton::Left)) {
				gwin->show(true);
				buttons[1]->unpush(MouseButton::Left);
				gwin->show(true);
				buttons[1]->activate(MouseButton::Left);
			}
		}
		break;
	case SDLK_BACKSPACE:
		if (focus->delete_left()) {
			// Can't restore now.
			delete buttons[0];
			buttons[0] = nullptr;
		}
		if (!focus->get_length()) {
			// Last char.?
			delete buttons[0];
			delete buttons[1];
			buttons[0] = buttons[1] = nullptr;
			paint();
		}
		return true;
	case SDLK_DELETE:
		if (focus->delete_right()) {
			// Can't restore now.
			delete buttons[0];
			buttons[0] = nullptr;
		}
		if (!focus->get_length()) {
			// Last char.?
			delete buttons[0];
			delete buttons[1];
			buttons[0] = buttons[1] = nullptr;
			paint();
		}
		return true;
	case SDLK_LEFT:
		focus->set_cursor(focus->get_cursor() - 1);
		return true;
	case SDLK_RIGHT:
		focus->set_cursor(focus->get_cursor() + 1);
		return true;
	case SDLK_HOME:
		focus->set_cursor(0);
		return true;
	case SDLK_END:
		focus->set_cursor(focus->get_length());
		return true;
	}

	if (unicode < ' ') {
		return Modal_gump::key_down(
				chr, unicode);    // Ignore other special chars and let parent
								  // class handle them
	}
	if (unicode < 256 && isascii(unicode)) {
		const int old_length = focus->get_length();
		focus->insert(unicode);
		// Added first character?  Need
		//   'Save' button.
		if (!old_length && focus->get_length() && !buttons[1]) {
			buttons[1] = new Load_save_button(
					this, btn_cols[1], btn_rows[0],
					game->get_shape("gumps/savebtn"));
			buttons[1]->paint();
		}
		if (buttons[0]) {    // Can't load now.
			delete buttons[0];
			buttons[0] = nullptr;
			paint();
		}
		gwin->set_painted();
	}
	return true;
}
