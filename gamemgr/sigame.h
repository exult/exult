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

#ifndef SIGAME_H
#define SIGAME_H

#include "game.h"

class SI_Game : public Game {
public:
	SI_Game();

	void                      play_intro() override;
	void                      end_game(bool success, bool within_game) override;
	void                      top_menu() override;
	void                      show_quotes() override;
	void                      show_credits() override;
	std::vector<unsigned int> get_congratulations_messages() override;
	bool                      new_game(Vga_file& shapes) override;

	int get_start_tile_x() override {
		return 25 * c_tiles_per_chunk;
	}

	int get_start_tile_y() override {
		return 155 * c_tiles_per_chunk;
	}

	void         show_journey_failed() override;
	Shape_frame* get_menu_shape() override;
};

#endif
