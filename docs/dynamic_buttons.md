# Dynamic Buttons

## Overview

`Dynamic_button` is a data-driven clickable button widget for dynamic container gumps. Buttons are configured entirely through `gump_info.txt` — each button has an idle frame, clicked frame, optional sound effect, usecode callback, action type, and visibility control. No C++ changes are needed to add new buttons to a gump layout.

**Design Philosophy:** Buttons extend `Gump_button` and follow the existing click chain (`push → unpush → on_button → activate`). Each button that needs distinct behaviour uses a unique `usecode_fn` shape number, resolved at runtime through the usecode symbol table.

**Prerequisite:** Dynamic buttons are created in `Gump::set_object_area()` (the base class), so any gump whose shape has `dynamic_buttons` entries in `gump_info.txt` will have them instantiated — even on a plain `Container_gump`. This is **safe and will not crash**. However, for full functionality the gump shape should also have a `container_area` entry, which causes `Gump_manager` to create a **Dynamic Container Gump**. Without it:

| Feature | Dynamic Container Gump | Plain Container_gump |
|---------|----------------------|---------------------|
| Buttons created & painted | Yes | Yes |
| Button clicks (on_button) | Yes | Yes |
| Intrinsic access (find_gump) | Yes | Yes |
| On-open usecode fires | Yes | **No** — gated by `dynamic_cast<Dynamic_container_gump*>` |
| Screen centering | Yes | **No** — cascading/saved position used |
| Bounding-box hit testing | Yes (override) | Base Gump RLE + elems fallback (works) |
| Debug visualization | Yes | No (DCG-only paint) |

See [dynamic_gumps.md](dynamic_gumps.md) for full container gump setup.

---

## Table of Contents

1. [Class Hierarchy](#class-hierarchy)
2. [Key Members](#key-members)
3. [Configuration](#configuration)
4. [Click Chain](#click-chain)
5. [Activation Flow](#activation-flow)
6. [Paint and Frame Management](#paint-and-frame-management)
7. [Visibility Control](#visibility-control)
8. [Usecode Integration](#usecode-integration)
9. [Bug Fixes](#bug-fixes)
10. [Design Notes](#design-notes)

---

## Class Hierarchy

```
Gump_Base
  └── Gump
        └── Container_gump
              └── Dynamic_container_gump
  └── Gump_widget
        └── Gump_button
              └── Dynamic_button
```

**Files:**
- `gumps/Dynamic_button.h`
- `gumps/Dynamic_button.cc`

---

## Key Members

| Field | Type | Description |
|-------|------|-------------|
| `default_frame` | int | Frame displayed when idle |
| `clicked_frame` | int | Frame displayed when pushed |
| `sound_fx` | int | SFX index on click (-1 = none) |
| `usecode_fn` | int | Shape number for usecode lookup (-1 = none) |
| `action_type` | int | 0=nothing, 1=close gump after activate, 2=toggle |
| `visibility_flag` | int | Global flag controlling visibility (-1 = always visible) |
| `usecode_param` | int | Legacy field, read but unused at runtime |
| `toggled` | bool | Current toggle state (action_type 2 only) |

---

## Configuration

### gump_info.txt Format

Buttons are defined in the `dynamic_buttons` section:

```
dynamic_buttons
:gump_shape/button_shape/pos_x/pos_y/default_frame/clicked_frame/sound_fx/usecode_fn/action_type/visibility_flag/usecode_param
```

### Field Reference

| Field | Description |
|-------|-------------|
| `gump_shape` | The gump shape number this button belongs to |
| `button_shape` | Shape number in `gumps.vga` for the button graphic |
| `pos_x` / `pos_y` | Position relative to gump origin |
| `default_frame` | Frame when idle (typically 0) |
| `clicked_frame` | Frame when pushed (typically 1) |
| `sound_fx` | SFX index on click (-1 = none) |
| `usecode_fn` | Shape number for usecode lookup (-1 = no callback) |
| `action_type` | 0=nothing, 1=close gump after callback, 2=toggle |
| `visibility_flag` | Global flag index for visibility (-1 = always visible) |
| `usecode_param` | Legacy, read but unused (event is always `double_click`) |

### Example Configuration

A gump with a confirm button and a cancel button:

```
dynamic_buttons
; Confirm button — closes gump after callback
:200/130/10/120/0/1/-1/1300/1/-1/0
; Cancel button — closes gump after callback
:200/131/60/120/0/1/-1/1301/1/-1/0
; Toggle button — stays open, toggles state
:200/132/110/120/0/1/67/1302/2/-1/0
```

In this example:
- Gump shape 200 has three buttons
- The confirm button (shape 130) calls usecode shape 1300 then closes the gump (`action_type=1`)
- The cancel button (shape 131) calls usecode shape 1301 then closes the gump
- The toggle button (shape 132) plays SFX 67, calls usecode shape 1302, and toggles visual state (`action_type=2`)

---

## Click Chain

Dynamic buttons participate in the standard non-modal gump click chain:

1. **Mouse down** → `exult.cc` → `gwin->start_dragging(x, y)`
2. **`Dragging_info` constructor** → `find_gump(x,y)` → `gump->on_button(x,y)` → `button->push()`
3. **Mouse up** → `drag.cc` `drop()` → `button->unpush()` → `button->on_button(x,y)` → if HIT: `button->activate()`

`on_button()` uses `on_widget()` for pixel-level hit-testing against the current frame's `Shape_frame`. The resting frame must always be `default_frame` for this test to use the correct shape origin (see [Paint and Frame Management](#paint-and-frame-management)).

---

## Activation Flow

When `activate()` is called after a successful click:

1. Play `sound_fx` if >= 0
2. Toggle visual state if `action_type == 2`
3. Resolve `usecode_fn` to function ID via `ucm->get_shape_fun(usecode_fn)` — this consults the symbol table, supporting `#autonumber` mods
4. Call `ucm->call_usecode(fn_id, item, double_click)` where `item` is the gump's container object
5. Close gump if `action_type == 1`

**Important:** Buttons always fire with `Usecode_machine::double_click` as the event. The `usecode_param` field is read from config but not used at runtime. To distinguish between buttons, assign each a unique `usecode_fn`.

---

## Paint and Frame Management

`Dynamic_button::paint()` paints directly using `paint_shape()`, bypassing `Gump_button::paint()`. This is critical because `Gump_button::paint()` applies its own frame adjustment (`frame + 1` when pushed), which double-adjusts the frame and leaves it stuck at the wrong value after painting.

```
paint() flow:
1. Determine paint_frame:
   - If action_type=2 and toggled → clicked_frame
   - If is_pushed() → clicked_frame
   - Otherwise → default_frame
2. Save current frame (prev_frame)
3. Set frame to paint_frame
4. Paint the shape at screen coordinates
5. Restore frame to prev_frame (default_frame)
```

This save/restore ensures `on_widget()` always tests hit against `default_frame`'s `Shape_frame`, whose origin matches the configured position. If the frame were stuck at `clicked_frame` — which may have a different shape origin — the RLE pixel-level hit test would use wrong coordinates and the button would stop responding to clicks.

---

## Visibility Control

The `visibility_flag` field controls whether a button is rendered and clickable:

- **`-1`** — Always visible (default for most buttons)
- **`>= 0`** — Visible only when global flag `visibility_flag` is set to true

`is_visible()` checks `ucm->get_global_flag_bool(visibility_flag)`. When false, the button is neither painted nor included in hit testing.

This allows usecode to show/hide buttons by toggling global flags, useful for context-sensitive UI (e.g., showing a "Confirm" button only after a selection is made).

---

## Usecode Integration

### Intrinsics

Dynamic buttons do not have widget-specific intrinsics. They interact with usecode through the callback mechanism:

- Each button's `usecode_fn` points to a shape number
- When clicked, the usecode function mapped to that shape is invoked
- The function receives `item` (the gump's container) and `event` (always `double_click`, i.e. 1)

### Callback Pattern

```usecode
// Define a callback function for a confirm button.
// The usecode_fn in gump_info.txt should be set to 1300.
void confirmAction shape#(1300) ()
{
    if (event != 1) { return; }

    // 'item' is the container whose gump hosts this button.
    // Perform your action here.
    UI_close_gump2(item);
}
```

### Toggle Button Pattern

Toggle buttons (`action_type=2`) maintain visual state but do not pass toggle state to usecode. Your callback should track state separately:

```usecode
// Toggle callback — tracks state via a global flag.
void toggleOption shape#(1302) ()
{
    if (event != 1) { return; }

    if (gflags[MY_TOGGLE_FLAG])
    {
        gflags[MY_TOGGLE_FLAG] = false;
    }
    else
    {
        gflags[MY_TOGGLE_FLAG] = true;
    }
}
```

### Multiple Buttons with Shared Logic

When several buttons share similar logic (e.g., quantity adjustment per row), each still needs a unique `usecode_fn`. Use a shared helper function:

```usecode
// Shared helper — adjusts a value at the given row index.
void adjustRow(var container, var row, var delta)
{
    // Read current value, apply change, clamp, update display.
    var current = readRowValue(row);
    var next = current + delta;
    if (next < 0)  { next = 0; }
    if (next > 99) { next = 99; }
    writeRowValue(row, next);

    // Update the text field for this row.
    var label;
    if (next == 0) { label = ""; }
    else           { label = "x" + next; }
    UI_set_gump_text(container, 40 + row, label);
}

// Per-row callbacks — each with a unique usecode_fn shape.
void incRow0 shape#(1206) () { if (event == 1) { adjustRow(item, 0,  1); } }
void incRow1 shape#(1207) () { if (event == 1) { adjustRow(item, 1,  1); } }
void decRow0 shape#(1211) () { if (event == 1) { adjustRow(item, 0, -1); } }
void decRow1 shape#(1212) () { if (event == 1) { adjustRow(item, 1, -1); } }
```

---

## Bug Fixes

### Bug 3: Dynamic Button Frame Management

**Problem:** Buttons visually pushed/unpushed correctly but never executed their usecode callback. The release hit-test (`on_button()`) always returned MISS.

**Root Cause:** `Dynamic_button::paint()` set the frame to `clicked_frame`, then delegated to `Gump_button::paint()` which applied its own +1 adjustment. The double-adjustment left the frame stuck at `clicked_frame` outside of painting. On mouse release, `on_widget()` tested hit against the wrong frame's shape origin → MISS → `activate()` never reached.

**Fix:** `paint()` now paints directly, bypassing `Gump_button::paint()`, with proper save/restore of the frame number. See [Paint and Frame Management](#paint-and-frame-management).

---

## Design Notes

### Why Direct Paint Instead of Delegating

`Gump_button::paint()` adjusts the frame by +1 when pushed, which conflicts with `Dynamic_button`'s explicit frame selection (idle→frame 0, clicked→frame 1). The double-adjustment corrupted the resting frame, breaking `on_widget()` hit-tests. Direct painting with save/restore avoids this entirely.

### Why usecode_fn Instead of usecode_param

Originally, `usecode_param` was intended to pass a parameter to the callback to distinguish buttons — multiple buttons could share one `usecode_fn` and encode their identity via `usecode_param` (passed as the event). This was changed because the Exult usecode engine always fires callbacks with the `double_click` event, and there is no reliable mechanism to pass arbitrary parameters from the button to the usecode function. Each button needing distinct behaviour must now use a unique `usecode_fn` (shape number) to route to a distinct usecode function.

**Migration:** If you previously had multiple buttons sharing one `usecode_fn` and using `usecode_param` to differentiate, split them into separate usecode functions with unique shape numbers. For example, 10 quantity +/− buttons that shared one function were split into 10 per-row functions (shapes 1206–1215).

### No Save Persistence

Button toggle state (`toggled`) is transient — it resets when the gump is closed and reopened. By design, persistent game state belongs in usecode (e.g., global flags), not in UI widgets. If you need a toggle to persist across gump opens, read and apply the state from a global flag in your on-open handler.

### Per-Instance State

Two gumps with the same shape number have independent button state. Toggle state, visibility, and press state are stored per-widget-instance on the `Dynamic_button` object, not on the shared `Gump_info` configuration singleton.

### Frame Origin Compensation

When rendering the clicked frame, `paint()` checks whether the two frames have different shape origins (`xleft`, `yabove`). If they differ, it adjusts the paint position by the delta so both frames render at the same visual screen location:

```
sx += f1->get_xleft()  - f0->get_xleft();
sy += f1->get_yabove() - f0->get_yabove();
```

This means frame 0 and frame 1 do **not** need matching origins in Exult Studio — the code handles the mismatch automatically. However, `default_frame` (frame 0) must have its origin set correctly because it is the frame used for `on_widget()` hit-testing at all times.

### Missing Clicked Frame

If `clicked_frame` refers to a frame that does not exist in the shape (e.g., the shape only has frame 0), the behaviour is:

- `get_shape()` returns `nullptr` for the missing frame
- The `if (f0 && f1)` guard in `paint()` skips origin compensation — no crash
- `paint_shape()` is still called with the invalid frame set, which renders nothing (blank)
- The frame is immediately restored to `default_frame`, so hit-testing is unaffected

**Result:** The button appears to briefly vanish when clicked (no visible pressed state), then reappears on release. The usecode callback still fires normally via `activate()`. If you want a visible pressed state, ensure `clicked_frame` exists in the button's shape file.
