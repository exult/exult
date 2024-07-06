/*
Copyright (C) 2000-2024 The Exult Team

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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "Gump_button.h"
#include "Gump_manager.h"
#include "Yesno_gump.h"
#include "exult.h"
#include "font.h"
#include "game.h"
#include "gamewin.h"
#include "mouse.h"
#include "touchui.h"

#include <cstring>

/*
 *  Statics:
 */

short Yesno_gump::yesx   = 63;
short Yesno_gump::yesnoy = 45;
short Yesno_gump::nox    = 84;

/*
 *  A 'yes' or 'no' button.
 */

class Yesno_button : public Gump_button {
	int isyes;    // 1 for 'yes', 0 for 'no'.
public:
	Yesno_button(Gump* par, int px, int py, int yes)
			: Gump_button(
					  par,
					  yes ? game->get_shape("gumps/yesbtn")
						  : game->get_shape("gumps/nobtn"),
					  px, py),
			  isyes(yes) {}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Handle 'yes' or 'no' button.
 */

bool Yesno_button::activate(MouseButton button) {
	if (button != MouseButton::Left) {
		return false;
	}
	static_cast<Yesno_gump*>(parent)->set_answer(isyes);
	return true;
}

/*
 *  Create a yes/no box.
 */

Yesno_gump::Yesno_gump(const std::string& txt, const char* font)
		: Modal_gump(nullptr, game->get_shape("gumps/yesnobox")), text(txt),
		  fontname(font), answer(-1) {
	set_object_area(TileRect(6, 5, 116, 32));
	add_elem(new Yesno_button(this, yesx, yesnoy, 1));
	add_elem(new Yesno_button(this, nox, yesnoy, 0));
}

/*
 *  Paint on screen.
 */

void Yesno_gump::paint() {
	// Paint the gump itself.
	paint_shape(x, y);
	paint_elems();    // Paint buttons.
	// Paint text.
	fontManager.get_font(fontname)->paint_text_box(
			gwin->get_win()->get_ib8(), text.c_str(), x + object_area.x,
			y + object_area.y, object_area.w, object_area.h, 2);
	if (touchui != nullptr) {
		touchui->showButtonControls();
	}
	gwin->set_painted();
}

/*
 *  Handle mouse-down events.
 */

bool Yesno_gump::mouse_down(
		int mx, int my, MouseButton button    // Position in window.
) {
	if (button != MouseButton::Left) {
		return Modal_gump::mouse_down(mx, my, button);
	}
	pushed = on_button(mx, my);
	if (pushed) {
		pushed->push(button);    // Show it.
		return true;
	}
	return Modal_gump::mouse_down(mx, my, button);
}

/*
 *  Handle mouse-up events.
 */

bool Yesno_gump::mouse_up(
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
		return true;
	}
	return Modal_gump::mouse_up(mx, my, button);
}

/*
 *  Handle ASCII character typed.
 */

bool Yesno_gump::key_down(SDL_Keycode chr, SDL_Keycode unicode) {
	ignore_unused_variable_warning(unicode);
	if (chr == SDLK_Y || chr == SDLK_RETURN) {
		set_answer(1);
	} else if (chr == SDLK_N || chr == SDLK_ESCAPE) {
		set_answer(0);
	}
	return true;
}

/*
 *  Get an answer to a question.
 *
 *  Output: 1 if yes, 0 if no or ESC.
 */

bool Yesno_gump::ask(
		const char* txt,    // What to ask.
		Paintable* paint, const char* font) {
	// make sure gumps are loaded
	auto sm = Shape_manager::get_instance();
	if (!sm->are_gumps_loaded())
		sm->load_gumps_minimal();
	Yesno_gump dlg(txt, font);
	bool       answer;
	if (!gumpman->do_modal_gump(&dlg, Mouse::hand, paint)) {
		answer = false;
	} else {
		answer = dlg.get_answer();
	}
	if (touchui != nullptr && gumpman->gump_mode()) {
		touchui->hideButtonControls();
	}
	return answer;
}

Countdown_gump::Countdown_gump(
		const std::string& txt, int timeout, const char* font)
		: Yesno_gump(std::string(), font), text_fmt(txt), timer(timeout) {
	answer     = false;
	start_time = SDL_GetTicks();
}

bool Countdown_gump::run() {
	const int elapsed   = SDL_GetTicks() - start_time;
	const int remaining = timer * 1000 - elapsed;

	if (remaining <= 0) {
		set_answer(0);
	}

	char* new_text = new char[text_fmt.size() + 32];
	snprintf(
			new_text, text_fmt.size() + 32, "%s %i...", text_fmt.c_str(),
			remaining / 1000);
	text = new_text;
	delete[] new_text;

	return true;
}

bool Countdown_gump::ask(const char* txt, int timeout, const char* font) {
	Countdown_gump dlg(txt, timeout, font);
	bool           answer;
	if (!gumpman->do_modal_gump(&dlg, Mouse::hand)) {
		answer = false;
	} else {
		answer = dlg.get_answer();
	}
	if (touchui != nullptr && gumpman->gump_mode()) {
		touchui->hideButtonControls();
	}
	return answer;
}
