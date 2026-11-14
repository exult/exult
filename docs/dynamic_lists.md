# Dynamic Lists

## Overview

`Dynamic_list_widget` is a data-driven scrolling list for gumps. Each list shows a column of rows with an optional icon, a left text and a right-aligned column, a highlighted selection, and scroll controls. Rows come either from the gump's container (source 0: one row per object, which drags in and out like the objects in the picture) or from usecode (source 1: quest logs, shop lists, spell lists). Usecode reads and sets rows and the selection through dedicated intrinsics and is called back when the player selects or activates a row.

**Design Philosophy:** Lists extend the existing dynamic gump system rather than adding a parallel one. They are configured in a new `gump_info.txt` section parsed like the others, created in `Gump::set_object_area()` like the other widgets, and their scroll controls are a `Dynamic_slider` owned by the list (see [dynamic_sliders.md](dynamic_sliders.md)). A `dynamic_shapes` entry can be bound to a list to show the selected row enlarged (see [Selection-bound Shape Display](#selection-bound-shape-display)).

**Prerequisite:** Lists are created for any gump whose shape has `dynamic_lists` entries, even on a plain `Container_gump`. This is **safe and will not crash**. For full functionality the gump shape should also have a `container_area` entry, which makes `Gump_manager` create a **Dynamic Container Gump**:

| Feature | Dynamic Container Gump | Plain Container_gump |
|---------|----------------------|---------------------|
| Lists created and painted | Yes | Yes |
| Click to select, scroll arrows, thumb, track paging | Yes | Yes |
| Double-click / Enter activation | Yes | Double-click only |
| Source 0 rows are drag sources and drop targets | Yes | **No** — rows show and select only |
| Keyboard, mouse wheel, arrow auto-repeat | Yes | **No** |
| `rows_fn`, and `select_fn` after drags | Yes | **No** — they run from `update_gump()` |
| Intrinsics | Yes | Yes |

The list view of containers (Game Display Options, Containers: List) is a second host for the widget, laid out by the `list_layout` section; see [Container List View](#container-list-view-list_layout).

---

## Table of Contents

1. [Class Hierarchy](#class-hierarchy)
2. [Key Members](#key-members)
3. [Configuration](#configuration)
4. [Rows and Sources](#rows-and-sources)
5. [Selection-bound Shape Display](#selection-bound-shape-display)
6. [Input Handling](#input-handling)
7. [Paint Behaviour](#paint-behaviour)
8. [Intrinsics](#intrinsics)
9. [Usecode Integration](#usecode-integration)
10. [Container List View (`list_layout`)](#container-list-view-list_layout)
11. [Design Notes](#design-notes)
12. [Not Yet Supported](#not-yet-supported)

---

## Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump   (routes object queries to source-0 lists)
  └── Gump_widget
        ├── Gump_button
        │     └── Dynamic_list_widget
        │           └── owns a Dynamic_slider (scroll controls)
        ├── Dynamic_shape_widget            (optionally bound to a list)
        └── Dynamic_slider
```

**Files:**
- `gumps/Dynamic_list_widget.h`
- `gumps/Dynamic_list_widget.cc`
- `gumps/Dynamic_list_layout.h` — row arithmetic with no engine types (clamping, hit rows, name shortening, preview scale, key navigation)

---

## Key Members

| Field | Type | Description |
|-------|------|-------------|
| `def` | Dynamic_list_def | The parsed `gump_info.txt` entry |
| `slider` | unique_ptr\<Dynamic_slider\> | Scroll controls, parent = the list |
| `first` | int | First visible row |
| `rows` | vector\<Row\> | Rows: text, right text, icon shape/frame, flags, font |
| `objs` | vector\<Game_object*\> | Source 0: the object of each row |
| `signature` | vector | Source 0: object, shape, frame and quantity of each row; rows are rebuilt only when it changes |
| `sel_index` | int | Source 1: selected row (-1 = none) |
| `sel_obj`, `sel_hint` | Game_object*, int | Source 0: selected object and the row to select if it goes away |
| `rows_pending`, `select_pending` | bool | Callbacks due from `run()` |

---

## Configuration

### gump_info.txt Format

Lists are defined in the `dynamic_lists` section. All 16 fields are required:

```
%%section dynamic_lists
:gump_shape/field_id/x/y/width/rows/row_height/font/source/up_shape/down_shape/thumb_shape/select_fn/activate_fn/rows_fn/flags
%%endsection
```

### Field Reference

| Field | Description |
|-------|-------------|
| `gump_shape` | The gump shape number this list belongs to |
| `field_id` | Unique identifier used by the list intrinsics and by bound `dynamic_shapes` |
| `x` / `y` | Top-left of the first row, relative to the gump origin |
| `width` | Row width in pixels. The scroll controls sit at `x + width + 3`; with Exult's controls the list is `width + 15` pixels wide |
| `rows` | Number of visible rows |
| `row_height` | Pixel height of each row |
| `font` | Row font (a `fonts.vga` number, as in `dynamic_texts`) |
| `source` | 0 = the gump container's contents; 1 = rows set by usecode |
| `up_shape` | `gumps.vga` scroll-up arrow. **-1 = Exult's own scroll controls** (the save/load gump's arrows and slider from `exult.flx`, with a shaded groove); `down_shape` and `thumb_shape` are then ignored |
| `down_shape` | `gumps.vga` scroll-down arrow |
| `thumb_shape` | `gumps.vga` thumb. -1 with `up_shape >= 0` = spinner: arrows only, as in `dynamic_sliders` |
| `select_fn` | Usecode shape function called when the player changes the selection (-1 = none) |
| `activate_fn` | Usecode called on a double-click or Enter on a source-1 row (-1 = none) |
| `rows_fn` | Usecode called once when the gump opens and after source-0 rows were rebuilt because the contents changed (-1 = none) |
| `flags` | 0x01 = an "N-M of T" count line under the rows (font 4); 0x02 = "Empty" centred in the rows when there are none |

Entries with a width, row count or row height of 0 or less are ignored.

### Example Configuration

```
%%section dynamic_lists
; Contents of the bag gump 200: Exult's scroll controls, count line, "Empty",
; and a weight column filled in by usecode shape 1310.
:200/1/20/30/123/8/12/2/0/-1/-1/-1/-1/-1/1310/3
; A quest log on gump 201: rows from usecode, own arrows 117/118 and thumb 120,
; select_fn 1311, activate_fn 1312.
:201/2/16/24/150/10/10/2/1/117/118/120/1311/1312/-1/2
%%endsection
```

### Scroll Controls

The arrows sit at the top and bottom of the rows' column, the track between them:

| | Exult's controls (`up_shape = -1`) | Gump shapes |
|--|--|--|
| Shapes | `exult.flx` `SAV_UP`, `SAV_DOWN`, `SAV_SLIDER` | `up_shape`, `down_shape`, `thumb_shape` from `gumps.vga` |
| Placed by | Frame top-left | Hotspot, as `dynamic_sliders` |
| Gap between arrow and track | 1 pixel | none |
| Thumb | 2 pixels into the arrows' column | at the controls' x |
| Groove | Shaded 70%, 5 pixels wide | none (part of the gump art) |

The track length is `rows * row_height` minus both arrows' heights (and the gaps). The thumb is hidden while everything fits. With `x = 51`, `width = 123`, 8 rows of 12 pixels, Exult's controls reproduce the container list view's scroll bar exactly: arrows at x 177, y 42 and 126, track 55–125, 63 pixels of travel.

---

## Rows and Sources

### Source 0: Container Contents

One row per object in the gump's container that has a shape, in container order. Each row shows the object's icon centred in a 14 x (`row_height` - 2) cell, its name (without a leading quantity, capitalised, shortened at a word boundary with "..." to fit) and its quantity right-aligned when more than one.

The rows are the objects themselves. On a dynamic container gump:
- pressing a row selects it and starts dragging the object, as in the picture;
- dropping on a row's object gives it to that object (into a bag, onto a matching stack); dropping elsewhere on the list puts it in the container and lets the picture choose its place;
- a double-click or Enter uses the object (`activate()`), exactly as a double-click in the picture would;
- dragging between two lists, into another gump or onto a snap zone works because it is Exult's own object drag.

The selection follows the container list view's rules: the first row is selected when the gump opens, and when the selected object leaves, the row that took its place (or the new last row) is selected. `UI_set_list_selection(item, id, 0)` clears it until the player selects again.

Rows are rebuilt only when the contents change (object, shape, frame or quantity of any row), never from paint. After a rebuild, and once when the gump opens, `rows_fn` runs; it can use `UI_set_list_row` to fill in the right column, a font or the disabled flag. Those last until the next rebuild.

### Source 1: Usecode Rows

The list starts empty. Usecode fills it with `UI_set_list_rows` (all rows) and `UI_set_list_row` (one row, or append). A row is a string (its text) or an array:

```
[text, right_text, shape, frame, flags, font]
```

Trailing elements are optional, and `text` and `right_text` may be numbers. `shape` is a `shapes.vga` icon (-1 = none); when no row has an icon, text starts 2 pixels in instead of after the icon cell. `flags & 1` = disabled. `font` -1 = the list's font. Usecode strings are translated as for `UI_set_gump_text`.

### Disabled Rows

A disabled row is drawn and then shaded to 70%. It cannot be selected, activated or dragged, and the keys skip it. Disabling the selected row clears the selection (source 0 then selects the next enabled row).

---

## Selection-bound Shape Display

A `dynamic_shapes` entry takes optional trailing fields:

```
:gump_shape/field_id/x/y/shape_num/frame_num/shapefile[/list_id[/box_w/box_h]]
```

- `list_id >= 0`: the display shows the selection of the list with that `field_id` on the same gump: the object itself (source 0) or the row's icon (source 1). With nothing selected, or a row without an icon, it shows its own shape (none when `shape_num` is 0).
- `box_w`, `box_h > 0`: `x, y` is a box's top-left. The shape is centred in the box and enlarged by the largest factor up to 4 that fits (`w * k <= box_w - 8`, `h * k <= box_h`; at least 1). Transparent pixels show what is under the box and translucent ones are blended with it, as in the container list view's preview.

The binding is read when the display paints, so there is no callback between the widgets and one list can feed several displays. See [dynamic_shapes.md](dynamic_shapes.md).

---

## Input Handling

### Mouse

| Action | Behaviour |
|--------|-----------|
| Click a row | Selects it (not a disabled one). `select_fn` runs on release if the selection changed |
| Double-click a row | Source 1: `activate_fn`. Source 0: the object is used. Never closes the gump |
| Click or hold an arrow | Scrolls one row; holding repeats (500 ms, then every 50 ms), as sliders |
| Click the track above or below the thumb | Scrolls a page |
| Drag the thumb | Scrolls with it |
| Mouse wheel over the list | One row; with Alt, a page |

The wheel goes first to a list under the pointer, then to the other widgets in gump order. A `dynamic_sliders` entry takes the wheel wherever the pointer is, so over a list the list scrolls and elsewhere the slider moves.

`Gump::on_button()` finds the list only over its rows. Clicks on the scroll controls reach it through `Gump::forward_mouse_down()`, which it hands to its slider.

### Keyboard

| Key | Behaviour |
|-----|-----------|
| Up / Down | Previous / next enabled row |
| Page Up / Page Down | A page up / down |
| Home / End | First / last enabled row |
| Enter | Activate the selected row (as a double-click) |

Moving the selection scrolls it into view and runs `select_fn`. Keys reach widgets in gump order and the first that takes a key wins, so on a gump with a `dynamic_sliders` entry before the list, the slider takes Up/Down. With several lists, the one clicked last takes the keys (the first list until one is clicked).

### Callbacks

All three callbacks run like slider callbacks: `call_usecode(get_shape_fun(fn), container, double_click)`, with `item` = the gump's container. Read the state with the intrinsics; nothing is passed. Setting rows or the selection from usecode never runs a callback.

---

## Paint Behaviour

Paint order:
1. **Rows** — the selected row shaded 85%, odd rows 95%; icon clipped to its cell; text from `x + 18` (`x + 2` without icons), 2 pixels below the row top in a 12-pixel row (centred the same way in other heights); right column right-aligned at `x + width - 2`; disabled rows shaded 70% over everything
2. **Count line** (flag 0x01) — "N-M of T" in font 4, centred over `width`, 2 pixels below the rows; or **"Empty"** (flag 0x02) centred over `width + 15` and the rows' height when there are no rows
3. **Groove** (Exult's controls only), then the slider's arrows and thumb

The shades are tables made from the current palette, as the list view's, so they keep to the art's own tones in both games.

---

## Intrinsics

> **Provisional slots.** These intrinsic numbers are the next free ones after `UI_get_shape_flag` at the time of writing and are marked `(Exult, provisional)` in the tables. They may move before this is merged upstream; compile usecode against the headers of the Exult you run.

| Intrinsic | BG | SI | SI beta | Returns |
|-----------|----|----|---------|---------|
| `UI_set_list_rows(item, field_id, rows)` | 0xe5 | 0xe6 | 0xe5 | Row count. Source 1 only (source 0: 0). Clears the selection, scrolls to the top. A string = one row |
| `UI_set_list_row(item, field_id, index, row)` | 0xe6 | 0xe7 | 0xe6 | 1/0. Source 1: rows 1..n are replaced, n+1 appends. Source 0: sets right text, flags and font of rows 1..n |
| `UI_get_list_selection(item, field_id)` | 0xe7 | 0xe8 | 0xe7 | Selected row, 1-based; 0 = none |
| `UI_set_list_selection(item, field_id, index)` | 0xe8 | 0xe9 | 0xe8 | 1/0 (0 for a missing or disabled row). 0 clears. Scrolls the row into view. No callback |
| `UI_get_list_count(item, field_id)` | 0xe9 | 0xea | 0xe9 | Number of rows |
| `UI_get_list_object(item, field_id, index)` | 0xea | 0xeb | 0xea | Source 0: the object in row `index` (0 = the selected row); otherwise 0 |
| `UI_open_gump(item[, gump_shape])` | 0xeb | 0xec | 0xeb | 1 if the gump is open afterwards |

`item` is the container whose gump holds the list. The list is looked up on the gump whose container is `item`, then on the gump showing `item`, then on its owner's, so a bag inside a backpack finds the bag's own gump.

`UI_open_gump` opens a container's gump (`Gump_manager::add_gump`) as a double-click on it would, with its own gump shape unless `gump_shape` is given: a dynamic gump when `gump_info.txt` makes it one, with its on-open usecode. NPCs are not opened. It lets a list gump be opened from a button or a script, which no other intrinsic does.

**Registration:**
- `usecode/bgintrinsics.h` — 0xe5–0xeb
- `usecode/siintrinsics.h` — 0xe6–0xec
- `usecode/sibetaintrinsics.h` — 0xe5–0xeb
- Declarations `usecode/ucinternal.h`, implementations `usecode/intrinsics.cc`

---

## Usecode Integration

### A Quest Log (Source 1)

A journal object whose gump 201 has a source-1 list (field 2) and a text field (field 3) for the details. A button elsewhere opens it with `UI_open_gump`:

```usecode
void openQuestLog shape#(1320) ()
{
    if (event != 1) { return; }
    UI_open_gump(getJournal());
}

// Gump 201's own shape function: runs when the gump opens.
void questLogOpen shape#(201) ()
{
    UI_set_list_rows(item, 2, ["Find the Wizard", "Return the ring", "Speak to the Monks"]);
    // A finished quest: greyed, with a tick in the right column.
    UI_set_list_row(item, 2, 4, ["Free the prisoners", "done", -1, 0, 1, -1]);
    UI_set_list_selection(item, 2, 1);
    UI_set_gump_text(item, 3, "Seek him in the swamp.");
}

// select_fn 1311.
void questSelected shape#(1311) ()
{
    var details = ["Seek him in the swamp.", "The ring is in the crypt.", "They are in the temple."];
    var sel = UI_get_list_selection(item, 2);
    if (sel > 0 && sel <= 3) {
        UI_set_gump_text(item, 3, details[sel]);
    }
}

// activate_fn 1312: double-click or Enter.
void questActivated shape#(1312) ()
{
    var sel = UI_get_list_selection(item, 2);
    // ... mark the quest, show a map, etc.
}
```

### A Weight Column (Source 0)

`rows_fn` runs when the contents change, so it is the place for a computed column:

```usecode
// rows_fn 1310 on a contents list (field 1).
void fillWeights shape#(1310) ()
{
    var n = UI_get_list_count(item, 1);
    var i = 1;
    while (i <= n) {
        var obj = UI_get_list_object(item, 1, i);
        var wt = UI_get_item_weight(obj);
        UI_set_list_row(item, 1, i, ["", wt]);    // text is ignored for source 0
        i = i + 1;
    }
}
```

### Reading the Selection

```usecode
var obj = UI_get_list_object(item, 1, 0);    // The selected object, or 0.
if (obj) {
    UI_set_gump_shape(item, 4, UI_get_item_shape(obj), UI_get_item_frame(obj));
}
```

---

## Container List View (`list_layout`)

The list view of containers (Game Display Options, **Containers: List**) is the second host for the widget. `Container_list_gump` shows a container as a scroll: a source-0 list of its contents on the left, the selected item enlarged with its details on the right, and the container's own picture (with its objects and close button) hanging off the bottom-left. Where all of that goes is data: the `list_layout` section of `gump_info.txt`.

### When It Applies

`list_layout` is consulted only in `Gump_manager::add_gump()`, only when the player has chosen Containers: List, and only after the existing checks: special gumps (spellbook, jawbone, stats, spell scroll), actors, and gumps with a `container_area` entry (dynamic gumps, including ritual containers with `snap_zones`) are created as before. So a `list_layout` entry never turns a container into a dynamic gump, and with Containers: Picture nothing here is read. A container opens in the list view when its effective layout is not `off` and the background frame is the one the layout describes (one frame of `width` x `height` with its hotspot at the top-left); otherwise it opens in the picture view.

### Format

```
:gump_shape/part/fields...
```

One line per part, keyed by the container's gump shape. `gump_shape` `-1` is the default for every container gump. Numbers are decimal.

| Part | Fields | Shipped default (SI; BG uses background shape 55) |
|------|--------|---------------------------------------------------|
| `off` | none | absent. On a shape: that gump opens in the picture view. `:-1/off`: the list view is off everywhere |
| `background` | `shape/width/height/strip_x0/strip_x1/extra_w` | `49/224/173/80/140/96`. `shape` -1 = the game's `gumps/scroll`. Columns `strip_x0` to `strip_x1 - 1` are repeated to widen the art by `extra_w` (0 = drawn as it is) |
| `list` | the 15 `dynamic_lists` fields after `gump_shape` | `0/51/42/123/8/12/2/0/-1/-1/-1/-1/-1/-1/3`. `source` is always treated as 0 |
| `title` | `x/y/width/font` | `51/31/138/2`: the container's name centred over `width` at `y - 2` (shortened to `width - 28`), a rule at `y + 8` from `x + 4` to `x + width - 4` |
| `divider` | `x/y0/y1` | `193/33/145`: a dotted line at `x`, rows `y0` to `y1 - 1` |
| `panel` | `x/y/width/font/small_font` | `198/31/91/2/4`: the selected item's name and rule as the title, then quantity, weight and two hints from `y + 66`, one every 8 rows |
| `preview` | `x/y/box_w/box_h` | `198/43/91/50`: the selected item, centred and enlarged in this box (a bound, boxed `Dynamic_shape_widget`) |
| `picture` | `right/top` | `46/150`: the top-right corner of the container's own picture |
| `nav` | `left_x/right_x/y` | `51/177/27`: top-left of the ◀ ▶ buttons that step through nested containers; any value < 0 hides both |
| `usecode` | `open_fn` | `-1`: usecode run when the view opens (event `double_click`, item = the container), like a dynamic gump's on-open function; -1 = none |

All positions are relative to the top-left of the widened background. The C++ defaults (`List_layout` in `gumpinf.h`) are the same numbers, with background shape -1; `data/bg/gump_info.txt` and `data/si/gump_info.txt` ship them spelled out.

### Merging and `off`

The layout for a gump is the built-in one, overlaid part by part with the `-1` entry's parts and then with that gump shape's own parts. A line replaces its part only, so a mod overrides just what it changes; a patch `gump_info.txt` line replaces the same part of the shipped file. The gump is `off` if either its own entry or the `-1` entry has `off`. The layout chosen when the view opens stays while ◀ ▶ step through nested containers; containers whose gump is `off` (or a dynamic gump, or a special gump) are skipped.

### Hooks

| Hook | Where | Called |
|------|-------|--------|
| On open | `usecode/open_fn` | once, after the view opened |
| On select | `list/.../select_fn` | when the player's press, drop or a removal changes the selection |
| Row text | `list/.../rows_fn` | once when the view opens and again whenever the contents change (or ◀ ▶ shows another container); never from paint |

All three get the shown container as `item`, so the list intrinsics address the view with `UI_get_list_count(item, field_id)` and friends, `field_id` being the list part's (0 by default). In `rows_fn`, `UI_set_list_row(item, field_id, index, ["", text])` sets a row's right-aligned column (replacing the quantity) and `flags`/`font` as for any source-0 list; the values last until the rows are rebuilt, which is when `rows_fn` runs again.

### Example: a Mod's Own Scroll and a Weight Column

```
%%section list_layout
# Every container: rows a little narrower, weights in the right column.
:-1/list/0/51/42/123/8/12/2/0/-1/-1/-1/-1/-1/1310/3
# Gump 52: a mod's own art (gumps.vga 160, 280 x 180), not widened,
# without the nested-container arrows.
:52/background/160/280/180/0/0/0
:52/nav/-1/-1/-1
# Gump 57 keeps its picture view.
:57/off
%%endsection
```

`1310` would be a `fillWeights` function like the one in [A Weight Column](#a-weight-column-source-0), using field id 0.

### What Stays in C++

The panel's content (name, quantity, weight, the two hints), the stepping order (the opened container, then the containers inside it depth-first) and the input rules are fixed: a click selects, a double-click uses the item, items drag in and out of rows and the picture, the wheel scrolls anywhere on the gump. The list view takes no keyboard focus, so Up/Down and the other keys keep their game functions while a container is open.

---

## Design Notes

### Why a Gump_button

`Gump::on_button()` returns `as_button()` of the widget under the mouse, and `Gump_manager::double_clicked()` only reaches widgets through it. As a `Gump_button`, the list gets the press (select), the release (`activate()`, where `select_fn` runs) and the double-click (`double_clicked()`), and a double-click on a row never closes the gump when "double-click closes gumps" is on. `on_button()` answers only over the rows, so the scroll controls still get `mouse_down()` through `forward_mouse_down()`.

### Scrolling Reuses Dynamic_slider

The list owns one `Dynamic_slider` and sets it up through C++-only fields of `Dynamic_slider_def` (`shapefile`, `track_pad`, `thumb_inset`, `top_left`, `hide_idle_thumb`) and a C++ change hook used instead of the usecode callback. Its range is 0 to `count - rows`. The list adds paging on track clicks (`Dynamic_slider::hit_part()`) and paints the groove; arrows, thumb drag and auto-repeat are the slider's.

### Usecode Never Runs from Paint

Source-0 rows are rebuilt by `sync_rows()` (pure C++, at the start of paint and before any query) only when their signature changes; that only marks `rows_fn` as due, and `run()` (each frame, from `update_gump()`) calls it. `select_fn` runs from the release, key and drop handlers. During a press or a drag the engine still uses the widget afterwards, so the call waits for the release or for `run()`. After a callback the widget returns at once, because usecode may have closed the gump.

### Any Gump

Like the other dynamic widgets, lists are created on any `gumps.vga` gump with entries. The drag-and-drop routing for source 0 needs the host to send object queries to the list; `Dynamic_container_gump` and `Container_list_gump` do. On other gumps a source-0 list shows and selects rows only.

### No Save Persistence

Rows, selection and scroll position live in the open gump only.

---

## Not Yet Supported

Guesses at what the Ultima 8 mod team (U8E) might need that did not fit cleanly yet:
- more than two columns, with per-column alignment;
- an icon-only grid mode;
- dragging source-1 (usecode) rows;
- keyboard focus that a list keeps across gumps (keys go to the gump with keyboard focus, first list or last clicked).
