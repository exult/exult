# Dynamic Text Fields

## Overview

`Dynamic_text_widget` is a data-driven text display widget for dynamic container gumps. Text fields show a string at a fixed position using a configurable font. Content is set at runtime by usecode — text fields start empty when the gump opens and are populated by usecode callbacks (typically in the on-open handler).

**Design Philosophy:** Text widgets extend `Gump_widget` directly (they are not clickable). They serve as display-only labels whose content and appearance are controlled entirely from usecode. Each text field is identified by a `field_id` for addressing from intrinsic functions.

**Prerequisite:** Dynamic text fields are created in `Gump::set_object_area()` (the base class), so any gump whose shape has `dynamic_texts` entries in `gump_info.txt` will have them instantiated — even on a plain `Container_gump`. This is **safe and will not crash**. However, for full functionality the gump shape should also have a `container_area` entry, which causes `Gump_manager` to create a **Dynamic Container Gump**. Without it:

| Feature | Dynamic Container Gump | Plain Container_gump |
|---------|----------------------|---------------------|
| Text fields created & painted | Yes | Yes |
| Intrinsics (set_gump_text, set_gump_text_font) | Yes | Yes |
| On-open usecode fires | Yes | **No** — text fields stay empty until manually triggered |
| Screen centering | Yes | **No** — cascading/saved position used |

The most significant gap is that the on-open usecode handler does not fire on a plain `Container_gump`, so text fields remain empty until usecode populates them through some other trigger (e.g., a button callback). See [dynamic_gumps.md](dynamic_gumps.md) for full container gump setup.

---

## Table of Contents

1. [Class Hierarchy](#class-hierarchy)
2. [Key Members](#key-members)
3. [Configuration](#configuration)
4. [Paint Behaviour](#paint-behaviour)
5. [Intrinsics](#intrinsics)
6. [Usecode Integration](#usecode-integration)
7. [Font System](#font-system)
8. [Design Notes](#design-notes)

---

## Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump
  └── Gump_widget
        └── Dynamic_text_widget
```

**Files:**
- `gumps/Dynamic_text_widget.h`
- `gumps/Dynamic_text_widget.cc`

---

## Key Members

| Field | Type | Description |
|-------|------|-------------|
| `field_id` | int | Unique ID for usecode addressing |
| `font_num` | int | Font index from `fonts.vga` (changeable at runtime) |
| `text` | string | Current display text (per-instance, initially empty) |

**Constructor:** Uses shape number -1 (no background shape), positioned at `(pos_x, pos_y)` relative to the parent gump.

---

## Configuration

### gump_info.txt Format

Text fields are defined in the `dynamic_texts` section:

```
dynamic_texts
:gump_shape/field_id/pos_x/pos_y/font_num
```

### Field Reference

| Field | Description |
|-------|-------------|
| `gump_shape` | The gump shape number this text field belongs to |
| `field_id` | Unique identifier used by `UI_set_gump_text()` and `UI_set_gump_text_font()` |
| `pos_x` / `pos_y` | Position relative to gump origin |
| `font_num` | Default font index from `fonts.vga` (e.g., 2 for standard, 19 for highlight) |

### Example Configuration

A gump with item name labels, cost labels, and a total line:

```
dynamic_texts
; Item name labels (field_ids 1-5)
:200/1/40/10/2
:200/2/40/25/2
:200/3/40/40/2
:200/4/40/55/2
:200/5/40/70/2
; Cost labels (field_ids 6-10)
:200/6/150/10/2
:200/7/150/25/2
:200/8/150/40/2
:200/9/150/55/2
:200/10/150/70/2
; Total line (field_id 11)
:200/11/40/90/2
; Currency counts with highlight support (field_ids 30-33)
:200/30/180/10/2
:200/31/180/25/2
:200/32/180/40/2
:200/33/180/55/2
```

---

## Paint Behaviour

`paint()` calls `Shape_manager::get_instance()->paint_text(font_num, text.c_str(), sx, sy)` after converting widget-local coordinates to screen coordinates. When `text` is empty, painting is skipped entirely — no blank space is rendered.

This means all text fields appear empty when the gump first opens. Your on-open usecode handler must populate them.

---

## Intrinsics

Dynamic text fields are controlled by two dedicated intrinsics:

### `UI_set_gump_text(item, field_id, text)`

Sets the displayed text on a text field identified by `field_id`.

**Calle form:** `item->set_gump_text(field_id, text)`

| Parameter | Description |
|-----------|-------------|
| `item` | The container object whose gump contains the text field |
| `field_id` | Matches the `field_id` in `gump_info.txt` |
| `text` | String to display (use `"" + number` for integer-to-string conversion) |

**Returns:** `int` — 1 on success, 0 on failure (gump not open, field_id not found, or null text).

| Game | Intrinsic Slot |
|------|----------------|
| Black Gate | 0xdf |
| Serpent Isle | 0xe0 |

**Implementation:** Finds the open gump for `item`, iterates `gump->get_elems()`, matches the first `Dynamic_text_widget` with the given `field_id`, calls `set_text()`.

### `UI_set_gump_text_font(item, field_id, font_num)`

Changes the font used by a text field at runtime. Useful for highlighting active selections or dimming inactive labels.

**Calle form:** `item->set_gump_text_font(field_id, font_num)`

| Parameter | Description |
|-----------|-------------|
| `item` | The container object whose gump contains the text field |
| `field_id` | Matches the `field_id` in `gump_info.txt` |
| `font_num` | Font index from `fonts.vga` (e.g., 2 = standard, 19 = highlight) |

**Returns:** `int` — 1 on success, 0 on failure (gump not open, field_id not found).

| Game | Intrinsic Slot |
|------|----------------|
| Black Gate | 0xe3 |
| Serpent Isle | 0xe4 |

**Implementation:** Finds the open gump for `item`, iterates `gump->get_elems()`, matches the first `Dynamic_text_widget` with the given `field_id`, calls `set_font(font_num)`.

**Registration:**
- `usecode/bgintrinsics.h` — slots 0xdf, 0xe3
- `usecode/siintrinsics.h` — slots 0xe0, 0xe4
- `usecode/sibetaintrinsics.h` — slot 0xe4

---

## Usecode Integration

### On-Open Initialization

Text fields start empty. Populate them in the gump's on-open handler:

```usecode
void myGumpOpen shape#(200) ()
{
    // Runs automatically when gump 200 opens.
    // 'item' is the container that was double-clicked.
    UI_set_gump_text(item, 1, "Iron Sword");
    UI_set_gump_text(item, 2, "Leather Armor");
    UI_set_gump_text(item, 6, "75 Gold");
    UI_set_gump_text(item, 7, "120 Gold");
    UI_set_gump_text(item, 11, "SUM: 195 Gold");
}
```

### Integer-to-String Conversion

Usecode has no explicit integer-to-string conversion. Concatenate with an empty string:

```usecode
var cost = 500;
var label = "" + cost + " Gold";
UI_set_gump_text(item, 6, label);
```

### Font Highlighting

Use `UI_set_gump_text_font()` to visually distinguish active vs. inactive labels:

```usecode
// Highlight the active option with font 19, dim others with font 2.
var active_row = getActiveOption();
var row = 0;
while (row < 4)
{
    if (row == active_row)
    {
        UI_set_gump_text_font(item, 30 + row, 19);
    }
    else
    {
        UI_set_gump_text_font(item, 30 + row, 2);
    }
    row = row + 1;
}
```

### Updating Text from Button Callbacks

Button callbacks commonly update text fields to reflect state changes:

```usecode
void cycleOption shape#(1302) ()
{
    if (event != 1) { return; }

    var current = getActiveOption();
    var next = current + 1;
    if (next > 3) { next = 0; }
    setActiveOption(next);

    // Refresh display with new option values.
    refreshDisplay(item);
}
```

---

## Font System

Font numbers reference entries in the active font file (typically `fonts.vga` or `fonts_original.vga`). Common font indices:

| Font | Typical Use |
|------|------------|
| 2 | Standard text (white) |
| 19 | Highlighted text (different colour) |

The font can be changed at any time via `UI_set_gump_text_font()`. The change takes effect on the next paint cycle — no explicit repaint call is needed.

---

## Design Notes

### Why field_id Instead of Positional Indexing

Multiple text fields can belong to the same gump, and their order in `gump_info.txt` is not guaranteed. The `field_id` provides a stable identifier that usecode can reference regardless of how many text fields exist or in what order they are parsed.

### Why Text Starts Empty

Text content is instance-specific and cannot be statically configured in `gump_info.txt` — the content depends on game state at the moment the gump opens. By starting empty and relying on the on-open usecode handler, text fields support fully dynamic content (inventory counts, names, computed costs, etc.).

### Shape Number -1

Text widgets use shape number -1 because they have no background shape. This means `Gump_widget::on_widget()` will not find them via pixel-level hit testing, and `cache_shape()` in `shapeid.cc` has a guard to skip caching for negative shape numbers. Text fields are display-only and do not participate in click detection.

### No Save Persistence

Text content and font selection are transient — they reset when the gump is closed and reopened. By design, persistent game state belongs in usecode (e.g., global flags, static variables), not in UI widgets. Your on-open handler must repopulate all text fields each time the gump opens.

### Per-Instance State

Two gumps with the same shape number have independent text state. The `text` and `font_num` fields are stored per-widget-instance on the `Dynamic_text_widget` object, not on the shared `Gump_info` configuration singleton. This means opening two containers that use the same gump shape will show different content if their usecode callbacks write different values.

### Gump Lookup for Intrinsics

All text intrinsics use `find_gump(item)` to locate the open gump for the given container. If the container is a world-placed object (no owner), a fallback scan of the open gumps list finds it. If the gump is not open, the intrinsic silently returns 0.
