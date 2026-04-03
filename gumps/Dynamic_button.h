/*
Copyright (C) 2025 The Exult Team

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

#ifndef DYNAMIC_BUTTON_H
#define DYNAMIC_BUTTON_H

#include "Gump_button.h"

struct Dynamic_button_def;

/*
 *  A data-driven button on a gump, configured via gump_info.txt.
 */
class Dynamic_button : public Gump_button {
	int  default_frame;      // Frame when idle
	int  clicked_frame;      // Frame when pushed
	int  sound_fx;           // SFX to play (-1 = none)
	int  usecode_fn;         // Usecode function (-1 = none)
	int  action_type;        // 0=none, 1=close, 2=toggle
	int  visibility_flag;    // Global flag for visibility (-1 = always)
	int  usecode_param;      // Legacy field from gump_info.txt (unused at runtime)
	bool toggled;            // For action_type 2 (toggle state)

public:
	Dynamic_button(Gump* par, const Dynamic_button_def& def);

	Gump_widget* clone(Gump* par) override;

	bool activate(MouseButton button) override;
	void paint() override;

	// Visibility check based on global usecode flag
	bool is_visible() const;
};

#endif
