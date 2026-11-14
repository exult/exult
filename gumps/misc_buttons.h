/*
Copyright (C) 2000-2024 The Exult Team

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

#ifndef MISC_BUTTONS_H
#define MISC_BUTTONS_H

#include "Gump_button.h"

class Actor;

/*
 *  A checkmark for closing its parent:
 */
class Checkmark_button : public Gump_button {
	// Optional tab behind the checkmark (Exult's checkmark background from
	// exult.flx), for gump art with no tab of its own. See gump_info.txt
	// checkmark_pos. Painted by Gump::paint() under the gump's shape.
	bool    has_tab;
	ShapeID tab;

public:
	Checkmark_button(Gump* par, int px, int py, bool with_tab = false);

	Gump_widget* clone(Gump* par) override {
		return new Checkmark_button(par, x, y, has_tab);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;

	bool is_checkmark() const override {
		return true;
	}

	// The tab, centred on the checkmark; an empty rect without a tab.
	TileRect get_tab_rect() const;
	void     paint_tab();
	// Is a screen point on the tab? Not a click on the button: the tab
	// belongs to the gump, so clicking it drags the gump.
	bool on_tab(int mx, int my) const;
};

/*
 *  A 'heart' button for bringing up stats.
 */
class Heart_button : public Gump_button {
public:
	Heart_button(Gump* par, int px, int py);

	Gump_widget* clone(Gump* par) override {
		return new Heart_button(par, x, y);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  A diskette for bringing up the 'save' box.
 */
class Disk_button : public Gump_button {
public:
	Disk_button(Gump* par, int px, int py);

	Gump_widget* clone(Gump* par) override {
		return new Disk_button(par, x, y);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  The combat toggle button.
 */
class Combat_button : public Gump_button {
public:
	Combat_button(Gump* par, int px, int py);

	Gump_widget* clone(Gump* par) override {
		return new Combat_button(par, x, y);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
	void paint() override;
};

/*
 *  The halo button.
 */
class Halo_button : public Gump_button {
	Actor* actor;    // Who this represents.
public:
	Halo_button(Gump* par, int px, int py, Actor* a);

	Gump_widget* clone(Gump* par) override {
		return new Halo_button(par, x, y, actor);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  Combat mode.  Has 10 frames corresponding to Actor::Attack_mode.
 */
class Combat_mode_button : public Gump_button {
	Actor* actor;    // Who this represents.
public:
	Combat_mode_button(Gump* par, int px, int py, Actor* a);

	Gump_widget* clone(Gump* par) override {
		return new Combat_mode_button(par, x, y, actor);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

/*
 *  The Serpent Isle Combat Stats Button
 */
class Cstats_button : public Gump_button {
public:
	Cstats_button(Gump* par, int px, int py);

	Gump_widget* clone(Gump* par) override {
		return new Cstats_button(par, x, y);
	}

	// What to do when 'clicked':
	bool activate(MouseButton button) override;
};

#endif
