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
// Want to enforce braces around all statements
#strictbraces "true"

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
// Standard structure definitions
#include "header/si/si_structs.uc"
// Calls defined in SI Usecode
#include "header/si/si_externals.uc"
// SI Shapes
#include "header/si/si_shapes.uc"
// SI NPCs
#include "header/si/si_npcs.uc"
// SI Faces
#include "header/si/si_faces.uc"
// SI Sprites
#include "header/si/si_sprites.uc"

// New functions
#include "header/functions.uc"

// From here down, all functions have preassigned function numbers:

// Fixes spelling in Draxinar's second riddle
#include "misc/draxinar_earrings_riddle.uc"
// Fixes the incorrect answer to Draxinar's last riddle
#include "misc/draxinar_cloth_riddle.uc"
// Fixes a few bugs in the cleanup of Fawn Tower
#include "misc/fawn_tower_cleanup.uc"
// Fixes a few bugs when returning the shield to Luther
#include "misc/luther_return_shield.uc"
// Fixes a few bugs in the exchanged item list
#include "misc/exchanged_item_list.uc"
// Prevents Companions from being "resurrected" after banes are released,
// Dupre from being resurrected after sacrifice and Gwenno from talking
// to the Avatar while insane
#include "misc/resurrect.uc"
// Fixes a few wrongly identified locations
#include "misc/location_ids.uc"
// Fixes the broken native gwaniCloakCheck
#include "misc/gwani_cloak_check.uc"

// Inn keys are now deleted/doors locked
#include "misc/inn_keys.uc"
// Set bear skull flag when Shamino sees the bear
#include "misc/egg_starting_hints.uc"
// Modifies the bane holocaust to give inn keys to innkeepers
#include "misc/egg_bane_holocaust.uc"
// Prevents player from taking companions to dream world
#include "misc/egg_gorlab_swamp_sleep.uc"
// Prevents an Order automaton from joining the Gwani attack in Skullcrusher
#include "misc/egg_gwani_attack.uc"
// Gives the automatons in Skullcrusher normal schedules
#include "misc/egg_skullcrusher_automatons.uc"

// Dupre now refuses to leave in Spinebreaker mountains
#include "npcs/dupre.uc"
// Fixing the exchanged item list; also, Shamino now refuses to leave when you
// are in the Spinebreaker mountains
#include "npcs/shamino.uc"
// Iolo now refuses to leave in Spinebreaker mountains
#include "npcs/iolo.uc"
// She now really gives dried fish when asked
#include "npcs/baiyanda.uc"
// For curing Cantra, from exult/content/si
#include "npcs/cantra.uc"
// Fixes fur cap/misplaced item list bug
#include "npcs/frigidazzi.uc"
// Removes the False Chaos Hierophant bug
#include "npcs/ghost.uc"
// Gives the inn keys to Simon before he dies
#include "npcs/goblin_simon.uc"
// Fixing the diamond necklace thingy
#include "npcs/gwenno.uc"
// Prevents resurrecting companions after banes are released
#include "npcs/thoxa.uc"
// Give Neyobi a schedule post-cure
#include "npcs/neyobi.uc"
// Fixes a flag to allow Delin to talk about Batlin
#include "npcs/delin.uc"
// Fixes a flag to allow Edrin to talk about Siranush being real
#include "npcs/edrin.uc"
// Clears a flag to allow asking Kylista about the breastplate
#include "npcs/kylista.uc"
// Fixes setting the Met flag so his name appears on single-click
#include "npcs/myauri.uc"
// Fixes the gwaniCloakCheck and endless Talk activity, and sets the Met flag
#include "npcs/bwundiai.uc"
// Fixes the gwaniCloakCheck and improves the party's reply if only he notices
#include "npcs/mwaerno.uc"
// Brings the automatons in Skullcrusher to life
#include "npcs/skullcrusher_automatons.uc"
// Fixing wrong book quoting from automaton guard.
#include "npcs/spinebreaker_automatons.uc"

// For curing Cantra, from exult/content/si; modified to allow companions
// to thank you (and rejoin) after you cure them but before Xenka returns
#include "items/bucket_cure.uc"
// Fixes Shrine of Order issues
#include "items/hourglass.uc"
// Fixes the missing face for Smith in the Dream Realm.
#include "items/nightmare.uc"
// Can no longer get to Test of Purity from SS
#include "items/pillar.uc"
// Iolo, Shamino and Dupre refuse blue potions in Spinebreaker mountains
#include "items/potion.uc"
// Fixes bugs with the misplaced item list
#include "items/scroll.uc"
// Changes watches/sundials to 24 hour time
// if you ask Shamino to do it.
#include "items/time_tellers.uc"
// Using new and improved UI_is_water for better fishing experience.
#include "items/fishingrod.uc"

// Fixes to spells the Avatar can cast
#include "spells/spells.uc"

// Fixes the Fawn storm so that Iolo's lute is not duplicated
#include "cutscenes/fawn_storm.uc"
// Fixes the Fawn trial so some extra barks intented are shown
#include "cutscenes/fawn_trial.uc"
// Prevents deletion of the training pikeman egg
#include "cutscenes/monitor_banquet.uc"
// Fix invisible party members not joining back.
#include "cutscenes/moonshade_banquet.uc"
// Absolutely force companions to be there and force-kills them after
#include "cutscenes/wall_of_lights.uc"
// Fixes gang planck blocking check
#include "misc/is_obj_blocked.uc"
// Fixes Boydon's tournament flag post List Field
#include "misc/list_field.uc"
// Allows Hound of Doskar to track Anti-Shamino with Beatrix' Book.
#include "npcs/hound.uc"
// Fixes Topo's schedule to let him be at the inn when he is eating.
#include "npcs/topo.uc"
