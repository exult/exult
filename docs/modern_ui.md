# Themed Menus (Modern UI) — Implementation Summary
## Overview
This document describes the themed menu system. It adds a small widget toolkit, two built-in themes ("Ultima" and "Modern") and five themed screens: the game menu, the Options window (six classic tabs plus an Advanced exult.cfg editor), the Journey Log (load and save), and the cheat window.

The themed conversation screen, which uses this toolkit, is described in [modern_dialogue.md](modern_dialogue.md).

**Design Philosophy:** The classic gumps stay the default and are untouched in classic style. Every themed screen is an ordinary `Modal_gump`, so pausing, quitting, Escape and the modal loop behave as before. Where a classic screen already holds the behaviour (option gumps, Newfile_gump's save model, the text-mode cheat menus), the themed screen reuses it instead of copying it.

---
## Table of Contents
1. [Selecting the Style](#selecting-the-style)
2. [Architecture](#architecture)
   - Mui_window and the Modal Loop
   - The UiLayerMenus Layer and the Scrim
   - Input Mapping
   - Z Hints and Nested Classic Gumps
   - Router
3. [Toolkit](#toolkit)
   - Widgets
   - Keyboard Model
   - Window Services
4. [Themes](#themes)
   - File Format
   - Adding a Theme from a Mod
5. [Fonts](#fonts)
6. [Screens](#screens)
7. [Configuration Keys](#configuration-keys)
8. [Config Metadata Table](#config-metadata-table)
9. [Save Format: Play Time and Rename](#save-format-play-time-and-rename)
10. [Self-Test Driver](#self-test-driver)
11. [Files](#files)
12. [Known Limits](#known-limits)

---
## Selecting the Style
`config/gameplay/menu_style` selects the style:

| Value | Effect |
|---|---|
| `classic` (default, also when the key is missing) | The classic Gamemenu_gump, Newfile_gump, option gumps and text-mode cheat screen, exactly as before. |
| `ultima` | The look of Exult's original menu gumps (black outline, bevelled brown body, the green checkmark tab) with the themed layout and controls, gold accents, serif type, tabs on top. |
| `modern` | Dark flat panels, rounded corners, blue accent, sans type. |
| any other name | A theme file of that name (see [Adding a Theme from a Mod](#adding-a-theme-from-a-mod)). |

Ways to change it:
- Menu style on the UI page of Options, with a description of each style. Apply writes the key; every open themed window follows (the game menu when the nested window closes). Choosing Classic closes the themed windows and the game menu goes on as the classic Gamemenu_gump.
- The classic game menu (Gamemenu_gump) has one extra line, "Menu style: Classic" (also Ultima, Modern), outside the Exult launcher menu. Each press cycles classic → ultima → modern → classic and writes the key, so Classic is never a dead end. It needs no restart: the next game menu, Options, Journey Log or cheat window that opens follows the setting. The rest of the classic menu code is unchanged.
- The new key action `MENU_STYLE` ("Cycle menu style", unbound by default) cycles classic → ultima → modern → classic.
- Options → Advanced → `config/gameplay/menu_style`.

The Exult launcher menu (no game loaded) always stays classic.

---
## Architecture
### Mui_window and the Modal Loop
`Mui_window` (gumps/Mui_window.h) derives from `Modal_gump` and is run with `gumpman->do_modal_gump(win, Mouse::hand)`. A screen subclasses it and overrides:

| Hook | Purpose |
|---|---|
| `build(Mui_box& body)` | Create the widgets of the current tab. |
| `tab_labels()`, `tab_keys()`, `on_tab(i)` | Tabs (top or left sidebar, as the theme says). |
| `build_title(Mui_box& bar)`, `title_bar_width()` | Extra widgets in the title bar (search fields). |
| `build_footer(Mui_box&)` | Footer buttons. |
| `on_cancel()`, `on_apply()` | Escape / Apply. |
| `preferred_size(w, h)` | Size in reference pixels (h = 0 fits the content). |
| `adjust_theme(Mui_theme&)` | Tweak a private copy of the theme (the cheat window uses its red accent). |
| `default_help()` | Text for the help line when nothing is hovered or focused. |

The window owns a tree of `Mui_widget`s. Building and layout happen in reference pixels, scaled by `Mui_theme::ui_scale(display_w, display_h, percent) = clamp(min(h / 960, w / 1440), 1, 4) × percent / 100`, where percent is `config/gameplay/menu_scale` (Menu size on the UI page, 50–200, default 100). The shorter side sets the size, so the 1120×660 Options window takes about 69% of the height from 960 px up and the same size on an ultrawide as on a 16:9 display of that height; the game area does not change it. The width only limits narrow displays. A changed percent rebuilds open windows on their next frame. Rebuilds are deferred to a safe point; callbacks that rebuild, close overlays, open nested modals or delete widgets go through `post(fn)`.

### The UiLayerMenus Layer and the Scrim
Themed windows return false from `uses_render_layer()` and paint into their own 8-bit layer instead of the game buffer:
- The new layer kind `UiLayerMenus` (imagewin/imagewin.h) is protected, unscaled (`NoScaler`, `Fill`, point) and uses the day palette (`UiPaletteDay`), so text is drawn 1:1 with display pixels and no palette cycling or day/night palette change reaches the menus.
- The window creates a full-display layer named "mui" and a 1×1 black "scrim" layer under it, stretched to the display with `layer_set_alpha(backdrop_alpha)` to dim the game.
- `Image_window::screenshot_display(dst, w, h)` reads the renderer back without the 4-px guard-band crop of `Image_window::screenshot`; the self-test driver uses it for full-size PNGs.

### Input Mapping
Two new `Gump` virtuals, `wants_display_coords()` (false by default) and `display_to_local(sx, sy, lx, ly)`, let `Gump_manager::handle_modal_gump_event` hand mouse, finger and wheel coordinates to a themed window in layer pixels instead of game pixels. Every other gump keeps exactly the old path. Hover is polled in `Mui_window::run()`. Themed windows set `no_dragging`.

### Z Hints and Nested Classic Gumps
`Gump_manager::render_gumps_to_layer` calls the new `Gump::set_layer_z_hint(z)` for gumps that paint themselves, with `gump_layer_z_base + tier + index` (base 1<<18, text +1<<16, modal +1<<17, mouse 1<<20). A themed window places its layers from that hint, so a classic modal gump opened from it (Mixer_gump, the MIDI AdvancedOptions_gump, a text-mode cheat menu) stacks above it. Dropdown lists, confirmations and text prompts of the themed screens are overlays inside the window, not new modal loops.

### Themed Questions
While any themed window is open, `Yesno_gump::themed_ask` and `Gump_manager::themed_number_hook` are set, so `Yesno_gump::ask`, `Countdown_gump::ask` (the Video "Settings applied. Keep?" countdown and others) and `Gump_manager::prompt_for_number` show a `Mui_dialog_window` in the current style instead of the classic gump: a small themed modal window (title, text or number field, No/Yes or Cancel/OK, no help line) run by `do_modal_gump` above the window that asked. Answers and timing are the classic ones: Yes, Enter or Y answers yes; No, Escape, N, the close button or a right-click outside answers no; the countdown shows the whole seconds left and answers no when the time runs out, so Video settings roll back as before. Ids: `dialog.text`, `dialog.yes`, `dialog.no`, `dialog.number`, `dialog.ok`, `dialog.cancel`.

### Router
`gumps/Mui_router.h` (namespace `Mui`) is the only switch between classic and themed screens:

| Function | Called from |
|---|---|
| `menu_style()`, `themed()` | Everywhere below. |
| `open_game_menu()` | `ActionMenuGump` (keyactions.cc). |
| `open_journey_log(bool* restored)` | `ActionFileGump`, the themed game menu. |
| `open_options(tab)` | The themed game menu. |
| `open_cheats()` | `Cheat::cheat_screen()` (cheat.cc), the themed game menu. |
| `cycle_menu_style()` | `ActionMenuStyle`. |

Each returns false in classic style and the caller then runs its old code unchanged.

---
## Toolkit
### Widgets
All in gumps/Mui_widgets.h. Widgets hold theme roles, never colours.

| Widget | Notes |
|---|---|
| `Mui_box` | Vertical or horizontal container: gap, padding, optional panel, centring; `add<T>(...)`. |
| `Mui_label`, `Mui_section_header`, `Mui_help_line`, `Mui_spacer` | Text and spacing. |
| `Mui_badge` | "Applies now", "Needs restart", "Changed from default". |
| `Mui_button` | primary, secondary, destructive, flat; optional icon. |
| `Mui_toggle` | On/off switch. |
| `Mui_segmented` | Two or more exclusive choices; parts `<id>.<key>`. |
| `Mui_dropdown` | Popup overlay list with type-ahead. |
| `Mui_slider` | Value slider with a format function. |
| `Mui_number_field` | Integer field with range and arrow keys. |
| `Mui_text_field` | Text entry (optional search icon, `on_change`, `on_submit`). |
| `Mui_list` | Selectable rows. |
| `Mui_grid` | Card grid with a `paint_card` callback; parts `<id>.<n>`. |
| `Mui_tabs` | Tab strip (top) or sidebar (left). |
| `Mui_scroll_view` | Vertical scrolling with the wheel and keyboard. |

`Mui_painter` (gumps/Mui_painter.h) draws for them: fills, frames, rounded rectangles, bevels, the parchment and stone textures, dividers, icons, focus rings, shapes (`blit_shape`), scaled pixels (thumbnails) and text.

### Keyboard Model
- Tab / Shift+Tab move focus through the focusable widgets in reading order; arrow keys move within lists, grids, sliders, segmented controls and tabs, and between controls in a column.
- Enter or Space activates the focused widget; Escape closes an open overlay, then the window (`on_cancel`).
- Typing goes to a focused text field; dropdowns jump by type-ahead.
- Screens add their own keys (Journey Log: Delete, F2 rename, `/` search, Enter load).

### Window Services
`rebuild([focus_id])`, `relayout()`, `post(fn)`, `finish()`, `switch_theme(name)`, `show_confirm(msg, destructive, cb)`, `show_prompt(msg, initial, cb)`, `open_popup()`/`close_overlay()`, `find(id)`, `widget_rect(id-or-part, rect)`, `set_focus()`/`get_focus()`, `force_paint()` and `run_hidden(fn)` (hides the layers while a screen with its own event loop runs, such as the shape browser). Chrome ids: `mui.close`, `mui.tabs` (parts `mui.tabs.<key>`), `mui.body`, `mui.footer`, `mui.help`, dialog ids `mui.dialog.ok`, `mui.dialog.cancel`, `mui.dialog.field`.

---
## Themes
### File Format
A theme is XML in exult.cfg syntax, read with `Configuration::read_config_string`. Comments must not contain `<`. Missing keys keep the built-in Modern values. Metrics are in pixels at a 720-px display height.

| Key | Values |
|---|---|
| `meta/name` | Theme name (matches the file name). |
| `meta/label` | Label text, or `0xNNN` for an exultmsg.txt string. |
| `layout/tabs` | `top` or `left`. |
| `layout/sidebar_width` | Sidebar width when tabs are on the left. |
| `metrics/unit`, `gap`, `control_h`, `row_h`, `tab_h`, `radius`, `border`, `title_h`, `footer_h` | Sizes. |
| `colours/<role>` | `#RRGGBB` for each role: `backdrop`, `window`, `panel`, `panel_alt`, `frame`, `frame_light`, `frame_dark`, `text`, `text_dim`, `text_on_accent`, `text_on_danger`, `title_text`, `accent`, `accent_hover`, `focus`, `danger`, `warning`, `success`, `badge_now`, `badge_restart`, `badge_changed`, `cheat_accent`. |
| `colours/backdrop_alpha` | Scrim opacity, 0–255. |
| `fonts/<role>/face`, `fonts/<role>/px` | Roles `title`, `heading`, `body`, `small`, `mono`; faces `sans`, `sans_bold`, `serif`, `serif_bold`; px 8–96 at the reference height. |
| `frame/style` | `stone`, `gump` (Exult's original menu gumps: black outline, bevelled body and the checkmark tab from exult.flx and gumps.vga, scaled by a whole number; the tab closes the window like the close button) or `flat`. |
| `panel/texture` | `parchment` or `none`. |

Colours resolve to the nearest **day palette** entry (indices 0–223, so no cycling colours) through `Palette::find_color`. The palette has few blue-greys, so Modern's greys and blue come out slightly different from their hex values; with `EXULT_MUI_TEST` set, `Mui_theme` logs each role's mapping once.

### Adding a Theme from a Mod
Put `<name>.thm` into `mui_themes/` in the mod's patch folder (`<PATCH>/mui_themes/<name>.thm`) or in the data folder (`<DATA>/mui_themes/<name>.thm`) and set `config/gameplay/menu_style` to `<name>`. Lookup order is `<PATCH>`, `<DATA>`, then the built-ins in exult.flx, so a file named `ultima.thm` or `modern.thm` overrides a built-in theme. `Mui_theme::list()` returns the built-in and the found names.

---
## Fonts
The themed menus use anti-aliased bitmap fonts rendered offline, not a runtime TrueType renderer.

- `tools/mui/gen_mui_fonts.py` (Python 3 with Pillow) renders DejaVu Sans, Sans Bold, Serif and Serif Bold at 13, 16, 20, 24, 30, 36 and 48 px into Exult font shapes; font index = face × 7 + size index. Each pixel holds a coverage level 1–4.
- The 28 fonts are one Flex, `data/mui_fonts.vga`, committed with the generator and `data/fonts/DejaVu-LICENSE.txt` (Bitstream Vera licence), and embedded in exult.flx as `EXULT_FLX_MUI_FONTS_VGA`.
- Byte codes follow data/fonts/font_map.txt: ASCII, the builtin special-character frames, and ISO-8859-1 160–255, so translated exultmsg strings work.
- `Mui_fonts::get().pick(face, px)` returns the size nearest to the theme size × UI scale; `Mui_text_font::draw` remaps levels 1–4 to a ramp from the background to the text colour.
- Regenerate: `gen_mui_fonts.py --ttf DIR -o data/mui_fonts.vga`; check: `gen_mui_fonts.py --verify data/mui_fonts.vga`.

Why not FreeType now: Exult itself links neither FreeType nor SDL_ttf (FreeType is only an optional Exult Studio dependency), and a runtime dependency would add work for the iOS, Android and MSVC builds while the TrueType discussion (RFC #561) is open. The glyph source sits behind `Mui_font`, so a FreeType backend could fill the same cache later.

---
## Screens
| Screen | Class | Notes |
|---|---|---|
| Game menu | `Mui_game_menu` | Return, Journey Log, Options, Cheats (when enabled), Quit (classic Yesno). |
| Options | `Mui_options_window` | Tabs Video, Audio, Engine, Display, Input, UI, Advanced. Cross-tab search in the title bar; help line; badges; Reset page, Cancel, Apply. |
| Advanced | `Mui_config_editor` (inside Options) | exult.cfg tree with counts, filter, full paths, typed controls, Applies now / Needs restart, Changed from default, Reset to default, Not set rows, "Other" for unknown keys. |
| Journey Log | `Mui_journey_window` | Cards or list, thumbnails, search, sort, details panel (saved at, in-game time, play time, party, game, mod, running build), New save, Save here, Load, Rename, Delete. |
| Cheats | `Mui_cheat_window` | Red accent, standing warning, sidebar Player, World & time, NPCs, Teleport, Global flags, Usecode, Map editor; opens the text-mode menus for the deep tools and "Open classic cheat screen" for the whole RootMenu. |

**Options models.** `Mui_options_model` builds each classic option gump (VideoOptions_gump … UIOptions_gump) without showing it (each header gains only `friend class Mui_options_model;`), reads its buttons (`Gump_ToggleTextButton::get_selections()`, `getselection()`), changes values with the gump's own `toggle_*` methods into a scratch render target, and applies with the gump's own `save_settings()`. Keys and side effects are therefore those of the classic gumps. No/Yes, Off/On and Disabled/Enabled become toggles, ordered numeric choices sliders, other two-choice sets segmented controls, the rest dropdowns. Game Area is always a dropdown and offers every size the classic gump knows on any platform (Auto, 320×200, 400×250, 480×300 and a custom size from exult.cfg), ordered by size. The UI page also has two rows with no classic button (`Row::native_key`): Menu style and Menu size, written to exult.cfg on Apply.

**Cheat screen bridge.** `CheatScreen::run_menu_screen(Which, Actor*, unsigned flag)` runs one text-mode submenu (Root, NPC, Usecode, Teleport, GlobalFlag) with `show_screen()`'s setup and teardown, now split into `begin_screen()`/`end_screen()`. `CheatScreen::toggle_paperdolls()` is shared by both screens.

---
## Configuration Keys
| Key | Values | Default |
|---|---|---|
| `config/gameplay/menu_style` | `classic`, `ultima`, `modern`, or a theme name | `classic` |
| `config/gameplay/menu_scale` | 50–200 (percent; the UI page offers 75, 90, 100, 110, 125, 150) | `100` |
| `config/gameplay/menu_journey_view` | `grid`, `list` | `grid` |
| `config/gameplay/menu_journey_sort` | `newest`, `oldest`, `name`, `day` | `newest` |
| `config/gameplay/dialogue_style` | `follow`, `classic`, `ultima`, `modern`, or a theme name | `follow` |
| `config/gameplay/dialogue_text_size` | `small`, `normal`, `large` | `normal` |
| `config/gameplay/dialogue_auto_advance` | `off`, `slow`, `normal`, `fast` | `off` |

---
## Config Metadata Table
`data/mui_config_keys.txt` (embedded as `EXULT_FLX_MUI_CONFIG_KEYS_TXT`) describes every exult.cfg key for the Advanced tab. One row per key:

```
path|type|default|range-or-choices|apply|description
```

- `path`: full key path; `*` stands for one dynamic segment or the end of one (`config/disk/game/*/path`, `config/audio/midi/volume_*`). An exact path beats a pattern; the longer pattern wins.
- `type`: `bool` (yes/no), `enum`, `int`, `float`, `path`, `string`.
- `default`: the value the code uses when the key is missing (empty when it depends on the platform or another key).
- `range-or-choices`: `enum` comma-separated choices; `int`/`float` `min..max`.
- `apply`: `now` (the Advanced tab applies it at once) or `restart`.
- `description`: one line; "Old …" marks a legacy key converted at startup, listed only when present.

Workflow with `tools/mui/scan_config_keys.py` (Python 3, standard library only):

```
python3 tools/mui/scan_config_keys.py --list                         # keys found in the code, with origins
python3 tools/mui/scan_config_keys.py --update data/mui_config_keys.txt   # add new keys with TODO descriptions
python3 tools/mui/scan_config_keys.py --check                        # fail on missing or TODO rows
python3 -m unittest discover -s tools/mui -p 'test_*.py'
```

The scanner reads `config->value/set/key_exists/remove` calls in `*.cc`, `*.h`, `*.cpp` and `*.mm` (except mapedit/, tools/, msvcstuff/ and two config tools): literal keys, concatenations (as `*` patterns), ternaries and simple local variables. Descriptions are written by hand. Keys the code builds at run time (for example the per-layer `config/video/ui/*` keys) are hand-written rows.

The editor keeps edits pending until Apply, validates them all, then calls `config->set` for each changed key and one `write_back`, so untouched keys are left as they were.

---
## Save Format: Play Time and Rename
`SaveGame_Details` (gumps/Newfile_gump.h) now reads `char reserved0; uint32 play_seconds; char reserved1[44];` (still 64 bytes). `play_seconds` is written at file offset 16 of GSAVEINFO; old saves read 0 and show a dash, older Exults ignore the field. `Game_window::get_play_seconds()` counts from the value restored with the game, including time spent in menus.

`Game_window::rename_savegame(num, name)` (gamedat.cc) rewrites the 80-byte save title (the Flex header title, or the title written before a zip archive) and, for zip saves, the archive's global comment at the end of the file. It writes `<file>.rename` and renames it over the original, so a failure leaves the save untouched.

---
## Self-Test Driver
`Mui_selftest` (gumps/Mui_selftest.h) is inert unless `EXULT_MUI_TEST=<script>` is set (`EXULT_MUI_TEST_TIMEOUT` = watchdog seconds). It is polled from `Handle_events` in exult.cc, from `Mui_window::run`, and for classic modal gumps through `Gump_manager::modal_frame_hook`. It prints `MUI-TEST PASS|FAIL|info ...` lines and `MUI-TEST DONE n pass m fail`. `${VAR}` in a script expands from the environment.

| Command | Purpose |
|---|---|
| `wait MS` | Let the game run. |
| `style NAME\|none`, `display W H` | Change the style key or the display size. |
| `open menu\|journey\|options [tab]\|cheats\|classic-menu\|classic-file\|classic-cheats` | Open a screen. |
| `click ID-or-part`, `rclick X Y`, `key [shift+\|ctrl+\|alt+]NAME`, `type TEXT`, `select ID INDEX`, `tab KEY` | Input. |
| `shot PATH` | Full-display PNG. |
| `close`, `quit`, `escape-classic` | Close the window, quit, queue Escape for the next text-mode cheat menu. |
| `expect-config KEY VALUE\|<absent>`, `expect-widget ID VALUE`, `expect-widget-atleast ID N`, `expect-focus ID`, `expect-open CLASS`, `expect-window none\|open` | Checks. |
| `expect-cheat NAME on\|off`, `expect-time D H M`, `expect-flag N on\|off` | Cheat checks. |
| `expect-style NAME`, `expect-scale N` (UI scale × 100), `expect-frame [W H]` (window size; logs it), `expect-no-widget ID`, `expect-game-area W H` | Window and display checks. |
| `ask TEXT`, `expect-answer yes\|no`, `prompt-number MIN MAX STEP DEFAULT`, `expect-number N [escaped]` | Ask a Yes/No or a number from the window on top (themed dialogs). |
| `gclick X Y [right]`, `expect-gump CLASS\|none` | Click a point of the classic modal gump on top (relative to its origin, through its layer); check which classic gump is on top. |
| `save-game NUM NAME`, `save-compression N`, `play-time SECS`, `restore-game NUM`, `expect-saves N`, `expect-visible N`, `expect-card-name I NAME`, `select-card NAME`, `expect-save-name NUM NAME`, `expect-save-play-time NUM none\|MIN`, `expect-restored yes\|no` | Journey Log checks. |

Run it headless only: `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy EXULT_MUI_TEST=script.mui exult -c test.cfg --game ... --nomenu`. Use game area 320×200 with fill mode Fit in the test configuration; under the dummy driver an Auto game area renders a tiled world.

---
## Files
| File | Role |
|---|---|
| gumps/Mui_theme.{h,cc} | Theme loading, colour roles, palette mapping, UI scale. |
| gumps/Mui_font.{h,cc} | Font loading, measuring, eliding, wrapping, drawing. |
| gumps/Mui_painter.{h,cc} | Drawing primitives. |
| gumps/Mui_widgets.{h,cc} | Widget toolkit. |
| gumps/Mui_window.{h,cc} | Window shell, layers, input, focus, overlays. |
| gumps/Mui_router.{h,cc} | Classic or themed. |
| gumps/Mui_game_menu.{h,cc} | Game menu. |
| gumps/Mui_options_model.{h,cc}, gumps/Mui_options.{h,cc} | Options window. |
| gumps/Mui_config_meta.{h,cc}, gumps/Mui_config_editor.{h,cc} | Advanced tab. |
| gumps/Mui_journey_log.{h,cc} | Journey Log. |
| gumps/Mui_cheats.{h,cc} | Cheat window. |
| gumps/Mui_selftest.{h,cc} | Headless test driver. |
| data/mui_fonts.vga, data/mui_ultima.thm, data/mui_modern.thm, data/mui_config_keys.txt, data/fonts/DejaVu-LICENSE.txt | Data. |
| tools/mui/gen_mui_fonts.py, tools/mui/scan_config_keys.py, tools/mui/test_scan_config_keys.py | Maintainer tools. |

User-visible strings are in data/exultmsg.txt, block 0xC00–0xDFF.

---
## Known Limits
- Classic sub-screens opened from a themed screen (the mixer, MIDI settings, text-mode cheat menus) keep their classic look; their Yes/No questions are themed.
- In the classic mixer a slider under the mouse takes every key, Escape too (classic behaviour): move the mouse off the sliders, or use Cancel.
- Colours are the nearest day-palette entries, so Modern is an approximation of its hex values.
- Play time includes time spent in menus and while paused.
- Saves do not record the Exult build that wrote them; the details panel shows the running build, labelled as such.
- Comments in exult.cfg follow `Configuration::write_back` behaviour.
- The Exult launcher menu stays classic.
