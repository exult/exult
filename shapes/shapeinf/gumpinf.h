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

#include "shapeid.h"

#include <map>
#include <string>
#include <vector>

/**
 * A single shortcutbar icon variant (normal or translucent).
 * shapefile_type: 0=shortcutbar.vga, 1=shapes.vga, 2=gumps.vga,
 *                 3=paperdol.vga, -1=hidden/skip slot
 */
struct Shortcutbar_icon_entry {
	int  shapefile_type       = 0;
	int  shape                = 0;
	int  frame                = 0;
	int  extra_frame          = -1;
	bool check_party_item     = false;
	int  fallback_vga         = -1;
	int  fallback_shape       = 0;
	int  fallback_frame       = 0;
	int  fallback_extra_frame = -1;
	bool valid                = false;

	ShapeFile get_shapefile() const {
		switch (shapefile_type) {
		case 1:
			return SF_SHAPES_VGA;
		case 2:
			return SF_GUMPS_VGA;
		case 3:
			return SF_PAPERDOL_VGA;
		default:
			return SF_SHORTCUTBAR_VGA;
		}
	}

	ShapeFile get_fallback_shapefile() const {
		switch (fallback_vga) {
		case 1:
			return SF_SHAPES_VGA;
		case 2:
			return SF_GUMPS_VGA;
		case 3:
			return SF_PAPERDOL_VGA;
		default:
			return SF_SHORTCUTBAR_VGA;
		}
	}
};

/**
 * Shortcutbar icon info for a single slot.
 * Each slot has a normal and translucent variant.
 */
struct Shortcutbar_icon_info {
	Shortcutbar_icon_entry normal;
	Shortcutbar_icon_entry translucent;
	Shortcutbar_icon_entry found;
};

/**
 * Action binding for a shortcutbar slot.
 * primary = single click, secondary = double click.
 * Action strings: Action function names, "activate_item",
 * "activate_item(N)", "inventory".
 */
struct Shortcutbar_action_entry {
	std::string primary_cmd;
	bool        primary_cheat = false;
	std::string secondary_cmd;
	bool        secondary_cheat = false;
	bool        valid           = false;
};

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

struct Dynamic_button_def {
	int button_shape;       // Shape in gumps.vga for button graphic
	int pos_x;              // Position X relative to gump origin
	int pos_y;              // Position Y relative to gump origin
	int default_frame;      // Frame displayed when idle
	int clicked_frame;      // Frame displayed when pushed
	int sound_fx;           // SFX index on click (-1 = none)
	int usecode_fn;         // Usecode function to call (-1 = none)
	int action_type;        // 0=none, 1=close, 2=toggle
	int visibility_flag;    // Global flag controlling visibility (-1 = always)
	int usecode_param;      // Legacy field, read but unused (event is always
							// double_click)

	Dynamic_button_def()
			: button_shape(0), pos_x(0), pos_y(0), default_frame(0), clicked_frame(1), sound_fx(-1), usecode_fn(-1), action_type(0),
			  visibility_flag(-1), usecode_param(-1) {}
};

struct Dynamic_text_def {
	int field_id;    // Unique ID for usecode to address this field
	int pos_x;       // Position X relative to gump origin
	int pos_y;       // Position Y relative to gump origin
	int font_num;    // Font number from fonts.vga
	int halign;      // Horizontal: 0=left (pos_x is left edge),
					 //   1=right (pos_x is right edge), 2=center
	int valign;      // Vertical: 0=top, 1=bottom (pos_y is bottom
					 //   edge), 2=center

	Dynamic_text_def() : field_id(0), pos_x(0), pos_y(0), font_num(0), halign(0), valign(0) {}
};

struct Dynamic_shape_def {
	int field_id;     // Unique ID for usecode to address this field
	int pos_x;        // Position X relative to gump origin
	int pos_y;        // Position Y relative to gump origin
	int shape_num;    // Initial shape number (0 = none until set)
	int frame_num;    // Initial frame number
	int shapefile;    // 0 = SF_SHAPES_VGA, 1 = SF_GUMPS_VGA

	Dynamic_shape_def() : field_id(0), pos_x(0), pos_y(0), shape_num(0), frame_num(0), shapefile(0) {}
};

struct Dynamic_slider_def {
	int field_id;       // Unique ID for usecode to address this slider
	int pos_x;          // Position X relative to gump origin
	int pos_y;          // Position Y relative to gump origin
	int dec_shape;      // gumps.vga shape for decrement arrow
	int inc_shape;      // gumps.vga shape for increment arrow
	int thumb_shape;    // gumps.vga shape for draggable thumb
	int min_val;        // Minimum slider value (inclusive)
	int max_val;        // Maximum slider value (inclusive)
	int step;           // Step size per click/key
	int default_val;    // Initial value
	int extent;         // Sliding region size in pixels
	int usecode_fn;     // Usecode function on change (-1 = none)
	int orientation;    // 0 = vertical, 1 = horizontal

	Dynamic_slider_def()
			: field_id(0), pos_x(0), pos_y(0), dec_shape(0), inc_shape(0), thumb_shape(0), min_val(0), max_val(0), step(1),
			  default_val(0), extent(60), usecode_fn(-1), orientation(0) {}
};

class Gump_info {
	static std::map<int, Gump_info>                gump_info_map;
	static std::map<int, Shortcutbar_icon_info>    shortcutbar_icon_map;
	static std::map<int, Shortcutbar_action_entry> shortcutbar_action_map;
	static bool                                    any_modified;

	friend class Shapes_vga_file;

	bool container_from_patch;
	bool checkmark_from_patch;
	bool special_from_patch;
	bool snapzones_from_patch;
	bool dynbuttons_from_patch;
	bool dyntexts_from_patch;
	bool dynshapes_from_patch;
	bool dynsliders_from_patch;
	bool gumpname_from_patch;

	bool container_modified;
	bool checkmark_modified;
	bool special_modified;
	bool snapzones_modified;
	bool dynbuttons_modified;
	bool dyntexts_modified;
	bool dynshapes_modified;
	bool dynsliders_modified;
	bool gumpname_modified;

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

	std::vector<Snap_zone>          snap_zones;         // Snap zones for ritual placement
	std::vector<Dynamic_button_def> dynamic_buttons;    // Data-driven buttons
	std::vector<Dynamic_text_def>   dynamic_texts;      // Data-driven text fields
	std::vector<Dynamic_shape_def>  dynamic_shapes;     // Data-driven shape displays
	std::vector<Dynamic_slider_def> dynamic_sliders;    // Data-driven sliders
	std::string                     gump_name;          // Custom click-name shown by show_items

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

	bool is_dynbuttons_dirty() const {
		return dynbuttons_from_patch || dynbuttons_modified;
	}

	bool is_dyntexts_dirty() const {
		return dyntexts_from_patch || dyntexts_modified;
	}

	bool is_dynshapes_dirty() const {
		return dynshapes_from_patch || dynshapes_modified;
	}

	bool is_dynsliders_dirty() const {
		return dynsliders_from_patch || dynsliders_modified;
	}

	bool is_gumpname_dirty() const {
		return gumpname_from_patch || gumpname_modified;
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

	void set_dynbuttons_from_patch(bool tf) {
		dynbuttons_from_patch = tf;
	}

	void set_dyntexts_from_patch(bool tf) {
		dyntexts_from_patch = tf;
	}

	void set_dynshapes_from_patch(bool tf) {
		dynshapes_from_patch = tf;
	}

	void set_dynsliders_from_patch(bool tf) {
		dynsliders_from_patch = tf;
	}

	// Clear vector on first patch entry to prevent duplication.
	// Patch entries fully replace any base entries for the same gump.
	void begin_patch_snapzones() {
		if (!snapzones_from_patch) {
			snap_zones.clear();
			snapzones_from_patch = true;
		}
	}

	void begin_patch_dynbuttons() {
		if (!dynbuttons_from_patch) {
			dynamic_buttons.clear();
			dynbuttons_from_patch = true;
		}
	}

	void begin_patch_dyntexts() {
		if (!dyntexts_from_patch) {
			dynamic_texts.clear();
			dyntexts_from_patch = true;
		}
	}

	void begin_patch_dynshapes() {
		if (!dynshapes_from_patch) {
			dynamic_shapes.clear();
			dynshapes_from_patch = true;
		}
	}

	void begin_patch_dynsliders() {
		if (!dynsliders_from_patch) {
			dynamic_sliders.clear();
			dynsliders_from_patch = true;
		}
	}

	void set_gumpname_from_patch(bool tf) {
		gumpname_from_patch = tf;
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

	void set_dynbuttons_modified(bool v) {
		dynbuttons_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_dyntexts_modified(bool v) {
		dyntexts_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_dynshapes_modified(bool v) {
		dynshapes_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_dynsliders_modified(bool v) {
		dynsliders_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	void set_gumpname_modified(bool v) {
		gumpname_modified = v;
		if (v) {
			any_modified = true;
		}
	}

	// Check if snap zones are configured for this gump
	bool has_snap_zones() const {
		return !snap_zones.empty();
	}

	// Check if dynamic buttons are configured for this gump
	bool has_dynamic_buttons() const {
		return !dynamic_buttons.empty();
	}

	// Check if dynamic text fields are configured for this gump
	bool has_dynamic_texts() const {
		return !dynamic_texts.empty();
	}

	bool has_dynamic_shapes() const {
		return !dynamic_shapes.empty();
	}

	bool has_dynamic_sliders() const {
		return !dynamic_sliders.empty();
	}

	bool has_gump_name() const {
		return !gump_name.empty();
	}

	static bool was_any_modified() {
		return any_modified;
	}

	static const Gump_info* get_gump_info(int shapenum);
	static Gump_info&       get_or_create_gump_info(int shapenum);
	static void             clear();

	static const Shortcutbar_icon_info*                get_shortcutbar_icon(int slot);
	static const std::map<int, Shortcutbar_icon_info>& get_shortcutbar_icons();
	static const Shortcutbar_action_entry*             get_shortcutbar_action(int slot);
};

#endif