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

#ifndef GUMPINFO_H
#define GUMPINFO_H

#include <map>
#include <string>
#include <vector>

/**
 * A snap zone defines an area in a container gump where objects
 * automatically snap to a specific position when placed nearby.
 * Used for ritual containers like pentagrams.
 */
/**
 * Debug visualization flags for Dynamic_container_gump.
 * Can be combined as bitmask in gump_info.txt container_area section.
 */
enum Gump_debug_flags {
	GUMP_DEBUG_BORDER      = 0x01,    // Show container area border
	GUMP_DEBUG_GRID_MAJOR  = 0x02,    // Show major grid lines (every 10px)
	GUMP_DEBUG_GRID_MINOR  = 0x04,    // Show minor grid lines (5px offset)
	GUMP_DEBUG_SNAP_BORDER = 0x08,    // Show snap zone borders
	GUMP_DEBUG_SNAP_CROSS  = 0x10,    // Show snap zone crosshairs
	GUMP_DEBUG_CONSOLE     = 0x20,    // Enable console debug logging
	GUMP_DEBUG_ALL         = 0x3F     // All debug features enabled
};

struct Snap_zone {
	std::string zone_id;      // Unique identifier (e.g., "ap_candle")
	int         zone_x;       // Zone area top-left X
	int         zone_y;       // Zone area top-left Y
	int         zone_w;       // Zone area width
	int         zone_h;       // Zone area height
	int         snap_x;       // Snap target X position
	int         snap_y;       // Snap target Y position
	int         priority;     // Higher = takes precedence in overlaps
	std::string zone_type;    // Descriptive category ("candle", "reagent", "focus")

	Snap_zone() : zone_x(0), zone_y(0), zone_w(0), zone_h(0), snap_x(0), snap_y(0), priority(0) {}
};

class Gump_info {
	static std::map<int, Gump_info> gump_info_map;
	static bool                     any_modified;

	friend class Shapes_vga_file;

	bool container_from_patch;
	bool checkmark_from_patch;
	bool special_from_patch;
	bool snapzones_from_patch;

	bool container_modified;
	bool checkmark_modified;
	bool special_modified;
	bool snapzones_modified;

public:
	int  container_x;
	int  container_y;
	int  container_w;
	int  container_h;
	int  debug_flags;    // Bitmask of Gump_debug_flags (default: all enabled)
	int  checkmark_x;
	int  checkmark_y;
	int  checkmark_shape;
	bool has_area;
	bool has_checkmark;
	bool is_checkmark;
	bool is_special;

	std::vector<Snap_zone> snap_zones;    // Snap zones for ritual placement

	Gump_info();

	// Check if we need to write these sections
	bool is_container_dirty() const {
		return container_from_patch || container_modified;
	}

	bool is_checkmark_dirty() const {
		return checkmark_from_patch || checkmark_modified;
	}

	bool is_special_dirty() const {
		return special_from_patch || special_modified;
	}

	bool is_snapzones_dirty() const {
		return snapzones_from_patch || snapzones_modified;
	}

	// Setters for patch flags
	void set_container_from_patch(bool tf) {
		container_from_patch = tf;
	}

	void set_checkmark_from_patch(bool tf) {
		checkmark_from_patch = tf;
	}

	void set_special_from_patch(bool tf) {
		special_from_patch = tf;
	}

	void set_snapzones_from_patch(bool tf) {
		snapzones_from_patch = tf;
	}

	// Setters for modified flags
	void set_container_modified(bool v) {
		container_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_checkmark_modified(bool v) {
		checkmark_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_special_modified(bool v) {
		special_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_snapzones_modified(bool v) {
		snapzones_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	// Check if snap zones are configured for this gump
	bool has_snap_zones() const {
		return !snap_zones.empty();
	}

	static bool was_any_modified() {
		return any_modified;
	}

	static const Gump_info* get_gump_info(int shapenum);
	static Gump_info&       get_or_create_gump_info(int shapenum);
	static void             clear();
};

#endif