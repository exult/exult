/*
 *
 *  Copyright (C) 2006  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// Tells the compiler the game type
#game "serpentisle"

// Starts autonumbering at function number 0xC00.
// I leave function numbers in the range 0xA00 to
// 0xBFF for eggs and weapon functions; this is a
// total of 512 unique functions. That (hopefully)
// is enough...
#autonumber 0xC00

// Misc constants used everywhere
#include "header/constants.uc"
// SI Global Flags
#include "header/si/si_gflags.uc"
// Calls defined in SI Usecode
#include "header/si/si_externals.uc"
// SI Shapes
#include "header/si/si_shapes.uc"
// SI NPCs
#include "header/si/si_npcs.uc"

// New functions
#include "header/functions.uc"

// From here down, all functions have preassigned function numbers:


// Miscellaneous
// Fixes the incorrect answer to Draxinar's last riddle
#include "misc/draxinar_cloth_riddle.uc"
// Fixes spelling in Draxinar's second riddle
#include "misc/draxinar_earrings_riddle.uc"
// Fixes a few bugs in the exchanged item list
#include "misc/exchanged_item_list.uc"
// Fixes a few bugs in the cleanup of Fawn Tower
#include "misc/fawn_tower_cleanup.uc"
// Sets flags on new NPCs at the start of the game
#include "misc/game_start.uc"
// Fixes the broken native gwaniCloakCheck
#include "misc/gwani_cloak_check.uc"
// Inn keys are now deleted/doors locked
#include "misc/inn_keys.uc"
// Fixes a few wrongly identified locations
#include "misc/location_ids.uc"
// Fixes a few bugs when returning the shield to Luther
#include "misc/luther_return_shield.uc"
// New function to remove spell shapes from dying NPCs
#include "misc/remove_spell_shapes.uc"
// Prevents Companions from being "resurrected" after banes are released,
// Dupre from being resurrected after sacrifice and Gwenno from talking
// to the Avatar while insane
#include "misc/resurrect.uc"
// Prevent spawning of the original Silver Seed NPCs
#include "misc/silver_seed_spawn.uc"
// Fixes to spells the Avatar can cast
#include "misc/spells.uc"


// Items
// For curing Cantra, from exult/content/si; modified to allow companions
// to thank you (and rejoin) after you cure them but before Xenka returns
#include "items/bucket_cure.uc"
// Fixes Shrine of Order issues
#include "items/hourglass.uc"
// Can no longer get to Test of Purity from SS
#include "items/pillar.uc"
// Iolo, Shamino and Dupre refuse blue potions in Spinebreaker mountains
#include "items/potion.uc"
// Fixes bugs with the misplaced item list
#include "items/scroll.uc"
// Changes watches/sundials to 24 hour time
// if you ask Shamino to do it.
#include "items/time_tellers.uc"


// Eggs
// Set bear skull flag when Shamino sees the bear
#include "eggs/egg_starting_hints.uc"
// Modifies the bane holocaust to give inn keys to innkeepers
#include "eggs/egg_bane_holocaust.uc"
// Prevents player from taking companions to dream world
#include "eggs/egg_gorlab_swamp_sleep.uc"
// Prevents an Order automaton from joining the Gwani attack in Skullcrusher
#include "eggs/egg_gwani_attack.uc"
// Gives the automatons in Skullcrusher normal schedules
#include "eggs/egg_skullcrusher_automatons.uc"
// Have Draxinar notice your first approach of his home
#include "eggs/egg_approach_draxinar.uc"
// Prevent spawning the original Ernesto.
#include "eggs/egg_spawn_ernesto.uc"


// Cutscenes
// Fixes the Fawn storm so that Iolo's lute is not duplicated
#include "cutscenes/fawn_storm.uc"
// Fixes the Fawn trial so some extra barks intented are shown
#include "cutscenes/fawn_trial.uc"
// Prevents deletion of the training pikeman egg
#include "cutscenes/monitor_banquet.uc"
// Absolutely force companions to be there and force-kills them after
#include "cutscenes/wall_of_lights.uc"


// NPCs
// She now really gives dried fish when asked
#include "npcs/baiyanda.uc"
// Fixes the gwaniCloakCheck and endless Talk activity, and sets the Met flag
#include "npcs/bwundiai.uc"
// For curing Cantra, from exult/content/si
#include "npcs/cantra.uc"
// Fixes a flag to allow Delin to talk about Batlin
#include "npcs/delin.uc"
// Dupre now refuses to leave in Spinebreaker mountains
#include "npcs/dupre.uc"
// Fixes a flag to allow Edrin to talk about Siranush being real
#include "npcs/edrin.uc"
// Ernesto is now a real NPC with dialog that changes depending on items you have
#include "npcs/ernesto.uc"
// Fixes fur cap/misplaced item list bug
#include "npcs/frigidazzi.uc"
// Removes the False Chaos Hierophant bug
#include "npcs/ghost.uc"
// Gives the inn keys to Simon before he dies
#include "npcs/goblin_simon.uc"
// Fixes the diamond necklace so she accepts the correct one
#include "npcs/gwenno.uc"
// Iolo now refuses to leave in Spinebreaker mountains
#include "npcs/iolo.uc"
// Clears a flag to allow asking Kylista about the breastplate
#include "npcs/kylista.uc"
// Fixes the gwaniCloakCheck and improves the party's reply if only he notices
#include "npcs/mwaerno.uc"
// Fixes setting the Met flag so his name appears on single-click
#include "npcs/myauri.uc"
// Give Neyobi a schedule post-cure
#include "npcs/neyobi.uc"
// Fixing the exchanged item list; also, Shamino now refuses to leave when you
// are in the Spinebreaker mountains
#include "npcs/shamino.uc"
// Silver Seed NPCs are now standard numbered NPCs
#include "npcs/silver_seed_npcs.uc"
// Brings the automatons in Skullcrusher to life
#include "npcs/skullcrusher_automatons.uc"
// Prevents resurrecting companions after the Banes are released
#include "npcs/thoxa.uc"
