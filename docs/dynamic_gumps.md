# Gump Updates Branch - Comprehensive Change Documentation

## Overview

This document provides a comprehensive summary of all changes made in the `gumpUpdates` branch. The changes introduce a dynamic container gump system with snap zone support for ritual containers, two new usecode intrinsic functions, and bug fixes discovered during development.

**Design Philosophy:** All changes were made to be as lightweight and consistent with existing Exult code patterns as possible. Where new functionality was needed, I extended existing systems rather than creating parallel alternatives.

---

## Table of Contents

1. [Bug Fixes](#bug-fixes)
   - Bug 1: Empty Container Positioning
   - Bug 2: Configuration Parser
2. [New Intrinsic Functions](#new-intrinsic-functions)
   - `get_item_gump_position(item)`
   - `set_item_gump_position(item, x, y)`
3. [Dynamic Container Gump System](#dynamic-container-gump-system)
   - Overview
   - Snap Zones Feature
   - Debug Visualization
4. [Integration with Exult](#integration-with-exult)
5. [Design Decisions](#design-decisions)
6. [File Change Summary](#file-change-summary)
7. [Potential Impact Analysis](#potential-impact-analysis)
8. [Related Documentation](#related-documentation)

---

## Bug Fixes

### Bug 1: Empty Container Positioning Bug (Gump.cc)

**Location:** `gumps/Gump.cc`, function `check_elem_positions()`

**Problem:**
The container positioning logic in `Gump.cc` caused empty containers after starting a new game to incorrectly rearrange all items to the top-left position by resetting their coordinates to `(255, 255)`. This affected containers with only a single item or empty containers.

**Root Cause:**
The `check_elem_positions()` function was detecting "new game" containers by checking if all objects had the same position. However, it applied this reset unconditionally, including for single-item containers where the single item legitimately occupied a specific position.

**Fix Applied:**
Added a check to only reset positions when **multiple objects** share the same coordinates:

```cpp
void check_elem_positions(Object_list& objects) {
    int count = 0;
    // ... position checking logic ...
    
    // Only reset to 255,255 if all objects have same position AND
    // there are multiple objects (new game detection).
    // Single objects with a set position should not be reset.
    if (count <= 1) {
        return;    // Single object or empty - don't reset
    }
    
    // Reset logic for multi-item same-position containers
    // ...
}
```

**Impact:** Minimal - this is a defensive fix that prevents incorrect repositioning. Existing containers with properly placed items are unaffected.

---

### Bug 2: Snap Zones Parser Bug (shapevga.cc)

**Location:** `shapes/shapevga.cc`, `Snap_zones_functor`

**Problem:**
The snap zones configuration parser was failing to load any zones, resulting in `snap_zones=0` being reported. The issue was traced to a mismatch between the parser implementation and Exult's `ReadInt()` function behavior.

**Root Cause:**
Exult's `ReadInt()` function uses `/` as a field delimiter and automatically consumes characters up to and including the delimiter. The snap zones parser was calling `std::getline(in, zone.zone_type, '/')` for the final field, which looked for a trailing `/` that didn't exist.

**Fix Applied:**
Single-line change - remove the delimiter parameter from the final `getline()` call:

```cpp
// BEFORE (wrong):
std::getline(in, zone.zone_type, '/');

// AFTER (correct):
std::getline(in, zone.zone_type);
```

**Why this works:** `getline()` without a delimiter reads until newline, which is correct for the last field in a configuration line. This matches Exult's standard parsing pattern throughout the codebase.

**Impact:** None on existing functionality - this fix only affects the new snap_zones section parsing.

**Full Technical Explanation:** See [PARSER_FIX_EXPLANATION.md](PARSER_FIX_EXPLANATION.md)

---

## New Intrinsic Functions

Two new usecode intrinsic functions were added to enable programmatic inspection and manipulation of item positions within container gumps.

### `get_item_gump_position(item)`

**Location:** `usecode/intrinsics.cc`

**Purpose:** Returns the `[x, y]` coordinates of any item within its currently open container gump.

**Usage:**
```usecode
var pos = UI_get_item_gump_position(item);
if (pos) {
    var x = pos[0];
    var y = pos[1];
}
```

**Returns:**
- `[x, y]` array if the item is in an open container gump
- Empty array if item is invalid, not in a container, or container gump is not open

**Implementation:**
```cpp
USECODE_INTRINSIC(get_item_gump_position) {
    Game_object* item = get_item(parms[0]);
    if (!item) return Usecode_value(0, nullptr);  // Empty array
    
    Game_object* owner = item->get_owner();
    if (!owner) return Usecode_value(0, nullptr);
    
    Gump* gump = gumpman->find_gump(owner);
    if (!gump) return Usecode_value(0, nullptr);
    
    // Return [tx, ty] coordinates
    Usecode_value arr(2, nullptr);
    arr.put_elem(0, Usecode_value(item->get_tx()));
    arr.put_elem(1, Usecode_value(item->get_ty()));
    return arr;
}
```

---

### `set_item_gump_position(item, x, y)`

**Location:** `usecode/intrinsics.cc`

**Purpose:** Programmatically repositions items within container gumps to specific coordinates.

**Usage:**
```usecode
var success = UI_set_item_gump_position(item, 65, 65);
```

**Returns:**
- `1` on success
- `0` on failure (invalid item, not in container, gump not open, or negative coordinates)

**Implementation:**
```cpp
USECODE_INTRINSIC(set_item_gump_position) {
    Game_object* item = get_item(parms[0]);
    if (!item) return Usecode_value(0);
    
    const int new_x = parms[1].get_int_value();
    const int new_y = parms[2].get_int_value();
    if (new_x < 0 || new_y < 0) return Usecode_value(0);
    
    Game_object* owner = item->get_owner();
    if (!owner) return Usecode_value(0);
    
    Gump* gump = gumpman->find_gump(owner);
    if (!gump) return Usecode_value(0);
    
    item->set_shape_pos(new_x, new_y);
    gwin->set_all_dirty();
    return Usecode_value(1);
}
```

### Intrinsic Registration

Both intrinsics are registered in the intrinsic tables:
- **Black Gate:** `usecode/bgintrinsics.h` at index `0xdd` and `0xde`
- **Serpent Isle:** `usecode/siintrinsics.h` at index `0xde` and `0xdf`

**Impact Analysis:**
- New intrinsics are additive and do not modify existing intrinsic behavior
- Uses existing Exult API calls (`get_owner()`, `find_gump()`, `set_shape_pos()`)
- Properly validates all inputs before performing operations

---

## Dynamic Container Gump System

### Overview

A new `Dynamic_container_gump` class was implemented to provide:
1. **Debug visualization** for container area boundaries
2. **Snap zone support** for guided item placement in ritual containers

**Files Added:**
- `gumps/Dynamic_container_gump.h`
- `gumps/Dynamic_container_gump.cc`

### Class Hierarchy

```
Gump (base)
  └── Container_gump
        └── Dynamic_container_gump (NEW)
```

`Dynamic_container_gump` extends `Container_gump` and overrides:
- `paint()` - adds debug visualization
- `add()` - implements snap zone logic
- `clone()` - proper cloning support

### Snap Zones Feature

**Purpose:** Enables guided object placement for complex rituals (like Ultima 8 sorcery), where items must be positioned precisely at specific locations.

**How it works:**
1. Snap zones are defined in `gump_info.txt` under a `snap_zones` section
2. When an item is dropped in a zone, it automatically snaps to the zone's target position
3. Zone ownership is resolved via a hybrid priority + axis-aligned split algorithm

**Configuration Format:**
```ini
%%section snap_zones
# Format: :shape/zone_id/zone_x/zone_y/zone_w/zone_h/snap_x/snap_y/priority/zone_type
:111/pz_candle/20/0/20/20/25/5/1/candle
:111/locus/55/55/20/20/65/65/3/focus
%%endsection
```

**Zone Resolution Algorithm:**
1. **Priority-based:** Higher priority zones take precedence
2. **Axis-aligned split:** When zones have equal priority and overlap, ownership is resolved geometrically

**Full Design Documentation:** See [SNAP_ZONES_DESIGN.md](SNAP_ZONES_DESIGN.md) and [SNAP_ZONES_AXIS_SPLIT.md](SNAP_ZONES_AXIS_SPLIT.md)

### Debug Visualization

Debug features are controlled by a bitmask in the `container_area` config:

| Flag | Value | Description |
|------|-------|-------------|
| `GUMP_DEBUG_BORDER` | 0x01 | Container area border |
| `GUMP_DEBUG_GRID_MAJOR` | 0x02 | Major grid lines (10px) |
| `GUMP_DEBUG_GRID_MINOR` | 0x04 | Minor grid lines (5px offset) |
| `GUMP_DEBUG_SNAP_BORDER` | 0x08 | Snap zone borders |
| `GUMP_DEBUG_SNAP_CROSS` | 0x10 | Snap zone crosshairs |
| `GUMP_DEBUG_CONSOLE` | 0x20 | Console debug logging |
| `GUMP_DEBUG_ALL` | 0x3F | All features |

**Configuration Example:**
```ini
%%section container_area
:111/30/20/130/130/63    # All debug enabled (63 = 0x3F)
:111/30/20/130/130/0     # No debug (production)
%%endsection
```

---

## Integration with Exult

### Automatic Gump Selection

The `Gump_manager` was modified to automatically select `Dynamic_container_gump` when a gump shape has configuration in `gump_info.txt`:

**Location:** `gumps/Gump_manager.cc`, `add_gump()` function

```cpp
// Check if this gump shape has container_area in gump_info.txt
if (Dynamic_container_gump::has_config(shapenum)) {
    auto* dgump = new Dynamic_container_gump(cont, px, py, shapenum, false);
    // ...
}
```

This ensures backward compatibility - containers without `gump_info.txt` entries continue using the standard `Container_gump`.

### Configuration System Integration

**Why `gump_info.txt`?**

I chose `gump_info.txt` over alternatives like `gump_area_info.txt` because:

1. **Already exists in Exult:** The file and parsing infrastructure (`Gump_info` class, `Read_Gumpinf_text_data_file()`) were already established
2. **Already contains container_area section:** Our snap_zones naturally belong alongside container_area configuration
3. **Consistent naming:** Follows Exult's `*_info.txt` convention (e.g., `shape_info.txt`, `paperdol_info.txt`)
4. **No new file handling:** Reuses existing file path constants (`GUMP_INFO`, `PATCH_GUMP_INFO` in `fnames.h`)

### Parser Integration

The snap_zones section was added to the existing `Read_Gumpinf_text_data_file()` function:

```cpp
std::array sections{
    "container_area"sv, "checkmark_pos"sv, "special"sv, "snap_zones"sv};  // Added snap_zones

// ... existing functors ...

struct Snap_zones_functor {
    bool operator()(...) const {
        // Uses existing ReadInt() pattern
        zone.zone_x = ReadInt(in);
        zone.zone_y = ReadInt(in);
        // ...
    }
};
```

### Existing API Usage

Throughout the implementation, I used existing Exult APIs rather than creating new ones:

| Need | Exult API Used |
|------|----------------|
| Get gump manager | `gumpman` global |
| Find container gump | `gumpman->find_gump()` |
| Read integers from config | `ReadInt()` from `files/utils.h` |
| Shape information | `Shape_frame`, `get_shape()` |
| Game window | `Game_window::get_instance()` |
| Image window painting | `Image_window8::fill8()` |
| Object positioning | `set_shape_pos()`, `get_tx()`, `get_ty()` |

---

## Design Decisions

### 1. Extending Container_gump vs. Modifying It

**Decision:** Create new `Dynamic_container_gump` subclass

**Rationale:**
- Avoids modifying the well-tested `Container_gump` class
- Isolates new functionality for easier maintenance
- Allows selective use via `gump_info.txt` configuration
- Can be disabled entirely by removing configuration entries

### 2. Pointer to Snap Zones vs. Copying

**Decision:** Store pointer to `Gump_info`'s snap_zones vector

```cpp
const std::vector<Snap_zone>* snap_zones_;  // Pointer, not copy
```

**Rationale:**
- `Gump_info` is a singleton-like shared resource
- Avoids memory duplication
- Zones are read-only at runtime
- Ownership remains clear (Gump_info owns the data)

### 3. Debug Flags as Bitmask

**Decision:** Use bitmask enum for debug flags

**Rationale:**
- Matches existing Exult patterns (various flags throughout codebase)
- Allows granular control for modders and developers
- Single integer field in configuration
- Easy to combine and check flags

### 4. Hybrid Priority + Geometric Resolution

**Decision:** Use priorities for explicit control, geometric splitting for ties

**Rationale:**
- Pure priority can create "dead zones"
- Pure geometric can't express intentional precedence
- Hybrid provides both explicit control and automatic conflict resolution
- Documented in [SNAP_ZONES_AXIS_SPLIT.md](SNAP_ZONES_AXIS_SPLIT.md)

---

## File Change Summary

### New Files

| File | Purpose |
|------|---------|
| `gumps/Dynamic_container_gump.h` | Header for dynamic gump class |
| `gumps/Dynamic_container_gump.cc` | Implementation of dynamic gump class |
| `PARSER_FIX_EXPLANATION.md` | Technical documentation for parser bug fix |
| `SNAP_ZONES_DESIGN.md` | Design document for snap zones system |
| `SNAP_ZONES_AXIS_SPLIT.md` | Algorithm documentation for overlap resolution |
| `GUMP_UPDATES_CHANGELOG.md` | This document |

### Modified Files (Key Changes Only)

| File | Changes |
|------|---------|
| `gumps/Gump.cc` | Bug fix for `check_elem_positions()` |
| `shapes/shapevga.cc` | Added `snap_zones` section parser, fixed `getline()` call |
| `shapes/shapeinf/gumpinf.h` | Added `Snap_zone` struct, `Gump_debug_flags` enum, members to `Gump_info` |
| `usecode/intrinsics.cc` | Added two new intrinsic implementations |
| `usecode/ucinternal.h` | Added intrinsic declarations |
| `usecode/bgintrinsics.h` | Registered BG intrinsics |
| `usecode/siintrinsics.h` | Registered SI intrinsics |
| `gumps/Gump_manager.cc` | Auto-selection of Dynamic_container_gump |
| `data/bg/gump_info.txt` | Updated template with snap_zones section and improved documentation |
| `data/si/gump_info.txt` | Updated template with snap_zones section and improved documentation |

### Template File Updates

**Files Modified:** `data/bg/gump_info.txt`, `data/si/gump_info.txt`

These are the template configuration files that get packaged with Black Gate and Serpent Isle distributions. They were updated to:

1. **Add snap_zones section**: Comprehensive documentation and examples for the new snap zones feature
2. **Document debug flags**: Added detailed bitmask flag table with hex and decimal values for container_area
3. **Update file header**: Added mention of all four sections (container_area, checkmark_pos, special, snap_zones)

**Rationale for Changes:**
- **Completeness**: Template files should document all available features so modders can discover functionality
- **Consistency**: Both BG and SI templates now have identical documentation structure
- **Examples included**: Commented-out pentagram example shows modders how to use snap zones

**Sample of improved documentation:**
```ini
# Format: :gump_shape/x/y/width/height/debug_flags
#         (debug_flags is optional - omit the field entirely if not needed)
#
# gump_shape  = Shape number in gumps.vga
# x/y         = Top-left corner of the container area (relative to gump origin)
# width/height= Size of the container area (not bottom-right corner)
# debug_flags = Optional bitmask for debug visualization (omit field or use 0 for disabled)
#               0x01 (1)  = Border outline
#               0x02 (2)  = Major grid lines
#               ...
```

**Impact:**
- **Zero breaking changes**: Existing configurations continue to work unchanged
- **Better modder experience**: Clear documentation helps modders create custom container gumps
- **Professional presentation**: Matches quality of other Exult documentation files

---

## Potential Impact Analysis

### Low Risk

| Area | Assessment |
|------|------------|
| **Existing containers** | No impact - only containers with `gump_info.txt` entries use new code |
| **Existing intrinsics** | No impact - new intrinsics are additive |
| **Game saves** | No impact - no new save data structures |
| **Configuration parsing** | Low risk - new section added, existing sections unchanged |

### Areas Requiring Attention

#### 1. Debug Output Volume

**Concern:** When `GUMP_DEBUG_CONSOLE` is enabled, significant output is written to `stderr.txt`

**Mitigation:** Debug flags default to 0 (disabled) in production. Only enables when explicitly configured.

**Recommendation:** Consider adding a compile-time flag to strip debug logging in release builds if you are worried about such things.

#### 2. Intrinsic Indices

**Concern:** New intrinsics occupy specific indices (`0xdd`, `0xde` for BG; `0xde`, `0xdf` for SI)

**Mitigation:** These indices were selected to be beyond existing used intrinsics.

**Risk:** If upstream Exult adds intrinsics at these indices, a conflict would occur.

**Recommendation:** Exult team to register these inicies in docs.

#### 3. Gump_info Memory Lifetime

**Concern:** `Dynamic_container_gump` holds a pointer to `Gump_info`'s snap_zones vector

**Mitigation:** `Gump_info` is a static map that persists for the entire game session. The pointer remains valid as long as the gump exists.

**Risk:** If `Gump_info` were to be cleared or reloaded during gameplay, pointers could become invalid.

**Recommendation:** Document this assumption. Consider weak reference or notification pattern if hot-reloading is ever needed.

#### 4. Container_gump Virtual Override

**Concern:** `Dynamic_container_gump` overrides `add()` and `paint()` virtual methods

**Mitigation:** Properly calls base class methods (`Container_gump::add()`, `Container_gump::paint()`)

**Risk:** If `Container_gump`'s behavior changes significantly, `Dynamic_container_gump` may need updates.

**Recommendation:** Ensure unit tests cover both base and derived gump behavior.

---

## Related Documentation

- [PARSER_FIX_EXPLANATION.md](PARSER_FIX_EXPLANATION.md) - Detailed explanation of the parser bug and fix
- [SNAP_ZONES_DESIGN.md](SNAP_ZONES_DESIGN.md) - Full design document for snap zones feature
- [SNAP_ZONES_AXIS_SPLIT.md](SNAP_ZONES_AXIS_SPLIT.md) - Algorithm details for overlap resolution

---

## Version History

| Date | Author | Description |
|------|--------|-------------|
| 2026-02-15 | theGreyWanderer-uc | Initial documentation |

