/*
 *  Gump_manager.cc - Object that manages all available gumps
 *
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

#include "Gump_manager.h"

#include "Actor_gump.h"
#include "Audio.h"
#include "CombatStats_gump.h"
#include "Configuration.h"
#include "Gump.h"
#include "Jawbone_gump.h"
#include "Paperdoll_gump.h"
#include "ShortcutBar_gump.h"
#include "Slider_gump.h"
#include "Spellbook_gump.h"
#include "Stats_gump.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "jawbone.h"
#include "npcnear.h"
#include "spellbook.h"
#include "touchui.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#endif    // __GNUC__
static const SDL_MouseID EXSDL_TOUCH_MOUSEID = SDL_TOUCH_MOUSEID;
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using std::cout;
using std::endl;

Gump_manager::Gump_manager() {
	std::string str;
	config->value("config/gameplay/right_click_closes_gumps", str, "yes");
	if (str == "no") {
		right_click_close = false;
	}
	config->set("config/gameplay/right_click_closes_gumps", str, true);

	config->value("config/gameplay/gumps_dont_pause_game", str, "no");
	dont_pause_game = str == "yes";
	config->set(
			"config/gameplay/gumps_dont_pause_game",
			dont_pause_game ? "yes" : "no", true);
}

/*
 *  Showing gumps.
 */

bool Gump_manager::showing_gumps(bool no_pers) const {
	// If no gumps, or we do want to check for persistent, just check to see if
	// any exist
	if (!no_pers || !open_gumps) {
		return open_gumps != nullptr;
	}

	// If we don't want to check for persistend
	for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
		if (!gump->gump->is_persistent()) {
			return true;
		}
	}

	return false;
}

/*
 *  Find the highest gump that the mouse cursor is on.
 *
 *  Output: ->gump, or null if none.
 */

Gump* Gump_manager::find_gump(
		int x, int y,    // Pos. on screen.
		bool pers        // Persistent?
) {
	Gump_list* gmp;
	Gump*      found = nullptr;    // We want last found in chain.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		Gump* gump = gmp->gump;
		if (gump->has_point(x, y) && (pers || !gump->is_persistent())) {
			found = gump;
		}
	}
	return found;
}

/*
 *  Find gump containing a given object.
 */

Gump* Gump_manager::find_gump(const Game_object* obj) {
	// Get container object is in.
	const Game_object* owner = obj->get_owner();
	if (!owner) {
		return nullptr;
	}
	// Look for container's gump.
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_container() == owner) {
			return gmp->gump;
		}
	}

	Gump* dragged = gwin->get_dragging_gump();
	if (dragged && dragged->get_container() == owner) {
		return dragged;
	}

	return nullptr;
}

/*
 *  Find gump with a given owner & shapenum.
 */

Gump* Gump_manager::find_gump(
		const Game_object* owner,
		int                shapenum    // May be c_any_shapenum
) {
	Gump_list* gmp;    // See if already open.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_owner() == owner
			&& (shapenum == c_any_shapenum
				|| gmp->gump->get_shapenum() == shapenum)) {
			return gmp->gump;
		}
	}

	Gump* dragged = gwin->get_dragging_gump();
	if (dragged && dragged->get_owner() == owner
		&& (shapenum == c_any_shapenum
			|| dragged->get_shapenum() == shapenum)) {
		return dragged;
	}

	return nullptr;
}

/*
 *  Add a gump to the end of a chain.
 */

void Gump_manager::add_gump(Gump* gump) {
	auto* g = new Gump_list(gump);

	set_kbd_focus(gump);
	if (!open_gumps) {
		open_gumps = g;    // First one.
	} else {
		Gump_list* last = open_gumps;
		while (last->next) {
			last = last->next;
		}
		last->next = g;
	}
	if (!gump->is_persistent()) {    // Count 'gump mode' gumps.
		// And pause the game, if we want it
		non_persistent_count++;
		if (!dont_pause_game) {
			gwin->get_tqueue()->pause(Game::get_ticks());
		}
	}
}

/*
 *  Close a gump and delete it
 */

bool Gump_manager::close_gump(Gump* gump) {
	const bool ret     = remove_gump(gump);
	Gump*      dragged = gwin->get_dragging_gump();
	if (dragged == gump) {
		gwin->stop_dragging();
	}
	delete gump;
	if (touchui != nullptr && non_persistent_count == 0) {
		touchui->showGameControls();
	}
	return ret;
}

/*
 *  Remove a gump from the chain
 */

bool Gump_manager::remove_gump(Gump* gump) {
	if (gump == kbd_focus) {
		set_kbd_focus(nullptr);
	}
	if (open_gumps) {
		if (open_gumps->gump == gump) {
			Gump_list* p = open_gumps->next;
			delete open_gumps;
			open_gumps = p;
		} else {
			Gump_list* p = open_gumps;    // Find prev. to this.
			while (p->next != nullptr && p->next->gump != gump) {
				p = p->next;
			}

			if (p->next) {
				Gump_list* g = p->next->next;
				delete p->next;
				p->next = g;
			} else {
				return true;
			}
		}
		if (!gump->is_persistent()) {    // Count 'gump mode' gumps.
			// And resume queue if last.
			// Gets messed up upon 'load'.
			if (non_persistent_count > 0) {
				non_persistent_count--;
			}
			if (!dont_pause_game) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
		}
	}

	return false;
}

/*
 *  Show a gump.
 */

void Gump_manager::add_gump(
		Game_object* obj,         // Object gump represents.
		int          shapenum,    // Shape # in 'gumps.vga'.
		bool         actorgump    // If showing an actor's gump
) {
	bool paperdoll = false;

	// overide for paperdolls
	if (actorgump
		&& (sman->can_use_paperdolls() && sman->are_paperdolls_enabled())) {
		paperdoll = true;
	}

	Gump* dragged = gwin->get_dragging_gump();

	// If we are dragging the same, just return
	if (dragged && dragged->get_owner() == obj
		&& dragged->get_shapenum() == shapenum) {
		return;
	}

	static int cnt = 0;    // For staggering them.
	Gump_list* gmp;        // See if already open.
	for (gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->get_owner() == obj
			&& gmp->gump->get_shapenum() == shapenum) {
			break;
		}
	}

	if (gmp) {    // Found it?
		// Move it to end.
		Gump* gump = gmp->gump;
		if (gmp->next) {
			remove_gump(gump);
			add_gump(gump);
		} else {
			set_kbd_focus(gump);
		}
		gwin->paint();
		return;
	}

	int x = (1 + cnt) * gwin->get_width() / 10;
	int y = (1 + cnt) * gwin->get_height() / 10;

	const ShapeID s_id(shapenum, 0, paperdoll ? SF_PAPERDOL_VGA : SF_GUMPS_VGA);
	Shape_frame*  shape = s_id.get_shape();

	if (x + shape->get_xright() > gwin->get_width()
		|| y + shape->get_ybelow() > gwin->get_height()) {
		cnt = 0;
		x   = gwin->get_width() / 10;
		y   = gwin->get_width() / 10;
	}

	Gump* new_gump = nullptr;
	if (obj) {
		Actor* npc = obj->as_actor();
		if (npc && paperdoll) {
			new_gump = new Paperdoll_gump(npc, x, y, npc->get_npc_num());
		} else if (npc && actorgump) {
			new_gump = new Actor_gump(npc, x, y, shapenum);
		} else if (shapenum == game->get_shape("gumps/statsdisplay")) {
			new_gump = Stats_gump::create(obj, x, y);
		} else if (shapenum == game->get_shape("gumps/spellbook")) {
			new_gump = new Spellbook_gump(static_cast<Spellbook_object*>(obj));
		} else if (shapenum == game->get_shape("gumps/jawbone")) {
			new_gump
					= new Jawbone_gump(static_cast<Jawbone_object*>(obj), x, y);
		} else if (shapenum == game->get_shape("gumps/spell_scroll")) {
			new_gump = new Spellscroll_gump(obj);
		}
		// If we have an object, we can force a container gump.
		if (!new_gump && obj->as_container()) {
			new_gump = new Container_gump(obj->as_container(), x, y, shapenum);
		}
	} else if (
			Game::get_game_type() == SERPENT_ISLE
			&& shapenum >= game->get_shape("gumps/cstats/1")
			&& shapenum <= game->get_shape("gumps/cstats/6")) {
		new_gump = new CombatStats_gump(x, y);
	}

	if (!new_gump) {
		// We failed; so bail out (we did nothing but waste time)
		CERR("Failed to create gump: " << obj << ", " << shapenum << ", "
									   << actorgump);
		return;
	}

	// Paint new one last.
	add_gump(new_gump);
	if (touchui != nullptr && !gumps_dont_pause_game()) {
		touchui->hideGameControls();
	}
	if (++cnt == 8) {
		cnt = 0;
	}
	const int sfx = Audio::game_sfx(14);
	Audio::get_ptr()->play_sound_effect(sfx);    // The weird noise.
	gwin->paint();                               // Show everything.
}

/*
 *  End gump mode.
 */

void Gump_manager::close_all_gumps(bool pers) {
	bool removed = false;

	Gump_list* prev = nullptr;
	Gump_list* next = open_gumps;

	while (next) {    // Remove all gumps.
		Gump_list* gump = next;
		next            = gump->next;

		// Don't delete if persistant or modal.
		if ((!gump->gump->is_persistent() || pers) && !gump->gump->is_modal()) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
			if (prev) {
				prev->next = gump->next;
			} else {
				open_gumps = gump->next;
			}
			delete gump->gump;
			delete gump;
			removed = true;
		} else {
			prev = gump;
		}
	}
	non_persistent_count = 0;
	set_kbd_focus(nullptr);
	gwin->get_npc_prox()->wait(4);    // Delay "barking" for 4 secs.
	if (removed) {
		gwin->paint();
	}
	if (touchui != nullptr && !modal_gump_count && non_persistent_count == 0
		&& !gwin->is_in_exult_menu()) {
		touchui->showGameControls();
	}
}

/*
 *  Set the keyboard focus to a given gump.
 */

void Gump_manager::set_kbd_focus(Gump* gump    // May be nullptr.
) {
	if (gump && gump->can_handle_kbd()) {
		kbd_focus = gump;
	} else {
		kbd_focus = nullptr;
	}
}

/*
 *  Handle a double-click.
 */

bool Gump_manager::double_clicked(
		int x, int y,    // Coords in window.
		Game_object*& obj) {
	Gump* gump = find_gump(x, y);

	if (gump) {
		// If avatar cannot act, a double-click will only close gumps, and
		// nothing else.
		if (!gwin->main_actor_can_act()) {
			if (gwin->get_double_click_closes_gumps()) {
				gump->close();
				gwin->paint();
			}
			return true;
		}
		// Find object in gump.
		obj = gump->find_object(x, y);
		if (!obj) {    // Maybe it's a spell.
			Gump_button* btn = gump->on_button(x, y);
			if (btn) {
				btn->double_clicked(x, y);
			} else if (gwin->get_double_click_closes_gumps()) {
				gump->close();
				gwin->paint();
			}
		}
		return true;
	}

	return false;
}

/*
 *  Send kbd. event to gump that has focus.
 *  Output: true if handled here.
 */
bool Gump_manager::handle_kbd_event(void* ev) {
	return kbd_focus ? kbd_focus->handle_kbd_event(ev) : false;
}

/*
 *  Update the gumps
 */
void Gump_manager::update_gumps() {
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
		gmp->gump->update_gump();
	}
}

/*
 *  Paint the gumps
 */
void Gump_manager::paint(bool modal) {
	if (modal && background) {
		background->paint();
	}
	for (Gump_list* gmp = open_gumps; gmp; gmp = gmp->next) {
		if (gmp->gump->is_modal() == modal) {
			gmp->gump->paint();
		}
	}
}

/*
 *  Verify user wants to quit.
 *
 *  Output: true to quit.
 */
bool Gump_manager::okay_to_quit(Paintable* paint) {
	// Prevent reentering this function
	static bool inthis = false;
	if (inthis) {
		return false;
	}
	inthis = true;
	if (Yesno_gump::ask("Do you really want to quit?", paint)) {
		quitting_time = QUIT_TIME_YES;
	}
	inthis = false;
	return quitting_time != QUIT_TIME_NO;
}

static Gump_Base::MouseButton SDL_MouseButton_to_Gump(Uint8 sdlbutton) {
	switch (sdlbutton) {
	case 1:
		return Gump_Base::MouseButton::Left;
	case 2:
		return Gump_Base::MouseButton::Middle;
	case 3:
		return Gump_Base::MouseButton::Right;
	}
	return Gump_Base::MouseButton::Unknown;
}

bool Gump_manager::handle_modal_gump_event(Modal_gump* gump, SDL_Event& event) {
	//  Game_window *gwin = Game_window::get_instance();
	// int scale_factor = gwin->get_fastmouse() ? 1
	//          : gwin->get_win()->get_scale();
	static bool rightclick;

	int           gx;
	int           gy;
	SDL_Keycode   keysym_unicode = 0;
	SDL_Keycode   chr            = 0;
	SDL_Renderer* renderer
			= SDL_GetRenderer(gwin->get_win()->get_screen_window());

	switch (event.type) {
	case SDL_EVENT_FINGER_DOWN: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		if ((!Mouse::use_touch_input) && (event.tfinger.fingerID != 0)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(
				event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);

#ifdef DEBUG
		cout << "(x,y) rel. to gump is (" << (gx - gump->get_x()) << ", "
			 << (gy - gump->get_y()) << ")" << endl;
#endif
		if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
			break;
		}
		if (event.button.button == 1) {
			gump->mouse_down(
					gx, gy, SDL_MouseButton_to_Gump(event.button.button));
		} else if (event.button.button == 2) {
			if (!gump->mouse_down(
						gx, gy, SDL_MouseButton_to_Gump(event.button.button))
				&& gwin->get_mouse3rd()) {
				gump->key_down(SDLK_RETURN, SDLK_RETURN);
			}
		} else if (event.button.button == 3) {
			rightclick = true;
			gump->mouse_down(
					gx, gy, SDL_MouseButton_to_Gump(event.button.button));
		} else {
			gump->mouse_down(
					gx, gy, SDL_MouseButton_to_Gump(event.button.button));
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_UP:
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(
				event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
		if (g_shortcutBar && g_shortcutBar->handle_event(&event)) {
			break;
		}
		if (event.button.button != 3) {
			gump->mouse_up(
					gx, gy, SDL_MouseButton_to_Gump(event.button.button));
		} else if (rightclick) {
			rightclick = false;
			if (!gump->mouse_up(
						gx, gy, SDL_MouseButton_to_Gump(event.button.button))
				&& gumpman->can_right_click_close()) {
				return false;
			}
		}
		break;
	case SDL_EVENT_FINGER_MOTION: {
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(
				event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
		static int   numFingers = 0;
		SDL_Finger** fingers
				= SDL_GetTouchFingers(event.tfinger.touchID, &numFingers);
		if (fingers) {
			SDL_free(fingers);
		}
		if (numFingers > 1) {
			if (event.tfinger.dy < 0) {
				if (!gump->mouse_down(
							gx, gy,
							SDL_MouseButton_to_Gump(event.button.button))) {
					gump->mousewheel_up(
							Mouse::mouse()->get_mousex(),
							Mouse::mouse()->get_mousey());
				}
			} else if (event.tfinger.dy > 0) {
				if (!gump->mouse_down(
							gx, gy,
							SDL_MouseButton_to_Gump(event.button.button))) {
					gump->mousewheel_down(
							Mouse::mouse()->get_mousex(),
							Mouse::mouse()->get_mousey());
				}
			}
		}
		break;
	}
	// Mousewheel scrolling with SDL2.
	case SDL_EVENT_MOUSE_WHEEL: {
		if (event.wheel.y > 0) {
			gump->mousewheel_up(
					Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey());
		} else if (event.wheel.y < 0) {
			gump->mousewheel_down(
					Mouse::mouse()->get_mousex(), Mouse::mouse()->get_mousey());
		}
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		if (Mouse::use_touch_input
			&& event.motion.which != EXSDL_TOUCH_MOUSEID) {
			Mouse::use_touch_input = false;
		}
		SDL_ConvertEventToRenderCoordinates(renderer, &event);
		gwin->get_win()->screen_to_game(
				event.motion.x, event.motion.y, gwin->get_fastmouse(), gx, gy);

		Mouse::mouse()->move(gx, gy);
		Mouse::mouse_update = true;
		// Dragging with left button?
		if (event.motion.state & SDL_BUTTON_LMASK) {
			gump->mouse_drag(gx, gy);
		}
		break;
	case SDL_EVENT_QUIT:
		if (okay_to_quit()) {
			return false;
		}
		break;
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_TEXT_INPUT:
		Translate_keyboard(event, chr, keysym_unicode, true);
		{
			if ((chr == SDLK_S) && (event.key.mod & SDL_KMOD_ALT)
				&& (event.key.mod & SDL_KMOD_CTRL)) {
				make_screenshot(true);
				return true;
			}
			// Alt-x for quit
			if ((chr == SDLK_X)
				&& ((event.key.mod & SDL_KMOD_ALT)
					|| event.key.mod & SDL_KMOD_GUI)) {
				if (okay_to_quit()) {
					return false;
				}
			}

			bool handled = gump->key_down(chr, keysym_unicode);
			// we'll allow the gump to handle escape first
			// before closing the gump
			if (!handled) {
				if (chr == SDLK_ESCAPE) {
					return false;
				}
			}
		}
		break;
	default:
		if (event.type == TouchUI::eventType) {
			if (event.user.code == TouchUI::EVENT_CODE_TEXT_INPUT) {
				if (event.user.data1 != nullptr) {
					const char* text
							= static_cast<const char*>(event.user.data1);
					if (text) {
						gump->text_input(text);
					}
					free(event.user.data1);
				}
			}
		}
		break;
	}
	return true;
}

/*
 *  Handle a modal gump, like the range slider or the save box, until
 *  the gump self-destructs.
 *
 *  Output: false if user hit ESC.
 */

bool Gump_manager::do_modal_gump(
		Modal_gump*         gump,     // What the user interacts with.
		Mouse::Mouse_shapes shape,    // Mouse shape to use.
		Paintable*          paint     // Paint this over everything else.
) {
	modal_gump_count++;

	//  Game_window *gwin = Game_window::get_instance();

	// maybe make this selective? it's nice for menus, but annoying for sliders
	//  gwin->end_gump_mode();

	// Pause the game
	gwin->get_tqueue()->pause(SDL_GetTicks());

	const Mouse::Mouse_shapes saveshape = Mouse::mouse()->get_shape();
	if (shape != Mouse::dontchange) {
		Mouse::mouse()->set_shape(shape);
	}
	bool escaped = false;
	background   = dynamic_cast<BackgroundPaintable*>(paint);
	if (background) {
		paint = nullptr;
	}
	add_gump(gump);
	gump->run();
	gwin->paint();    // Show everything now.
	if (paint) {
		paint->paint();
	}
	Mouse::mouse()->show();
	gwin->show();
	if (touchui != nullptr) {
		touchui->hideGameControls();
	}
	do {
		Delay();                   // Wait a fraction of a second.
		Mouse::mouse()->hide();    // Turn off mouse.
		Mouse::mouse_update = false;
		SDL_Event event;
		while (!escaped && !gump->is_done() && SDL_PollEvent(&event)) {
			escaped = !handle_modal_gump_event(gump, event);
		}

		if (gump->run() || gwin->is_dirty()) {
			gwin->paint();    // Paint each cycle.
			if (paint) {
				paint->paint();
			}
		}
		gwin->rotatecolours();
		Mouse::mouse()->show();       // Re-display mouse.
		if (!gwin->show() &&          // Blit to screen if necessary.
			Mouse::mouse_update) {    // If not, did mouse change?
			Mouse::mouse()->blit_dirty();
		}
	} while (!gump->is_done() && !escaped && quitting_time == QUIT_TIME_NO);
	Mouse::mouse()->hide();
	remove_gump(gump);
	Mouse::mouse()->set_shape(saveshape);
	// Leave mouse off.
	gwin->paint();
	gwin->show(true);
	// Resume the game
	gwin->get_tqueue()->resume(SDL_GetTicks());

	modal_gump_count--;
	if (touchui != nullptr) {
		if (!gwin->is_in_exult_menu()) {
			touchui->showButtonControls();
		}
		if ((non_persistent_count == 0 || gumpman->gumps_dont_pause_game())
			&& !modal_gump_count && !gwin->is_in_exult_menu()) {
			touchui->showGameControls();
		}
	}
	background = nullptr;
	return !escaped;
}

/*
 *  Prompt for a numeric value using a slider.
 *
 *  Output: Value, or 0 if user hit ESC.
 */

int Gump_manager::prompt_for_number(
		int minval, int maxval,    // Range.
		int        step,
		int        defval,    // Default to start with.
		Paintable* paint      // Should be the conversation.
) {
	auto*      slider = new Slider_gump(minval, maxval, step, defval);
	const bool ok     = do_modal_gump(slider, Mouse::hand, paint);
	const int  ret    = !ok ? 0 : slider->get_val();
	delete slider;
	return ret;
}

/*
 *  Show a number.
 */

void Gump_manager::paint_num(
		int                   num,
		int                   x,    // Coord. of right edge of #.
		int                   y,    // Coord. of top of #.
		std::shared_ptr<Font> font) {
	//  Shape_manager *sman = Shape_manager::get_instance();
	char buf[20];
	snprintf(buf, sizeof(buf), "%d", num);
	if (font == nullptr) {
		font = sman->get_font(2);
	}
	sman->paint_text(font, buf, x - font->get_text_width(buf), y);
}

/*
 *
 */
void Gump_manager::set_gumps_dont_pause_game(bool p) {
	// Don't do anything if they are the same
	if (dont_pause_game == p) {
		return;
	}

	dont_pause_game = p;

	// If pausing enabled, we need to go through and pause each gump
	if (!dont_pause_game) {
		for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->pause(Game::get_ticks());
			}
		}
	}
	// Otherwise we need to go through and resume each gump :-)
	else {
		for (Gump_list* gump = open_gumps; gump; gump = gump->next) {
			if (!gump->gump->is_persistent()) {
				gwin->get_tqueue()->resume(Game::get_ticks());
			}
		}
	}
}
