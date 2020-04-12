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
	UNREPLICATABLE_CLASS(Paperdoll_gump)

protected:

	// Statics
	static short coords[36];        // Coords. of where to draw things,
	static short coords_blue[36];       // indexed by spot # (0-17).
	static short shapes_blue[36];
	static short coords_hot[36];        // Hot spots

	static short diskx, disky;      // Where to show 'diskette' button.
	static short heartx, hearty;        // Where to show 'stats' button.
	static short combatx, combaty;      // Combat button.
	static short cstatx, cstaty;        // Combat mode.
	static short halox, haloy;  // "Protected" halo.
	static short cmodex, cmodey;    // Combat mode.

	static short bodyx, bodyy;      // Body
	static short headx, heady;      // Head
	static short beltfx, beltfy;        // Female Belt
	static short neckfx, neckfy;        // Female Neck
	static short beltmx, beltmy;        // Male Belt
	static short neckmx, neckmy;        // Male Neck
	static short legsx, legsy;      // Legs
	static short feetx, feety;      // Feet
	static short handsx, handsy;        // Hands
	static short lhandx, lhandy;        // Left Hand
	static short rhandx, rhandy;        // Right Hand
	static short ahandx, ahandy;        // Ammo in Left Hand
	static short ammox, ammoy;      // Quiver

	static short backfx, backfy;        // Female Back
	static short backmx, backmy;        // Male Back
	static short back2fx, back2fy;      // Female Back Weapon
	static short back2mx, back2my;      // Male Back Weapon
	static short shieldfx, shieldfy;    // Female Back Shield
	static short shieldmx, shieldmy;    // Male Back Shield


	// Non Statics

	Heart_button *heart_button;     // For bringing up stats.
	Disk_button *disk_button;       // For bringing up 'save' box. (Avatar Only)
	Combat_button *combat_button;       // Combat Toggle (Avatar Only)
	Cstats_button *cstats_button;       // Combat Stats (Not BG)
	Halo_button *halo_button;       // Halo (protection) (BG Only)
	Combat_mode_button *cmode_button;   // Combat Modes (BG Only)


	// Statics

	// Get the X and Y from a spot
	static int spotx(int i) {
		return coords[2 * i];
	}
	static int spoty(int i) {
		return coords[2 * i + 1];
	}

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
