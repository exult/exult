/*
 *  Copyright (C) 2000-2013  The Exult Team
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
#  include <config.h>
#endif

#include "menulist.h"
#include "exult.h"
#include "font.h"
#include "gamewin.h"
#include "mouse.h"
#include "rect.h"
#include "shapeid.h"
#include "gump_utils.h"

// MenuEntry: a selectable menu entry (a button)
MenuEntry::MenuEntry(Shape_frame *on, Shape_frame *off, int xpos, int ypos) {
	frame_on = on;
	frame_off = off;
	int max_width = on->get_width() > off->get_width() ? on->get_width() : off->get_width();
	int max_height = on->get_height() > off->get_height() ? on->get_height() : off->get_height();
	x = xpos;
	y1 = y = ypos;
	x1 = xpos - max_width / 2;
	x2 = x1 + max_width;
	y2 = y1 + max_height;
	selected = false;
	dirty = true;
}

void MenuEntry::paint(Game_window *gwin) {
	if (!dirty)
		return;
	dirty = false;

	Shape_frame *shape;
	if (selected)
		shape = frame_on;
	else
		shape = frame_off;
	Shape_manager::get_instance()->paint_shape(
	    x - shape->get_width() / 2, y, shape);
	gwin->get_win()->show(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

bool MenuEntry::handle_event(SDL_Event &event) {
	SDL_Keysym &key = event.key.keysym;
	return (event.type == SDL_KEYDOWN &&
	        (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER)) ||
	       event.type == SDL_MOUSEBUTTONUP;
}

// MenuTextEntry: a selectable menu entry (a button)
MenuTextEntry::MenuTextEntry(Font *fnton, Font *fnt, const char *txt, int xpos, int ypos)
	: MenuTextObject(fnt, fnton, txt), enabled(true) {
	int max_width = font->get_text_width(text.c_str());
	int max_height = font->get_text_height();
	x = xpos;
	y1 = y = ypos;
	x1 = xpos - max_width / 2 + 1;
	x2 = x1 + max_width + 1;
	y2 = y1 + max_height + 1;
	selected = false;
	dirty = true;
}

void MenuTextEntry::paint(Game_window *gwin) {
	if (!dirty)
		return;
	dirty = false;

	Font *fnt;
	if (selected && enabled)
		fnt = font_on;
	else
		fnt = font;
	fnt->paint_text_box(gwin->get_win()->get_ib8(), text.c_str(),
	                    x1, y1, x2 - x1, y2 - y1, 0, false, true, nullptr);
	gwin->get_win()->show(x1, y1, x2 - x1, y2 - y1);
}

bool MenuTextEntry::handle_event(SDL_Event &event) {
	SDL_Keysym &key = event.key.keysym;
	return (((event.type == SDL_KEYDOWN &&
	          (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER)) ||
	         event.type == SDL_MOUSEBUTTONUP)) && enabled;
}

// MenuGameEntry: a selectable menu entry (a button)
MenuGameEntry::MenuGameEntry(
    Font *fnton,
    Font *fnt,
    const char *txt,
    Shape_frame *sfx,
    int xpos, int ypos
)
	: MenuTextEntry(fnton, fnt, txt, xpos, ypos) {
	sfxicon = sfx;
	int max_width = 0;
	int width;
	int max_height = font->get_text_height();
	//For game titles, which may have more than one line:
	std::string localcopy = txt;
	char *ptr = &localcopy[0];
	char *lineptr = ptr;
	int lines = 1;
	while (*ptr != 0) {
		if (*ptr == '\n') {
			lines++;
			*ptr = 0;
			width = font->get_text_width(lineptr);
			if (width > max_width)
				max_width = width;
			*ptr = '\n';
			lineptr = ptr + 1;
		}
		ptr++;
	}
	width = font->get_text_width(lineptr);
	if (width > max_width)
		max_width = width;
	max_height *= lines;
	x = xpos;
	y1 = y = ypos;
	x1 = xpos - max_width / 2 + 1;
	x2 = x1 + max_width + 1;
	y2 = y1 + max_height + lines;
	selected = false;
	dirty = true;
}

void MenuGameEntry::paint(Game_window *gwin) {
	if (!dirty)
		return;
	dirty = false;

	if (sfxicon) {
		Shape_manager::get_instance()->paint_shape(x1 - sfxicon->get_width() - 3, y, sfxicon);
		gwin->get_win()->show(x1 - sfxicon->get_width() - 3, y, sfxicon->get_width(), sfxicon->get_height());
	}

	Font *fnt;
	if (selected && is_enabled())
		fnt = font_on;
	else
		fnt = font;
	fnt->paint_text_box(gwin->get_win()->get_ib8(), text.c_str(),
	                    x1, y1, x2 - x1, y2 - y1, 0, false, true, nullptr);
	gwin->get_win()->show(x1, y1, x2 - x1, y2 - y1);
}

bool MenuGameEntry::handle_event(SDL_Event &event) {
	SDL_Keysym &key = event.key.keysym;
	return (((event.type == SDL_KEYDOWN &&
	          (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER)) ||
	         event.type == SDL_MOUSEBUTTONUP)) && is_enabled();
}

// MenuTextChoice: a multiple-choice menu entry
MenuTextChoice::MenuTextChoice(Font *fnton, Font *fnt, const char *txt, int xpos, int ypos)
	: MenuTextObject(fnt, fnton, txt) {
	int max_width = font->get_text_width(text.c_str());
	int max_height = font->get_text_height();
	x = xpos;
	x1 = x - max_width;
	y1 = y = ypos;
	x2 = x1 + max_width;
	y2 = y1 + max_height;
	selected = false;
	choice = -1;
	max_choice_width = 0;
}

void MenuTextChoice::add_choice(const char *s) {
	choices.emplace_back(s);
	int len = font->get_text_width(s);
	max_choice_width = (len > max_choice_width) ? len : max_choice_width;
	x2 = x + 32 + max_choice_width;
}

void MenuTextChoice::paint(Game_window *gwin) {
	if (!dirty)
		return;
	dirty = false;

	Font *fnt;
	if (selected)
		fnt = font_on;
	else
		fnt = font;
	fnt->draw_text(gwin->get_win()->get_ib8(), x1, y1, text.c_str());
	gwin->get_win()->show(x1, y1, x1 + fnt->get_text_width(text.c_str()), y2);

	if (choice >= 0) {
		gwin->get_win()->fill8(0, max_choice_width, font->get_text_height(), x + 32, y);
		font->draw_text(gwin->get_win()->get_ib8(),
		                x + 32, y, choices[choice].c_str());
		gwin->get_win()->show(x + 32, y, x + 32 + max_choice_width, y2);
	}
}

bool MenuTextChoice::handle_event(SDL_Event &event) {
	if (event.type == SDL_MOUSEBUTTONUP) {
		dirty = true;
		choice++;
		if (choice >= static_cast<int>(choices.size()))
			choice = 0;
	} else if (event.type == SDL_KEYDOWN) {
		switch (event.key.keysym.sym) {
		case SDLK_LEFT:
			dirty = true;
			choice--;
			if (choice < 0)
				choice = choices.size() - 1;
			break;
		case SDLK_RIGHT:
			dirty = true;
			choice++;
			if (choice >= static_cast<int>(choices.size()))
				choice = 0;
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
	selected = true;
	selection = sel;
	entries[selection]->set_selected(true);
}

void MenuList::set_selection(int x, int y) {

	// deselect the previous one, unless nothing changed
	if (selected) {
		auto& entry = entries[selection];
		if (entry->is_mouse_over(x, y))
			return;

		entry->set_selected(false);
	}

	// select the new one, and return when found
	for (size_t i = 0; i < entries.size(); i++) {
		auto& entry = entries[i];
		if (entry->is_mouse_over(x, y)) {
			entry->set_selected(true);
			selected = true;
			selection = i;
			return;
		}
	}

	// nothing has been selected
	selected = false;
}

int MenuList::handle_events(Game_window *gwin, Mouse *mouse) {
	bool mouse_visible;
	int count = entries.size();
	bool exit_loop = false;
	//int scale = gwin->get_fastmouse() ? 1 : gwin->get_win()->get_scale();
	int gx;
	int gy;
	SDL_Event event;
	for (int i = 0; i < count; i++)
		entries[i]->dirty = true;

	gwin->show(true);
	mouse->show();
	do {
		Delay();
		mouse_visible = mouse->is_onscreen();
		if (mouse_visible)
			mouse->hide();
		// redraw items if they're dirty
		for (int i = 0; i < count; i++) {
			entries[i]->paint(gwin);
		}
		// redraw mouse if visible
		if (mouse_visible) {
			mouse->show();
			mouse->blit_dirty();
		}
		bool mouse_updated = false;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_MOUSEMOTION) {
				gwin->get_win()->screen_to_game(event.motion.x, event.motion.y, gwin->get_fastmouse(), gx, gy);
				if (!mouse_updated) mouse->hide();
				mouse_updated = true;
				mouse->move(gx, gy);
				set_selection(gx, gy);
				//mouse->show();
				//mouse->blit_dirty();
			} else if (event.type == SDL_MOUSEBUTTONDOWN) {
				if (!mouse_visible) {
					gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);

					// if invisible, redraw mouse
					set_selection(gx, gy);
					mouse->show();
					mouse->blit_dirty();
					mouse_updated = false;
				}
			} else if (event.type == SDL_MOUSEBUTTONUP) {
				gwin->get_win()->screen_to_game(event.button.x, event.button.y, gwin->get_fastmouse(), gx, gy);
				auto& entry = entries[selection];
				if (entry->is_mouse_over(gx, gy)) {
					exit_loop = entry->handle_event(event);
				}
			} else if (event.type == SDL_KEYDOWN) {
				mouse_updated = false;
				mouse->hide();
				mouse->blit_dirty();
				switch (event.key.keysym.sym) {
				case SDLK_q:
				case SDLK_x:
					if (event.key.keysym.mod & KMOD_ALT
					        || event.key.keysym.mod & KMOD_GUI) {
						return -1;
					}
					break;
				case SDLK_UP:
					if (!selected) {
						// if unselected (by 'MouseOut' event), just re-select
						set_selection(selection);
						continue;
					}
					if (selection <= 0)
						set_selection(count - 1);
					else
						set_selection(selection - 1);
					continue;
				case SDLK_DOWN:
					if (!selected) {
						// if unselected (by 'MouseOut' event), just re-select
						set_selection(selection);
						continue;
					}
					if (selection >= (count - 1))
						set_selection(0);
					else
						set_selection(selection + 1);
					continue;
				case SDLK_s:
					if ((event.key.keysym.mod & KMOD_ALT) &&
					        (event.key.keysym.mod & KMOD_CTRL))
						make_screenshot(true);
					// FALLTHROUGH
				default: {
					// let key be processed by selected menu-item
					if (selected) {
						exit_loop = entries[selection]->handle_event(event);
					}
				}
				break;
				}
			} else if (event.type == SDL_QUIT) {
				return -1;
			}
		}
		if (mouse_updated) {
			mouse->show();
			mouse->blit_dirty();
			mouse_updated = false;
		}
	} while (!exit_loop);
	mouse->hide();
	if (entries[selection]->get_has_id())
		return entries[selection]->get_id();
	else
		return selection;
}

