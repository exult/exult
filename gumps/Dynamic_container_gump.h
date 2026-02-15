/*
 *  Dynamic_container_gump.h - A container gump with debug visualization.
 *
 *  Copyright (C) 2024-2026  The Exult Team
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

#ifndef DYNAMIC_CONTAINER_GUMP_H
#define DYNAMIC_CONTAINER_GUMP_H

#include "Gump.h"

#include <vector>

class Container_game_object;
class Game_object;
struct Snap_zone;

/**
 * A container gump with debug grid visualization and snap zone support.
 * Uses gump_info.txt for container_area and snap_zones configuration.
 * Displays a white border and grid overlay for debugging placement.
 * Snap zones provide guided object placement for ritual containers.
 * Debug visualization is controlled by Gump_debug_flags bitmask.
 */
class Dynamic_container_gump : public Container_gump {
private:
	bool                        debug_enabled;       // Whether to show debug overlay
	int                         debug_flags_;        // Bitmask of Gump_debug_flags
	mutable int                 paint_log_count;     // Tracks paint calls to log
	const std::vector<Snap_zone>* snap_zones_;       // Pointer to snap zones (owned by Gump_info)

	/**
	 * Paint debug grid overlay showing container area.
	 */
	void paint_debug_grid() const;

	/**
	 * Paint snap zone indicators if debug is enabled.
	 */
	void paint_snap_zones() const;

	/**
	 * Find the best snap zone for the given position.
	 * @param rel_x  X position relative to object_area
	 * @param rel_y  Y position relative to object_area
	 * @return Pointer to best snap zone, or nullptr if none found
	 */
	const Snap_zone* find_snap_zone(int rel_x, int rel_y) const;

public:
	/**
	 * Construct a dynamic container gump.
	 * @param cont       Container object this gump displays
	 * @param initx      Initial X screen position
	 * @param inity      Initial Y screen position
	 * @param shapenum   Gump shape number from gumps.vga
	 * @param debug      Whether to enable debug grid display
	 */
	Dynamic_container_gump(
			Container_game_object* cont,
			int                    initx,
			int                    inity,
			int                    shapenum,
			bool                   debug = false);

	~Dynamic_container_gump() override = default;

	/**
	 * Clone this gump for the given container.
	 */
	Gump* clone(Container_game_object* cont, int initx, int inity) override;

	/**
	 * Add an object to the gump with extensive debug logging.
	 */
	bool add(
			Game_object* obj,
			int          mx        = -1,
			int          my        = -1,
			int          sx        = -1,
			int          sy        = -1,
			bool         dont_check = false,
			bool         combine    = false) override;

	/**
	 * Paint the gump, including background and debug grid if enabled.
	 */
	void paint() override;

	/**
	 * Check if a gump shape has container_area configured in gump_info.txt.
	 * @param shapenum  Gump shape number to check
	 * @return true if container_area is configured for this shape
	 */
	static bool has_config(int shapenum);

	/**
	 * Check if this gump has snap zones configured.
	 * @return true if snap zones are available
	 */
	bool has_snap_zones() const noexcept {
		return snap_zones_ != nullptr && !snap_zones_->empty();
	}
};

#endif    // DYNAMIC_CONTAINER_GUMP_H
