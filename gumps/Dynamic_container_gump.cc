/*
 *  Dynamic_container_gump.cc - A container gump with debug visualization.
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "Dynamic_container_gump.h"

#include "contain.h"
#include "game.h"
#include "gamewin.h"
#include "shapeinf/gumpinf.h"
#include "objiter.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

Dynamic_container_gump::Dynamic_container_gump(
		Container_game_object* cont,
		int                    initx,
		int                    inity,
		int                    shapenum,
		bool                   debug)
		: Container_gump(cont, initx, inity, shapenum),
		  debug_enabled(debug),
		  debug_flags_(GUMP_DEBUG_ALL),
		  paint_log_count(0),
		  snap_zones_(nullptr) {
	// Load snap zones and debug flags from gump_info if available
	const Gump_info* info = Gump_info::get_gump_info(shapenum);
	if (info) {
		if (info->has_snap_zones()) {
			snap_zones_ = &info->snap_zones;
		}
		debug_flags_ = info->debug_flags;
		// Auto-enable debug if any debug flags are set in config
		if (debug_flags_ != 0) {
			debug_enabled = true;
		}
	}

	// Debug: show what we're working with
	if (debug_flags_ & GUMP_DEBUG_CONSOLE) {
		std::cerr << "[DynamicGump] Created for shape " << shapenum 
		          << ", debug=" << debug_enabled
		          << ", debug_flags=0x" << std::hex << debug_flags_ << std::dec
		          << ", object_area=(" << object_area.x << "," << object_area.y
		          << "," << object_area.w << "," << object_area.h << ")"
		          << ", snap_zones=" << (snap_zones_ ? snap_zones_->size() : 0)
		          << std::endl;
	}
}

Gump* Dynamic_container_gump::clone(
		Container_game_object* cont, int initx, int inity) {
	auto* cloned = new Dynamic_container_gump(
			cont, initx, inity, get_shapenum(), debug_enabled);
	// Snap zones pointer will be set in constructor from Gump_info
	return cloned;
}

void Dynamic_container_gump::paint_debug_grid() const {
	if (!debug_enabled) {
		return;
	}

	Game_window*   gwin = Game_window::get_instance();
	Image_window8* win  = gwin->get_win();

	// Get absolute screen position of object area
	const int box_x = get_x() + object_area.x;
	const int box_y = get_y() + object_area.y;

	// Draw object area border (white - color 15)
	if (debug_flags_ & GUMP_DEBUG_BORDER) {
		const unsigned char border_color = 15;    // White
		
		// Top border
		win->fill8(border_color, object_area.w, 1, box_x, box_y);
		// Bottom border
		win->fill8(border_color, object_area.w, 1, box_x, box_y + object_area.h - 1);
		// Left border
		win->fill8(border_color, 1, object_area.h, box_x, box_y);
		// Right border
		win->fill8(border_color, 1, object_area.h, box_x + object_area.w - 1, box_y);
	}

	// Draw grid lines within the object area
	const unsigned char white_color  = 15;    // White
	const unsigned char blue_color   = 9;     // Red (distinct from brown)
	const unsigned char green_color  = 2;     // Green
	const int           grid_spacing = 10;    // 10 pixel grid

	// Major grid: White vertical grid lines (every 10 pixels starting at 0)
	if (debug_flags_ & GUMP_DEBUG_GRID_MAJOR) {
		for (int gx = box_x + grid_spacing; gx < box_x + object_area.w;
			 gx += grid_spacing) {
			win->fill8(white_color, 1, object_area.h, gx, box_y);
		}

		// White horizontal grid lines (every 10 pixels starting at 0)
		for (int gy = box_y + grid_spacing; gy < box_y + object_area.h;
			 gy += grid_spacing) {
			win->fill8(white_color, object_area.w, 1, box_x, gy);
		}
	}

	// Minor grid: Blue/Green lines (every 10 pixels starting at 5)
	if (debug_flags_ & GUMP_DEBUG_GRID_MINOR) {
		// Blue horizontal grid lines (every 10 pixels starting at 5)
		for (int gy = box_y + 5; gy < box_y + object_area.h; gy += grid_spacing) {
			win->fill8(blue_color, object_area.w, 1, box_x, gy);
		}

		// Green vertical grid lines (every 10 pixels starting at 5)
		for (int gx = box_x + 5; gx < box_x + object_area.w; gx += grid_spacing) {
			win->fill8(green_color, 1, object_area.h, gx, box_y);
		}
	}
}

const Snap_zone* Dynamic_container_gump::find_snap_zone(int rel_x, int rel_y) const {
	if (!snap_zones_ || snap_zones_->empty()) {
		return nullptr;
	}

	const bool log_enabled = (debug_flags_ & GUMP_DEBUG_CONSOLE) != 0;

	// Collect all zones that contain this point
	std::vector<const Snap_zone*> candidates;
	for (const auto& zone : *snap_zones_) {
		if (rel_x >= zone.zone_x && rel_x < zone.zone_x + zone.zone_w
			&& rel_y >= zone.zone_y && rel_y < zone.zone_y + zone.zone_h) {
			candidates.push_back(&zone);
		}
	}

	if (candidates.empty()) {
		return nullptr;  // No zone contains this point
	}

	if (candidates.size() == 1) {
		// Only one zone - no conflict
		if (log_enabled) {
			std::cerr << "Snap: Single zone '" << candidates[0]->zone_id
					  << "' owns point (" << rel_x << "," << rel_y << ")" << std::endl;
		}
		return candidates[0];
	}

	// Multiple zones overlap - use axis-aligned split to determine ownership
	if (log_enabled) {
		std::cerr << "Snap: Multiple zones (" << candidates.size() 
				  << ") contain point (" << rel_x << "," << rel_y << ")" << std::endl;
	}

	// Start with highest priority zone
	const Snap_zone* owner = candidates[0];
	for (const auto* zone : candidates) {
		if (zone->priority > owner->priority) {
			owner = zone;
		}
	}

	// Check for overlaps with same-priority zones and resolve via axis-aligned split
	for (const auto* other : candidates) {
		if (other == owner || other->priority != owner->priority) {
			continue;
		}

		// Calculate overlap rectangle
		const int overlap_x = std::max(owner->zone_x, other->zone_x);
		const int overlap_y = std::max(owner->zone_y, other->zone_y);
		const int overlap_x2 = std::min(
				owner->zone_x + owner->zone_w, other->zone_x + other->zone_w);
		const int overlap_y2 = std::min(
				owner->zone_y + owner->zone_h, other->zone_y + other->zone_h);
		const int overlap_w = overlap_x2 - overlap_x;
		const int overlap_h = overlap_y2 - overlap_y;

		if (overlap_w <= 0 || overlap_h <= 0) {
			continue;  // No actual overlap
		}

		// Check if point is in overlap region
		if (rel_x >= overlap_x && rel_x < overlap_x2 && rel_y >= overlap_y
			&& rel_y < overlap_y2) {
			// Point is in overlap - use axis-aligned split
			const int overlap_center_x = overlap_x + overlap_w / 2;
			const int overlap_center_y = overlap_y + overlap_h / 2;

			if (overlap_w > overlap_h) {
				// Wider overlap - split vertically along center
				if (rel_x >= overlap_center_x) {
					// Point is on right side of split
					owner = (other->zone_x > owner->zone_x) ? other : owner;
					if (log_enabled) {
						std::cerr << "  Vertical split at x=" << overlap_center_x
								  << ", point on right, owner: '" << owner->zone_id
								  << "'" << std::endl;
					}
				} else {
					// Point is on left side of split
					owner = (other->zone_x < owner->zone_x) ? other : owner;
					if (log_enabled) {
						std::cerr << "  Vertical split at x=" << overlap_center_x
								  << ", point on left, owner: '" << owner->zone_id
								  << "'" << std::endl;
					}
				}
			} else {
				// Taller overlap - split horizontally along center
				if (rel_y >= overlap_center_y) {
					// Point is on bottom side of split
					owner = (other->zone_y > owner->zone_y) ? other : owner;
					if (log_enabled) {
						std::cerr << "  Horizontal split at y=" << overlap_center_y
								  << ", point on bottom, owner: '" << owner->zone_id
								  << "'" << std::endl;
					}
				} else {
					// Point is on top side of split
					owner = (other->zone_y < owner->zone_y) ? other : owner;
					if (log_enabled) {
						std::cerr << "  Horizontal split at y=" << overlap_center_y
								  << ", point on top, owner: '" << owner->zone_id
								  << "'" << std::endl;
					}
				}
			}
		}
	}

	if (log_enabled) {
		std::cerr << "Snap: Final owner is '" << owner->zone_id << "'" << std::endl;
	}
	return owner;
}

void Dynamic_container_gump::paint_snap_zones() const {
	if (!debug_enabled || !snap_zones_ || snap_zones_->empty()) {
		return;
	}

	// Check if any snap zone visualization is enabled
	if (!(debug_flags_ & (GUMP_DEBUG_SNAP_BORDER | GUMP_DEBUG_SNAP_CROSS))) {
		return;
	}

	Game_window*   gwin = Game_window::get_instance();
	Image_window8* win  = gwin->get_win();

	const int box_x = get_x() + object_area.x;
	const int box_y = get_y() + object_area.y;

	// Color codes for zone types - avoiding grid colors (2,9,15)
	const unsigned char candle_color  = 1;     // Dark color
	const unsigned char reagent_color = 8;     // Medium color
	const unsigned char focus_color   = 13;    // Light color (as requested)
	const unsigned char default_color = 11;    // Another distinct color

	for (const auto& zone : *snap_zones_) {
		unsigned char color = default_color;
		if (zone.zone_type == "candle") {
			color = candle_color;
		} else if (zone.zone_type == "reagent") {
			color = reagent_color;
		} else if (zone.zone_type == "focus") {
			color = focus_color;
		}

		const int zx = box_x + zone.zone_x;
		const int zy = box_y + zone.zone_y;

		// Draw zone border
		if (debug_flags_ & GUMP_DEBUG_SNAP_BORDER) {
			win->fill8(color, zone.zone_w, 1, zx, zy);                          // Top
			win->fill8(color, zone.zone_w, 1, zx, zy + zone.zone_h - 1);        // Bottom
			win->fill8(color, 1, zone.zone_h, zx, zy);                          // Left
			win->fill8(color, 1, zone.zone_h, zx + zone.zone_w - 1, zy);        // Right
		}

		// Draw snap target crosshair
		if (debug_flags_ & GUMP_DEBUG_SNAP_CROSS) {
			const int tx = box_x + zone.snap_x;
			const int ty = box_y + zone.snap_y;
			if (tx >= box_x && tx < box_x + object_area.w
				&& ty >= box_y && ty < box_y + object_area.h) {
				// Horizontal line of crosshair
				if (tx > box_x) {
					win->fill8(color, 3, 1, tx - 1, ty);
				}
				// Vertical line of crosshair
				if (ty > box_y) {
					win->fill8(color, 1, 3, tx, ty - 1);
				}
			}
		}
	}
}

bool Dynamic_container_gump::add(
		Game_object* obj, int mx, int my, int sx, int sy, bool dont_check,
		bool combine) {
	const bool log_enabled = (debug_flags_ & GUMP_DEBUG_CONSOLE) != 0;
	
	if (log_enabled) {
		std::cerr << "[DynamicGump::add] ========== ADD CALLED ==========" << std::endl;
		std::cerr << "[DynamicGump::add] gump shape=" << get_shapenum() << std::endl;
		std::cerr << "[DynamicGump::add] gump pos: x=" << get_x() << ", y=" << get_y() << std::endl;
		std::cerr << "[DynamicGump::add] object_area: x=" << object_area.x 
		          << ", y=" << object_area.y 
		          << ", w=" << object_area.w 
		          << ", h=" << object_area.h << std::endl;
		std::cerr << "[DynamicGump::add] Input: mx=" << mx << ", my=" << my 
		          << ", sx=" << sx << ", sy=" << sy << std::endl;
		std::cerr << "[DynamicGump::add] snap_zones available: " 
		          << (has_snap_zones() ? "yes" : "no") << std::endl;
	}

	// Check for snap zone if we have valid screen coordinates
	int snap_sx = sx;
	int snap_sy = sy;
	if (has_snap_zones() && sx != -1 && sy != -1 && sx != -2 && sy != -2) {
		// Calculate relative position within object_area
		int rel_x = sx - get_x() - object_area.x;
		int rel_y = sy - get_y() - object_area.y;
		
		const Snap_zone* zone = find_snap_zone(rel_x, rel_y);
		if (zone) {
			if (log_enabled) {
				std::cerr << "[DynamicGump::add] SNAP ZONE FOUND: " << zone->zone_id
				          << " (type=" << zone->zone_type << ", priority=" << zone->priority << ")"
				          << std::endl;
				std::cerr << "[DynamicGump::add] Snapping from (" << rel_x << "," << rel_y 
				          << ") to (" << zone->snap_x << "," << zone->snap_y << ")" << std::endl;
			}
			
			// Convert snap target back to screen coordinates
			snap_sx = get_x() + object_area.x + zone->snap_x;
			snap_sy = get_y() + object_area.y + zone->snap_y;
		}
	}
	
	if (log_enabled && obj) {
		std::cerr << "[DynamicGump::add] obj shape=" << obj->get_shapenum() 
		          << ", frame=" << obj->get_framenum() << std::endl;
		std::cerr << "[DynamicGump::add] obj BEFORE: tx=" << obj->get_tx() 
		          << ", ty=" << obj->get_ty() << std::endl;
		
		Shape_frame* shape = obj->get_shape();
		if (shape) {
			std::cerr << "[DynamicGump::add] shape dims: xleft=" << shape->get_xleft()
			          << ", xright=" << shape->get_xright()
			          << ", yabove=" << shape->get_yabove()
			          << ", ybelow=" << shape->get_ybelow()
			          << ", w=" << shape->get_width()
			          << ", h=" << shape->get_height() << std::endl;
		}
	}
	
	// Calculate what the position SHOULD be (using snapped coordinates if available)
	if (log_enabled && snap_sx != -1 && snap_sy != -1 && mx != -1 && my != -1 && snap_sx != -2 && snap_sy != -2) {
		int calc_x = snap_sx - get_x() - object_area.x;
		int calc_y = snap_sy - get_y() - object_area.y;
		std::cerr << "[DynamicGump::add] Calculated relative pos: (" << calc_x << "," << calc_y << ")" << std::endl;
		
		if (obj) {
			Shape_frame* shape = obj->get_shape();
			if (shape) {
				// Apply clamping like parent does
				if (calc_x - shape->get_xleft() < 0) {
					std::cerr << "[DynamicGump::add] CLAMPING X: " << calc_x << " -> " << shape->get_xleft() << std::endl;
					calc_x = shape->get_xleft();
				} else if (calc_x + shape->get_xright() > object_area.w) {
					int new_x = object_area.w - shape->get_xright();
					std::cerr << "[DynamicGump::add] CLAMPING X: " << calc_x << " -> " << new_x << std::endl;
					calc_x = new_x;
				}
				if (calc_y - shape->get_yabove() < 0) {
					std::cerr << "[DynamicGump::add] CLAMPING Y: " << calc_y << " -> " << shape->get_yabove() << std::endl;
					calc_y = shape->get_yabove();
				} else if (calc_y + shape->get_ybelow() > object_area.h) {
					int new_y = object_area.h - shape->get_ybelow();
					std::cerr << "[DynamicGump::add] CLAMPING Y: " << calc_y << " -> " << new_y << std::endl;
					calc_y = new_y;
				}
				std::cerr << "[DynamicGump::add] Final calculated pos after clamping: (" << calc_x << "," << calc_y << ")" << std::endl;
			}
		}
	}
	
	// Call parent add with snapped coordinates
	bool result = Container_gump::add(obj, mx, my, snap_sx, snap_sy, dont_check, combine);
	
	if (log_enabled) {
		std::cerr << "[DynamicGump::add] Parent add returned: " << (result ? "true" : "false") << std::endl;
		
		if (obj) {
			std::cerr << "[DynamicGump::add] obj AFTER: tx=" << obj->get_tx() 
			          << ", ty=" << obj->get_ty() << std::endl;
			
			// Check if position is valid for object_area
			Shape_frame* shape = obj->get_shape();
			if (shape) {
				int objx = obj->get_tx() - shape->get_xleft() + 1 + object_area.x;
				int objy = obj->get_ty() - shape->get_yabove() + 1 + object_area.y;
				bool in_area = object_area.has_point(objx, objy)
				            && object_area.has_point(
				                   objx + shape->get_xright() - 1,
				                   objy + shape->get_ybelow() - 1);
				std::cerr << "[DynamicGump::add] Paint check: objx=" << objx << ", objy=" << objy 
				          << ", in_area=" << (in_area ? "YES" : "NO - WILL BE REPOSITIONED!") << std::endl;
			}
		}
		std::cerr << "[DynamicGump::add] ========== ADD END ==========" << std::endl;
	}
	
	// Reset paint counter so we log the next few paints after add
	paint_log_count = 0;
	
	return result;
}

void Dynamic_container_gump::paint() {
	paint_log_count++;
	
	// Only log the first few paint calls after an add to avoid spam
	const bool log_enabled = (debug_flags_ & GUMP_DEBUG_CONSOLE) != 0;
	bool should_log = log_enabled && (paint_log_count <= 3);
	
	if (should_log) {
		std::cerr << "[DynamicGump::paint] ========== PAINT #" << paint_log_count << " ==========" << std::endl << std::flush;
		std::cerr << "[DynamicGump::paint] gump shape=" << get_shapenum() << std::endl << std::flush;
		std::cerr << "[DynamicGump::paint] gump pos: x=" << get_x() << ", y=" << get_y() << std::endl << std::flush;
		std::cerr << "[DynamicGump::paint] object_area: x=" << object_area.x 
		          << ", y=" << object_area.y 
		          << ", w=" << object_area.w 
		          << ", h=" << object_area.h << std::endl << std::flush;
		
		// Log all object positions BEFORE paint
		Container_game_object* cont = get_container();
		if (cont) {
			Object_list& objects = cont->get_objects();
			if (!objects.is_empty()) {
				std::cerr << "[DynamicGump::paint] Objects BEFORE Container_gump::paint():" << std::endl << std::flush;
				Game_object* obj;
				Object_iterator next(objects);
				while ((obj = next.get_next()) != nullptr) {
					Shape_frame* shape = obj->get_shape();
					if (shape) {
						int objx = obj->get_tx() - shape->get_xleft() + 1 + object_area.x;
						int objy = obj->get_ty() - shape->get_yabove() + 1 + object_area.y;
						bool pt1_ok = object_area.has_point(objx, objy);
						bool pt2_ok = object_area.has_point(
							objx + shape->get_xright() - 1,
							objy + shape->get_ybelow() - 1);
						std::cerr << "  - shape=" << obj->get_shapenum() 
						          << " tx=" << obj->get_tx() << " ty=" << obj->get_ty()
						          << " objx=" << objx << " objy=" << objy
						          << " pt1_in=" << (pt1_ok ? "Y" : "N")
						          << " pt2_in=" << (pt2_ok ? "Y" : "N")
						          << " xleft=" << shape->get_xleft()
						          << " xright=" << shape->get_xright()
						          << " yabove=" << shape->get_yabove()
						          << " ybelow=" << shape->get_ybelow()
						          << std::endl << std::flush;
					}
				}
			}
		}
	}
	
	// Call parent paint
	Container_gump::paint();
	
	if (should_log) {
		// Log all object positions AFTER paint
		Container_game_object* cont = get_container();
		if (cont) {
			Object_list& objects = cont->get_objects();
			if (!objects.is_empty()) {
				std::cerr << "[DynamicGump::paint] Objects AFTER Container_gump::paint():" << std::endl << std::flush;
				Game_object* obj;
				Object_iterator next(objects);
				while ((obj = next.get_next()) != nullptr) {
					std::cerr << "  - shape=" << obj->get_shapenum() 
					          << " tx=" << obj->get_tx() << " ty=" << obj->get_ty()
					          << std::endl << std::flush;
				}
			}
		}
		std::cerr << "[DynamicGump::paint] ========== PAINT END ==========" << std::endl << std::flush;
	}

	// Paint debug overlay if enabled
	paint_debug_grid();
	paint_snap_zones();
}

bool Dynamic_container_gump::has_config(int shapenum) {
	const Gump_info* info = Gump_info::get_gump_info(shapenum);
	return info && info->has_area;
}
