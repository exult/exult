# Flat (Terrain) Shapes Outside 0–149

## Overview

Flat shapes are the 8x8 terrain tiles painted into chunks. The original games keep them at shapes 0–149 (`c_first_obj_shape` = `0x96`), and parts of Exult, mostly Exult Studio, still assume that range. This change lets a flat live at any shape number by testing the **shape data** (is frame 0 an 8x8 flat or an RLE shape?) instead of the shape number.

**Design Philosophy:** The shape data is the single source of truth. The loader already tells flats from RLE shapes (`Shape_frame::read`, `shapes/vgafile.cc`), and the engine already branches on `is_rle()` when it renders and edits terrain. No new shape info flag is added, because it would store the same fact twice and the two could disagree. `c_first_obj_shape` stays only where a file format needs it, such as the `shpdims.dat` offset.

**Compatibility:** Shapes 0–149 in BG and SI already load as flats, so the data-based test gives the same answer for original data. Chunk files already support 16-bit shape numbers (v2 chunks are written automatically when a terrain shape number is above `0x3ff`).

---

## Table of Contents

1. [Helper: Vga_file::is_flat](#helper-vga_fileis_flat)
2. [Exult Studio: Shape Browser](#exult-studio-shape-browser)
3. [Guards](#guards)
4. [Known Limits](#known-limits)
5. [Status](#status)
6. [Testing](#testing)

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

## Exult Studio: Shape Browser

The old `IS_FLAT(shnum)` macro in `mapedit/shapelst.cc` (`shnum < c_first_obj_shape`) is replaced by a file-local helper:

```cpp
static bool Is_flat(Shape_file_info* finfo, int shnum);
```

- Returns `false` unless `finfo` is `shapes.vga` (`studio->get_vgafile()`), so faces, gumps, sprites and other files are never treated as tiles. This check used to be repeated next to every `IS_FLAT` use.
- For a shape that has frames, the answer comes from `Vga_file::is_flat()`.
- For an empty shape slot there is no data to test, so it falls back to the original rule (`shnum < c_first_obj_shape`). Empty slots behave exactly as before.

It is used where Studio works on an existing shape:

| Function | What the flat test decides |
|----------|----------------------------|
| `Shape_chooser::create_popup` | Whether "Edit tiled..." is offered |
| `Shape_chooser::export_tiled_png` | Assert that the shape is a flat |
| `Import_png` | A PNG imported into a flat must be 8x8 |
| `Shape_chooser::import_all_pngs` | All PNGs imported into a flat must be 8x8 |
| `Shape_chooser::import_shape` | A `.shp` file must match the shape's type (flat to flat, RLE to RLE) |
| `Shape_chooser::new_frame` | A new frame in a flat is an 8x8 tile |

**New shapes still go by number.** `Shape_chooser::new_shape` (the dialog's frame-count limit) and `Shape_chooser::create_new_shape` (the frames it creates) keep `shnum < c_first_obj_shape`. There is no data to test there: the slot is empty, or `Vga_file::new_shape()` has just reset it. A later step replaces this with a "tile or object?" choice.

---

## Guards

| Guard | Where | Protects against |
|-------|-------|------------------|
| Shape number range check before any lookup | `Vga_file::is_flat` | `Vga_file::get_shape()` indexes `shapes[shapenum]` with no bounds check, so a bad number would read out of range |
| Missing or empty frame 0 is "not flat" | `Vga_file::is_flat` | Empty shape slots being treated as tiles |
| Only `shapes.vga` can hold flats | `Is_flat` (`mapedit/shapelst.cc`) | Shapes in other files (faces, gumps, sprites) being offered tile editing or 8x8 import checks |
| Null file or missing `Vga_file` is "not flat" | `Is_flat` (`mapedit/shapelst.cc`) | A null dereference if a browser has no file loaded |
| Negative shape number is "not flat" | `Is_flat` (`mapedit/shapelst.cc`) | The old macro returned `true` for any negative number |
| Range check before `get_num_frames()` | `Is_flat` (`mapedit/shapelst.cc`) | `Vga_file::get_num_frames()` has no bounds check either |
| Empty slot falls back to `shnum < c_first_obj_shape` | `Is_flat` (`mapedit/shapelst.cc`) | Changing how Studio treats empty slots before the "tile or object?" step exists |

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
| Exult Studio: replace `IS_FLAT` (`mapedit/shapelst.cc`) | Done (new-shape creation still by number) |
| Exult Studio: object, NPC, egg, container and weapon/ammo validators | To do |
| "Unused shapes" reports (`gamemap.cc`, `mapedit/studio.cc`) | To do |
| Comment fix in `mapedit/chunklst.cc` | Done |
| Exult Studio: new empty shape type, mixed frames, 32-frame limit, RLE-misdetection warning | To do |

---

## Testing

**Original data (regression)**
- Load BG and SI. Shapes 0–149 still render, paint and save the same, and terrain and chunk saves don't change.
- Exult Studio's tiled PNG export/import and 8x8 import check still work for 0–149.
- Exult Studio's shape browser: "Edit tiled..." appears for shapes 0–149 in `shapes.vga`, not for shapes 150 and above, and never in other files (faces, gumps, sprites).
- Importing a PNG that isn't 8x8 into a shape 0–149 is still refused; importing any size into an object shape still works.
- "New frame" on a shape 0–149 still adds an 8x8 tile.

**New flat at shape 150 or above**
- Paint it as terrain, save, check that the chunks were written as v2, reload and confirm it renders.
- It can't be placed as an object, NPC, egg or container.
- It doesn't show up as an unused object shape.
- Water, solid and poison flags behave the same as on original flats.
