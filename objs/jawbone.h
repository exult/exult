/*
Copyright (C) 2001 The Exult Team

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

#ifndef JAWBONE_H
#define JAWBONE_H

#include "contain.h"
#include "ignore_unused_variable_warning.h"

class Jawbone_object : public Container_game_object {
	friend class Jawbone_gump;

public:
	Jawbone_object(int shapenum, int framenum, unsigned int tilex,
	               unsigned int tiley, unsigned int lft,
	               char res = 0)
		: Container_game_object(shapenum, framenum, tilex, tiley, lft, res), toothcount(0)
	{ }
	Jawbone_object() = default;

	// Add an object.
	bool add(Game_object *obj, bool dont_check = false,
	         bool combine = false, bool noset = false) override;
	// Remove an object.
	void remove(Game_object *obj) override;

	// Under attack. -> do nothing
	Game_object *attacked(Game_object *attacker, int weapon_shape = -1,
	                      int ammo_shape = -1, bool explosion = false) override {
		ignore_unused_variable_warning(attacker, weapon_shape, ammo_shape, explosion);
		return this;
	}

private:

	Game_object *teeth[19];
	int toothcount;
	void find_teeth();

	void update_frame();

};

#endif
