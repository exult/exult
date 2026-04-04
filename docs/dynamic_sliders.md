# Dynamic Sliders

## Overview

`Dynamic_slider` is a data-driven slider control for dynamic container gumps. Each slider is a composite widget with decrement/increment arrow buttons and a draggable thumb, supporting vertical and horizontal orientations, keyboard input, mouse wheel, and auto-repeat when holding arrow buttons. Slider values are read and written by usecode via dedicated intrinsics.

**Design Philosophy:** Sliders extend `Gump_widget` directly and manage their own sub-components (arrow buttons, thumb) internally. All click detection, frame toggling, and auto-repeat are handled at the `Dynamic_slider` level rather than delegating to child buttons, avoiding coordinate-chain issues with deeply nested widgets.

**Prerequisite:** Dynamic sliders are created in `Gump::set_object_area()` (the base class), so any gump whose shape has `dynamic_sliders` entries in `gump_info.txt` will have them instantiated — even on a plain `Container_gump`. This is **safe and will not crash**. However, for full functionality the gump shape should also have a `container_area` entry, which causes `Gump_manager` to create a **Dynamic Container Gump**. Without it:

| Feature | Dynamic Container Gump | Plain Container_gump |
|---------|----------------------|---------------------|
| Sliders created & painted | Yes | Yes |
| Arrow clicks, thumb drag, track click | Yes | Yes |
| Keyboard input | Yes | **No** — `handle_kbd_event()` is a `Dynamic_container_gump` override |
| Mousewheel input | Yes | **No** — `mousewheel_up/down()` are `Dynamic_container_gump` overrides |
| Auto-repeat (held arrows) | Yes | **No** — `update_gump()` is a `Dynamic_container_gump` override |
| Intrinsics (set/get_slider_value) | Yes | Yes |
| Usecode callback on value change | Yes | Yes |
| On-open usecode fires | Yes | **No** — slider stays at `default_val` |
| Screen centering | Yes | **No** — cascading/saved position used |

Sliders are partially interactive on a plain `Container_gump` — mouse clicks (arrow buttons, thumb drag, track click) and intrinsics work because they route through the drag system and base `Gump` class. However, keyboard input, mousewheel, and auto-repeat require `Dynamic_container_gump` because they depend on overrides (`handle_kbd_event()`, `mousewheel_up/down()`, `update_gump()`) that only exist on that subclass. For full functionality, the gump shape should have a `container_area` entry in `gump_info.txt`. See [dynamic_gumps.md](dynamic_gumps.md) for full container gump setup.

---

## Table of Contents

1. [Class Hierarchy](#class-hierarchy)
2. [Key Members](#key-members)
3. [Configuration](#configuration)
4. [Layout and Geometry](#layout-and-geometry)
5. [Input Handling](#input-handling)
6. [Paint Behaviour](#paint-behaviour)
7. [Value-Thumb Mapping](#value-thumb-mapping)
8. [Intrinsics](#intrinsics)
9. [Usecode Integration](#usecode-integration)
10. [Orientation Comparison](#orientation-comparison)
11. [Bug Fixes](#bug-fixes)
12. [Design Notes](#design-notes)

---

## Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump
  └── Gump_widget
        └── Dynamic_slider
              └── DynSlider_button (inner class, shape container only)
```

**Files:**
- `gumps/Dynamic_slider.h`
- `gumps/Dynamic_slider.cc`

---

## Key Members

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `vertical` | bool | Orientation (true=vertical, false=horizontal) |
| `min_val` | int | Minimum value |
| `max_val` | int | Maximum value |
| `step_val` | int | Increment per click/key |
| `val` | int | Current value |
| `usecode_fn` | int | Callback shape on value change (-1 = none) |
| `track_extent` | int | Pixel length of the sliding region |
| `thumb_travel` | int | Usable travel = `track_extent` minus thumb size along travel axis |
| `thumb` | ShapeID | Thumb shape from `gumps.vga` |
| `dec_btn` | unique_ptr\<Gump_button\> | Decrement arrow button (shape container only) |
| `inc_btn` | unique_ptr\<Gump_button\> | Increment arrow button (shape container only) |
| `arrow_clicked` | int | -1=dec pressed, 0=none, 1=inc pressed |

---

## Configuration

### gump_info.txt Format

Sliders are defined in the `dynamic_sliders` section:

```
dynamic_sliders
:gump_shape/field_id/pos_x/pos_y/dec_shape/inc_shape/thumb_shape/min_val/max_val/step/default_val/extent/usecode_fn/orientation
```

### Field Reference

| Field | Description |
|-------|-------------|
| `gump_shape` | The gump shape number this slider belongs to |
| `field_id` | Unique identifier used by `UI_set_slider_value()` and `UI_get_slider_value()` |
| `pos_x` / `pos_y` | Position relative to gump origin |
| `dec_shape` | Shape number in `gumps.vga` for the decrement arrow |
| `inc_shape` | Shape number in `gumps.vga` for the increment arrow |
| `thumb_shape` | Shape number in `gumps.vga` for the draggable thumb |
| `min_val` | Minimum slider value |
| `max_val` | Maximum slider value |
| `step` | Value change per arrow click or key press |
| `default_val` | Initial value when gump opens |
| `extent` | Pixel length of the track (sliding area between arrows) |
| `usecode_fn` | Shape number for usecode callback on value change (-1 = none) |
| `orientation` | 0 = vertical, 1 = horizontal |

### Example Configuration

A vertical scroll slider and a horizontal volume slider:

```
dynamic_sliders
; Vertical scroll slider (field_id 50, range 0-5, step 1)
:200/50/148/4/117/118/120/0/5/1/0/124/1300/0
; Horizontal volume slider (field_id 60, range 0-100, step 5)
:200/60/10/180/121/122/123/0/100/5/50/200/1301/1
```

In this example:
- The vertical slider (field_id 50) uses shapes 117/118 for arrows, shape 120 for the thumb, scrolls from 0-5 in steps of 1, starts at 0, has a 124px track, and calls usecode shape 1300 on change
- The horizontal slider (field_id 60) uses shapes 121/122 for arrows, shape 123 for the thumb, ranges 0-100 in steps of 5, starts at 50, has a 200px track, and calls usecode shape 1301 on change

---

## Layout and Geometry

`compute_layout()` positions the decrement button, track, and increment button sequentially along the slider axis:

### Vertical Layout
```
dec_btn at (0, 0)              ← top arrow
track starts at (0, dec_h)    ← sliding region
inc_btn at (0, dec_h + extent) ← bottom arrow
```

### Horizontal Layout
```
dec_btn at (0, 0)              ← left arrow
track starts at (dec_w, 0)    ← sliding region
inc_btn at (dec_w + extent, 0) ← right arrow
```

Sizes are derived from the arrow button shape frames — `get_height()` for vertical spacing, `get_width()` for horizontal spacing.

### Thumb Travel

The usable thumb travel is the track extent minus the thumb's size along the travel axis:

```
thumb_travel = track_extent - thumb_size_along_travel_axis
```

- **Vertical:** subtracts `thumb.get_height()`
- **Horizontal:** subtracts `thumb.get_width()`

This prevents the thumb from overlapping the increment arrow at the end of the track.

---

## Input Handling

### Mouse

| Action | Behaviour |
|--------|-----------|
| Click arrow button | `hit_arrow()` detects which arrow, calls `move_value()`, sets `arrow_clicked` |
| Hold arrow button | `run()` auto-repeats: 500ms initial delay, then 50ms repeats |
| Click track | Thumb jumps to click position, value updates to match |
| Drag thumb | `mouse_drag()` moves thumb by mouse delta along travel axis, updates value |
| Mouse wheel | `mousewheel_up/down` moves value by one step |

### Track Click Bounds

Track clicks are bounded to the actual track area:
- **Vertical:** checks `lx >= 0 && lx < track_width && ly >= track_start && ly <= track_start + track_extent`
- **Horizontal:** checks `ly >= 0 && ly < track_height && lx >= track_start && lx <= track_start + track_extent`

The cross-axis bound uses the thumb shape's width (vertical) or height (horizontal) to define the clickable area. This prevents clicks outside the track from registering.

### Keyboard

| Key | Vertical | Horizontal |
|-----|----------|------------|
| Up | Decrement | — |
| Down | Increment | — |
| Left | — | Decrement |
| Right | — | Increment |

Keyboard events reach the slider via `Dynamic_container_gump::handle_kbd_event()`, which translates the SDL key event and calls `key_down()` on each child widget. The container gump receives keyboard focus because it sets `handles_kbd = true`, which opts into Exult's `kbd_focus` system managed by `Gump_manager`.

### Mouse Wheel Dispatch

Mouse wheel events reach the slider via `Gump_manager::handle_mouse_wheel()` → `Dynamic_container_gump::mousewheel_up/down()` → `Dynamic_slider::mousewheel_up/down()`. The container gump forwards wheel events to **all** child widgets without per-widget hit testing — if the cursor is over the gump, any slider in it responds. `Gump_manager::handle_mouse_wheel()` always consumes the event when the cursor is over a gump, preventing the game-world cheat-scroll from firing through open gumps.

### Auto-Repeat Dispatch

`Dynamic_slider::run()` implements auto-repeat for held arrow buttons (500ms initial delay, 50ms repeat). It is called each frame by `Dynamic_container_gump::update_gump()`, which is invoked from `Gump_manager::update_gumps()` during the main game loop.

### Arrow Hit Testing

Arrow clicks use `hit_arrow(mx, my)` which converts screen coordinates to slider-local coordinates with a single `screen_to_local` call (2-level parent chain: `Dynamic_slider → Gump`). This avoids the 3-level coordinate chain issue that caused the earlier `on_button()` approach to fail (see Bug 6 in [Bug Fixes](#bug-fixes)).

---

## Paint Behaviour

Paint order:
1. **Decrement arrow** — painted with frame 1 when `arrow_clicked == -1`, frame 0 otherwise (with safety fallback if frame 1 doesn't exist)
2. **Increment arrow** — painted with frame 1 when `arrow_clicked == 1`, frame 0 otherwise
3. **Thumb** — painted at its current offset along the track

Frame toggling for arrows uses save/restore: set frame 1, paint, restore frame 0. This matches the `Dynamic_button` pattern.

---

## Value-Thumb Mapping

### Value → Thumb Position

```
thumb_pos = (val - min_val) * thumb_travel / (max_val - min_val)
```

Called by `update_thumb_from_val()` whenever the value changes programmatically or via arrow clicks.

### Thumb Position → Value

```
val = min_val + (thumb_pos * (max_val - min_val) / thumb_travel)
```

Rounded to the nearest `step_val`. Called by `val_from_thumb()` during drag and track-click operations.

---

## Intrinsics

Dynamic sliders are controlled by two dedicated intrinsics:

### `UI_set_slider_value(item, field_id, value)`

Sets the current value of a slider identified by `field_id`. The thumb position updates automatically. The value is clamped to `min_val`..`max_val` by the C++ implementation.

**Calle form:** `item->set_slider_value(field_id, value)`

| Parameter | Description |
|-----------|-------------|
| `item` | The container object whose gump contains the slider |
| `field_id` | Matches the `field_id` in `gump_info.txt` |
| `value` | Integer value (clamped to `min_val`..`max_val`) |

**Returns:** `int` — 1 on success, 0 on failure (gump not open, field_id not found).

| Game | Intrinsic Slot |
|------|----------------|
| Black Gate | 0xe1 |
| Serpent Isle | 0xe2 |

### `UI_get_slider_value(item, field_id)`

Returns the current integer value of a slider identified by `field_id`, or `0` if the slider is not found.

**Calle form:** `item->get_slider_value(field_id)`

| Parameter | Description |
|-----------|-------------|
| `item` | The container object whose gump contains the slider |
| `field_id` | Matches the `field_id` in `gump_info.txt` |

**Returns:** `int` — current slider value, or 0 on failure (gump not open, field_id not found).

| Game | Intrinsic Slot |
|------|----------------|
| Black Gate | 0xe2 |
| Serpent Isle | 0xe3 |

**Registration:**
- `usecode/bgintrinsics.h` — slots 0xe1, 0xe2
- `usecode/siintrinsics.h` — slots 0xe2, 0xe3

---

## Usecode Integration

### Slider as Scroll Control

Use a slider to scroll through a list of items. The slider callback reads the value and refreshes the display:

```usecode
// Slider callback — usecode_fn in gump_info.txt points here.
void onScroll shape#(1300) ()
{
    // Read the slider's current value (the scroll offset).
    var offset = UI_get_slider_value(item, 50);
    if (offset < 0) { offset = 0; }
    if (offset > 5) { offset = 5; }

    // Refresh visible item names/shapes for the new offset.
    var all_names = ["Sword", "Shield", "Helm", "Boots", "Ring",
                     "Amulet", "Scroll", "Potion", "Gem", "Key"];
    var slot = 1;
    while (slot <= 5)
    {
        var idx = offset + slot;
        UI_set_gump_text(item, slot, all_names[idx]);
        slot = slot + 1;
    }
}
```

### Slider as Quantity Control

Read the slider value in a confirm callback to determine a selected quantity:

```usecode
void onConfirm shape#(1301) ()
{
    if (event != 1) { return; }

    var quantity = UI_get_slider_value(item, 60);
    // Use quantity for the purchase/action.
    processOrder(item, quantity);
    UI_close_gump2(item);
}
```

### Setting Slider Value from Usecode

Pre-set a slider to a specific value (e.g., restoring a saved position):

```usecode
void myGumpOpen shape#(200) ()
{
    // Restore the scroll position from a saved flag.
    var saved_offset = getSavedScrollOffset();
    UI_set_slider_value(item, 50, saved_offset);

    // Populate the display based on this offset.
    refreshDisplay(item, saved_offset);
}
```

### Callback Convention

On value change, the slider calls `ucm->call_usecode(fn_id, container, double_click)`. The usecode function should read the current value via `UI_get_slider_value(item, field_id)` — the value is not passed as a parameter.

```usecode
// The slider does NOT pass its value as a parameter.
// Always read it explicitly:
void sliderCallback shape#(1300) ()
{
    var val = UI_get_slider_value(item, 50);
    // ... use val ...
}
```

---

## Orientation Comparison

| Aspect | Vertical (0) | Horizontal (1) |
|--------|-------------|-----------------|
| Button layout | dec top, inc bottom | dec left, inc right |
| Thumb travel along | Y axis | X axis |
| Thumb size subtracted | `get_height()` | `get_width()` |
| Keyboard | Up/Down arrows | Left/Right arrows |
| Drag tracks | `my` delta | `mx` delta |
| Track click checks | `ly` in track range | `lx` in track range |

Both orientations use the same `dynamic_sliders` section in `gump_info.txt` — only the last field (`orientation`) differs. No separate section is needed.

---

## Bug Fixes

### Bug 4: Slider Arrow Buttons Not Clickable

**Problem:** Clicking arrow buttons had no effect — the slider value didn't change and no callback fired.

**Root Cause:** `DynSlider_button` instances had `parent = this` (the `Dynamic_slider`), not the parent `Gump`. When `Gump::on_button()` iterated its `elems`, the arrow buttons were invisible to the click dispatch chain.

**Initial Fix:** Added `on_button()` override to `Dynamic_slider` that delegated to child arrow buttons. This was later replaced by the Bug 6 fix.

---

### Bug 5: Slider Thumb Drag Moves Entire Gump

**Problem:** Dragging the thumb moved the entire gump instead of scrolling the slider.

**Root Cause:** The drag system only knew about objects, buttons, and gump dragging. When `on_button()` returned null for the thumb area, the system fell through to gump drag.

**Fix:** Added `Gump::forward_mouse_down()` virtual method. The `Dragging_info` constructor now calls `forward_mouse_down` after `on_button` fails. If a widget captures the click, the drag is forwarded to the widget instead of the gump.

---

### Bug 6: Arrow Hit-Testing Fails Through 3-Level Parent Chain

**Problem:** Clicking arrow buttons caused them to visually disappear instead of showing the pressed frame. Clicks fell through to track-click or gump dragging.

**Root Cause:** The Bug 4 fix relied on `on_widget()` for hit-testing, which calls `screen_to_local()` through the parent chain: `DynSlider_button → Dynamic_slider → Gump` (3 levels). This coordinate chain never correctly resolved screen coordinates to button-local coordinates.

**Fix:** Removed `on_button()` override entirely. All arrow click handling is now at the `Dynamic_slider` level:
1. `hit_arrow(mx, my)` converts screen→slider-local coords (single `screen_to_local` call, 2-level chain only)
2. `mouse_down()` calls `hit_arrow()` first; if hit, sets `arrow_clicked` and calls `move_value()`
3. `paint()` renders buttons with frame 1 when pressed
4. `run()` auto-repeats using `arrow_clicked` state directly

`DynSlider_button` objects are retained as shape containers and position holders only.

---

### Bug 7: Thumb Travel Uses Height for Both Orientations

**Problem:** Horizontal sliders used `get_height()` instead of `get_width()` for thumb travel calculation, causing the thumb to overrun the increment button.

**Root Cause:** Only vertical sliders were initially deployed; the horizontal path was never tested.

**Fix:** `thumb_travel` now subtracts the correct dimension:
- Vertical: `tf->get_height()`
- Horizontal: `tf->get_width()`

---

### Bug 8: Slider Track Hit Area Unbounded

**Problem:** Clicking anywhere to the right of a vertical slider (or below a horizontal slider) registered as a track click, jumping the thumb.

**Root Cause:** The track-click hit test checked the primary axis range but had no upper bound on the cross axis.

**Fix:** Added upper-bound checks using the thumb shape's cross-axis dimension (width for vertical, height for horizontal) to define the clickable track area.

---

## Design Notes

### Why Arrow Clicks Are Handled at the Slider Level

The standard `Gump_button` push/unpush chain relies on `on_widget()` for pixel-level hit-testing. `on_widget()` calls `screen_to_local()` through the parent chain, and `DynSlider_button` objects have a 3-level chain (`button → slider → gump`). This coordinate translation never produced correct results. Handling clicks at the `Dynamic_slider` level (2-level chain: `slider → gump`) works reliably.

### Why DynSlider_button Still Exists

`DynSlider_button` is retained as an inner class for two purposes:
1. **Shape container** — holds the arrow shape/frame from `gumps.vga`
2. **Position holder** — stores the arrow's offset within the slider for paint coordinate calculation

No click logic passes through `DynSlider_button`. All interaction is managed by `Dynamic_slider` directly.

### Usecode Callback Design

The slider does not pass its value as a parameter to the usecode callback. The callback must read the value explicitly via `UI_get_slider_value()`. This is consistent with the button callback design (which always fires `double_click` as the event) and avoids adding parameter-passing infrastructure to the usecode call mechanism.

### Auto-Repeat Timing

Arrow auto-repeat uses 500ms initial delay followed by 50ms repeat intervals. These values match common UI conventions for scroll buttons and provide responsive feel without accidental over-scrolling.

### No Save Persistence

Slider values, thumb position, and arrow press state are transient — they reset to `default_val` when the gump is closed and reopened. By design, persistent game state belongs in usecode (e.g., global flags, static variables), not in UI widgets. If you need a slider value to persist, save it to a global flag in your usecode callback and restore it via `UI_set_slider_value()` in the on-open handler.

### Per-Instance State

Two gumps with the same shape number have independent slider state. The `val`, `thumb_pos`, and `arrow_clicked` fields are stored per-widget-instance on the `Dynamic_slider` object, not on the shared `Gump_info` configuration singleton.

### Gump Lookup for Intrinsics

Both slider intrinsics use `find_gump(item)` to locate the open gump for the given container. If the container is a world-placed object (no owner), a fallback scan of the open gumps list finds it. If the gump is not open, the intrinsic silently returns 0.
