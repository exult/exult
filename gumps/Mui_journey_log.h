/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_JOURNEY_LOG_H
#define MUI_JOURNEY_LOG_H

#include "Mui_window.h"

#include <memory>
#include <string>
#include <vector>

/*
 *  The themed load/save screen ("Journey Log"). Saves are shown as cards
 *  with their screenshot (or as a list), can be searched and sorted, and
 *  the selected one is described in a details panel. A "New save" card
 *  saves the current game to the first free slot. Loading, saving,
 *  deleting and the save list use the same Game_window calls and checks as
 *  the classic Newfile_gump.
 */
class Mui_journey_window : public Mui_window {
public:
	struct Save;    // One card: the current game, the quick save or a slot.

	Mui_journey_window();
	~Mui_journey_window() override;

	// True after a game was loaded (callers close their own menus).
	bool restored_game() const {
		return restored;
	}

	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;

	// Test driver access.
	int         saves_count() const;      // Numbered saves found.
	int         visible_count() const;    // Saves shown (after the search).
	std::string visible_name(int index) const;
	bool        select_by_name(const std::string& name);

	// Used by the card, row and details painters.
	const Save* card(int index) const;
	const Save* selected_save() const;

	int card_count() const {
		return int(cards.size());
	}

protected:
	void        build(Mui_box& body) override;
	void        build_title(Mui_box& bar) override;
	void        build_footer(Mui_box& footer) override;
	std::string default_help() const override;
	void        preferred_size(float& w, float& h) const override;

private:
	enum class Sort {
		newest,
		oldest,
		name,
		day
	};

	std::vector<std::unique_ptr<Save>> saves;      // Numbered saves.
	std::unique_ptr<Save>              current;    // The game being played.
	std::unique_ptr<Save>              quick;      // Gamedat, if it has details.
	std::vector<Save*>                 cards;      // Shown, in order.
	int                                first_free = 0;
	int                                sel_slot   = -3;    // -2 current, -1 quick, else slot number.
	std::string                        search;
	Sort                               sort          = Sort::newest;
	bool                               list_view     = false;
	bool                               restored      = false;
	Mui_button*                        load_button   = nullptr;
	Mui_button*                        save_button   = nullptr;
	Mui_button*                        rename_button = nullptr;
	Mui_button*                        delete_button = nullptr;

	void load_model();
	void make_cards();
	int  default_slot() const;
	int  card_index(int slot) const;
	void select_card(int index);
	void update_buttons();
	void reload(int keep_slot);

	void load_selected();
	void save_selected();
	void new_save();
	void rename_selected();
	void delete_selected();
	void write_save(int num, const std::string& name);
	void show_error(const std::string& msg);
};

#endif
