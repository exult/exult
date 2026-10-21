# Flat (Terrain) Shapes Outside 0–149

## Overview

Flat shapes are the 8x8 terrain tiles painted into chunks. The original games keep them at shapes 0–149 (`c_first_obj_shape` = `0x96`), and parts of Exult, mostly Exult Studio, still assume that range. This change lets a flat live at any shape number by testing the **shape data** (is frame 0 an 8x8 flat or an RLE shape?) instead of the shape number.

**Design Philosophy:** The shape data is the single source of truth. The loader already tells flats from RLE shapes (`Shape_frame::read`, `shapes/vgafile.cc`), and the engine already branches on `is_rle()` when it renders and edits terrain. No new shape info flag is added, because it would store the same fact twice and the two could disagree. `c_first_obj_shape` stays only where a file format needs it, such as the `shpdims.dat` offset.

**Compatibility:** Shapes 0–149 in BG and SI already load as flats, so the data-based test gives the same answer for original data. Chunk files already support 16-bit shape numbers (v2 chunks are written automatically when a terrain shape number is above `0x3ff`).

---

## Table of Contents

1. [Helper: Vga_file::is_flat](#helper-vga_fileis_flat)
2. [Guards](#guards)
3. [Known Limits](#known-limits)
4. [Status](#status)
5. [Testing](#testing)

---

## Helper: Vga_file::is_flat

```cpp
bool Vga_file::is_flat(int shapenum);
```

Defined in `shapes/vgafile.h`. Returns `true` when the shape exists and its frame 0 is an 8x8 flat; returns `false` for RLE shapes, empty shapes and out-of-range shape numbers. Frame 0 is loaded if it isn't already in memory.

Use this instead of a shape-number range test. In Exult Studio, keep it paired with the existing `file_info == studio->get_vgafile()` check, so only shapes in `shapes.vga` are treated as tiles.

**Why it is on `Vga_file`, not `Shape`:** `Shape::is_rle()` and `Shape::get_frame()` only look at frames that are already in memory. A `Shape`-level test would report "not flat" for any shape that hasn't been loaded yet. `Vga_file::get_shape()` loads the frame on demand, so the answer is always based on the data.

**Why it isn't written as `!is_rle()`:** `Shape::is_rle()` returns `false` for an empty or missing shape, so `!is_rle()` would call every empty slot a flat.

---

## Guards

| Guard | Where | Protects against |
|-------|-------|------------------|
| Shape number range check before any lookup | `Vga_file::is_flat` | `Vga_file::get_shape()` indexes `shapes[shapenum]` with no bounds check, so a bad number would read out of range |
| Missing or empty frame 0 is "not flat" | `Vga_file::is_flat` | Empty shape slots being treated as tiles |

More guards are added as the change goes on (see [Status](#status)).

---

## Known Limits

| Limit | Source | Notes |
|-------|--------|-------|
| A flat has at most 32 frames | `shapes/vgafile.cc` (`framenum &= 31` in the flat loader) | Not tied to shape number |
| A flat whose first 4 bytes, read as a little-endian number, equal its byte length, or that length minus 1, loads as RLE | `Shape_frame::read`, `shapes/vgafile.cc` | Rare; Exult Studio should warn when saving such a flat |
| All frames of a shape must be the same type | `Shape::write`, `shapes/vgafile.cc` (assert) | Exult Studio must refuse mixed frames |

---

## Status

| Step | State |
|------|-------|
| `Vga_file::is_flat` helper | Done |
| Exult Studio: replace `IS_FLAT` (`mapedit/shapelst.cc`) | To do |
| Exult Studio: object, NPC, egg, container and weapon/ammo validators | To do |
| "Unused shapes" reports (`gamemap.cc`, `mapedit/studio.cc`) | To do |
| Comment fix in `mapedit/chunklst.cc` | To do |
| Exult Studio: new empty shape type, mixed frames, 32-frame limit, RLE-misdetection warning | To do |

---

## Testing

**Original data (regression)**
- Load BG and SI. Shapes 0–149 still render, paint and save the same, and terrain and chunk saves don't change.
- Exult Studio's tiled PNG export/import and 8x8 import check still work for 0–149.

**New flat at shape 150 or above**
- Paint it as terrain, save, check that the chunks were written as v2, reload and confirm it renders.
- It can't be placed as an object, NPC, egg or container.
- It doesn't show up as an unused object shape.
- Water, solid and poison flags behave the same as on original flats.
