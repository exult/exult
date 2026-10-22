# Flat (Terrain) Shapes Outside 0–149

## Overview

Flat shapes are the 8x8 terrain tiles painted into chunks. The original games keep them at shapes 0–149 (`c_first_obj_shape` = `0x96`), and parts of Exult, mostly Exult Studio, still assume that range. This change lets a flat live at any shape number by testing the **shape data** (is frame 0 an 8x8 flat or an RLE shape?) instead of the shape number.

**Design Philosophy:** The shape data is the single source of truth. The loader already tells flats from RLE shapes (`Shape_frame::read`, `shapes/vgafile.cc`), and the engine already branches on `is_rle()` when it renders and edits terrain. No new shape info flag is added, because it would store the same fact twice and the two could disagree. `c_first_obj_shape` stays only where a file format needs it, such as the `shpdims.dat` offset.

**Compatibility:** Shapes 0–149 in BG and SI already load as flats, so the data-based test gives the same answer for original data. Chunk files already support 16-bit shape numbers (v2 chunks are written automatically when a terrain shape number is above `0x3ff`).

---

## Table of Contents

1. [Helper: Vga_file::is_flat](#helper-vga_fileis_flat)
2. [Exult Studio: Shape Browser](#exult-studio-shape-browser)
3. [Exult Studio: Object Shape Fields](#exult-studio-object-shape-fields)
4. [Unused Shapes Report](#unused-shapes-report)
5. [Guards](#guards)
6. [Known Limits](#known-limits)
7. [Status](#status)
8. [Testing](#testing)

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

## Exult Studio: Object Shape Fields

Fields that must hold an object shape (not a flat) use a shared test:

```cpp
bool ExultStudio::is_object_shape(int shnum);
```

Defined in `mapedit/studio.cc`. Returns `true` when `shnum` is in `c_first_obj_shape`..`c_max_shapes - 1` **and** is not a flat according to `Vga_file::is_flat()`. If `shapes.vga` isn't loaded, it keeps the range test alone.

**The 150 floor stays for now.** Keeping `shnum >= c_first_obj_shape` means this step only ever rejects more shapes than before (flats at 150 and above), never fewer. Allowing RLE object shapes below 150 would be a separate change.

**Validators.** Each `Shape_single` takes a validator, a plain function pointer (`bool (*)(int)`), so the lambdas can't capture anything; they call `ExultStudio::get_instance()->is_object_shape(shnum)`. The validator decides whether a dropped shape is accepted (`Shape_single::on_shape_dropped`), whether shape 0 means "none", and the frame count used by the animation preview. A flat at 150 or above is now treated the same as an original flat in all three. Fields using it:

| Editor | Fields |
|--------|--------|
| Object (`mapedit/objedit.cc`) | Shape |
| Container (`mapedit/contedit.cc`) | Shape |
| NPC (`mapedit/npcedit.cc`) | Shape |
| Egg (`mapedit/eggedit.cc`) | Monster shape |
| Shape info (`mapedit/shapeedit.cc`) | Weapon ammo and projectile, ammo family and sprite, container rules shape, and the frame previews for the edited shape (frame flags, hit points, frame names, frame usecode, object paperdoll, brightness, warmth) |

The egg missile shape keeps its own rule (`shnum >= 0`) and is unchanged.

**Saving.** The object and container windows let you type a shape number from `c_first_obj_shape` up. Before this change, every number in that range was an object; now a flat can be there too. `save_obj_window` and `save_cont_window` therefore check `is_object_shape()` before sending to Exult, and refuse with "You must set a valid object shape #." This copies the existing range check in `save_npc_window`.

The NPC and egg windows let you type any shape number from 0, so a flat could be saved there too (true on master as well). Their save paths refuse flats only, so no other shape that saves today is refused:
- `save_npc_window` adds `is_flat()` to its existing range check and keeps its "You must set a valid shape #." message.
- `save_egg_window` (Monster tab) had no shape check. It now refuses a flat with "You must set a valid monster shape #."

They use `Vga_file::is_flat()` directly, not `is_object_shape()`, because the latter's 150 floor would be a new limit on NPC and monster shapes.

---

## Unused Shapes Report

Exult's map editor can list object shapes that aren't used anywhere in the game (`Game_map::find_unused_shapes`, `gamemap.cc`). It sends the result to Exult Studio as a bit set, which Studio shows in its message window (`ExultStudio::show_unused_shapes`, `mapedit/studio.cc`). Both lists used to skip flats by starting at `c_first_obj_shape`. Flats at 150 or above are never placed as objects, so they would have been listed as unused.

Both lists still start at `c_first_obj_shape`, and now also skip a shape when `Vga_file::is_flat()` says it is a flat:
- Exult checks its own `shapes.vga` (`Shape_manager::get_shapes()`).
- Studio checks its own `shapes.vga`, and if none is loaded it lists the shape as before.

The bit set sent between them is unchanged, so a new Studio works with an older Exult and the other way round. The "found" bit is tested first, so `is_flat()` only runs for shapes that would otherwise be listed.

**Display fix (also on master).** Studio's message window showed nothing: `show_unused_shapes` cast its `GtkTextView` to `GtkEditable`, which a text view isn't in GTK3, so every line failed with `Gtk-CRITICAL: gtk_editable_insert_text: assertion 'GTK_IS_EDITABLE (editable)' failed` in `studio_err.txt`. The file's `Insert_text` helper now writes through the view's `GtkTextBuffer` with `gtk_text_buffer_insert_at_cursor()`, the same call `mapedit/execbox.cc` uses. It was the only user of the helper and the only place that made this cast.

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
| Object fields keep the `c_first_obj_shape` floor | `ExultStudio::is_object_shape` | Accepting shapes below 150 that were refused before |
| Missing `shapes.vga` keeps the range test alone | `ExultStudio::is_object_shape` | A null dereference, and rejecting every shape when no file is loaded |
| Flat shapes refused on save | `save_obj_window`, `save_cont_window` | Sending a flat typed into the shape field to Exult as an object |
| Flat shapes refused on save, nothing else | `save_npc_window`, `save_egg_window` (Monster) | Creating an NPC or monster egg with a flat shape, without adding a new limit on other shapes |
| Null `vgafile` or `Vga_file` skips the egg check | `save_egg_window` | A null dereference in a check that didn't exist before |
| Unused-shapes lists keep starting at `c_first_obj_shape` | `Game_map::find_unused_shapes`, `ExultStudio::show_unused_shapes` | Empty slots below 150 suddenly being listed as unused |
| Each side tests its own `shapes.vga`; the bit set is unchanged | Same | Breaking a newer Studio with an older Exult, or the other way round |
| Studio lists the shape if no `shapes.vga` is loaded | `ExultStudio::show_unused_shapes` | A null dereference |

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
| Exult Studio: object, NPC, egg, container and weapon/ammo validators | Done (150 floor kept) |
| "Unused shapes" reports (`gamemap.cc`, `mapedit/studio.cc`) | Done |
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
- Dragging a shape 0–149 onto the object, container, NPC, egg monster or weapon/ammo shape fields is still refused; dragging an object shape still works.
- Editing an object or container and pressing Apply still saves.
- Saving an NPC, or a Monster egg, with a creature shape still works.
- Typing a shape 0–149 into the NPC window or a Monster egg and saving shows the alert; on master it was accepted.
- The unused-shapes list (in Exult Studio's message window and Exult's `stdout.txt`) is the same as on master: no shapes below 150, and the same object shapes listed.
- Exult Studio's message window now shows the unused-shapes list, and `studio_err.txt` has no `gtk_editable_insert_text` criticals.

**New flat at shape 150 or above**
- Paint it as terrain, save, check that the chunks were written as v2, reload and confirm it renders.
- It can't be placed as an object, NPC, egg or container.
- Dragging it onto any object shape field is refused.
- Typing its number into the object or container window and pressing Apply shows "You must set a valid object shape #." and nothing is sent to Exult.
- It doesn't show up in either unused-shapes list.
- Water, solid and poison flags behave the same as on original flats.
