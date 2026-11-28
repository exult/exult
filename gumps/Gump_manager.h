/*
 *  Gump_manager.h - Object that manages all available gumps
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

#ifndef GUMP_MANAGER_INCLUDED
#define GUMP_MANAGER_INCLUDED

#include "mouse.h"
#include "singles.h"

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

#include "font.h"
#include "imagebuf.h"
#include "shapeid.h"

extern bool Translate_keyboard(const SDL_Event& event, SDL_Keycode& code, SDL_Keycode& unicode, bool numpad);

class Gump;
class Game_object;
class Game_window;
class Modal_gump;
class Paintable;

class Gump_manager : public Game_singletons {
	struct Gump_list {
		Gump*      gump = nullptr;
		Gump_list* next = nullptr;

		Gump_list() = default;

		Gump_list(Gump* g) : gump(g) {}
	};

	Gump_list* open_gumps = nullptr;
	Gump*      kbd_focus  = nullptr;    // This gump gets kbd strokes.
	// So we can test for 'gump mode' quickly:
	int  non_persistent_count = 0;
	int  modal_gump_count     = 0;
	bool right_click_close    = true;
	bool dont_pause_game      = false;    // NEVER SET THIS MANUALLY! YOU MUST
										  // CALL set_gumps_dont_pause_game.
	bool container_list_view = false;     // Open containers as a list.
	// Bumped whenever the chain changes, so a walk that runs usecode
	// (which may open or close gumps) can tell its nodes may be gone.
	unsigned chain_changes = 0;

public:
	void add_gump(Gump* gump);    // Add a single gump to screen
	// Show gump for obj
	void add_gump(Game_object* obj, int shapenum, bool actorgump = false);

	bool remove_gump(Gump* gump);               // Detatch a gump from the list
	bool close_gump(Gump* gump);                // Close a gump
	void close_all_gumps(bool pers = false);    // Close all gumps
	void set_kbd_focus(Gump* gump);

	bool showing_gumps(bool no_pers = false) const;    // Are gumps showing?

	bool gump_mode() const {    // Fast check.
		return non_persistent_count > 0;
	}

	bool modal_gump_mode() const {    // displaying a modal gump?
		return modal_gump_count > 0;
	}

	Gump* find_gump(int x, int y, bool pers = true);    // Find gump x,y is in
	Gump* find_gump(const Game_object* obj);            // Find gump that object is in
	// Find gump for object obj:
	Gump* find_gump(const Game_object* owner, int shapenum);

	void update_gumps();

	unsigned get_chain_changes() const {
		return chain_changes;
	}

	void paint(bool modal);
	// Map a game-coordinate point into a specific gump's (scaled) overlay
	// layer, giving the gump-local coordinate used for hit-testing. Returns
	// false if the point is outside the gump's layer. If the gump has no
	// layer yet, the point is returned unchanged (native coordinates).
	bool map_game_to_gump(const Gump* g, int gx, int gy, int& lx, int& ly) const;
	// Render a single gump into its own overlay layer (used for the gump
	// currently being dragged, which lives outside the open-gump list).
	void render_gump_to_layer(Gump* g, int z);
	// Render one part of a gump (part 0 uses render_layer, part 1 uses
	// render_layer2). HUD gumps with >1 part anchor each part to its own edge.
	void render_gump_part_to_layer(Gump* g, int z, int part, bool is_hud);

	bool double_clicked(int x, int y, Game_object*& obj);
	bool handle_kbd_event(void* ev);
	bool handle_mouse_wheel(float y, float x, int mx, int my);

	inline bool can_right_click_close() {
		return right_click_close;
	}

	inline void set_right_click_close(bool r) {
		right_click_close = r;
	}

	inline bool gumps_dont_pause_game() {
		return dont_pause_game;
	}

	void set_gumps_dont_pause_game(bool p);

	bool get_container_list_view() const {
		return container_list_view;
	}

	void set_container_list_view(bool l) {
		container_list_view = l;
	}

	bool okay_to_quit(Paintable* paint = nullptr);
	int  prompt_for_number(int minval, int maxval, int step, int def, Paintable* paint = nullptr, bool* escaped = nullptr);
	bool do_modal_gump(Modal_gump*, Mouse::Mouse_shapes, Paintable* paint = nullptr);
	void paint_num(int num, int x, int y, std::shared_ptr<Font> font = nullptr);

	Gump_manager();

	~Gump_manager() {
		close_all_gumps(true);
	}

private:
	bool handle_modal_gump_event(Modal_gump* gump, SDL_Event& event);
	void render_gumps_to_layer(bool modal);

	BackgroundPaintable* background = nullptr;

public:
	struct GumpListIterator {
	private:
		Gump_list* node;

	public:
		GumpListIterator(Gump_list* node) : node(node) {}

		GumpListIterator() = default;

		Gump* operator*() {
			return node->gump;
		}

		Gump* operator->() {
			return node->gump;
		}

		GumpListIterator& operator++(int) {
			node = node->next;
			return *this;
		}

		GumpListIterator operator++() {
			GumpListIterator ret = *this;
			node                 = node->next;
			return ret;
		}

		bool operator==(const GumpListIterator& other) {
			return node == other.node;
		}

		bool operator!=(const GumpListIterator& other) {
			return node != other.node;
		}
	};

	GumpListIterator begin() {
		return GumpListIterator(open_gumps);
	}

	GumpListIterator end() {
		return GumpListIterator();
	}
};

#endif    // GUMP_MANAGER_INCLUDED
