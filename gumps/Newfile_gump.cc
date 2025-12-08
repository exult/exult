/*
 *  Copyright (C) 2001-2024  The Exult Team
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

#include "Newfile_gump.h"

#include "Audio.h"
#include "Configuration.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Text_button.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "exult.h"
#include "exult_flx.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "items.h"
#include "listfiles.h"
#include "miscinf.h"
#include "mouse.h"
#include "party.h"
#include "touchui.h"

#include <cctype>
#include <cstring>
#include <ctime>

using std::atoi;
using std::cout;
using std::endl;
using std::isdigit;
using std::localtime;
using std::qsort;
using std::string;
using std::strncpy;
using std::time;
using std::time_t;
using std::tm;

class Strings : public GumpStrings {
public:
	static auto Avatar() {
		return get_text_msg(0x660 - msg_file_start);
	}

	static auto Exp() {
		return get_text_msg(0x661 - msg_file_start);
	}

	static auto Hp() {
		return get_text_msg(0x662 - msg_file_start);
	}

	static auto Str() {
		return get_text_msg(0x663 - msg_file_start);
	}

	static auto Dxt() {
		return get_text_msg(0x664 - msg_file_start);
	}

	static auto Int() {
		return get_text_msg(0x665 - msg_file_start);
	}

	static auto Trn() {
		return get_text_msg(0x666 - msg_file_start);
	}

	static auto GameDay() {
		return get_text_msg(0x667 - msg_file_start);
	}

	static auto GameTime() {
		return get_text_msg(0x668 - msg_file_start);
	}

	static auto SaveCount() {
		return get_text_msg(0x669 - msg_file_start);
	}

	static auto Date() {
		return get_text_msg(0x66A - msg_file_start);
	}

	static auto Time() {
		return get_text_msg(0x66B - msg_file_start);
	}

	static auto File() {
		return get_text_msg(0x66C - msg_file_start);
	}

	static auto LOAD() {
		return get_text_msg(0x66D - msg_file_start);
	}

	static auto SAVE() {
		return get_text_msg(0x66E - msg_file_start);
	}
#ifdef DELETE
#	undef DELETE
#endif
	static auto DELETE() {
		return get_text_msg(0x66F - msg_file_start);
	}

	static auto month_Abbreviation(int month) {
		if (month < 0 || month > 11) {
			return "";
		}

		return get_text_msg(0x670 - msg_file_start + month);
	}

	static auto ordinal_numeral_suffix(int num) {
		if ((num % 10) == 1 && num != 11) {
			return get_text_msg(0x67C - msg_file_start);    // st
		} else if ((num % 10) == 2 && num != 12) {
			return get_text_msg(0x67D - msg_file_start);    // nd
		} else if ((num % 10) == 3 && num != 13) {
			return get_text_msg(0x67E - msg_file_start);    // rd
		}
		return get_text_msg(0x67F - msg_file_start);    // th
	}
};

/*
 *  Macros:
 */

/*
 *  Statics:
 */
// Button Coords
const short Newfile_gump::btn_rows[5] = {186, 2, 15, 156, 169};
const short Newfile_gump::btn_cols[5] = {2, 46, 88, 150, 209};

// Text field info
const short Newfile_gump::fieldx     = 2;      // Start Y of each field
const short Newfile_gump::fieldy     = 2;      // Start X of first
const short Newfile_gump::fieldw     = 207;    // Width of each field
const short Newfile_gump::fieldh     = 12;     // Height of each field
const short Newfile_gump::fieldgap   = 1;      // Gap between fields
const short Newfile_gump::fieldcount = 14;     // Number of fields
const short Newfile_gump::textx      = 12;     // X Offset in field
const short Newfile_gump::texty      = 2;      // Y Offset in field
const short Newfile_gump::textw
		= 190;    // Maximum allowable width of text (pixels)
const short Newfile_gump::iconx = 2;    // X Offset in field
const short Newfile_gump::icony = 2;    // Y Offset in field

// Scrollbar and Slider Info
const short Newfile_gump::scrollx = 212;    // X Offset
const short Newfile_gump::scrolly = 28;     // Y Offset
const short Newfile_gump::scrollh = 129;    // Height of Scroll Bar
const short Newfile_gump::sliderw = 7;      // Width of Slider
const short Newfile_gump::sliderh = 7;      // Height of Slider

const short Newfile_gump::infox = 224;
const short Newfile_gump::infoy = 67;
const short Newfile_gump::infow = 92;
const short Newfile_gump::infoh = 79;

/*
 *  One of our buttons.
 */
using Newfile_button     = CallbackButton<Newfile_gump>;
using Newfile_Textbutton = CallbackTextButton<Newfile_gump>;

/*
 *  Create the load/save box.
 */

Newfile_gump::Newfile_gump()
		: Modal_gump(
				  nullptr, gwin->get_width() / 2 - 160,
				  gwin->get_height() / 2 - 100, EXULT_FLX_SAVEGUMP_SHP,
				  SF_EXULT_FLX) {
	set_object_area(TileRect(0, 0, 320, 200), -6, 178);    //+++++ ???

	newname[0] = 0;

	gwin->get_tqueue()->pause(SDL_GetTicks());
	back = gwin->get_win()->create_buffer(
			gwin->get_width(), gwin->get_height());
	gwin->get_win()->get(back.get(), 0, 0);

	// Cancel
	buttons[id_close] = std::make_unique<Newfile_Textbutton>(
			this, &Newfile_gump::close, Strings::CANCEL(), btn_cols[3],
			btn_rows[0], 59);

	// Scrollers.
	buttons[id_page_up] = std::make_unique<Newfile_button>(
			this, &Newfile_gump::page_up, EXULT_FLX_SAV_UPUP_SHP, btn_cols[4],
			btn_rows[1], SF_EXULT_FLX);
	buttons[id_line_up] = std::make_unique<Newfile_button>(
			this, &Newfile_gump::line_up, EXULT_FLX_SAV_UP_SHP, btn_cols[4],
			btn_rows[2], SF_EXULT_FLX);
	buttons[id_line_down] = std::make_unique<Newfile_button>(
			this, &Newfile_gump::line_down, EXULT_FLX_SAV_DOWN_SHP, btn_cols[4],
			btn_rows[3], SF_EXULT_FLX);
	buttons[id_page_down] = std::make_unique<Newfile_button>(
			this, &Newfile_gump::page_down, EXULT_FLX_SAV_DOWNDOWN_SHP,
			btn_cols[4], btn_rows[4], SF_EXULT_FLX);

	LoadSaveGameDetails();
	if (touchui != nullptr) {
		touchui->hideGameControls();
		touchui->hideButtonControls();
	}
}

/*
 *  Delete the load/save box.
 */

Newfile_gump::~Newfile_gump() {
	gwin->get_tqueue()->resume(SDL_GetTicks());
	FreeSaveGameDetails();
	if (touchui != nullptr) {
		touchui->showButtonControls();
		if (!gumpman->gump_mode()
			|| (!gumpman->modal_gump_mode()
				&& gumpman->gumps_dont_pause_game())) {
			touchui->showGameControls();
		}
	}
}

/*
 *  'Load' clicked.
 */

void Newfile_gump::load() {
	// Shouldn't ever happen.
	if (selected == -2 || selected == -3) {
		return;
	}

	// Aborts if unsuccessful.
	if (selected != -1) {
		gwin->restore_gamedat(games[selected].num);
	}

	// Read Gamedat
	gwin->read();

	// Set Done
	done     = true;
	restored = 1;

	// Reset Selection
	selected = -3;

	buttons[id_load].reset();
	buttons[id_save].reset();
	buttons[id_delete].reset();

	// Reread save game details (quick save gets overwritten)
	// FreeSaveGameDetails();
	// LoadSaveGameDetails();
	// paint();
	// gwin->set_painted();
}

/*
 *  'Save' clicked.
 */

void Newfile_gump::save() {
	// Shouldn't ever happen.
	if (!strlen(newname) || selected == -3) {
		return;
	}

	// Already a game in this slot? If so ask to delete
	if (selected != -2) {
		if (!Yesno_gump::ask("Okay to write over existing saved game?")) {
			return;
		}
	}

	// Write to gamedat
	gwin->write();

	// Now write to savegame file
	if (selected >= 0) {
		gwin->save_gamedat(games[selected].num, newname);
	} else if (selected == -2) {
		gwin->save_gamedat(first_free, newname);
	}

	cout << "Saved game #" << selected << " successfully." << endl;

	// Reset everything
	selected = -3;

	buttons[id_load].reset();
	buttons[id_save].reset();
	buttons[id_delete].reset();

	FreeSaveGameDetails();
	LoadSaveGameDetails();
	paint();
	gwin->set_painted();
	gwin->got_bad_feeling(4);
}

/*
 *  'Delete' clicked.
 */

void Newfile_gump::delete_file() {
	// Shouldn't ever happen.
	if (selected == -1 || selected == -2 || selected == -3) {
		return;
	}

	// Ask to delete
	if (!Yesno_gump::ask("Okay to delete saved game?")) {
		return;
	}

	U7remove(games[selected].filename);
	filename    = nullptr;
	is_readable = false;

	cout << "Deleted Save game #" << selected << " ("
		 << games[selected].filename << ") successfully." << endl;

	// Reset everything
	selected = -3;

	buttons[id_load].reset();
	buttons[id_save].reset();
	buttons[id_delete].reset();

	FreeSaveGameDetails();
	LoadSaveGameDetails();
	paint();
	gwin->set_painted();
}

/*
 *  Scroll Line
 */

void Newfile_gump::scroll_line(int dir) {
	list_position += dir;

	if (list_position > num_games - fieldcount) {
		list_position = num_games - fieldcount;
	}

	if (list_position < -2) {
		list_position = -2;
	}

#ifdef DEBUG
	cout << "New list position " << list_position << endl;
#endif

	paint();
	gwin->set_painted();
}

/*
 *  Scroll Page
 */

void Newfile_gump::scroll_page(int dir) {
	scroll_line(dir * fieldcount);
}

void Newfile_gump::PaintSaveName(int line) {
	const int actual_game = line + list_position;

	if (actual_game < -2 || actual_game >= num_games) {
		return;
	}

	const char* text;

	if (actual_game == -1) {
		text = "Quick Save";
	} else if (actual_game == -2 && selected != -2) {
		text = "Empty Slot";
	} else if (actual_game != selected || buttons[id_load]) {
		text = games[actual_game].savename;
	} else {
		text = newname;
	}

	sman->paint_text(
			2, text, x + fieldx + textx,
			y + fieldy + texty + line * (fieldh + fieldgap));

	// Being Edited? If so paint cursor
	if (selected == actual_game && cursor != -1) {
		gwin->get_win()->fill8(
				0, 1, sman->get_text_height(2),
				x + fieldx + textx + sman->get_text_width(2, text, cursor),
				y + fieldy + texty + line * (fieldh + fieldgap));
	}

	// If selected, show selected icon
	if (selected == actual_game) {
		ShapeID icon(EXULT_FLX_SAV_SELECTED_SHP, 0, SF_EXULT_FLX);
		icon.paint_shape(
				x + fieldx + iconx,
				y + fieldy + icony + line * (fieldh + fieldgap));
	}
}

/*
 *  Paint on screen.
 */

void Newfile_gump::paint() {
	if (!games) {
		return;    // No list, so skip out.
	}
	Modal_gump::paint();

	// Paint text objects.
	int i;

	for (i = 0; i < fieldcount; i++) {
		PaintSaveName(i);
	}

	// Paint Buttons
	for (auto& btn : buttons) {
		if (btn) {
			btn->paint();
		}
	}

	// Paint scroller

	// First thing, work out number of positions that the scroller can be in
	int num_pos = (2 + num_games) - fieldcount;
	if (num_pos < 1) {
		num_pos = 1;
	}

	// Now work out the position
	const int pos = ((scrollh - sliderh) * (list_position + 2)) / num_pos;

	ShapeID slider_shape(EXULT_FLX_SAV_SLIDER_SHP, 0, SF_EXULT_FLX);
	slider_shape.paint_shape(x + scrollx, y + scrolly + pos);

	// Now paint the savegame details
	if (screenshot) {
		sman->paint_shape(x + 222, y + 2, screenshot->get_frame(0));
	}

	// Need to ensure that the avatar's shape actually exists
	if (party && !sman->have_si_shapes()
		&& Shapeinfo_lookup::IsSkinImported(party[0].shape)) {
		// Female if odd, male if even
		if (party[0].shape % 2) {
			party[0].shape = Shapeinfo_lookup::GetFemaleAvShape();
		} else {
			party[0].shape = Shapeinfo_lookup::GetMaleAvShape();
		}
	}

	if (details && party) {
		int i;

		for (i = 0; i < 4 && i < details->party_size; i++) {
			ShapeID shape(
					party[i].shape, 16,
					static_cast<ShapeFile>(party[i].shape_file));
			shape.paint_shape(x + 249 + i * 23, y + 169);
		}

		for (i = 4; i < 8 && i < details->party_size; i++) {
			ShapeID shape(
					party[i].shape, 16,
					static_cast<ShapeFile>(party[i].shape_file));
			shape.paint_shape(x + 249 + (i - 4) * 23, y + 198);
		}

		char info[320];

		snprintf(
				info, std::size(info),
				"%s: %s\n"
				"%s: %i  %s: %i\n"
				"%s: %i  %s %i\n"
				"%s: %i  %s: %i\n"
				"\n"
				"%s: %i\n"
				"%s: %02i:%02i\n"
				"\n"
				"%s: %i\n"
				"%s: %i%s %s %04i\n"
				"%s: %02i:%02i",

				Strings::Avatar(), party[0].name, Strings::Exp(), party[0].exp,
				Strings::Hp(), party[0].health, Strings::Str(), party[0].str,
				Strings::Dxt(), party[0].dext, Strings::Int(), party[0].intel,
				Strings::Trn(), party[0].training, Strings::GameDay(),
				details->game_day, Strings::GameTime(), details->game_hour,
				details->game_minute, Strings::SaveCount(), details->save_count,
				Strings::Date(), details->real_day,
				Strings::ordinal_numeral_suffix(details->real_day),
				Strings::month_Abbreviation(details->real_month - 1),
				details->real_year, Strings::Time(), details->real_hour,
				details->real_minute);
		info[std::size(info) - 1] = 0;

		if (filename) {
			size_t cursize = strlen(info);
			int    offset  = strlen(filename);

			while (offset--) {
				if (filename[offset] == '/' || filename[offset] == '\\') {
					offset++;
					break;
				}
			}

			snprintf(
					info + cursize, std::size(info) - cursize - 1, "\n%s: %s",
					Strings::File(), filename + offset);
			info[std::size(info) - 1] = 0;
		}

		sman->paint_text_box(4, info, x + infox, y + infoy, infow, infoh);

	} else {
		if (filename) {
			char info[64];

			int offset = strlen(filename);

			while (offset--) {
				if (filename[offset] == '/' || filename[offset] == '\\') {
					offset++;
					break;
				}
			}
			snprintf(
					info, std::size(info), "\n%s: %s", Strings::File(),
					filename + offset);
			info[std::size(info) - 1] = 0;
			sman->paint_text_box(4, info, x + infox, y + infoy, infow, infoh);
		}

		if (!is_readable) {
			sman->paint_text(
					2, "Unreadable",
					x + infox
							+ (infow - sman->get_text_width(2, "Unreadable"))
									  / 2,
					y + infoy + (infoh - 18) / 2);
			sman->paint_text(
					2, "Savegame",
					x + infox
							+ (infow - sman->get_text_width(2, "Savegame")) / 2,
					y + infoy + (infoh) / 2);
		} else {
			sman->paint_text(
					4, "No Info",
					x + infox
							+ (infow - sman->get_text_width(4, "No Info")) / 2,
					y + infoy + (infoh - sman->get_text_height(4)) / 2);
		}
	}
	gwin->set_painted();
}

/*
 *  Handle mouse-down events.
 */

bool Newfile_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return false;
	}

	slide_start = -1;

	pushed = Gump::on_button(mx, my);
	// Try buttons at bottom.
	if (!pushed) {
		for (auto& btn : buttons) {
			if (btn && btn->on_button(mx, my)) {
				pushed = btn.get();
				break;
			}
		}
	}

	if (pushed) {    // On a button?
		if (!pushed->push(button)) {
			pushed = nullptr;
		}
		return true;
	}

	const int gx = mx - x;
	const int gy = my - y;

	// Check for scroller
	if (gx >= scrollx && gx < scrollx + sliderw && gy >= scrolly
		&& gy < scrolly + scrollh) {
		int num_pos = (2 + num_games) - fieldcount;
		if (num_pos < 1) {
			num_pos = 1;
		}

		// Now work out the position
		const int pos = ((scrollh - sliderh) * (list_position + 2)) / num_pos;

		// Pressed above it
		if (gy < pos + scrolly) {
			scroll_page(-1);
			paint();
			return true;
		}
		// Pressed below it
		else if (gy >= pos + scrolly + sliderh) {
			scroll_page(1);
			paint();
			return true;
		}
		// Pressed on it
		else {
			slide_start = gy;
			return true;
		}
	}

	// Now check for text fields
	if (gx < fieldx || gx >= fieldx + fieldw) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	int hit = -1;
	int i;
	for (i = 0; i < fieldcount; i++) {
		const int fy = fieldy + i * (fieldh + fieldgap);
		if (gy >= fy && gy < fy + fieldh) {
			hit = i;
			break;
		}
	}

	if (hit == -1) {
		return Modal_gump::mouse_down(mx, my, button);
	}

	last_selected = selected;
	if (hit + list_position >= num_games || hit + list_position < -2
		|| selected == hit + list_position) {
		return Modal_gump::mouse_down(mx, my, button);
	}

#ifdef DEBUG
	cout << "Hit a save game field" << endl;
#endif
	selected = hit + list_position;

	bool want_load   = true;
	bool want_delete = true;
	bool want_save   = true;

	if (selected == -2) {
		want_load   = false;
		want_delete = false;
		want_save   = false;
		screenshot  = cur_shot.get();
		details     = cur_details;
		party       = cur_party;
		newname[0]  = 0;
		cursor      = 0;
		is_readable = true;
		filename    = nullptr;
	} else if (selected == -1) {
		want_delete = false;
		screenshot  = gd_shot.get();
		details     = gd_details;
		party       = gd_party;
		strcpy(newname, "Quick Save");
		cursor      = -1;    // No cursor
		is_readable = true;
		filename    = nullptr;
	} else {
		screenshot = games[selected].screenshot.get();
		details    = games[selected].details;
		party      = games[selected].party;
		strcpy(newname, games[selected].savename);
		cursor      = static_cast<int>(strlen(newname));
		is_readable = want_load = games[selected].readable;
		filename                = games[selected].filename;
	}

	if (!buttons[id_load] && want_load) {
		buttons[id_load] = std::make_unique<Newfile_Textbutton>(
				this, &Newfile_gump::load, Strings::LOAD(), btn_cols[1],
				btn_rows[0], 39);
	} else if (buttons[id_load] && !want_load) {
		buttons[id_load].reset();
	}

	if (!buttons[id_save] && want_save) {
		buttons[id_save] = std::make_unique<Newfile_Textbutton>(
				this, &Newfile_gump::save, Strings::SAVE(), btn_cols[0],
				btn_rows[0], 40);
	} else if (buttons[id_save] && !want_save) {
		buttons[id_save].reset();
	}

	if (!buttons[id_delete] && want_delete) {
		buttons[id_delete] = std::make_unique<Newfile_Textbutton>(
				this, &Newfile_gump::delete_file, Strings::DELETE(),
				btn_cols[2], btn_rows[0], 59);
	} else if (buttons[id_delete] && !want_delete) {
		buttons[id_delete].reset();
	}

	paint();    // Repaint.
	gwin->set_painted();
	return true;
	// See if on text field.
}

/*
 *  Handle mouse-up events.
 */

bool Newfile_gump::mouse_up(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_up(mx, my, button);
	}

	slide_start = -1;
	bool result = false;
	;

	if (pushed) {    // Pushing a button?
		pushed->unpush(button);
		if (pushed->on_button(mx, my)) {
			pushed->activate(button);
		}
		pushed = nullptr;
		result = true;
	}
	if (touchui != nullptr
		&& ((selected == -2 && last_selected != -4)
			|| (selected >= 0 && selected == last_selected))) {
		touchui->promptForName(newname);
		result = true;
	}
	// reset so the prompt doesn't pop up on closing
	last_selected = -4;

	return result || Modal_gump::mouse_up(mx, my, button);
}

bool Newfile_gump::mousewheel_up(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		scroll_page(-1);
	} else {
		scroll_line(-1);
	}
	return true;
}

bool Newfile_gump::mousewheel_down(int mx, int my) {
	ignore_unused_variable_warning(mx, my);
	const SDL_Keymod mod = SDL_GetModState();
	if (mod & SDL_KMOD_ALT) {
		scroll_page(1);
	} else {
		scroll_line(1);
	}
	return true;
}

/*
 *  Mouse was dragged with left button down.
 */

bool Newfile_gump::mouse_drag(
		int mx, int my    // Where mouse is.
) {
	// If not sliding don't do anything
	if (slide_start == -1) {
		return Modal_gump::mouse_drag(mx, my);
	}

	const int gx = mx - x;
	const int gy = my - y;

	// First if the position is too far away from the slider
	// We'll put it back to the start
	int sy = gy - scrolly;
	if (gx < scrollx - 20 || gx > scrollx + sliderw + 20) {
		sy = slide_start - scrolly;
	}

	if (sy < sliderh / 2) {
		sy = sliderh / 2;
	}
	if (sy > scrollh - sliderh / 2) {
		sy = scrollh - sliderh / 2;
	}
	sy -= sliderh / 2;

	// Now work out the number of positions
	const int num_pos = (2 + num_games) - fieldcount;

	// Can't scroll if there is less than 1 pos
	if (num_pos < 1) {
		return true;
	}

	// Now work out the closest position to here position
	const int new_pos = ((sy * num_pos * 2) / (scrollh - sliderh) + 1) / 2 - 2;

	if (new_pos != list_position) {
		list_position = new_pos;
		paint();
	}
	return true;
}

bool Newfile_gump::text_input(const char* text) {
	if (cursor == -1 || strlen(text) >= MAX_SAVEGAME_NAME_LEN - 1) {
		return true;
	}
	if (strcmp(text, newname) == 0) {    // Not changed
		return true;
	}

	strcpy(newname, text);
	cursor = static_cast<int>(strlen(text));

	if (newname[id_load] && !buttons[id_save]) {
		buttons[id_save] = std::make_unique<Newfile_Textbutton>(
				this, &Newfile_gump::save, Strings::SAVE(), btn_cols[0],
				btn_rows[0], 40);
		buttons[id_save]->paint();
	}

	// Remove Load and Delete Button
	buttons[id_load].reset();
	buttons[id_delete].reset();

	screenshot = cur_shot.get();
	details    = cur_details;
	party      = cur_party;

	paint();
	gwin->set_painted();
	return true;
}

/*
 *  Handle character that was typed.
 */

bool Newfile_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	bool update_details = false;
	int  repaint        = false;

	// Are we selected on some text?
	if (selected == -3) {
		return false;
	}

	switch (chr) {
	case SDLK_RETURN:    // If only 'Save', do it.
		if (!buttons[id_load] && buttons[id_save]) {
			if (buttons[id_save]->push(MouseButton::Left)) {
				gwin->show(true);
				buttons[id_save]->unpush(MouseButton::Left);
				gwin->show(true);
				buttons[id_save]->activate(MouseButton::Left);
			}
		}
		update_details = true;
		break;

	case SDLK_BACKSPACE:
		if (BackspacePressed()) {
			// Can't restore/delete now.
			buttons[id_load].reset();
			buttons[id_delete].reset();

			// If no chars cant save either
			if (!newname[0]) {
				buttons[id_save].reset();
			}
			update_details = true;
		}
		break;

	case SDLK_DELETE:
		if (DeletePressed()) {
			// Can't restore/delete now.
			buttons[id_load].reset();
			buttons[id_delete].reset();

			// If no chars cant save either
			if (!newname[0]) {
				buttons[id_save].reset();
			}
			update_details = true;
		}
		break;

	case SDLK_LEFT:
		repaint = MoveCursor(-1);
		break;

	case SDLK_RIGHT:
		repaint = MoveCursor(1);
		break;

	case SDLK_HOME:
		repaint = MoveCursor(-MAX_SAVEGAME_NAME_LEN);
		break;

	case SDLK_END:
		repaint = MoveCursor(MAX_SAVEGAME_NAME_LEN);
		break;

	default:
		if (unicode < ' ') {
			return Modal_gump::key_down(
					chr, unicode);    // Ignore other special chars and let
									  // parent class handle them
		}

		if (unicode < 256 && isascii(unicode)) {
			if (AddCharacter(unicode)) {
				// Added first character?  Need 'Save' button.
				if (newname[0] && !buttons[id_save]) {
					buttons[id_save] = std::make_unique<Newfile_Textbutton>(
							this, &Newfile_gump::save, Strings::SAVE(),
							btn_cols[0], btn_rows[0], 40);
					buttons[id_save]->paint();
				}

				// Remove Load and Delete Button
				if (buttons[id_load] || buttons[id_delete]) {
					buttons[id_load].reset();
					buttons[id_delete].reset();
				}
				update_details = true;
			}
		}
		break;
	}

	// This sets the game details to the cur set
	if (update_details) {
		screenshot = cur_shot.get();
		details    = cur_details;
		party      = cur_party;
		repaint    = true;
	}
	if (repaint) {
		paint();
		gwin->set_painted();
	}
	return true;
}

int Newfile_gump::BackspacePressed() {
	if (cursor == -1 || cursor == 0) {
		return 0;
	}
	cursor--;
	return DeletePressed();
}

int Newfile_gump::DeletePressed() {
	if (cursor == -1 || cursor == static_cast<int>(strlen(newname))) {
		return 0;
	}
	for (unsigned i = cursor; i < strlen(newname); i++) {
		newname[i] = newname[i + 1];
	}

	return 1;
}

int Newfile_gump::MoveCursor(int count) {
	if (cursor == -1) {
		return 0;
	}

	cursor += count;
	if (cursor < 0) {
		cursor = 0;
	}
	if (cursor > static_cast<int>(strlen(newname))) {
		cursor = strlen(newname);
	}

	return 1;
}

int Newfile_gump::AddCharacter(char c) {
	if (cursor == -1 || cursor >= MAX_SAVEGAME_NAME_LEN - 1) {
		return 0;
	}

	char text[MAX_SAVEGAME_NAME_LEN];

	strcpy(text, newname);
	text[cursor + 1] = 0;
	text[cursor]     = c;
	strncpy(text + cursor + 1, newname + cursor,
			MAX_SAVEGAME_NAME_LEN - cursor - 1);

	// Now check the width of the text
	if (sman->get_text_width(2, text) >= textw) {
		return 0;
	}

	cursor++;
	strcpy(newname, text);
	return 1;
}

void Newfile_gump::LoadSaveGameDetails() {
	int i;

	// Gamedat Details
	gwin->get_saveinfo(gd_shot, gd_details, gd_party);

	// Current screenshot
	cur_shot = gwin->create_mini_screenshot();

	// Current Details
	cur_details = new SaveGame_Details{};

	gwin->get_win()->put(back.get(), 0, 0);

	if (gd_details) {
		cur_details->save_count = gd_details->save_count;
	} else {
		cur_details->save_count = 0;
	}

	cur_details->party_size = partyman->get_count() + 1;
	cur_details->game_day  = static_cast<short>(gclock->get_total_hours() / 24);
	cur_details->game_hour = gclock->get_hour();
	cur_details->game_minute = gclock->get_minute();

	const time_t t        = time(nullptr);
	tm*          timeinfo = localtime(&t);

	cur_details->real_day    = timeinfo->tm_mday;
	cur_details->real_hour   = timeinfo->tm_hour;
	cur_details->real_minute = timeinfo->tm_min;
	cur_details->real_month  = timeinfo->tm_mon + 1;
	cur_details->real_year   = timeinfo->tm_year + 1900;
	cur_details->real_second = timeinfo->tm_sec;

	// Current Party
	cur_party = new SaveGame_Party[cur_details->party_size];
	for (i = 0; i < cur_details->party_size; i++) {
		Actor* npc;
		if (i == 0) {
			npc = gwin->get_main_actor();
		} else {
			npc = gwin->get_npc(partyman->get_member(i - 1));
		}

		SaveGame_Party& current = cur_party[i];
		std::string     namestr = npc->get_npc_name();
		namestr.resize(sizeof(current.name), '\0');
		std::memmove(current.name, namestr.data(), sizeof(current.name));
		current.shape      = npc->get_shapenum();
		current.shape_file = npc->get_shapefile();

		current.dext     = npc->get_property(Actor::dexterity);
		current.str      = npc->get_property(Actor::strength);
		current.intel    = npc->get_property(Actor::intelligence);
		current.health   = npc->get_property(Actor::health);
		current.combat   = npc->get_property(Actor::combat);
		current.mana     = npc->get_property(Actor::mana);
		current.magic    = npc->get_property(Actor::magic);
		current.training = npc->get_property(Actor::training);
		current.exp      = npc->get_property(Actor::exp);
		current.food     = npc->get_property(Actor::food_level);
		current.flags    = npc->get_flags();
		current.flags2   = npc->get_flags2();
	}

	party      = cur_party;
	screenshot = cur_shot.get();
	details    = cur_details;

	// Now read save game details
	char mask[256];

	snprintf(
			mask, sizeof(mask), SAVENAME2,
			GAME_BG   ? "bg"
			: GAME_SI ? "si"
					  : "dev");

	FileList filenames;
	U7ListFiles(mask, filenames);
	num_games = filenames.size();

	games = new SaveInfo[num_games];

	// Setup basic details
	for (i = 0; i < num_games; i++) {
		games[i].filename = new char[filenames[i].length() + 1];
		strcpy(games[i].filename, filenames[i].c_str());
		games[i].SetSeqNumber();
	}

	// First sort thet games so the will be sorted by number
	// This is so I can work out the first free game
	if (num_games) {
		qsort(games, num_games, sizeof(SaveInfo), SaveInfo::CompareGames);
	}

	// Reand and cache all details
	first_free = -1;
	for (i = 0; i < num_games; i++) {
		games[i].readable = gwin->get_saveinfo(
				games[i].num, games[i].savename, games[i].screenshot,
				games[i].details, games[i].party);

		if (first_free == -1 && i != games[i].num) {
			first_free = i;
		}
	}

	if (first_free == -1) {
		first_free = num_games;
	}

	// Now sort it again, with all the details so it can be done by date
	if (num_games) {
		qsort(games, num_games, sizeof(SaveInfo), SaveInfo::CompareGames);
	}

	// We'll now output the info if debugging
#ifdef DEBUG
	cout << "Listing " << num_games << " Save games" << endl;
	for (i = 0; i < num_games; i++) {
		cout << i << " = " << games[i].num << " : " << games[i].filename
			 << " : " << games[i].savename << endl;
	}

	cout << "First Free Game " << first_free << endl;
#endif
}

void Newfile_gump::FreeSaveGameDetails() {
	cur_shot.reset();
	delete cur_details;
	cur_details = nullptr;
	delete[] cur_party;
	cur_party = nullptr;

	gd_shot.reset();
	delete gd_details;
	gd_details = nullptr;
	delete[] gd_party;
	gd_party = nullptr;

	filename = nullptr;

	// The SaveInfo struct will delete everything that it's got allocated
	// So we don't need to worry about that
	delete[] games;
	games = nullptr;
}

// Save Info Methods

// Destructor
Newfile_gump::SaveInfo::~SaveInfo() {
	delete[] filename;
	delete[] savename;
	delete details;
	delete[] party;
}

// Set Sequence Number
void Newfile_gump::SaveInfo::SetSeqNumber() {
	int i;

	for (i = strlen(filename) - 1;
		 !isdigit(static_cast<unsigned char>(filename[i])); i--)
		;
	for (; isdigit(static_cast<unsigned char>(filename[i])); i--)
		;

	num = atoi(filename + i + 1);
}

// Compare This
int Newfile_gump::SaveInfo::CompareThis(const SaveInfo* other) const {
	// Check by time first, if possible
	if (details && other->details) {
		if (details->real_year < other->details->real_year) {
			return 1;
		}
		if (details->real_year > other->details->real_year) {
			return -1;
		}

		if (details->real_month < other->details->real_month) {
			return 1;
		}
		if (details->real_month > other->details->real_month) {
			return -1;
		}

		if (details->real_day < other->details->real_day) {
			return 1;
		}
		if (details->real_day > other->details->real_day) {
			return -1;
		}

		if (details->real_hour < other->details->real_hour) {
			return 1;
		}
		if (details->real_hour > other->details->real_hour) {
			return -1;
		}

		if (details->real_minute < other->details->real_minute) {
			return 1;
		}
		if (details->real_minute > other->details->real_minute) {
			return -1;
		}

		if (details->real_second < other->details->real_second) {
			return 1;
		}
		if (details->real_second > other->details->real_second) {
			return -1;
		}
	} else if (details) {    // If the other doesn't have time we are first
		return -1;
	} else if (other->details) {    // If we don't have time we are last
		return 1;
	}

	return num - other->num;
}

// Compare Games Static
int Newfile_gump::SaveInfo::CompareGames(const void* a, const void* b) {
	return static_cast<const Newfile_gump::SaveInfo*>(a)->CompareThis(
			static_cast<const Newfile_gump::SaveInfo*>(b));
}
