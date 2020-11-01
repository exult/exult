/*
Copyright (C) 2000-2013 The Exult Team

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

#ifndef PAPERDOLL_GUMP_H
#define PAPERDOLL_GUMP_H

#include "Gump.h"

class Heart_button;
class Disk_button;
class Combat_button;
class Halo_button;
class Cstats_button;
class Combat_mode_button;
class Paperdoll_npc;

//
// For best viewing use Tab size = 4
//

class Paperdoll_gump : public Gump {
private:

protected:
	struct Position {
		short x;
		short y;
	};

	// Statics
	static Position coords[18];        // Coords. of where to draw things,
	static Position coords_blue[18];   // indexed by spot # (0-17).
	static Position shapes_blue[18];
	static Position coords_hot[18];    // Hot spots

	static Position disk;         // Where to show 'diskette' button.
	static Position heart;        // Where to show 'stats' button.
	static Position combat;       // Combat button.
	static Position cstat;        // Combat mode.
	static Position halo;         // "Protected" halo.
	static Position cmode;        // Combat mode.

	static Position body;         // Body
	static Position headp;        // Head
	static Position beltf;        // Female Belt
	static Position neckf;        // Female Neck
	static Position beltm;        // Male Belt
	static Position neckm;        // Male Neck
	static Position legsp;        // Legs
	static Position feetp;        // Feet
	static Position hands;        // Hands
	static Position lhandp;       // Left Hand
	static Position rhandp;       // Right Hand
	static Position ahand;        // Ammo in Left Hand
	static Position ammo;         // Quiver

	static Position backf;        // Female Back
	static Position backm;        // Male Back
	static Position back2f;       // Female Back Weapon
	static Position back2m;       // Male Back Weapon
	static Position shieldf;      // Female Back Shield
	static Position shieldm;      // Male Back Shield


	// Non Statics

	Heart_button *heart_button;         // For bringing up stats.
	Disk_button *disk_button;           // For bringing up 'save' box. (Avatar Only)
	Combat_button *combat_button;       // Combat Toggle (Avatar Only)
	Cstats_button *cstats_button;       // Combat Stats (Not BG)
	Halo_button *halo_button;           // Halo (protection) (BG Only)
	Combat_mode_button *cmode_button;   // Combat Modes (BG Only)

	// Non Statics

	// Find index of closest spot to the mouse pointer
	int find_closest(int mx, int my, int only_empty = 0);

	// Set to location of an object a spot
	void set_to_spot(Game_object *obj, int index);
public:
	Paperdoll_gump(Container_game_object *cont, int initx, int inity,
	               int shnum);

	~Paperdoll_gump() override;

	// Is a given point on a button?
	Gump_button *on_button(int mx, int my) override;

	// Find the object the mouse is over
	Game_object *find_object(int mx, int my) override;

	// Add object.
	bool add(Game_object *obj, int mx = -1, int my = -1,
	                int sx = -1, int sy = -1, bool dont_check = false,
	                bool combine = false) override;

	// Paint it and its contents.
	void paint() override;


	//
	// Painting Helpers
	//

	// Generic Paint Object Method
	void paint_object(const Rectangle &box, const Paperdoll_npc *info, int spot,
	                  int sx, int sy, int frame = 0, int itemtype = -1);

	// Generic Paint Object Method for something that is armed dependant
	void paint_object_arms(const Rectangle &box, const Paperdoll_npc *info, int spot,
	                       int sx, int sy, int start = 0, int itemtype = -1);

	// Special 'Constant' Paint Methods
	void paint_body(const Rectangle &box, const Paperdoll_npc *info);
	void paint_belt(const Rectangle &box, const Paperdoll_npc *info);
	void paint_head(const Rectangle &box, const Paperdoll_npc *info);
	void paint_arms(const Rectangle &box, const Paperdoll_npc *info);

	// What are we holding?
	int get_arm_type();


	//
	// Finding Helpers
	//

	// Generic Check Object Method
	Game_object *check_object(int mx, int my, const Paperdoll_npc *info, int spot,
	                          int sx, int sy, int frame = 0, int itemtype = -1);


	// Generic Check Object Method for something that is armed dependant
	Game_object *check_object_arms(int mx, int my, const Paperdoll_npc *info, int spot,
	                               int sx, int sy, int start = 0, int itemtype = -1);

	// Special 'Constant' Check Methods
	bool check_body(int mx, int my, const Paperdoll_npc *info);
	bool check_belt(int mx, int my, const Paperdoll_npc *info);
	bool check_head(int mx, int my, const Paperdoll_npc *info);
	bool check_arms(int mx, int my, const Paperdoll_npc *info);

	// Generic Method to check a shape
	bool check_shape(int px, int py, int shape, int frame, ShapeFile file);

	Container_game_object *find_actor(int mx, int my) override;
};

#endif
