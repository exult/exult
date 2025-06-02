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

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL_keycode.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "exult.h"
#include "font.h"
#include "gamewin.h"
#include "gump_utils.h"
#include "menulist.h"
#include "mouse.h"
#include "rect.h"
#include "shapeid.h"

// MenuEntry: a selectable menu entry (a button)
MenuEntry::MenuEntry(Shape_frame* on, Shape_frame* off, int xpos, int ypos) {
	frame_on             = on;
	frame_off            = off;
	const int max_width  = on->get_width() > off->get_width() ? on->get_width()
															  : off->get_width();
	const int max_height = on->get_height() > off->get_height()
								   ? on->get_height()
								   : off->get_height();
	x                    = xpos;
	y1 = y   = ypos;
	x1       = xpos - max_width / 2;
	x2       = x1 + max_width;
	y2       = y1 + max_height;
	selected = false;
	dirty    = true;
}

void MenuEntry::paint(Game_window* gwin) {
	if (!dirty) {
		return;
	}
	dirty = false;

	Shape_frame* shape;
	if (selected) {
		shape = frame_on;
	} else {
		shape = frame_off;
	}
	Shape_manager::get_instance()->paint_shape(
			x - shape->get_width() / 2, y, shape);
	gwin->get_win()->show(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

bool MenuEntry::handle_event(SDL_Event& event) {
	return (event.type == SDL_EVENT_KEY_DOWN
			&& (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER))
		   || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
}

// MenuTextEntry: a selectable menu entry (a button)
MenuTextEntry::MenuTextEntry(
		std::shared_ptr<Font> fnton, std::shared_ptr<Font> fnt, const char* txt,
		int xpos, int ypos)
		: MenuTextObject(fnt, fnton, txt), enabled(true) {
	const int max_width  = font->get_text_width(text.c_str());
	const int max_height = font->get_text_height();
	x                    = xpos;
	y1 = y   = ypos;
	x1       = xpos - max_width / 2 + 1;
	x2       = x1 + max_width + 1;
	y2       = y1 + max_height + 1;
	selected = false;
	dirty    = true;
}

void MenuTextEntry::paint(Game_window* gwin) {
	if (!dirty) {
		return;
	}
	dirty = false;

	std::shared_ptr<Font> fnt;
	if (selected && enabled) {
		fnt = font_on;
	} else {
		fnt = font;
	}
	fnt->paint_text_box(
			gwin->get_win()->get_ib8(), text.c_str(), x1, y1, x2 - x1, y2 - y1,
			0, false, true, nullptr);
	gwin->get_win()->show(x1, y1, x2 - x1, y2 - y1);
}

bool MenuTextEntry::handle_event(SDL_Event& event) {
	return (((event.type == SDL_EVENT_KEY_DOWN
			  && (event.key.key == SDLK_RETURN
				  || event.key.key == SDLK_KP_ENTER))
			 || event.type == SDL_EVENT_MOUSE_BUTTON_UP))
		   && enabled;
}

// MenuGameEntry: a selectable menu entry (a button)
MenuGameEntry::MenuGameEntry(
		std::shared_ptr<Font> fnton, std::shared_ptr<Font> fnt, const char* txt,
		Shape_frame* sfx, int xpos, int ypos)
		: MenuTextEntry(fnton, fnt, txt, xpos, ypos) {
	sfxicon       = sfx;
	int max_width = 0;
	int width;
	int max_height = font->get_text_height();
	// For game titles, which may have more than one line:
	std::string localcopy = txt;
	char*       ptr       = &localcopy[0];
	char*       lineptr   = ptr;
	int         lines     = 1;
	while (*ptr != 0) {
		if (*ptr == '\n') {
			lines++;
			*ptr  = 0;
			width = font->get_text_width(lineptr);
			if (width > max_width) {
				max_width = width;
			}
			*ptr    = '\n';
			lineptr = ptr + 1;
		}
		ptr++;
	}
	width = font->get_text_width(lineptr);
	if (width > max_width) {
		max_width = width;
	}
	max_height *= lines;
	x  = xpos;
	y1 = y   = ypos;
	x1       = xpos - max_width / 2 + 1;
	x2       = x1 + max_width + 1;
	y2       = y1 + max_height + lines;
	selected = false;
	dirty    = true;
}

void MenuGameEntry::paint(Game_window* gwin) {
	if (!dirty) {
		return;
	}
	dirty = false;

	if (sfxicon) {
		Shape_manager::get_instance()->paint_shape(
				x1 - sfxicon->get_width() - 3, y, sfxicon);
		gwin->get_win()->show(
				x1 - sfxicon->get_width() - 3, y, sfxicon->get_width(),
				sfxicon->get_height());
	}

	std::shared_ptr<Font> fnt;
	if (selected && is_enabled()) {
		fnt = font_on;
	} else {
		fnt = font;
	}
	fnt->paint_text_box(
			gwin->get_win()->get_ib8(), text.c_str(), x1, y1, x2 - x1, y2 - y1,
			0, false, true, nullptr);
	gwin->get_win()->show(x1, y1, x2 - x1, y2 - y1);
}

bool MenuGameEntry::handle_event(SDL_Event& event) {
	return (((event.type == SDL_EVENT_KEY_DOWN
			  && (event.key.key == SDLK_RETURN
				  || event.key.key == SDLK_KP_ENTER))
			 || event.type == SDL_EVENT_MOUSE_BUTTON_UP))
		   && is_enabled();
}

// MenuTextChoice: a multiple-choice menu entry
MenuTextChoice::MenuTextChoice(
		std::shared_ptr<Font> fnton, std::shared_ptr<Font> fnt, const char* txt,
		int xpos, int ypos)
		: MenuTextObject(fnt, fnton, txt) {
	const int max_width  = font->get_text_width(text.c_str());
	const int max_height = font->get_text_height();
	x                    = xpos;
	x1                   = x - max_width;
	y1 = y           = ypos;
	x2               = x1 + max_width;
	y2               = y1 + max_height;
	selected         = false;
	choice           = -1;
	max_choice_width = 0;
}

void MenuTextChoice::add_choice(const char* s) {
	choices.emplace_back(s);
	const int len    = font->get_text_width(s);
	max_choice_width = (len > max_choice_width) ? len : max_choice_width;
	x2               = x + 32 + max_choice_width;
}

void MenuTextChoice::paint(Game_window* gwin) {
	if (!dirty) {
		return;
	}
	dirty = false;

	std::shared_ptr<Font> fnt;
	if (selected) {
		fnt = font_on;
	} else {
		fnt = font;
	}
	fnt->draw_text(gwin->get_win()->get_ib8(), x1, y1, text.c_str());
	gwin->get_win()->show(x1, y1, x1 + fnt->get_text_width(text.c_str()), y2);

	if (choice >= 0) {
		gwin->get_win()->fill8(
				0, max_choice_width, font->get_text_height(), x + 32, y);
		font->draw_text(
				gwin->get_win()->get_ib8(), x + 32, y, choices[choice].c_str());
		gwin->get_win()->show(x + 32, y, x + 32 + max_choice_width, y2);
	}
}

bool MenuTextChoice::handle_event(SDL_Event& event) {
	if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		dirty = true;
		choice++;
		if (choice >= static_cast<int>(choices.size())) {
			choice = 0;
		}
	} else if (event.type == SDL_EVENT_KEY_DOWN) {
		switch (event.key.key) {
		case SDLK_LEFT:
			dirty = true;
			choice--;
			if (choice < 0) {
				choice = choices.size() - 1;
			}
			break;
		case SDLK_RIGHT:
			dirty = true;
			choice++;
			if (choice >= static_cast<int>(choices.size())) {
				choice = 0;
			}
			break;
		default:
			break;
		}
	}
	return false;
}

void MenuList::set_selection(int sel) {
	// deselect the previous entry
	if (selected) {
		entries[selection]->set_selected(false);
	}

	// select the new one
	selected  = true;
	selection = sel;
	entries[selection]->set_selected(true);
}

void MenuList::set_selection(int x, int y) {
	// deselect the previous one, unless nothing changed
	if (selected) {
		auto& entry = entries[selection];
		if (entry->is_mouse_over(x, y)) {
			return;
		}

		entry->set_selected(false);
	}

	// select the new one, and return when found
	for (size_t i = 0; i < entries.size(); i++) {
		auto& entry = entries[i];
		if (entry->is_mouse_over(x, y)) {
			entry->set_selected(true);
			selected  = true;
			selection = i;
			return;
		}
	}

	// nothing has been selected
	selected = false;
}

void MenuList::set_cancel(int val) {
	cancel_id  = val;
	has_cancel = true;
}

int MenuList::handle_events(Game_window* gwin) {
	const int count = entries.size();
	for (int i = 0; i < count; i++) {
		entries[i]->dirty = true;
	}

	gwin->get_win()->FillGuardband();
	gwin->show(true);
	Mouse::mouse()->show();

	bool exit_loop = false;
	do {
		Delay();
		const bool mouse_visible = Mouse::mouse()->is_onscreen();
		if (mouse_visible) {
			Mouse::mouse()->hide();
		}
		// redraw items if they're dirty
		for (int i = 0; i < count; i++) {
			entries[i]->paint(gwin);
		}
		// redraw mouse if visible
		if (mouse_visible) {
			Mouse::mouse()->show();
			Mouse::mouse()->blit_dirty();
		}
		bool          mouse_updated = false;
		SDL_Renderer* renderer
				= SDL_GetRenderer(gwin->get_win()->get_screen_window());
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			SDL_ConvertEventToRenderCoordinates(renderer, &event);
			int gx;
			int gy;
			if (event.type == SDL_EVENT_MOUSE_MOTION) {
				if (Mouse::use_touch_input
					&& event.motion.which != EXSDL_TOUCH_MOUSEID) {
					Mouse::use_touch_input = false;
				}
				gwin->get_win()->screen_to_game(
						event.motion.x, event.motion.y, gwin->get_fastmouse(),
						gx, gy);
				if (!mouse_updated) {
					Mouse::mouse()->hide();
				}
				mouse_updated = true;
				Mouse::mouse()->move(gx, gy);
				set_selection(gx, gy);
				// mouse->show();
				// mouse->blit_dirty();
			} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				if (!mouse_visible) {
					gwin->get_win()->screen_to_game(
							event.button.x, event.button.y,
							gwin->get_fastmouse(), gx, gy);

					// if invisible, redraw mouse
					set_selection(gx, gy);
					Mouse::mouse()->show();
					Mouse::mouse()->blit_dirty();
					mouse_updated = false;
				}
			} else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
				gwin->get_win()->screen_to_game(
						event.button.x, event.button.y, gwin->get_fastmouse(),
						gx, gy);
				auto& entry = entries[selection];
				if (entry->is_mouse_over(gx, gy)) {
					exit_loop = entry->handle_event(event);
				}
			} else if (event.type == SDL_EVENT_KEY_DOWN) {
				mouse_updated = false;
				Mouse::mouse()->hide();
				Mouse::mouse()->blit_dirty();
				switch (event.key.key) {
				case SDLK_ESCAPE:
					if (has_cancel) {
						return cancel_id;
					}
					break;
				case SDLK_Q:
				case SDLK_X:
					if (event.key.mod & SDL_KMOD_ALT
						|| event.key.mod & SDL_KMOD_GUI) {
						return -1;
					}
					break;
				case SDLK_UP:
					if (!selected) {
						// if unselected (by 'MouseOut' event), just re-select
						set_selection(selection);
						continue;
					}
					if (selection <= 0) {
						set_selection(count - 1);
					} else {
						set_selection(selection - 1);
					}
					continue;
				case SDLK_DOWN:
					if (!selected) {
						// if unselected (by 'MouseOut' event), just re-select
						set_selection(selection);
						continue;
					}
					if (selection >= (count - 1)) {
						set_selection(0);
					} else {
						set_selection(selection + 1);
					}
					continue;
				case SDLK_S:
					if ((event.key.mod & SDL_KMOD_ALT)
						&& (event.key.mod & SDL_KMOD_CTRL)) {
						make_screenshot(true);
					}
					[[fallthrough]];
				default: {
					// let key be processed by selected menu-item
					if (selected) {
						exit_loop = entries[selection]->handle_event(event);
					}
				} break;
				}
			} else if (event.type == SDL_EVENT_QUIT) {
				return -1;
			} else if (event.type == SDL_EVENT_FINGER_DOWN) {
				if ((!Mouse::use_touch_input)
					&& (event.tfinger.fingerID != 0)) {
					Mouse::use_touch_input = true;
					gwin->set_painted();
				}
			}
		}
		if (mouse_updated) {
			Mouse::mouse()->show();
			Mouse::mouse()->blit_dirty();
		}
	} while (!exit_loop);
	Mouse::mouse()->hide();
	if (entries[selection]->get_has_id()) {
		return entries[selection]->get_id();
	} else {
		return selection;
	}
}
