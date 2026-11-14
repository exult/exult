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

#ifndef MUI_ROUTER_H
#define MUI_ROUTER_H

#include <string>

/*
 *  The only switch between the classic gumps and the themed menus. Each
 *  open_* function returns false when the classic screen should be used
 *  (menu style "classic", which is the default, no game loaded, or the
 *  theme cannot be loaded), so callers keep their classic code path as is.
 */
namespace Mui {
	// config/gameplay/menu_style: classic|ultima|modern|<mod theme>.
	std::string menu_style();
	bool        themed();
	bool        open_game_menu();
	bool        open_options(const char* tab = nullptr);
	bool        open_journey_log();
	bool        open_cheats();
	// Cycle classic -> ultima -> modern -> classic and save it.
	std::string cycle_menu_style();
}    // namespace Mui

#endif
