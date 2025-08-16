/*
Copyright (C) 2001-2022 The Exult Team

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

#ifndef FACE_STATS_H
#define FACE_STATS_H

#include "Configuration.h"
#include "Gump.h"

class Actor;
class Portrait_button;

class Face_stats : public Gump {
	// Only allow for one to be made
	static Face_stats* self;
	static int         mode;
	Face_stats();

	int              party_size;
	Portrait_button* party[8];
	int              npc_nums[8];

	int resx;
	int resy;
	int gamex;
	int gamey;

	void create_buttons();
	void delete_buttons();

	TileRect region;

public:
	~Face_stats() override;
	// Is a given point on a button?
	Gump_button* on_button(int mx, int my) override;
	void         paint() override;

	// Don't close on end_gump_mode
	bool is_persistent() const override {
		return true;
	}

	// Can't be dragged with mouse
	bool is_draggable() const override {
		return false;
	}

	// Show the hand cursor
	bool no_handcursor() const override {
		return true;
	}

	TileRect get_rect() const override {
		return region;
	}

	bool has_point(int x, int y) const override;

	// add dirty region, if dirty
	void update_gump() override;

	bool add(
			Game_object* obj, int mx = -1, int my = -1, int sx = -1,
			int sy = -1, bool dont_check = false,
			bool combine = false) override;

	Container_game_object* find_actor(int mx, int my) override;

	static int get_state() {
		return self ? mode : -1;
	}

	static void CreateGump();
	static void RemoveGump();
	static void AdvanceState();
	static void UpdateButtons();
	static void HideGump();
	static void ShowGump();
	static bool Visible();
	static void save_config(Configuration* config);
	static void load_config(Configuration* config);
};

#endif    // FACE_STATS_H
