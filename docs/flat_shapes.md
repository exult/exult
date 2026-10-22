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
5. [Exult Studio: Frame Types](#exult-studio-frame-types)
6. [Guards](#guards)
7. [Known Limits](#known-limits)
8. [Status](#status)
9. [Testing](#testing)

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

**New shapes: "8x8 flat (terrain tile)" checkbox.** A new shape has no data to test: the slot is empty, or `Vga_file::new_shape()` has just reset it. So the new-shape dialog (`new_shape_window` in `mapedit/exult_studio.glade`) has a checkbox that decides it:
- Its default follows the shape number in the dialog: ticked below `c_first_obj_shape`, unticked from there up (`Shape_chooser::new_shape_num_changed`). Creating shapes works as before unless you change it. Changing the number resets it to the default.
- It's only enabled for `shapes.vga`; in other files it's unticked and greyed out.
- Ticking it limits the frame count to 32 and turns off and greys out "From font" (`Shape_chooser::new_shape_flat_toggled`).
- `Shape_chooser::create_new_shape` creates 8x8 flat frames when it's ticked and the file is `shapes.vga`, and caps the frame count at 32.

This also fixes a bug on master: the dialog's frame limit came from the shape **selected in the browser**, while the shape was created from the number **in the dialog**, so the two could disagree.

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

## Exult Studio: Frame Types

All frames of a shape must be the same type: `Shape::write()` asserts that every frame is present and either all flat or all RLE. Once flats can be anywhere, Studio has to keep it that way itself.

**Import all frames (also on master).** `Shape_chooser::import_all_pngs` created every frame as RLE, even when importing into a flat. Importing as many PNGs as the flat had frames silently turned it into an RLE shape; importing fewer left flat and RLE frames mixed, and Studio hit the assert on save. It now creates frames of the shape's own type (flat frames with the 8x8 origin, as `Import_png` does), and its 8x8 check also requires an 8-pixel row size, because a flat frame copies exactly 64 bytes.

**32-frame limit.** The loader reads a flat's frame number as `framenum & 31` (`shapes/vgafile.cc`), so a 33rd frame would come back as frame 0. "New frame" (`Shape_chooser::new_frame`) and "Import all frames" now refuse to go past 32 frames on a flat, with "A flat can't have more than 32 frames." (`c_max_flat_frames` in `mapedit/shapelst.cc`).

**Safety net on save.** `Image_file_info::flush` (`mapedit/shapefile.cc`) checks every shape it is about to write. If a shape has a missing frame or mixes flat and RLE frames, it shows "Shape N has missing frames, or mixes 8x8 flat and RLE frames, so 'file' was not saved." and writes nothing. The file stays marked as modified, so no edits are lost. It checks exactly the shapes `write_file` writes: modified or patched shapes in a FLEX file, and shape 0 in a single-shape file.

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
| Imported frames match the shape's type | `Shape_chooser::import_all_pngs` | Turning a flat into an RLE shape, or mixing frame types |
| 8x8 check includes the PNG's row size | `Shape_chooser::import_all_pngs` | Copying the wrong 64 bytes into a flat frame |
| No more than 32 frames on a flat | `Shape_chooser::new_frame`, `Shape_chooser::import_all_pngs` | Frame 32 and up being read back as frames 0–31 |
| Check frames before writing; refuse the save instead | `Image_file_info::flush` | Studio crashing on `Shape::write()`'s assert |
| File stays modified after a refused save | `Image_file_info::flush` | Losing unsaved edits |
| Same shapes checked as `write_file` writes | `Image_file_info::flush` | Blocking a save over a shape that wouldn't be written |
| "8x8 flat" defaults from the shape number | `Shape_chooser::new_shape_num_changed` | Changing how new shapes are created unless you ask |
| "8x8 flat" only for `shapes.vga` | `Shape_chooser::new_shape_num_changed`, `Shape_chooser::create_new_shape` | Creating flats in faces, gumps or other files |
| Frame count capped at 32 for a new flat | `Shape_chooser::new_shape_flat_toggled`, `Shape_chooser::create_new_shape` | A new flat with more frames than the loader can read |
| No "From font" for a new flat | `Shape_chooser::new_shape_flat_toggled` | Font frames (RLE) in a flat shape |
| Sensitivity passed to `ExultStudio::set_toggle()` explicitly, and the flat state applied after the font set-up | `Shape_chooser::new_shape_num_changed`, `Shape_chooser::new_shape_flat_toggled`, `Shape_chooser::new_shape` | `set_toggle()`'s `sensitive` argument defaults to `true`, which would re-enable the checkbox in other files and "From font" for a flat |
| Handlers do nothing if the dialog has no chooser yet | `on_new_shape_flat_toggled`, `on_new_shape_num_changed` | A null dereference if a signal fires before `new_shape()` sets it |

More guards are added as the change goes on (see [Status](#status)).

---

## Known Limits

| Limit | Source | Notes |
|-------|--------|-------|
| A flat has at most 32 frames | `shapes/vgafile.cc` (`framenum &= 31` in the flat loader) | Not tied to shape number; Exult Studio enforces it |
| A flat whose first 4 bytes, read as a little-endian number, equal its byte length, or that length minus 1, loads as RLE | `Shape_frame::read`, `shapes/vgafile.cc` | Rare; Exult Studio should warn when saving such a flat |
| All frames of a shape must be the same type | `Shape::write`, `shapes/vgafile.cc` (assert) | Exult Studio keeps them the same and refuses to save otherwise |

---

## Status

| Step | State |
|------|-------|
| `Vga_file::is_flat` helper | Done |
| Exult Studio: replace `IS_FLAT` (`mapedit/shapelst.cc`) | Done |
| Exult Studio: object, NPC, egg, container and weapon/ammo validators | Done (150 floor kept) |
| "Unused shapes" reports (`gamemap.cc`, `mapedit/studio.cc`) | Done |
| Comment fix in `mapedit/chunklst.cc` | Done |
| Exult Studio: frame types (import all frames, 32-frame limit, save check) | Done |
| Exult Studio: "8x8 flat" checkbox in the new-shape dialog | Done |
| Exult Studio: RLE-misdetection warning on save | To do |

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
- "Import all frames" of 8x8 PNGs into a shape 0–149 keeps it a flat ("Edit tiled..." still offered); importing into an object shape works as before.
- "Import all frames" into a flat refuses PNGs that aren't 8x8, and more than 32 PNGs.
- "New frame" on a flat with 32 frames shows "A flat can't have more than 32 frames."
- Saving `shapes.vga` after these edits works, and Exult loads the changed flats as tiles.
- New-shape dialog: "8x8 flat (terrain tile)" is ticked for a number below 150 and unticked from 150 up, and changing the number updates it. A new shape is created as before with the default.
- New-shape dialog in another file (faces, gumps): the checkbox is unticked and greyed out.
- Ticking it limits "Initial number of frames" to 32 and greys out "From font"; unticking restores both.

**New flat at shape 150 or above**
- Create it with "8x8 flat (terrain tile)" ticked. It shows as 8x8 tiles, "Edit tiled..." is offered, and it reloads as a flat after saving.
- Paint it as terrain, save, reload and confirm it renders. If its shape number is above 1023 (`0x3ff`), check that the chunks were written as v2.
- It can't be placed as an object, NPC, egg or container.
- Dragging it onto any object shape field is refused.
- Typing its number into the object or container window and pressing Apply shows "You must set a valid object shape #." and nothing is sent to Exult.
- It doesn't show up in either unused-shapes list.
- Water, solid and poison flags behave the same as on original flats.
