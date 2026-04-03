# Dynamic Shape Displays

## Overview

`Dynamic_shape_widget` is a data-driven shape display widget for dynamic container gumps. It renders a shape/frame from either `shapes.vga` (world shapes) or `gumps.vga` (gump shapes) at a fixed position. The displayed shape can be changed at runtime by usecode, allowing gumps to show item icons, currency symbols, or any other game art that changes based on context.

**Design Philosophy:** Shape widgets extend `Gump_widget` directly (they are not clickable). They serve as display-only image slots whose content is controlled from usecode via a `field_id` for addressing. The initial shape/frame is configured in `gump_info.txt` and can be overridden at any time.

**Prerequisite:** Dynamic shape displays are created in `Gump::set_object_area()` (the base class), so any gump whose shape has `dynamic_shapes` entries in `gump_info.txt` will have them instantiated — even on a plain `Container_gump`. This is **safe and will not crash**. However, for full functionality the gump shape should also have a `container_area` entry, which causes `Gump_manager` to create a **Dynamic Container Gump**. Without it:

| Feature | Dynamic Container Gump | Plain Container_gump |
|---------|----------------------|---------------------|
| Shape displays created & painted | Yes | Yes |
| Intrinsic (set_gump_shape) | Yes | Yes |
| On-open usecode fires | Yes | **No** — dynamic shapes stay at gump_info.txt defaults |
| Screen centering | Yes | **No** — cascading/saved position used |

Statically configured shapes (non-zero `shape_num` in `gump_info.txt`) display correctly on both gump types. Dynamic shapes that start at 0 and rely on the on-open handler to populate them will remain blank on a plain `Container_gump`. See [dynamic_gumps.md](dynamic_gumps.md) for full container gump setup.

---

## Table of Contents

1. [Class Hierarchy](#class-hierarchy)
2. [Key Members](#key-members)
3. [Configuration](#configuration)
4. [Paint Behaviour](#paint-behaviour)
5. [Intrinsics](#intrinsics)
6. [Usecode Integration](#usecode-integration)
7. [Shape File Selection](#shape-file-selection)
8. [Design Notes](#design-notes)

---

## Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump
  └── Gump_widget
        └── Dynamic_shape_widget
```

**Files:**
- `gumps/Dynamic_shape_widget.h`
- `gumps/Dynamic_shape_widget.cc`

---

## Key Members

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `displayed` | ShapeID | Current shape/frame (per-instance, changeable at runtime) |

The `ShapeID` stores both the shape number and frame number, along with the shape file reference.

---

## Configuration

### gump_info.txt Format

Shape displays are defined in the `dynamic_shapes` section:

```
dynamic_shapes
:gump_shape/field_id/pos_x/pos_y/shape_num/frame_num/shapefile
```

### Field Reference

| Field | Description |
|-------|-------------|
| `gump_shape` | The gump shape number this shape display belongs to |
| `field_id` | Unique identifier used by `UI_set_gump_shape()` |
| `pos_x` / `pos_y` | Position relative to gump origin |
| `shape_num` | Initial shape number to display (0 or negative = nothing) |
| `frame_num` | Initial frame number |
| `shapefile` | Source file: 0 = `shapes.vga` (world shapes), 1 = `gumps.vga` (gump shapes) |

### Example Configuration

A gump with item preview slots and currency icons:

```
dynamic_shapes
; Item preview slots (field_ids 20-24, from shapes.vga)
:200/20/10/10/0/0/0
:200/21/10/25/0/0/0
:200/22/10/40/0/0/0
:200/23/10/55/0/0/0
:200/24/10/70/0/0/0
; Currency icons (field_ids 40-43, from gumps.vga, static shapes)
:200/40/170/10/140/0/1
:200/41/170/25/141/0/1
:200/42/170/40/142/0/1
:200/43/170/55/143/0/1
```

In this example:
- Item preview slots (20-24) start with shape 0 (nothing displayed) and are populated by usecode when the gump opens
- Currency icons (40-43) are statically configured to show gump shapes 140-143 at frame 0 — these never change at runtime

---

## Paint Behaviour

`paint()` calls `displayed.paint_shape(sx, sy)` after converting widget-local coordinates to screen coordinates. When `shape_num <= 0`, painting is skipped — no blank space is rendered.

This means shape displays configured with `shape_num=0` appear empty until usecode sets a shape via `UI_set_gump_shape()`. Static icons should be configured with their desired shape/frame in `gump_info.txt` and need no usecode interaction.

---

## Intrinsics

Dynamic shape displays are controlled by one dedicated intrinsic:

### `UI_set_gump_shape(item, field_id, shape, frame)`

Sets the displayed shape and frame on a shape widget identified by `field_id`.

**Calle form:** `item->set_gump_shape(field_id, shape, frame)`

| Parameter | Description |
|-----------|-------------|
| `item` | The container object whose gump contains the shape widget |
| `field_id` | Matches the `field_id` in `gump_info.txt` |
| `shape` | Shape number from the widget's configured shape file |
| `frame` | Frame number within the shape |

**Returns:** `int` — 1 on success, 0 on failure (gump not open, field_id not found).

| Game | Intrinsic Slot |
|------|----------------|
| Black Gate | 0xe0 |
| Serpent Isle | 0xe1 |

**Implementation:** Finds the open gump for `item`, iterates `gump->get_elems()`, matches the first `Dynamic_shape_widget` with the given `field_id`, calls `set_shape()` on the internal `ShapeID`.

**Registration:**
- `usecode/bgintrinsics.h` — slot 0xe0
- `usecode/siintrinsics.h` — slot 0xe1

---

## Usecode Integration

### Populating Shape Displays on Open

Set item icons in the on-open handler to show what the gump represents:

```usecode
void myGumpOpen shape#(200) ()
{
    // Display item icons in the preview slots.
    var shapes = [604, 552, 606, 591, 692];
    var frames = [0, 0, 0, 24, 0];
    var slot = 1;
    while (slot <= 5)
    {
        UI_set_gump_shape(item, 19 + slot, shapes[slot], frames[slot]);
        slot = slot + 1;
    }
}
```

### Updating Shapes from Callbacks

Shape displays can be updated at any time from button callbacks:

```usecode
// Scroll callback — update visible item icons after scrolling.
void scrollItems shape#(1300) ()
{
    if (event != 1) { return; }

    var offset = getScrollOffset();
    var all_shapes = [604, 552, 606, 591, 692, 603, 602, 601, 600, 599];
    var all_frames = [0, 0, 0, 24, 0, 0, 0, 0, 0, 0];

    var slot = 1;
    while (slot <= 5)
    {
        var idx = offset + slot;
        UI_set_gump_shape(item, 19 + slot, all_shapes[idx], all_frames[idx]);
        slot = slot + 1;
    }
}
```

### Clearing a Shape Display

Set shape to 0 to hide the display:

```usecode
UI_set_gump_shape(item, 20, 0, 0);    // field_id 20 now shows nothing
```

---

## Shape File Selection

The `shapefile` field in the configuration determines which VGA file supplies the shape data:

| Value | File | Constant | Use Case |
|-------|------|----------|----------|
| 0 | `shapes.vga` | `SF_SHAPES_VGA` | World objects (weapons, armor, items, NPCs) |
| 1 | `gumps.vga` | `SF_GUMPS_VGA` | Gump elements (icons, decorations, UI elements) |

**When to use `shapes.vga` (0):** Displaying game world items — swords, potions, food, quest items. The shapes match what the player sees in the game world and inventory.

**When to use `gumps.vga` (1):** Displaying UI-specific graphics — currency symbols, category icons, decorative elements designed for gump layouts. These shapes may not have world-space equivalents.

You can mix both in the same gump — item previews from `shapes.vga` alongside currency icons from `gumps.vga`.

---

## Design Notes

### Why field_id Instead of Positional Indexing

Like text fields, shape displays use `field_id` for stable addressing. Multiple shape widgets can belong to the same gump, and their parse order is not guaranteed. The `field_id` lets usecode reference specific slots regardless of configuration order.

### Static vs. Dynamic Content

Shape displays support two usage patterns:

1. **Static** — Configure the desired shape/frame in `gump_info.txt`. No usecode interaction needed. The shape is painted as-is every frame. Useful for decorative icons that never change.

2. **Dynamic** — Configure with `shape_num=0` (empty). Populate via `UI_set_gump_shape()` at runtime. Useful for item previews, status indicators, or anything that depends on game state.

Both patterns can coexist in the same gump.

### Shape Number 0 vs. Negative

A shape number of 0 or negative causes `paint()` to skip rendering. In `gump_info.txt`, use `0` for "start empty, set later by usecode". Negative shape numbers are never used in configuration but are handled safely by the `cache_shape()` guard in `shapeid.cc`.

### No Save Persistence

Displayed shape and frame are transient — they reset to the `gump_info.txt` defaults when the gump is closed and reopened. By design, persistent game state belongs in usecode (e.g., global flags, static variables), not in UI widgets. Your on-open handler must set dynamic shapes each time the gump opens.

### Per-Instance State

Two gumps with the same shape number have independent shape display state. The `displayed` ShapeID is stored per-widget-instance on the `Dynamic_shape_widget` object, not on the shared `Gump_info` configuration singleton.

### Gump Lookup for Intrinsics

`UI_set_gump_shape` uses `find_gump(item)` to locate the open gump for the given container. If the container is a world-placed object (no owner), a fallback scan of the open gumps list finds it. If the gump is not open, the intrinsic silently returns 0.
