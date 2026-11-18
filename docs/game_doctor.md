# Troubleshooting (Game_doctor) — Implementation Summary

## Overview

Troubleshooting (the game doctor in the code) helps players and mod authors look at a running game and repair it: global flags by name, an NPC browser with revive and bring, an item finder, data-driven check-up rules, plain-text state dumps and a bug report export. It lives in `Game_doctor` (gamedoctor.h, gamedoctor.cc, gamedoctor_checkup.cc) with three faces: five sections of the themed cheat window, command-line options for headless runs, and provisional usecode intrinsics.

**Off by default:** nothing changes in normal play. The UI needs cheats on (`config/gameplay/cheat`), the command-line actions run only when a doctor option is given, and the intrinsics do nothing unless a script calls them. The classic menu style and the text-mode cheat screen are unchanged.

---

## Table of Contents

1. [UI Sections and the GAME_DOCTOR Key](#ui-sections-and-the-game_doctor-key)
2. [Flag Table Format](#flag-table-format)
3. [Check-up Rules](#check-up-rules)
4. [State Dump Format](#state-dump-format)
5. [Usecode Intrinsics (Provisional)](#usecode-intrinsics-provisional)
6. [Command-Line Options and Exit Codes](#command-line-options-and-exit-codes)
7. [Safety and Undo](#safety-and-undo)
8. [Bug Report](#bug-report)
9. [Testing Harness](#testing-harness)
10. [Files](#files)
11. [Known Limits](#known-limits)

---

## UI Sections and the GAME_DOCTOR Key

The themed cheat window (`Mui_cheat_window`, see [modern_ui.md](modern_ui.md)) gains five sidebar sections after Map editor, implemented in gumps/Mui_cheats_doctor.cc. Every change asks first (`show_confirm`) and goes through `Game_doctor::apply_change()` (see [Safety and Undo](#safety-and-undo)).

| Section (tab key) | Widgets | Does |
|---|---|---|
| Named flags (`doctor_flags`) | `doctor.flags.search`, `.list`, `.name`, `.desc`, `.toggle` | Search the merged flag table by number (decimal or `0x` hex), name, alias or description; rows `0xNNNN NAME  on\|off`; set or clear the selected flag. |
| NPC browser (`doctor_npcs`) | `doctor.npcs.search`, `.list`, `.details`, `.location`, `.inventory`, `.bring`, `.goto`, `.revive`, `.schedule`, `.heal` | Rows `NNN name - alive\|dead`; schedule, alignment, health and flags; where the NPC (and a dead NPC's body) is; inventory. Bring beside the Avatar, go to, revive (dead NPCs only), restore the usual schedule, heal. |
| Item finder (`doctor_items`) | `doctor.items.shape`, `.frame`, `.quality` (-1 = any), `.name`, `.search`, `.list`, `.count`, `.goto`, `.fetch` | Rows `shape:frame q quality - x,y,z map m - in A > B  name`; go to the item, or fetch it into the Avatar's pack. |
| Check-up (`doctor_checkup`) | `doctor.checkup.run`, `.summary`, `.list`, `.goto` | Runs the rules; summary `N findings` or `No problems found`; rows `severity id: message`; go to where a finding points. |
| Safety & report (`doctor_safety`) | `doctor.safety.backup`, `.history`, `.undo`, `.export`, `.report` | The session's backup slot and title, the undo history (newest first), undo the last change, export a bug report and show its path. |

`Mui::open_doctor(section)` (gumps/Mui_router.cc) opens the window on a section in **any** menu style; in classic style the window uses the Modern theme. It needs cheats on. The key action `GAME_DOCTOR` (`ActionGameDoctor`, cheat keys, **unbound by default**) calls it with `doctor_checkup`. Bind it in a key file (format of data/si/defaultkeys.txt) as any other action, e.g. `Ctrl-Shift-D	game_doctor`. The classic text-mode root menu has no free row, so it is not changed.

Strings are in data/exultmsg.txt 0xD40–0xD93 (section labels 0xD40–0xD44, key label 0xD45). Action results come from `Game_doctor`.

## Flag Table Format

The doctor uses Exult's existing `global_flags.txt` tables (data/bg/global_flags.txt, data/si/global_flags.txt, embedded as resource `files/global_flags`), in Text_msg_file format:

```
# Comments start with '#'.
0x4:BANES_RELEASED
0x5:CAN_SAIL_SHIPS
%%section descriptions
0x47:Flag when Cantra is cured.
%%endsection
```

- The global section holds `0xNUM:NAME` lines; it is all the classic cheat screen reads.
- The optional `%%section descriptions` holds `0xNUM:text` lines shown under the name and searched by the doctor.
- Load order: the game's table, then `<PATCH>/global_flags.txt` overlaid **per number**. A mod's name wins; the base name stays as a searchable alias. (The classic cheat screen still replaces the whole table with the mod's file.)
- Names are matched case-insensitively (`flag_number`, `expect-gflag`, the intrinsics).

### Generator

`tools/doctor/uc_flags_to_table.py` (Python 3, standard library) reads UCC headers: `const int NAME = N; // text` and enum members `NAME = N, // text`, decimal or hex. The comment becomes the description.

```
# A fresh table:
python3 tools/doctor/uc_flags_to_table.py -o OUT.txt header.uc [more.uc ...]
# Add missing names and descriptions to an existing table (never overwrites a name):
python3 tools/doctor/uc_flags_to_table.py --merge-into data/si/global_flags.txt content/sifixes/src/header/si/si_gflags.uc
# Keep only a number range, e.g. a mod's own flags:
python3 tools/doctor/uc_flags_to_table.py --range 1500-1999 -o <mod>/data/global_flags.txt <mod>/src/header/flags.uc
```

Example: a mod keeps its own flags in `src/header/flags.uc`, numbered from 1500 up. `--range 1500-1999` drops its non-flag constants (such as frame numbers) and its aliases of base-game flags, and the output goes to the mod's `data/global_flags.txt` (its `<PATCH>`). The shipped SI and BG tables were merged from content/sifixes (si_gflags.uc) and content/bgkeyring (bg_gflags.uc, bg_gflags2.uc). Unit tests: `python3 -m unittest discover -s tools/doctor -p 'test_*.py'`.

## Check-up Rules

Rules live in `doctor_rules.txt`: the game's (data/bg/doctor_rules.txt, data/si/doctor_rules.txt, resource `files/doctor_rules`), then `<PATCH>/doctor_rules.txt`. A mod rule with the same id replaces the base rule; `id|off` disables it. One rule per line, `#` comments and blank lines ignored:

```
id|severity|kind|when|args|message
```

| Field | Values |
|---|---|
| `id` | Unique name, shown in findings. |
| `severity` | `error`, `warning` or `info`. |
| `kind` | `item`, `npc`, `keys`, `orphans` (below). |
| `when` | `always`, or space-separated flag tests that must all hold: `+N` / `-N` (set / clear; decimal or `0x` hex) or `+NAME` / `-NAME` (a name from the flag table). |
| `args` | Space-separated `key=value`. |
| `message` | The rest of the line, shown to the player. |

| Kind | Args | Finding when |
|---|---|---|
| `item` | `shape=N` `[frame=N]` `[quality=N]` `[min=N]` (default 1) | Fewer than `min` such items exist anywhere (containers and NPC inventories included). |
| `npc` | `npc=N` | The NPC is dead or missing. |
| `keys` | `key=N doors=N,N,.. locked=N containers=N,..` `[ignore=Q,Q,..]` | For each key quality that locks a door (frame % 4 == `locked`) or a locked container but has no key of that quality anywhere or on the SI keyring. |
| `orphans` | `[ignore=N,N,..]` | An object its owner does not list, containers nested deeper than 16 or in a cycle, a top-level object outside the map, a body of an NPC who is alive (except the NPCs in `ignore`, whose bodies the game places on purpose). |

A mod example (`<PATCH>/doctor_rules.txt`):

```
# Our new quest needs the silver key once the gate opens.
silver_key|error|item|+MYMOD_GATE_OPEN -MYMOD_QUEST_DONE|shape=641 quality=200|The silver key is gone; the vault cannot be opened.
# The ferryman must live while the quest runs.
ferryman|warning|npc|+MYMOD_QUEST_STARTED|npc=310|The ferryman is dead; nobody can take you to the island.
# Our keys use a few qualities that are never meant to have a key.
locked_without_key|warning|keys|always|key=641 doors=270,376,432,433 locked=2 containers=522 ignore=0,254,255|Something is locked and no key for it exists
# Not wanted in this mod.
dupres_shield|off
```

## State Dump Format

Plain text, one fact per line, `key: value`, in a stable order so two dumps diff well. The header is `# exult-doctor-dump 1`.

```
# exult-doctor-dump 1
meta.exult: Exult version 1.13.1git
meta.game: serpentisle
meta.mod: mymod
meta.usecode_crc32: 0x390A9620
meta.usecode_size: 453569
clock: day 0 06:00
avatar: 478,2483,0 map 0
party:
flag.0x0004: 1 BANES_RELEASED
keyring:
npc.0001: alive | Dupre | 395,2483,1 map 0 | wait | neutral | - | hp 22/22 | items 12
npc.0001.items: 1004:0:0x1 231:0:0x1 490:0:0x1 644:7:50x50 ...
item.0270: 293 | 1000,1563,0 map 0; 1000,1572,0 map 0; ...
finding.orphans: error Object tree problem: ...
```

- `meta.*` (Exult version and git, game, mod, CRC32 and size of `<PATCH>/usecode`) and `clock` change between runs.
- `flag.0xNNNN: 1 NAME` for every set flag; `keyring:` the SI keyring qualities.
- `npc.NNNN: alive|dead | name | x,y,z map m | schedule | alignment | flags | hp h/s | items n`, then `npc.NNNN.items:` sorted `shape:frame:quality x quantity`.
- `item.SSSS: count | first 20 locations` for every shape a rule names.
- `finding.<id>: severity message` when the check-up is included.

`tools/doctor/doctor_dump_diff.py BEFORE AFTER [--ignore PREFIX]... [--all]` prints `+`/`-`/`~` lines and exits 1 when the dumps differ. It ignores `meta.` and `clock` by default (`--ignore` replaces that list, `--all` compares everything).

## Usecode Intrinsics (Provisional)

The slot numbers are **provisional**: they sit after the dynamic-list block other local branches use and may move before anything is merged upstream. They are marked `(Exult, provisional)` in usecode/bgintrinsics.h, siintrinsics.h and sibetaintrinsics.h. docs/exult_intrinsics.md is generated upstream, so they are documented here.

| Intrinsic | BG | SI | SI beta | Returns |
|---|---|---|---|---|
| `UI_get_flag_number(name)` | 0xef | 0xf0 | 0xef | Flag number, or -1 if the name is unknown. |
| `UI_get_flag_by_name(name)` | 0xf0 | 0xf1 | 0xf0 | The flag's value; false if unknown. |
| `UI_set_flag_by_name(name, value)` | 0xf1 | 0xf2 | 0xf1 | True if the name is known (sets directly, no undo entry). |
| `UI_doctor_find_npcs(text)` | 0xf2 | 0xf3 | 0xf2 | Array of NPCs whose name contains `text` (or whose number it is). |
| `UI_doctor_find_items(shape[, frame, quality, max])` | 0xf3 | 0xf4 | 0xf3 | Array of objects anywhere in the loaded world; -359 means any; `max` defaults to 500. |
| `UI_doctor_dump(file_name)` | 0xf4 | 0xf5 | 0xf4 | True if `<SAVEGAME>/doctor/<file_name>` was written (with findings); names must match `[A-Za-z0-9._-]+`. |
| `UI_doctor_checkup()` | 0xf5 | 0xf6 | 0xf5 | Array of `"severity id: message"` strings. |

They are not cheat-gated: they read state, set flags as `gflags[]` does, or write inside the doctor folder. `tools/compiler/ucc` from the same build knows the names.

## Command-Line Options and Exit Codes

| Option | Effect |
|---|---|
| `--load-save SLOT\|FILE` | Start from a save: digits name a slot (`exultNNsi.sav` / `bg`), anything else is a path. Implies `--nomenu`. A missing file or a save of another game exits with 2 before the game starts (no Yes/No prompt). Alone it just plays. |
| `--doctor-dump PATH` | Write a state dump (with `finding.*` lines when `--doctor-checkup` is given too). The folder must exist. |
| `--doctor-checkup` | Run the check-up and print `DOCTOR-CHECKUP: n findings` and one `DOCTOR-FINDING severity id: message` line each. |
| `--doctor-save FILE` | Write a save (title `Doctor fixture`), e.g. to make test fixtures. |
| `--doctor-delay MS` | When the actions run, counted from the first game frame (default 1500). |
| `--doctor-at-exit` | Run the actions when the game loop ends (e.g. after another tool quits the game) instead of after the delay. |
| `--doctor-timeout S` | Watchdog: after S seconds (default 120) print a line and exit with 124. |

With an action option (dump, check-up or save) and without `--doctor-at-exit`, Exult quits after the actions. Progress lines start with `DOCTOR:` on stdout; the last is `DOCTOR: done, exit status N`.

| Exit status | Meaning |
|---|---|
| 0 | Done, and the check-up (if run) found no warnings or errors. |
| 2 | A doctor action failed (bad `--load-save`, a dump or save could not be written). |
| 3 | The check-up found at least one warning or error. |
| 124 | The watchdog fired. |

## Safety and Undo

- **Backup:** before the first Troubleshooting change of a session the game is saved to the first free numbered slot, titled `Troubleshooting backup day D hh:mm`, so it shows in the Journey Log and can be restored like any save. If the backup fails, the change is refused.
- **Undo:** the last 5 changes. Flag changes and "go to" teleports store their inverse. Every other change (revive, bring, heal, restore schedule, fetch) first writes a snapshot `<SAVEGAME>/doctor/undo-N.sav`; undo restores it. A failed change leaves no undo entry.
- **Session:** restoring a game by any other means (Journey Log, quick restore, `--load-save`) ends the session: the undo history and its snapshots are dropped and the next change makes a new backup.
- Usecode intrinsics change flags directly; they are not Troubleshooting UI changes and are not undone.

## Bug Report

Safety & report → Export bug report (or `Game_doctor::export_bug_report()`) writes `<SAVEGAME>/bug-reports/exult-report-<game>-<YYYYmmdd-HHMMSS>.zip` and prints `DOCTOR: bug report written to <path>`. Without zip support a folder of the same name is written.

| Entry | Contents |
|---|---|
| `save/game.sav` | The current game, written fresh. |
| `info.txt` | Exult version and git revision, game, mod, mod usecode CRC32 and size, Avatar tile and map, day and time, platform, menu style, report time. |
| `log.txt` | The last 500 lines of stdout and stderr (captured when cheats or a doctor option are on). |
| `flags.txt` | Set flags, `0xNNNN dec NAME`. |
| `dump.txt` | The state dump with findings. |
| `checkup.txt` | One finding per line, or `No findings`. |

Exult mods have no version key, so the mod is identified by its title and the CRC32 of `<PATCH>/usecode`. To replay a report, extract `save/game.sav` and start Exult with `--load-save` on it.

## Testing Harness

Everything runs headless (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`) with a throwaway `exult.cfg` (`-c`) that has its own gamedat and savegame paths.

- **Menu test driver:** `EXULT_MUI_TEST=script.mui` (see [modern_ui.md](modern_ui.md#self-test-driver)) has doctor commands: `open doctor [SECTION]`, `key-action GAME_DOCTOR`, `expect-npc NUM alive|dead`, `expect-npc-near-avatar NUM TILES`, `expect-gflag NAME on|off`, `expect-undo N`, `expect-file PATH[*]`. Start from a save by adding `--load-save` to the command line.
- **Command line:** a generic flow for a mod's test harness: copy a fixture save into the run's savegame folder, run `exult -c CFG --game G --mod M --load-save FILE --doctor-checkup --doctor-dump OUT.txt`, check the exit status (0/3/2) and the `DOCTOR-FINDING` lines, and compare dumps from before and after a change with `doctor_dump_diff.py`. To run usecode first, let the game run it and use `--doctor-at-exit` so the doctor acts when the loop ends. Make fixtures with `--doctor-save`.
- **Usecode tests** can call the intrinsics above and print their results.

## Files

| File | Role |
|---|---|
| gamedoctor.h, gamedoctor.cc | `Game_doctor`: flag table, log capture, NPC browser, item finder, dump, safety and undo, bug report, command line. |
| gamedoctor_checkup.cc | Rule loading and the check-up. |
| data/bg/doctor_rules.txt, data/si/doctor_rules.txt | Base rules (resource `files/doctor_rules`). |
| data/bg/global_flags.txt, data/si/global_flags.txt | Flag tables with descriptions. |
| gumps/Mui_cheats_doctor.cc | The five cheat window sections. |
| gumps/Mui_router.{h,cc} | `Mui::open_doctor()`. |
| keyactions.{h,cc}, keys.cc | `GAME_DOCTOR`. |
| gumps/Mui_selftest.cc | Driver commands. |
| usecode/intrinsics.cc, usecode/ucinternal.h, usecode/*intrinsics.h | Intrinsics. |
| tools/doctor/uc_flags_to_table.py, tools/doctor/doctor_dump_diff.py (+ tests) | Maintainer tools. |

## Known Limits

- **Orphan check is generic:** it finds broken ownership, deep or cyclic nesting, objects off the map and bodies of living NPCs. Whether an item is actually reachable (e.g. walled in) is not checked.
- **World scan:** the item finder, `item` rules and the dump read every superchunk of the maps loaded in this session; a map never visited in this session is not scanned.
- **Log capture** starts when cheats or a doctor option are on at startup; lines printed before that are not in `log.txt`.
- The classic text-mode cheat screen has no doctor entry; use `GAME_DOCTOR` (or a themed style) to reach the sections.
- Two bug reports in the same second share a name; the second overwrites the first.
- Intrinsic slot numbers are provisional.
