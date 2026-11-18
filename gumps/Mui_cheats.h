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

#ifndef MUI_CHEATS_H
#define MUI_CHEATS_H

#include "Mui_window.h"
#include "cheat_screen.h"

#include <string>
#include <vector>

class Actor;

/*
 *  The themed cheat window, shown instead of the text-mode cheat screen
 *  when a themed menu style is selected. Sections in a sidebar: Player,
 *  World & time, NPCs, Teleport, Global flags, Usecode and Map editor. The
 *  controls call the same engine functions as the cheat screen. The deep
 *  text-mode tools (NPC tool, usecode menu, more teleport options, global
 *  flag editor and the whole cheat screen) open from here through
 *  CheatScreen::run_menu_screen() and return to this window.
 */
class Mui_cheat_window : public Mui_window {
public:
	Mui_cheat_window();
	~Mui_cheat_window() override;

protected:
	void                     build(Mui_box& body) override;
	std::vector<std::string> tab_labels() const override;
	std::vector<std::string> tab_keys() const override;
	void                     on_tab(int index) override;
	void                     build_footer(Mui_box& footer) override;
	std::string              default_help() const override;
	void                     adjust_theme(Mui_theme& t) const override;

private:
	CheatScreen* cscreen;

	// World & time and Teleport fields (kept across rebuilds).
	int day      = 0;
	int hour     = 0;
	int minute   = 0;
	int tele_x   = 0;
	int tele_y   = 0;
	int tele_z   = 0;
	int tele_map = 0;
	// NPC and flag lists (filtered numbers) and their selection.
	std::string           npc_filter;
	std::vector<int>      npc_rows;
	int                   npc_selected = -1;    // NPC number.
	std::string           flag_filter;
	std::vector<unsigned> flag_rows;
	int                   flag_selected = -1;    // Flag number.
	std::string           message;               // Last action result.
	// Widgets of the current build (null otherwise).
	Mui_list*   npc_list     = nullptr;
	Mui_label*  npc_details  = nullptr;
	Mui_label*  npc_location = nullptr;
	Mui_button* npc_teleport = nullptr;
	Mui_button* npc_tools    = nullptr;
	Mui_list*   flag_list    = nullptr;
	Mui_label*  flag_name    = nullptr;
	Mui_toggle* flag_toggle  = nullptr;
	Mui_label*  flag_count   = nullptr;

	void build_warning(Mui_box& body);
	void build_player(Mui_box& body);
	void build_world(Mui_box& body);
	void build_npcs(Mui_box& body);
	void build_teleport(Mui_box& body);
	void build_flags(Mui_box& body);
	void build_usecode(Mui_box& body);
	void build_map_editor(Mui_box& body);
	void build_message(Mui_box& body);

	void read_world();
	void filter_npcs();
	void show_npc();
	void filter_flags();
	void show_flag();
	void toggle_flag(unsigned num);
	// Run a text-mode menu of the cheat screen, then refresh this window.
	void run_classic(CheatScreen::Which which, Actor* actor = nullptr, unsigned flag = 0);
	// Lay out again after list or label contents changed.
	void        invalidate_all();
	std::string flag_label(unsigned num) const;
};

#endif
