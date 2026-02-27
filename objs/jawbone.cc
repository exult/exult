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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "jawbone.h"

#include "ShortcutBar_gump.h"
#include "exult.h"
#include "ignore_unused_variable_warning.h"
#include "objiter.h"

// Add an object.
bool Jawbone_object::add(
		Game_object* obj,
		bool         dont_check,    // 1 to skip volume/recursion check.
		bool         combine,       // True to try to combine obj.  MAY
		//   cause obj to be deleted.
		bool noset    // True to prevent actors from setting sched. weapon.
) {
	ignore_unused_variable_warning(noset);
	if (!Container_game_object::add(obj, dont_check, combine)) {
		return false;    // Can't be added to.
	}

	find_teeth();
	update_frame();
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
	return true;
}

// Remove an object.
void Jawbone_object::remove(Game_object* obj) {
	Container_game_object::remove(obj);

	find_teeth();
	update_frame();
	if (g_shortcutBar) {
		g_shortcutBar->set_changed();
	}
}

void Jawbone_object::find_teeth() {
	for (auto& tooth : teeth) {
		tooth = nullptr;
	}
	toothcount = 0;

	Object_list& objects = get_objects();
	if (objects.is_empty()) {
		return;    // Empty.
	}

	Game_object*    obj;
	Object_iterator next(objects);

	while ((obj = next.get_next()) != nullptr) {
		teeth[obj->get_framenum()] = obj;
	}

	for (auto* tooth : teeth) {
		if (tooth) {
			toothcount++;
		}
	}
}

void Jawbone_object::update_frame() {
	change_frame(toothcount);
}
