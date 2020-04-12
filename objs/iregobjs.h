/*
 *  iregobjs.h - Ireg (moveable) game objects.
 *
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
 *  Copyright (C) 2000-2013  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef IREGOBJS_H
#define IREGOBJS_H  1

#include "common_types.h"
#include "objs.h"

inline uint8_t nibble_swap(uint8_t val) {
	constexpr const size_t shift = 4;
	constexpr const size_t mask = (8 * sizeof(uint8) - 1);
	return (val << shift) | (val>>( (-shift) & mask));
}

/*
 *  A moveable game object (from 'ireg' files):
 */
class Ireg_game_object : public Game_object {
	Container_game_object *owner = nullptr;   // Container this is in, or 0.
protected:
	unsigned flags: 32;     // 32 flags used in 'usecode'.
	unsigned flags2: 32;    // Another 32 flags used in 'usecode'.
	int lowlift = -1;
	int highshape = -1;
public:
	Ireg_game_object(int shapenum, int framenum, unsigned int tilex,
	                 unsigned int tiley, unsigned int lft = 0)
		: Game_object(shapenum, framenum, tilex, tiley, lft),
		  flags(0), flags2(0)
	{  }
	// Create fake entry.
	Ireg_game_object() : flags(0), flags2(0)
	{  }
	~Ireg_game_object() override = default;
	void set_flags(uint32 f) {  // For initialization.
		flags = f;
	}
	// Render.
	void paint() override;
	void paint_terrain() override {  }
	// Move to new abs. location.
	void move(int newtx, int newty, int newlift, int newmap = -1) override;
	void move(Tile_coord const &t, int newmap = -1) {
		move(t.tx, t.ty, t.tz, newmap);
	}
	// Remove/delete this object.
	void remove_this(Game_object_shared *keep = nullptr) override;
	Container_game_object *get_owner() const override {
		return owner;
	}
	void set_owner(Container_game_object *o) override {
		owner = o;
	}
	bool is_dragable() const override;// Can this be dragged?
	void set_flag(int flag) override {
		if (flag >= 0 && flag < 32)
			flags |= (static_cast<uint32>(1) << flag);
		else if (flag >= 32 && flag < 64)
			flags2 |= (static_cast<uint32>(1) << (flag - 32));
	}
	void clear_flag(int flag) override {
		if (flag >= 0 && flag < 32)
			flags &= ~(static_cast<uint32>(1) << flag);
		else if (flag >= 32 && flag < 64)
			flags2 &= ~(static_cast<uint32>(1) << (flag - 32));
	}
	bool get_flag(int flag) const override {
		if (flag >= 0 && flag < 32)
			return flags & (static_cast<uint32>(1) << flag);
		else if (flag >= 32 && flag < 64)
			return flags2 & (static_cast<uint32>(1) << (flag - 32));
		return false;
	}
	void set_flag_recursively(int flag) override {
		set_flag(flag);
	}
	uint32 get_flags() const {
		return flags;
	}
	uint32 get_flags2() const {
		return flags2;
	}

	// Write common IREG data.
	unsigned char *write_common_ireg(int norm_len, unsigned char *buf);
	int get_common_ireg_size() const {
		if (get_shapenum() >= 1024 || get_framenum() >= 64) {
			return 7;
		}
		if (get_lift() > 15) {
			return 6;
		}
		return 5;
	}
	// Write out to IREG file.
	void write_ireg(ODataSource *out) override;
	// Get size of IREG. Returns -1 if can't write to buffer
	int get_ireg_size() override;
	virtual int get_high_shape() const {
		return highshape;
	}
	virtual void set_high_shape(int s) {
		highshape = s;
	}
	virtual int get_low_lift() const {
		return lowlift;
	}
	virtual void set_low_lift(int l) {
		lowlift = l;
	}
};
using Ireg_game_object_shared = std::shared_ptr<Ireg_game_object>;

#endif
