# Dynamic Gump System — Implementation Summary

## Overview

This document describes the dynamic gump system added in the `gumpUpdates` branch. The system provides data-driven container gumps with configurable widgets (buttons, text fields, shape displays, sliders), snap zones for ritual containers, debug visualization, and seven new usecode intrinsic functions — all driven by `gump_info.txt` configuration without C++ changes for each new gump layout.

**Design Philosophy:** Extend existing Exult systems rather than creating parallel alternatives. All configuration reuses the established `gump_info.txt` file and its `ReadInt()`-based parser. Widget classes follow Exult's `Gump_widget`/`Gump_button` inheritance hierarchy.

---

## Table of Contents

1. [Bug Fixes](#bug-fixes)
   - Bug 1: Empty Container Positioning
   - Bug 2: Configuration Parser
   - Bug 3: Dynamic Button Frame Management
   - Bug 8: Slider Track Hit Area Unbounded
2. [New Intrinsic Functions](#new-intrinsic-functions)
   - `UI_get_item_gump_position(item)` — BG: 0xdd, SI: 0xde
   - `UI_set_item_gump_position(item, x, y)` — BG: 0xde, SI: 0xdf
   - `UI_set_gump_text(item, field_id, text)` — BG: 0xdf, SI: 0xe0
   - `UI_set_gump_shape(item, field_id, shape, frame)` — BG: 0xe0, SI: 0xe1
   - `UI_set_slider_value(item, field_id, value)` — BG: 0xe1, SI: 0xe2
   - `UI_get_slider_value(item, field_id)` — BG: 0xe2, SI: 0xe3
3. [Dynamic Container Gump System](#dynamic-container-gump-system)
   - Overview & Class Hierarchy
   - Bounding-Box Hit Testing
   - Snap Zones Feature
   - Debug Visualization
4. [Dynamic Widget System](#dynamic-widget-system)
   - Dynamic Buttons
   - Dynamic Text Fields
   - Dynamic Shape Displays
   - Dynamic Sliders
5. [Integration with Exult](#integration-with-exult)
   - Automatic Gump Selection
   - On-Open Usecode Invocation
   - Widget Creation in Gump_manager
   - Configuration System
6. [Configuration Reference](#configuration-reference)
7. [Design Decisions](#design-decisions)
8. [File Change Summary](#file-change-summary)
9. [Potential Impact Analysis](#potential-impact-analysis)

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
if (count <= 1) {
    return;    // Single object or empty - don't reset
}
```

**Impact:** Minimal — defensive fix. Existing containers with properly placed items are unaffected.

---

### Bug 2: Snap Zones Parser Bug (shapevga.cc)

**Location:** `shapes/shapevga.cc`, `Snap_zones_functor`

**Problem:**
The snap zones configuration parser was failing to load any zones, resulting in `snap_zones=0` being reported. The issue was traced to a mismatch between the parser implementation and Exult's `ReadInt()` function behavior.

**Root Cause:**
Exult's `ReadInt()` function uses `/` as a field delimiter and automatically consumes characters up to and including the delimiter. The snap zones parser was calling `std::getline(in, zone.zone_type, '/')` for the final field, which looked for a trailing `/` that didn't exist.

**Fix Applied:**
Single-line change — remove the delimiter parameter from the final `getline()` call:

```cpp
// BEFORE (wrong):
std::getline(in, zone.zone_type, '/');

// AFTER (correct):
std::getline(in, zone.zone_type);
```

**Impact:** None on existing functionality — this fix only affects the new snap_zones section parsing.

---

### Bug 3: Dynamic Button Frame Management (Dynamic_button.cc)

**Location:** `gumps/Dynamic_button.cc`, `paint()` method

**Problem:**
Dynamic buttons would visually push/unpush correctly but never execute their usecode callback. The release hit-test (`on_button()`) always returned MISS.

**Root Cause:**
`Dynamic_button::paint()` was setting the frame to `clicked_frame` (1) when pushed, then delegating to `Gump_button::paint()` which *also* adjusts the frame (`set_frame(prev_frame + (is_pushed() ? 1 : 0))`). This double-adjustment caused:

1. Frame set to `clicked_frame` (1) by `Dynamic_button::paint()`
2. `Gump_button::paint()` saves frame 1, sets frame to 2 (1+1), paints, restores to 1
3. Frame is now stuck at 1 outside of painting
4. On mouse release, `on_widget()` calls `get_shape()` which returns frame 1's `Shape_frame`
5. If frame 1 has a different shape origin than frame 0 (e.g., origin (0,0) vs (51,10)), the RLE pixel-level hit-test uses wrong scan coordinates → MISS
6. `activate()` is never reached

**Fix Applied:**
`Dynamic_button::paint()` now paints directly instead of delegating to `Gump_button::paint()`. It properly saves and restores the frame so that frame 0 (`default_frame`) stays current outside of painting:

```cpp
void Dynamic_button::paint() {
    if (!is_visible()) { return; }
    const int paint_frame = (action_type == 2 && toggled) ? clicked_frame
                           : is_pushed()                  ? clicked_frame
                                                          : default_frame;
    int sx = 0, sy = 0;
    local_to_screen(sx, sy);

    const int prev_frame = get_framenum();
    set_frame(paint_frame);
    paint_shape(sx, sy);
    set_frame(prev_frame);
}
```

This ensures `on_widget()` always tests hit against `default_frame`'s shape, whose origin matches the configured position.

**Impact:** Fixes all `Dynamic_button` usecode execution. No effect on other button types.

---

### Bug 4: Slider Arrow Buttons Not Clickable (Dynamic_slider.cc)

**Location:** `gumps/Dynamic_slider.h`, `gumps/Dynamic_slider.cc`

**Problem:** Clicking the up/down arrow buttons on a `Dynamic_slider` had no effect — the slider value didn't change and no usecode callback fired.

**Root Cause:** `DynSlider_button` instances are created with `parent = this` (the `Dynamic_slider` widget), not the parent `Gump`. When `Gump::on_button()` iterates its `elems` and calls `w->on_button()`, `Dynamic_slider` inherited the default `Gump_widget::on_button()` which returns `nullptr`. The arrow buttons were invisible to the click dispatch chain.

**Fix Applied:** Added `on_button()` override to `Dynamic_slider` that delegates to its child arrow buttons:

```cpp
Gump_button* Dynamic_slider::on_button(int mx, int my) {
    Gump_button* hit = nullptr;
    if (dec_btn) { hit = dec_btn->on_button(mx, my); }
    if (!hit && inc_btn) { hit = inc_btn->on_button(mx, my); }
    return hit;
}
```

**Impact:** Enables slider arrow button clicks. No effect on other widget types.

---

### Bug 5: Slider Thumb Drag Moves Entire Gump (drag.cc)

**Location:** `gumps/Gump.h`, `gumps/Gump.cc`, `drag.h`, `drag.cc`

**Problem:** Clicking and dragging the slider thumb caused the entire gump to move instead of scrolling the slider.

**Root Cause:** Non-modal gumps route ALL mouse events through `Dragging_info`. The drag system only knew about three possibilities: finding objects, finding buttons (push/activate), or gump dragging. There was no path for widget-level mouse event handling. When `on_button()` returned null for the slider thumb area, the system fell through to gump drag.

**Fix Applied:**
1. Added `Gump::forward_mouse_down(mx, my, button)` virtual method that iterates `elems` and calls `w->mouse_down()` on each widget. Returns the widget that handled the event, or `nullptr`.
2. Added `mouse_widget` and `widget_gump` members to `Dragging_info`.
3. Modified `Dragging_info` constructor: after `on_button` fails, calls `forward_mouse_down`. If a widget captures the click, stores it in `mouse_widget` and sets `gump = nullptr` to prevent gump dragging.
4. Modified `Dragging_info::moved()`: if `mouse_widget` is set, forwards drag to `mouse_widget->mouse_drag()`.
5. Modified `Dragging_info::drop()`: if `mouse_widget` is set, forwards to `mouse_widget->mouse_up()`.

`Dynamic_slider::mouse_down()` tries buttons first (they return false), then checks thumb hit (starts drag), then track click (jump to position). Text/shape widgets inherit default `mouse_down()` returning false, so they are safely skipped.

**Impact:** Enables slider thumb dragging and track clicking. No effect on other gump types.

---

### Bug 6: Slider Arrow Hit-Testing Fails Through 3-Level Parent Chain (Dynamic_slider.cc)

**Location:** `gumps/Dynamic_slider.h`, `gumps/Dynamic_slider.cc`, `gumps/Gump.cc`

**Problem:** Clicking the slider up/down arrow buttons (shapes 117/118) caused the buttons to visually disappear instead of showing frame 1. The slider value didn't change. Clicks either fell through to track-click (jumping the thumb) or to gump dragging.

**Root Cause:** The Bug 4 fix (`on_button()` override delegating to child buttons) relied on `Gump_widget::on_widget()` for pixel hit-testing, which calls `screen_to_local()` through the parent chain: `DynSlider_button → Dynamic_slider → Gump`. This 3-level coordinate chain never correctly resolved screen coordinates to button-local coordinates. The `has_point()` test always returned false.

Evidence from logs:
- stderr: Zero `[Drag::drop]` entries for shapes 117/118 (on_button never found them)
- stdout: Some clicks fell through to gump dragging (`(x,y) rel. to gump is`)
- Remaining clicks were captured by the track-click handler (jumping the thumb)

**Fix Applied:** Removed the `on_button()` override entirely. Instead, all arrow click handling is done at the `Dynamic_slider` level:

1. Added `int arrow_clicked` member (-1=dec pressed, 0=none, 1=inc pressed)
2. Added `hit_arrow(mx, my)` method that converts screen→slider-local coords (single `screen_to_local` call, 2-level chain only) then tests button shapes with slider-relative offsets
3. `mouse_down()` calls `hit_arrow()` first; if an arrow is hit, sets `arrow_clicked` and calls `move_value()`
4. `mouse_up()` clears `arrow_clicked` state
5. `paint()` directly renders buttons with frame toggling: sets frame 1 when pressed (with safety fallback if frame 1 doesn't exist), paints, restores frame 0
6. `run()` auto-repeat uses `arrow_clicked` state directly
7. `Input_first()` returns `this` when `arrow_clicked != 0`

The `DynSlider_button` objects are now used purely as shape containers + position holders. All click logic, visual frame toggling, and auto-repeat are handled at the `Dynamic_slider` level.

**Impact:** Fixes slider arrow button clicks for all dynamic sliders. No effect on other widget types.

---

### Bug 7: Slider Thumb Travel Uses Height for Both Orientations (Dynamic_slider.cc)

**Location:** `gumps/Dynamic_slider.cc`, constructor

**Problem:** The `thumb_travel` calculation always subtracted `get_height()` from `track_extent`, regardless of slider orientation. For horizontal sliders, this should subtract the thumb's width (the dimension along the travel axis). Using height for a horizontal slider means a tall-but-narrow thumb would barely reduce the travel distance, causing the thumb to overrun the increment button just as the vertical slider's thumb previously overlapped the bottom button.

**Root Cause:** The original implementation only deployed a vertical slider, so the height-only calculation worked. The horizontal path was never tested.

**Fix Applied:**

```cpp
// BEFORE (wrong for horizontal):
int th = tf ? tf->get_height() : 0;

// AFTER (correct for both orientations):
int th = tf ? (vertical ? tf->get_height() : tf->get_width()) : 0;
```

**Impact:** Prevents horizontal slider thumb from overlapping the increment button. No effect on vertical sliders (which already used the correct dimension).

---

### Bug 8: Slider Track Hit Area Unbounded (Dynamic_slider.cc)

**Location:** `gumps/Dynamic_slider.cc`, `mouse_down()` — track-click region check

**Problem:** Clicking anywhere to the right of a vertical slider (or below a horizontal slider) registered as a track click, causing the thumb to jump. The slider responded to clicks far outside its visual bounds.

**Root Cause:** The track-click hit test checked `lx >= 0` for vertical sliders (and `ly >= 0` for horizontal) without an upper bound. Any click at `lx >= 0` with a valid `ly` in the track range would be treated as a track click, regardless of how far right of the slider it was.

**Fix Applied:**

Added upper-bound checks using the thumb shape's width (vertical) or height (horizontal) as the track's cross-axis extent:

```cpp
// Vertical slider — BEFORE:
if (lx >= 0 && ly >= track_start && ly <= track_start + track_extent)

// Vertical slider — AFTER:
int track_w = 16;
auto* tw = thumb.get_shape();
if (tw) {
    track_w = tw->get_width();
}
if (lx >= 0 && lx < track_w
        && ly >= track_start
        && ly <= track_start + track_extent)

// Horizontal slider — same pattern with track_h and ly < track_h
```

**Impact:** Sliders no longer capture clicks outside their visual track area. Other widgets (buttons, text, shapes) to the right of a vertical slider now receive clicks correctly.

---

## New Intrinsic Functions

Seven new usecode intrinsic functions were added to enable programmatic control of container gump contents and dynamic widgets.

### Intrinsic Slot Summary

| Intrinsic | BG Slot | SI Slot | Purpose |
|-----------|---------|---------|---------|
| `UI_get_item_gump_position` | 0xdd | 0xde | Get item [x,y] within container gump |
| `UI_set_item_gump_position` | 0xde | 0xdf | Move item to [x,y] within container gump |
| `UI_set_gump_text` | 0xdf | 0xe0 | Set text on a Dynamic_text_widget |
| `UI_set_gump_shape` | 0xe0 | 0xe1 | Set shape/frame on a Dynamic_shape_widget |
| `UI_set_slider_value` | 0xe1 | 0xe2 | Set value on a Dynamic_slider |
| `UI_get_slider_value` | 0xe2 | 0xe3 | Read current value of a Dynamic_slider |
| `UI_set_gump_text_font` | 0xe3 | 0xe4 | Change font of a Dynamic_text_widget |

### `UI_get_item_gump_position(item)`

**Returns:** `[x, y]` array if the item is in an open container gump, empty array otherwise.

```usecode
var pos = UI_get_item_gump_position(item);
if (pos) {
    var x = pos[0];
    var y = pos[1];
}
```

### `UI_set_item_gump_position(item, x, y)`

**Returns:** `1` on success, `0` on failure (invalid item, not in container, gump not open, or negative coordinates).

```usecode
var success = UI_set_item_gump_position(item, 65, 65);
```

### `UI_set_gump_text(item, field_id, text)`

Sets the displayed text on a `Dynamic_text_widget` identified by `field_id`. The `item` parameter must be the container whose gump contains the text widget.

```usecode
UI_set_gump_text(item, 1, "Gold: 500");
```

**Implementation:** Finds the gump for `item`, iterates `gump->get_elems()`, matches `Dynamic_text_widget` by `field_id`, calls `set_text()`.

### `UI_set_gump_shape(item, field_id, shape, frame)`

Sets the displayed shape on a `Dynamic_shape_widget` identified by `field_id`.

```usecode
UI_set_gump_shape(item, 2, 604, 0);  // Display glass sword shape
```

### `UI_set_slider_value(item, field_id, value)`

Sets the current value of a `Dynamic_slider` identified by `field_id`.

```usecode
UI_set_slider_value(item, 3, 50);
```

### `UI_get_slider_value(item, field_id)`

**Returns:** Current integer value of the slider, or `0` if not found.

```usecode
var qty = UI_get_slider_value(item, 3);
```

### `UI_set_gump_text_font(item, field_id, font_num)`

Changes the font used by a `Dynamic_text_widget` at runtime. The `font_num` refers to a font shape in the active font file (e.g., `fonts_original.vga`). Useful for highlighting/dimming text labels dynamically.

```usecode
// Highlight active currency with font 17, dim others with font 2
UI_set_gump_text_font(item, 30, 17);
UI_set_gump_text_font(item, 31, 2);
```

**Implementation:** Finds the gump for `item`, iterates `gump->get_elems()`, matches `Dynamic_text_widget` by `field_id`, calls `set_font(font_num)`.

### Intrinsic Registration

- **Black Gate:** `usecode/bgintrinsics.h` — slots 0xdd–0xe3
- **Serpent Isle:** `usecode/siintrinsics.h` — slots 0xde–0xe4
- **Serpent Isle Beta:** `usecode/sibetaintrinsics.h` — slot 0xe4 for `set_gump_text_font`
- **Declarations:** `usecode/ucinternal.h`
- **Implementations:** `usecode/intrinsics.cc`

---

## Dynamic Container Gump System

### Overview

`Dynamic_container_gump` extends `Container_gump` to provide:
1. **Bounding-box hit testing** for gumps with non-zero shape origins
2. **Debug visualization** for container area boundaries and snap zones
3. **Snap zone support** for guided item placement in ritual containers
4. **Dynamic widget hosting** — buttons, text fields, shape displays, and sliders are created automatically from `gump_info.txt` configuration
5. **Custom click name** — `get_click_name()` returns the `gump_name` from config for status bar display
6. **Screen centering** — gumps are positioned at the center of the game window using `get_rect()` bounding box
7. **Keyboard input** — `handle_kbd_event()` translates SDL key events and forwards `key_down()` to child widgets (sliders respond to arrow keys)
8. **Mouse wheel dispatch** — `mousewheel_up/down()` forward wheel events to child widgets; `Gump_manager::handle_mouse_wheel()` prevents game-world cheat-scroll from firing through open gumps
9. **Auto-repeat** — `update_gump()` calls `run()` on child widgets each frame, enabling slider arrow button auto-repeat (500ms initial, 50ms repeat)

### Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump
  └── Gump_widget
        ├── Gump_button
        │     └── Dynamic_button
        ├── Dynamic_text_widget
        ├── Dynamic_shape_widget
        └── Dynamic_slider
```

**Files:**
- `gumps/Dynamic_container_gump.h` / `.cc`
- `gumps/Dynamic_button.h` / `.cc`
- `gumps/Dynamic_text_widget.h` / `.cc`
- `gumps/Dynamic_shape_widget.h` / `.cc`
- `gumps/Dynamic_slider.h` / `.cc`

### Bounding-Box Hit Testing

`Dynamic_container_gump` overrides `has_point()` to use `get_rect().has_point()` (bounding-box) instead of the base `Gump::has_point()` which does pixel-level RLE checks against the gump background shape.

**Why this is needed:** When gump shapes have non-zero origins (e.g., origin at bottom-right corner, `xright=0, ybelow=0`), widget positions at positive offsets from the anchor fall outside the gump shape's RLE data range. The bounding rect covers the entire gump visual area regardless of origin configuration.

```cpp
bool Dynamic_container_gump::has_point(int sx, int sy) const {
    const TileRect rect = get_rect();
    return rect.has_point(sx, sy);
}
```

### Snap Zones Feature

**Purpose:** Enables guided object placement for complex rituals (like Ultima 8 sorcery), where items must be positioned precisely at specific locations.

**How it works:**
1. Snap zones are defined in `gump_info.txt` under a `snap_zones` section
2. When an item is dropped in a zone, it automatically snaps to the zone's target position
3. Zone ownership is resolved via a hybrid priority + axis-aligned split algorithm

**Zone Resolution Algorithm:**
1. **Priority-based:** Higher priority zones take precedence
2. **Axis-aligned split:** When zones have equal priority and overlap, ownership is resolved geometrically — wider overlaps split vertically, taller overlaps split horizontally

### Debug Visualization

Debug features are controlled by a bitmask stored in the `container_area` configuration line (6th field):

| Flag | Value | Description |
|------|-------|-------------|
| `GUMP_DEBUG_BORDER` | 0x01 | Container area border (white) |
| `GUMP_DEBUG_GRID_MAJOR` | 0x02 | Major grid lines every 10px (white) |
| `GUMP_DEBUG_GRID_MINOR` | 0x04 | Minor grid lines at 5px offset (blue/green) |
| `GUMP_DEBUG_SNAP_BORDER` | 0x08 | Snap zone border outlines |
| `GUMP_DEBUG_SNAP_CROSS` | 0x10 | Snap zone target crosshairs |
| `GUMP_DEBUG_CONSOLE` | 0x20 | Console debug logging to stderr |
| `GUMP_DEBUG_ALL` | 0x3F | All features enabled |

Flags are defined in `shapes/shapeinf/gumpinf.h` as `enum Gump_debug_flags`.

When `GUMP_DEBUG_CONSOLE` is active, detailed logging is emitted from:
- `Dynamic_container_gump` constructor (widget positions, origins, rects)
- `has_point()` calls (screen coords, rect bounds, hit/miss)
- `add()` calls (snap zone resolution, position clamping)
- `paint()` calls (object positions before/after paint, first 3 cycles)
- `Gump::on_button()` (widget iteration for button hit-testing)
- `Gump_widget::on_widget()` (screen-to-local conversion, shape origins, hit/miss)
- All dynamic widget `paint()` methods (screen positions, field IDs)

---

## Dynamic Widget System

### Dynamic Buttons (`Dynamic_button`)

**Header:** `gumps/Dynamic_button.h`  
**Implementation:** `gumps/Dynamic_button.cc`  
**Inherits:** `Gump_button`

Data-driven clickable buttons configured via `gump_info.txt`. Each button has an idle frame, clicked frame, optional sound effect, usecode callback, and action type.

**Key members:**

| Field | Type | Description |
|-------|------|-------------|
| `default_frame` | int | Frame displayed when idle |
| `clicked_frame` | int | Frame displayed when pushed |
| `sound_fx` | int | SFX index on click (-1 = none) |
| `usecode_fn` | int | Shape number for usecode lookup (-1 = none) |
| `action_type` | int | 0=nothing, 1=close gump after activate, 2=toggle |
| `visibility_flag` | int | Global flag controlling visibility (-1 = always) |
| `usecode_param` | int | Legacy field, no longer used at runtime (always fires `double_click`) |
| `toggled` | bool | Current toggle state (action_type 2 only) |

**Click chain (non-modal gumps):**

1. Mouse down → `exult.cc` → `gwin->start_dragging(x, y)`
2. `drag.cc` `Dragging_info` constructor → `find_gump(x,y)` → `gump->on_button(x,y)` → `button->push()`
3. Mouse up → `drag.cc` `drop()` → `button->unpush()` → `button->on_button(x,y)` → if HIT: `button->activate()`

**`activate()` flow:**
1. Play `sound_fx` if >= 0
2. Toggle state if `action_type == 2`
3. Resolve `usecode_fn` to function ID via `ucm->get_shape_fun(usecode_fn)` (consults symbol table for `#autonumber` mods)
4. Call `ucm->call_usecode(fn_id, item, double_click)` where `item` is the gump's container
5. Close gump if `action_type == 1`

**Note:** Buttons always fire with `Usecode_machine::double_click` as the event, consistent with `Dynamic_slider`. Each button needing distinct behaviour must use a unique `usecode_fn`.

**`paint()` — frame management:**
Paints directly using `paint_shape()`, properly saving and restoring the frame so the resting frame stays at `default_frame` outside of painting. This is critical because `on_widget()` hit-testing uses `get_shape()` which returns the current frame's `Shape_frame` — if the frame were stuck at `clicked_frame` whose shape origin differs, hit-testing would fail.

**`is_visible()`:** Checks `ucm->get_global_flag_bool(visibility_flag)`. Returns true if `visibility_flag < 0` (always visible).

---

### Dynamic Text Fields (`Dynamic_text_widget`)

**Header:** `gumps/Dynamic_text_widget.h`  
**Implementation:** `gumps/Dynamic_text_widget.cc`  
**Inherits:** `Gump_widget`

Displays text at a fixed position using a configurable font. Text content is set at runtime by usecode via `UI_set_gump_text(item, field_id, text)`.

**Key members:**

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `font_num` | int | Font from `fonts.vga` (changeable at runtime via `set_font()`) |
| `text` | string | Current display text (per-instance, initially empty) |

**Constructor:** Uses shape number -1 (no shape), positioned at `(pos_x, pos_y)` relative to parent gump.

**`paint()`:** Calls `Shape_manager::get_instance()->paint_text(font_num, text.c_str(), sx, sy)` after converting to screen coordinates. Skips painting when text is empty.

---

### Dynamic Shape Displays (`Dynamic_shape_widget`)

**Header:** `gumps/Dynamic_shape_widget.h`  
**Implementation:** `gumps/Dynamic_shape_widget.cc`  
**Inherits:** `Gump_widget`

Displays a shape/frame at a fixed position. The displayed shape can be changed at runtime by usecode via `UI_set_gump_shape(item, field_id, shape, frame)`.

**Key members:**

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `displayed` | ShapeID | Current shape/frame (per-instance) |

**`shapefile` config:** 0 = `SF_SHAPES_VGA` (world shapes), 1 = `SF_GUMPS_VGA` (gump shapes).

**`paint()`:** Calls `displayed.paint_shape(sx, sy)`. Skips painting when `shape_num <= 0`.

---

### Dynamic Sliders (`Dynamic_slider`)

**Header:** `gumps/Dynamic_slider.h`  
**Implementation:** `gumps/Dynamic_slider.cc`  
**Inherits:** `Gump_widget`

A composite widget with decrement/increment arrow buttons and a draggable thumb. Supports vertical and horizontal orientations, keyboard input, mouse wheel, and auto-repeat when holding arrow buttons.

Both orientations are configured through the same `dynamic_sliders` section in `gump_info.txt` using the `orientation` field (0=vertical, 1=horizontal). No separate section is needed — the C++ code handles layout, painting, input, and thumb travel for both orientations through the single `vertical` bool.

**Key members:**

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `vertical` | bool | Orientation (true=vertical, false=horizontal) |
| `min_val`, `max_val` | int | Value range |
| `step_val` | int | Increment per click/key |
| `val` | int | Current value |
| `usecode_fn` | int | Callback shape on value change (-1 = none) |
| `track_extent` | int | Pixel length of sliding region |
| `thumb_travel` | int | Usable travel = `track_extent` minus thumb size along travel axis |
| `thumb` | ShapeID | Thumb shape from gumps.vga |
| `dec_btn`, `inc_btn` | unique_ptr\<Gump_button\> | Arrow buttons (shape containers only) |
| `arrow_clicked` | int | -1=dec pressed, 0=none, 1=inc pressed |

**Arrow buttons** use an inner class `DynSlider_button` as a shape container and position holder. All click detection, frame toggling, and auto-repeat are handled at the `Dynamic_slider` level via `hit_arrow()` and `arrow_clicked` state, bypassing the standard `Gump_button` push/unpush chain (see Bug 6).

**Layout:** `compute_layout()` positions dec button, track, and inc button sequentially along the slider axis:
- **Vertical:** dec at top (0,0), track below, inc at bottom (0, dec_h + track_extent)
- **Horizontal:** dec at left (0,0), track after, inc at right (dec_w + track_extent, 0)

Sizes are derived from the arrow button shape frames (`get_height()` for vertical, `get_width()` for horizontal).

**Thumb travel (the overlap fix):**

`thumb_travel = track_extent - thumb_size_along_travel_axis`

For vertical sliders, this subtracts `thumb.get_height()`. For horizontal, it subtracts `thumb.get_width()`. This ensures the thumb never overlaps the increment button at the end of the track — the bug that originally caused the vertical slider's thumb to cover the down arrow (see Bug 7).

**Value ↔ Thumb mapping:**
- `update_thumb_from_val()`: `thumb_pos = (val - min_val) * thumb_travel / (max_val - min_val)`
- `val_from_thumb()`: Inverse with rounding to nearest step

**Input handling:**
- `mouse_down` — calls `hit_arrow()` first (slider-local coordinates, single `screen_to_local` call), then thumb hit-test (starts drag), then track click (jump)
- `mouse_drag` — adjusts `thumb_pos` by mouse delta along travel axis, updates value, calls usecode callback
- `mouse_up` — clears `arrow_clicked` or releases drag, snaps thumb to final position
- `key_down` — Up/Down for vertical, Left/Right for horizontal
- `mousewheel_up/down` — move value by step
- `run()` — auto-repeat: 500ms initial delay, then 50ms repeats while `arrow_clicked != 0`

**Paint order:** Arrow buttons are painted with frame 1 when pressed (with safety fallback if frame 1 doesn't exist in the shape), then restored to frame 0. The thumb is painted at its current offset along the track.

**Usecode callback:** On value change, calls `ucm->call_usecode(fn_id, container, double_click)`. The usecode function should read the current value via `UI_get_slider_value(item, field_id)`.

**Orientation comparison:**

| Aspect | Vertical (0) | Horizontal (1) |
|--------|-------------|-----------------|
| Button layout | dec top, inc bottom | dec left, inc right |
| Thumb travel along | Y axis | X axis |
| Thumb size subtracted | `get_height()` | `get_width()` |
| Keyboard | Up/Down arrows | Left/Right arrows |
| Drag tracks | `my` | `mx` |
| Track click checks | `ly` in track range | `lx` in track range |

**gump_info.txt example (vertical):**
```
:119/50/148/4/117/118/120/0/5/1/0/124/1205/0
```

**gump_info.txt example (horizontal):**
```
:119/60/10/180/121/122/123/0/100/5/50/200/1206/1
```
(Same format — only the last field changes: 0=vertical, 1=horizontal)

---

## Integration with Exult

### Automatic Gump Selection

`Gump_manager::add_gump()` checks `Dynamic_container_gump::has_config(shapenum)` before creating gumps. If the shape has a `container_area` entry in `gump_info.txt`, a `Dynamic_container_gump` is created instead of a plain `Container_gump`.

```cpp
if (Dynamic_container_gump::has_config(shapenum)) {
    new_gump = new Dynamic_container_gump(obj->as_container(), x, y, shapenum);
} else {
    new_gump = new Container_gump(obj->as_container(), x, y, shapenum);
}
```

This ensures backward compatibility — containers without `gump_info.txt` entries continue using the standard `Container_gump`.

### On-Open Usecode Invocation

After creating a `Dynamic_container_gump`, `Gump_manager::add_gump()` automatically fires the gump shape's usecode function:

```cpp
if (dynamic_cast<Dynamic_container_gump*>(new_gump)) {
    auto* ucm    = gwin->get_usecode();
    const int fn = ucm->get_shape_fun(shapenum);
    ucm->call_usecode(fn, obj, Usecode_machine::double_click);
}
```

**Why this is needed:** `Container_game_object::activate()` calls `show_gump()` first. When the container has a gump shape, `show_gump()` succeeds and returns `true`, which causes `activate()` to skip its own `call_usecode()` call. Without this hook, the on-open usecode function would never run.

**Usecode convention:**
```usecode
void shopOpen shape#(119)() {
    // Runs automatically when gump 119 is opened.
    // 'item' is the container object that was double-clicked.
    shopInitDisplay(item);
}
```

### Widget Creation

When `Dynamic_container_gump` is constructed, it reads the `Gump_info` for its shape number. The `Gump_manager` creates dynamic widgets from the `dynamic_buttons`, `dynamic_texts`, `dynamic_shapes`, and `dynamic_sliders` vectors stored in `Gump_info`, adding them as child elements (`elems`) of the gump.

Each widget type has a corresponding `_def` struct (in `gumpinf.h`) that holds the parsed configuration values:
- `Dynamic_button_def` → `Dynamic_button`
- `Dynamic_text_def` → `Dynamic_text_widget`
- `Dynamic_shape_def` → `Dynamic_shape_widget`
- `Dynamic_slider_def` → `Dynamic_slider`

### Configuration System Integration

All configuration is stored in `gump_info.txt`, reusing the existing file and `ReadInt()`-based parser infrastructure. Nine sections are supported:

| Section | Purpose |
|---------|---------|
| `container_area` | Object placement area and debug flags |
| `checkmark_pos` | Close button position |
| `special` | Non-container / non-closeable gumps |
| `snap_zones` | Guided placement zones |
| `dynamic_buttons` | Clickable buttons with usecode |
| `dynamic_texts` | Text fields set by usecode |
| `dynamic_shapes` | Shape displays set by usecode |
| `dynamic_sliders` | Slider controls read/set by usecode |
| `gump_name` | Custom status bar name for gump clicks |

---

## Configuration Reference

### container_area
```
:gump_shape/x/y/width/height[/debug_flags]
```
`debug_flags` is optional (defaults to 0). Decimal or hex bitmask of `Gump_debug_flags`.

### checkmark_pos
```
:gump_shape/x/y/checkmark_shape
```

### special
```
:gump_shape
```

### snap_zones
```
:gump_shape/zone_id/zone_x/zone_y/zone_w/zone_h/snap_x/snap_y/priority/zone_type
```

### dynamic_buttons
```
:gump_shape/button_shape/pos_x/pos_y/default_frame/clicked_frame/sound_fx/usecode_fn/action_type/visibility_flag/usecode_param
```

| Field | Description |
|-------|-------------|
| `button_shape` | Shape number in gumps.vga |
| `pos_x/pos_y` | Position relative to gump origin |
| `default_frame` | Frame when idle (typically 0) |
| `clicked_frame` | Frame when pushed (typically 1) |
| `sound_fx` | SFX index on click (-1 = none) |
| `usecode_fn` | Shape number for usecode lookup (-1 = none) |
| `action_type` | 0=nothing, 1=close gump, 2=toggle |
| `visibility_flag` | Global flag for visibility (-1 = always) |
| `usecode_param` | Legacy field, read but unused at runtime (event is always `double_click`) |

### dynamic_texts
```
:gump_shape/field_id/pos_x/pos_y/font_num
```

### dynamic_shapes
```
:gump_shape/field_id/pos_x/pos_y/shape_num/frame_num/shapefile
```
`shapefile`: 0 = shapes.vga, 1 = gumps.vga

### dynamic_sliders
```
:gump_shape/field_id/pos_x/pos_y/dec_shape/inc_shape/thumb_shape/min_val/max_val/step/default_val/extent/usecode_fn/orientation
```
`orientation`: 0 = vertical, 1 = horizontal

### gump_name
```
:gump_shape/Name String With Spaces
```
Sets a custom name displayed in the status bar when the player clicks inside the gump area (but not on an item). The name string extends to end of line, so spaces are allowed without quoting.

**Runtime behavior:** `Dynamic_container_gump::get_click_name()` returns the configured name. `Game_window::show_items()` checks this override before falling back to `Get_object_name(obj)`.

**Writer support:** `shapewrite.cc` serializes `gump_name` back to `gump_info.txt` when dirty (for Exult Studio round-trip).

---

## Design Decisions

### 1. Extending Container_gump vs. Modifying It

**Decision:** Create new `Dynamic_container_gump` subclass.

**Rationale:** Avoids modifying the well-tested `Container_gump` class. Isolates new functionality for easier maintenance. Can be disabled entirely by removing configuration entries.

### 2. Pointer to Snap Zones vs. Copying

**Decision:** Store `const std::vector<Snap_zone>*` pointing to `Gump_info`'s data.

**Rationale:** `Gump_info` persists for the entire game session. Avoids memory duplication. Zones are read-only at runtime.

### 3. Debug Flags as Bitmask

**Decision:** Use `Gump_debug_flags` enum bitmask.

**Rationale:** Matches existing Exult patterns. Allows granular control. Single integer field in configuration.

### 4. Bounding-Box Hit Testing for Dynamic Gumps

**Decision:** Override `has_point()` to use `get_rect()` instead of RLE pixel test.

**Rationale:** When gump shapes have origins at bottom-right (as Exult Studio sets for custom shapes), the RLE scan range covers negative coordinates while widget positions are at positive offsets. The bounding rect covers the full visual area regardless of origin placement.

### 5. Direct Paint in Dynamic_button

**Decision:** `Dynamic_button::paint()` bypasses `Gump_button::paint()` and paints directly.

**Rationale:** `Gump_button::paint()` adjusts the frame by +1 when pushed, which conflicts with `Dynamic_button`'s explicit frame selection (idle vs. clicked). The double-adjustment left the frame stuck at `clicked_frame` after painting, causing `on_widget()` hit-tests to use the wrong frame's shape origin. Direct painting with proper save/restore avoids this.

### 6. Hybrid Priority + Geometric Zone Resolution

**Decision:** Use priorities for explicit control, geometric splitting for ties.

**Rationale:** Pure priority can create dead zones near boundaries. Pure geometric can't express intentional precedence. Hybrid provides both.

---

## File Change Summary

### New Files

| File | Purpose |
|------|---------|
| `gumps/Dynamic_container_gump.h` | Dynamic container gump header |
| `gumps/Dynamic_container_gump.cc` | Dynamic container gump implementation |
| `gumps/Dynamic_button.h` | Data-driven button header |
| `gumps/Dynamic_button.cc` | Data-driven button implementation |
| `gumps/Dynamic_text_widget.h` | Data-driven text field header |
| `gumps/Dynamic_text_widget.cc` | Data-driven text field implementation |
| `gumps/Dynamic_shape_widget.h` | Data-driven shape display header |
| `gumps/Dynamic_shape_widget.cc` | Data-driven shape display implementation |
| `gumps/Dynamic_slider.h` | Data-driven slider header |
| `gumps/Dynamic_slider.cc` | Data-driven slider implementation |
| `docs/dynamic_gumps.md` | This document |

### Modified Files

| File | Changes |
|------|---------|
| `gumps/Gump.h` | Added `forward_mouse_down()` virtual for widget-level mouse event routing; added `get_click_name()` virtual for custom status bar text |
| `gumps/Gump.cc` | Bug fix for `check_elem_positions()`; debug logging in `on_button()` (gated by `GUMP_DEBUG_CONSOLE`); `forward_mouse_down()` implementation |
| `gumps/Gump_widget.cc` | Debug logging in `on_widget()` (gated by `GUMP_DEBUG_CONSOLE`) |
| `gumps/Gump_manager.h` | Added `handle_mouse_wheel(float y, float x, int mx, int my)` declaration |
| `gumps/Gump_manager.cc` | Auto-selection of `Dynamic_container_gump`; on-open usecode invocation; widget creation from config; `handle_mouse_wheel()` finds gump under cursor and forwards wheel events, always consuming when cursor is over a gump |
| `gumps/Dynamic_container_gump.h` | Added `gump_name_` member and `get_click_name()` override; added `handle_kbd_event()`, `mousewheel_up()`, `mousewheel_down()`, `update_gump()` override declarations |
| `gumps/Dynamic_container_gump.cc` | Loads `gump_name` from `Gump_info` in constructor; set `handles_kbd = true` for keyboard focus; implemented `handle_kbd_event()` (SDL key translation + forward to child widgets), `mousewheel_up/down()` (forward to all children), `update_gump()` (call `run()` on children for auto-repeat) |
| `exult.cc` | `Handle_event()` `SDL_EVENT_MOUSE_WHEEL` case: try `gump_man->handle_mouse_wheel()` before cheat-scroll fallthrough |
| `gumps/Dynamic_slider.h` | Added `arrow_clicked` member, `hit_arrow()` method, `thumb_travel` member; removed `on_button()` override (Bug 6) |
| `gumps/Dynamic_slider.cc` | `hit_arrow()` implementation (slider-local hit test), arrow click handling in `mouse_down`/`mouse_up`/`paint`/`run`, orientation-aware `thumb_travel` calculation (Bug 7), track hit area upper-bound fix (Bug 8) |
| `gumps/Dynamic_text_widget.h` | Added `set_font()` method for runtime font changes |
| `shapes/shapevga.cc` | Added `snap_zones`, `dynamic_buttons`, `dynamic_texts`, `dynamic_shapes`, `dynamic_sliders`, `gump_name` section parsers; fixed `getline()` delimiter bug |
| `shapes/shapewrite.cc` | Added `gump_name` writer section for Exult Studio round-trip |
| `shapes/shapeinf/gumpinf.h` | Added `Snap_zone`, `Dynamic_button_def`, `Dynamic_text_def`, `Dynamic_shape_def`, `Dynamic_slider_def` structs; `Gump_debug_flags` enum; `gump_name` field with dirty tracking; extended `Gump_info` class |
| `shapes/shapeinf/gumpinf.cc` | Added `gumpname_from_patch`/`gumpname_modified` init |
| `usecode/intrinsics.cc` | Seven new intrinsic implementations |
| `usecode/ucinternal.h` | Seven new intrinsic declarations |
| `usecode/bgintrinsics.h` | Registered BG intrinsics at 0xdd–0xe3 |
| `usecode/siintrinsics.h` | Registered SI intrinsics at 0xde–0xe4 |
| `usecode/sibetaintrinsics.h` | Registered SI-beta intrinsic at 0xe4 (`set_gump_text_font`) |
| `drag.h` | Added `mouse_widget`, `widget_gump` members to `Dragging_info` for widget-level mouse forwarding |
| `drag.cc` | Widget mouse event routing in constructor, `moved()`, and `drop()`; debug logging for button click chain |
| `gamewin.cc` | `show_items()` checks `gump->get_click_name()` for custom status bar text when clicking empty gump area |
| `data/bg/gump_info.txt` | Documentation for all nine sections |
| `data/si/gump_info.txt` | Documentation for all nine sections |

---

## Potential Impact Analysis

### Low Risk

| Area | Assessment |
|------|------------|
| **Existing containers** | No impact — only containers with `gump_info.txt` `container_area` entries use new code |
| **Existing intrinsics** | No impact — new intrinsics are additive at previously unused slots |
| **Game saves** | No impact — no new save data structures |
| **Configuration parsing** | Low risk — new sections added, existing sections unchanged |

### Areas Requiring Attention

#### 1. Debug Output Volume

When `GUMP_DEBUG_CONSOLE` is enabled, significant output is written to `stderr`. Debug flags default to 0 (disabled) in production.

The `[Drag::drop]` logging for button release HIT/MISS is always active (not gated by debug flags). This is intentional for diagnosing click issues but should be removed or gated before final release.

#### 2. Intrinsic Indices

New intrinsics occupy slots 0xdd–0xe2 (BG) and 0xde–0xe3 (SI). If upstream Exult adds intrinsics at these indices, a conflict would occur.

#### 3. Gump_info Memory Lifetime

`Dynamic_container_gump` holds a pointer to `Gump_info`'s `snap_zones` vector. `Gump_info` is a static map that persists for the entire game session. If `Gump_info` were ever cleared during gameplay, pointers could become invalid.

#### 4. Dynamic_button Frame Assumption

The frame management fix assumes that `default_frame` (frame 0) has the correct shape origin for hit-testing. If both frames have origins set in Exult Studio, either would work. If only one frame has its origin set, it must be `default_frame`.

---

## Version History

| Date | Author | Description |
|------|--------|-------------|
| 2026-02-15 | theGreyWanderer-uc | Initial documentation (container gump, snap zones, two intrinsics) |
| 2026-03-18 | theGreyWanderer-uc | Added dynamic widget system (buttons, text, shapes, sliders), four new intrinsics, button frame management fix, bounding-box hit testing |
| 2026-03-19 | theGreyWanderer-uc | Slider bug fixes (arrow clicks via on_button override, thumb drag via forward_mouse_down), UI_set_gump_text_font intrinsic, gump_name config section, font highlight system, custom gump click name |
| 2026-03-20 | theGreyWanderer-uc | Arrow click reworked (hit_arrow bypasses 3-level parent chain — Bug 6), thumb_travel orientation fix for horizontal sliders (Bug 7), orientation documentation corrected (0=vertical, 1=horizontal), horizontal slider fully documented |
| 2026-03-26 | theGreyWanderer-uc | Slider track hit area upper-bound fix (Bug 8) — prevents clicks outside track from registering, text alignment support (halign/valign) in Dynamic_text_widget |
| 2026-04-04 | theGreyWanderer-uc | Keyboard input, mousewheel dispatch, and auto-repeat for `Dynamic_container_gump` — enables `handle_kbd_event()`, `mousewheel_up/down()`, `update_gump()` overrides; added `Gump_manager::handle_mouse_wheel()` and `exult.cc` event routing; mousewheel forwards to all child widgets without per-widget hit test |

