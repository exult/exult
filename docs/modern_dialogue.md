# Themed Conversations (Modern Dialogue) — Implementation Summary
## Overview
This document describes the themed conversation screen, built on the themed menu toolkit described in [modern_ui.md](modern_ui.md). When a themed style is selected, conversations are shown in a panel at the bottom of the screen: portrait cards, the speaker's name, paged text with a continue cue, answer buttons and a scroll-back history of the conversation so far.

**Design Philosophy:** Usecode scripts do not change. `Conversation` (usecode/conversation.cc) keeps all conversation state exactly as before; only the display and the two waits (click to continue, choose an answer) branch. In the classic style the themed code is never created and the classic code runs unchanged.

---
## Table of Contents
- [Selecting the Style](#selecting-the-style)
- [Configuration Keys](#configuration-keys)
- [Layout](#layout)
- [Input](#input)
- [How Scripts Stay Unchanged](#how-scripts-stay-unchanged)
- [Paging](#paging)
- [Speaker Names](#speaker-names)
- [Two Faces, Faceless and Large Faces](#two-faces-faceless-and-large-faces)
- [Scroll-back History](#scroll-back-history)
- [Test Driver](#test-driver)
- [Files](#files)
- [Known Limits](#known-limits)

---
## Selecting the Style
`config/gameplay/dialogue_style` selects the conversation style:

| Value | Effect |
|---|---|
| `follow` (default, also when the key is missing) | Use `config/gameplay/menu_style`, so the default stays classic and one switch changes menus and conversations together. |
| `classic` | The classic faces and text, exactly as before. |
| `ultima` | Parchment panel in a stone frame, gold accents, serif type. |
| `modern` | Dark flat panel, rounded corners, blue accent, sans type. |
| any other name | A theme file of that name (the same lookup as `menu_style`). |

So players can keep themed menus with classic conversations, or the other way round. The style is resolved once per conversation session (at the first face, line or answer list) and kept until the outermost usecode call returns, so a change applies from the next conversation. Reading the keys never creates them in exult.cfg. The Exult launcher menu always uses the classic style, and a theme that fails to load falls back to classic.

The keys are listed in Options → Advanced (and its search), like every other exult.cfg key.

---
## Configuration Keys
| Key | Values | Default |
|---|---|---|
| `config/gameplay/dialogue_style` | `follow`, `classic`, `ultima`, `modern`, or a theme name | `follow` |
| `config/gameplay/dialogue_text_size` | `small`, `normal`, `large` (body text × 0.85, 1.0, 1.3) | `normal` |
| `config/gameplay/dialogue_auto_advance` | `off`, `slow`, `normal`, `fast` | `off` |

All three are read when a conversation starts. Auto-advance turns text pages by itself after max(1.5 s, characters × 60, 40 or 25 ms). Answer choices always wait for the player, and the reading time starts again when the history is closed. Text size scales only the conversation text and the history text, not the headings, answers or footer.

---
## Layout
`Mui_dialogue` (gumps/Mui_dialogue.cc) is not a gump and not a `Mui_window`. It is a view object owned by `Conversation` and paints into two full-display `UiLayerMenus` layers (unscaled, day palette): `"mui dlg"` (panel, text, buttons) at z 0 and `"mui dlg faces"` (portraits) at z 1. Both lie below every gump layer, so gumps opened in the middle of a conversation (number prompts, Yes/No, shops) stack above it, and `close_all_gumps()` cannot close it.

Sizes are reference pixels at 720 lines, multiplied by the conversation scale, min(height / 720, width / 1280) clamped to 1–4. This is the menus' former formula: the menus' `Mui_theme::ui_scale()` (1 at 960 lines) and their Menu size setting (`config/gameplay/menu_scale`) do not apply to conversations:
- A bottom-centred panel, width min(1180, display − 48), margin 24, framed with the theme's frame look (stone or flat).
- Slot 0's portrait card on the left (136 × 136), slot 1's on the right when it is used. Faces are drawn at the largest integer scale that fits; translucent face pixels keep their alpha (the same translucency table as the classic conversation layer).
- The speaker's name above the text in the heading font; the active slot's card has an accent frame.
- Text in the body font, wrapped to the text column; the continue cue (a down arrow and "Continue") at the bottom right.
- A footer hint line in the small font and `text_dim`.
- No scrim: the world stays visible above the panel.

Answers appear below the text while the script waits for a choice: one column for up to 4 answers, two for up to 10, three above that. Each button shows its classic key (`1`–`9`, then `a`–`z`). `bye`, `goodbye`, `good-bye` and `farewell` are styled as secondary buttons. The answer area grows the panel upwards up to 60 % of the display and scrolls beyond that.

---
## Input
| Wait | Input |
|---|---|
| Continue (and between pages) | Left or right click, Space, Enter, keypad Enter, Escape, or any printable key that is not a history key. |
| Answer | `1`–`9`, `a`–`z` (the classic key map); click; arrows move the focus (answers run down the columns), Tab cycles, Enter or Space picks the focused answer; mouse hover highlights; the wheel over a long list scrolls it. Escape or right click answers "bye" when there is one (classic behaviour). |
| History | Wheel up over the panel, PgUp, Home, H, or Up (not while choosing) open it. See [Scroll-back History](#scroll-back-history). |

The wait loop (`Mui_dialogue::wait_input`) follows the static `Get_click` in exult.cc: it keeps game ticks, palette rotation and the time queue pause, handles focus events and the Ctrl+Alt+S screenshot, and calls `gwin->show()` every frame.

---
## How Scripts Stay Unchanged
- `Conversation` keeps faces, answers, the answer stack and `text_pending` exactly as before. Every method keeps its classic code; the themed branch is an added `if (view) { ...; return; }` after the shared state updates, and pushes copies of the state to the view (`set_faces`, `set_answers`, `say`, `show_choices`). `Mui_dialogue` never includes conversation.h.
- `Usecode_internal::click_to_continue()` and `get_user_choice_num()` call `Conversation::themed_continue()` / `themed_choose()` when `Conversation::is_themed()`; their classic code is unchanged.
- Sessions are counted with `Conversation::enter_usecode()` / `leave_usecode()` around `run()` in `Usecode_internal::call_usecode`. `init_conversation()` in the middle of a scene (for example from `show_npc_face` closing gumps) does not end the session or clear the history.
- When the resolved style is classic, the view is null and none of the new code runs. This is checked by comparing the classic conversation layers byte for byte with the build before the change (see [Test Driver](#test-driver)).

---
## Paging
`say_string` still splits a text at `~`/`~~` and a leading `*`. The view pages each part itself with the rules of `Shape_manager::paint_text_box`:
- `\n` breaks the line, `\r` is dropped, `^x` upper-cases x.
- `*` in the middle of a text starts a new page, unless it falls on the page's first line (then it is dropped).
- `@` is shown as a double quote, as the game fonts draw it (`"@Yes?@"`).
- Words wrap with `Mui_text_font::wrap`; pages hold whole lines (at least 3; 5 at 1920 × 1080 with the default text size).

Pages before the last wait with the continue cue. The last page stays and `text_pending` is set as in classic, so the next `click_to_continue` shows the cue. The cue does not blink.

---
## Speaker Names
`Usecode_internal::show_npc_face` passes the speaking NPC to `Conversation::show_face` (the NPC from `get_face_shape`, or the main actor for 356/−356/0), stored in `Npc_face_info::npc_num`. The label is the main actor's name for the Avatar, otherwise `Actor::get_name()`: the NPC's name once met, else its shape description, the same text Exult shows when the NPC is clicked (Black Gate sets "met" before the face shows, so Black Gate shows names). Faces given only by shape have no label. The label belongs to the face that said the shown text.

---
## Two Faces, Faceless and Large Faces
- Two faces: slot 0 on the left, slot 1 on the right; the active slot's card is framed in the accent colour, the other card's name is shown in `text_dim`. `UI_set_conversation_slot` switches the active card.
- Faceless (no face frame, a hidden face, or the Nightmare Smith `noface` case): no card; the text uses the full width; the name stays when the speaker is known.
- Large faces (frame width ≥ 119: the Guardian, the serpents) are drawn centred above the panel at an integer scale, and their text uses the `danger` colour (classic uses the red font).

---
## Scroll-back History
The view keeps one entry per said part ({speaker label, text without `*`/`^`}) and one per chosen answer (labelled with the Avatar's name, shown as `> text`). It is cleared when the session ends.

While open, the panel grows to 70 % of the display with the header "Conversation so far"; consecutive lines of one speaker share a label. Keys: Escape or H close; End goes to the bottom, then closes; Home top; PgUp/PgDn page; Up/Down line; the wheel scrolls 3 lines; the scrollbar thumb can be dragged and a click on the track pages; any other click, or the close button, closes it. Input that would continue or answer is ignored while it is open, and the portraits are hidden. In choice mode, H picks an answer instead when 17 or more answers make `h` an answer key.

---
## Test Driver
The menu test driver (`Mui_selftest`, inert unless `EXULT_MUI_TEST=<script.mui>`) has conversation commands in gumps/Mui_dialogue_test.cc. Commands starting with `conv-` or `expect-conv-` belong to it.

| Command | Purpose |
|---|---|
| `talk NAME\|NUM` | Start the NPC's conversation with the event its Talk schedule sends (double click in Black Gate, `chat` in Serpent Isle). Refused while a conversation has faces. |
| `call-usecode FN [NPC]` | Call a usecode function on the NPC (default the main actor). |
| `conv-log` | Print the conversation state (style, wait, faces, page, page log, answers, history). |
| `conv-dump PATH` | Write the classic `conv` and `convbg` layer buffers ("EXCV", then width, height and the index bytes of each). |
| `conv-choose-key TEXT` | Press the classic key of the answer TEXT. |
| `conv-click PART` | Click `answer.N`, `continue`, `text`, `face.0` or `face.1` (themed). |
| `conv-wheel N` | Wheel over the panel (themed). |
| `expect-conv-style S`, `expect-conv-wait continue\|page\|choice\|none`, `expect-conv-faces N`, `expect-conv-face SLOT shown\|faceless\|none`, `expect-conv-page SUBSTR`, `expect-conv-answers A\|B\|C` (`-` = none), `expect-conv-chosen TEXT` | Checks that also work in classic. |
| `expect-conv-speaker NAME\|@avatar\|@npc:N`, `expect-conv-page-log I\|SUBSTR`, `expect-conv-pages-atleast N` | Themed text checks. |
| `expect-conv-history-count N`, `expect-conv-history-atleast N`, `expect-conv-history I\|SPEAKER\|SUBSTR`, `expect-conv-history-open yes\|no\|top\|bottom\|middle\|scrolled` | History checks (SPEAKER `-` = no label, `*` = any). |

Classic conversations wait inside the static `Get_click` in exult.cc. A new inert hook, `Get_click_frame_hook(Paintable*)`, is called once per frame of that loop; the driver sets it and steps only when the paintable is the `Conversation`, so no other `Get_click` user changes behaviour. The themed wait loop steps the driver every frame itself.

---
## Files
| File | Contents |
|---|---|
| `gumps/Mui_dialogue.h`, `gumps/Mui_dialogue.cc` | The themed view: style resolution, layers, layout, paging, waits, answers, history, text size, auto-advance. |
| `gumps/Mui_dialogue_test.h`, `gumps/Mui_dialogue_test.cc` | Test driver commands. |
| `usecode/conversation.h`, `usecode/conversation.cc` | Session handling, the themed branches, `describe()`, `dump_layers()`, `note_choice()`. |
| `usecode/ucinternal.cc` | Themed branches of `click_to_continue` and `get_user_choice_num`, the speaker in `show_npc_face`, the session count in `call_usecode`. |
| `usecode/ucmachine.h` | `Usecode_machine::get_conversation()`. |
| `exult.h`, `exult.cc` | `Get_click_frame_hook`. |
| `gumps/Mui_selftest.h`, `gumps/Mui_selftest.cc` | The hook, helper statics for the conversation commands. |
| `data/exultmsg.txt` | Strings 0xDC0–0xDC5. |
| `data/mui_config_keys.txt` | The three keys for Options → Advanced. |

---
## Known Limits
- No typewriter effect (it would complicate paging); the continue cue does not blink.
- No Avatar portrait beside the answers; the history labels chosen answers with the Avatar's name instead.
- Unmet NPCs in Serpent Isle are labelled with their shape description in lower case (for example "noble"), as Exult's click text shows them.
- No new rows on the native Options tabs; the keys are set in Options → Advanced.
- Gumps opened by a script during a conversation (shops, number prompts) are the classic or themed gumps they always were; only the conversation itself is themed.
