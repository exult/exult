/**
 ** Barge.h - Ships, carts, flying-carpets.
 **
 ** Written: 7/13/2000 - JSF
 **/

/*
Copyright (C) 2000  Jeffrey S. Freedman
Copyright (C) 2001-2022  The Exult Team

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

#ifndef INCL_BARGE
#define INCL_BARGE 1

#include "contain.h"

/*
 *  A 'barge', such as a ship or horse-and-cart.  The elements of a barge
 *  are stored in the outside world, so rendering and obstacle detection
 *  don't have to be reimplemented.
 */
class Barge_object : public Container_game_object, public Time_sensitive {
	static Game_object_shared editing;    // Egg being edited by ExultStudio.
	std::vector<Game_object_shared> objects;    // All objects in/on barge.
	int perm_count;    // Counts permanent parts of barge,
	//   which proceed those placed on it.
	unsigned char xtiles, ytiles;     // Tiles covered (when vertical).
	unsigned char dir;                // Direction: 0=N, 1=E, 2=S, 3=W.
	bool          complete;           // Flag:  all members have been read.
	bool          gathered;           // Items on barge have been gathered.
	bool          ice_raft;           // For Serpent Isle.
	bool          first_step;         // So first motion can just be 1 tile.
	bool          taking_2nd_step;    // Skip animation on 2nd step.
	signed char   boat;               // 1 if a boat, 0 if not; -1=untested.
	int           frame_time;         // Time between frames in msecs.  0 if
	//   not moving.
	PathFinder* path;      // For traveling.
	Tile_coord  center;    // Center of barge.

	Game_object* get_object(int i) const {
		return objects[i].get();
	}

	void swap_dims();
	void set_center();
	int  okay_to_rotate(const Tile_coord& pos);
	void add_dirty();
	// Finish up move/rotate operation.
	void finish_move(Tile_coord* positions, int newmap = -1);

public:
	Barge_object(
			int shapenum, int framenum, unsigned int shapex,
			unsigned int shapey, unsigned int lft, int xt, int yt, int d)
			: Container_game_object(shapenum, framenum, shapex, shapey, lft),
			  perm_count(0), xtiles(xt), ytiles(yt), dir(d), complete(false),
			  gathered(false), ice_raft(false), first_step(true),
			  taking_2nd_step(false), boat(-1), frame_time(0), path(nullptr) {}

	TileRect get_tile_footprint();

	bool is_moving() const {
		return frame_time > 0;
	}

	int get_xtiles() const {    // Dims. in tiles.
		return xtiles;
	}

	int get_ytiles() const {
		return ytiles;
	}

	Tile_coord get_center() const {
		return center;
	}

	~Barge_object() override;

	void set_to_gather() {    // Require 'gather' on next move.
		gathered = false;
	}

	void gather();                    // Gather up objects on barge.
	void face_direction(int ndir);    // Face dir. (0-7).
	// Start rolling/sailing.
	void travel_to_tile(const Tile_coord& dest, int speed);
	void turn_right();    // Turn 90 degrees right.
	void turn_left();
	void turn_around();

	void stop() {    // Stop moving.
		frame_time = 0;
		first_step = true;
	}

	void done();            // No longer being operated.
	int  okay_to_land();    // See if clear to land.

	Barge_object* as_barge() override {
		return this;
	}

	const Barge_object* as_barge() const override {
		return this;
	}

	// For Time_sensitive:
	void handle_event(unsigned long curtime, uintptr udata) override;
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;
	// Remove an object.
	void remove(Game_object* obj) override;
	// Add an object.
	bool add(
			Game_object* obj, bool dont_check = false, bool combine = false,
			bool noset = false) override;
	bool contains(Game_object* obj);
	// Drop another onto this.
	bool drop(Game_object* obj) override;
	// Render.
	void paint() override;
	void activate(int event = 1) override;
	bool edit() override;            // Edit in ExultStudio.
	bool edit_basic_properties();    // Edit basic properties in ExultStudio.
	// Saved from ExultStudio.
	static void update_from_studio(unsigned char* data, int datalen);
	// Step onto an (adjacent) tile.
	bool step(Tile_coord t, int frame = -1, bool force = false) override;
	// Write out to IREG file.
	void write_ireg(ODataSource* out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int  get_ireg_size() override;
	void elements_read() override;    // Called when all member items read.
};

#endif
