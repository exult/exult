/*
 *  combat.h - Combat scheduling.
 *
 *  Copyright (C) 2000-2013  The Exult Team
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <memory>

#include "actors.h"
#include "combat.h"
#include "combat_opts.h"
#include "gamewin.h"
#include "gameclk.h"
#include "gamemap.h"
#include "paths.h"
#include "Astar.h"
#include "actions.h"
#include "items.h"
#include "effects.h"
#include "Audio.h"
#include "ready.h"
#include "game.h"
#include "monstinf.h"
#include "ucmachine.h"
#include "game.h"
#include "Gump_manager.h"
#include "spellbook.h"
#include "animate.h"
#include "ucsched.h"
#include "ucscriptop.h"
#include "cheat.h"
#include "ammoinf.h"
#include "weaponinf.h"
#include "ready.h"
#include "usefuns.h"

using std::cout;
using std::endl;
using std::rand;
using std::list;

unsigned long Combat_schedule::battle_time = static_cast<unsigned long>(-30000);
unsigned long Combat_schedule::battle_end_time = 0;

bool Combat::paused = false;
int Combat::difficulty = 0;
Combat::Mode Combat::mode = Combat::original;
bool Combat::show_hits = false;
bool Combat::charmed_more_difficult = false;

extern bool combat_trace;

const unsigned int dex_to_attack = 30;

static inline bool not_in_melee_range(
    const Weapon_info *winf,        // 0 for monsters or natural weaponry.
    int dist,
    int reach
) {
	if (!winf)
		return dist > reach;
	return (winf->get_uses() == Weapon_info::ranged) || (dist > reach);
}

/*
 *  Is a given ammo shape in a given family.
 */

bool In_ammo_family(int shnum, int family) {
	if (shnum == family)
		return true;
	const Ammo_info *ainf = ShapeID::get_info(shnum).get_ammo_info();
	return ainf != nullptr && ainf->get_family_shape() == family;
}

/*
 *  Start music if battle has recently started.
 */

void Combat_schedule::start_battle(
) {
	if (started_battle)
		return;
	// But only if Avatar is main char.
	if (gwin->get_camera_actor() != gwin->get_main_actor())
		return;
	unsigned long curtime = Game::get_ticks();
	// If this is the avatar, and it has been at least .5 minute since last
	// start, then change music. This allows the danger music to be heard.
	Game_object *target = npc->get_target();
	if (npc == gwin->get_main_actor() && curtime - battle_time >= 30000 &&
	        (!opponents.empty() || (target && target->as_actor()))) {
		Audio::get_ptr()->start_music_combat((rand() % 2) ?
		                                     CSAttacked1 : CSAttacked2, false);
		battle_time = curtime;
		battle_end_time = curtime - 1;
	}
	started_battle = true;
}

/*
 *  This (static) method is called when a monster dies.  It checks to
 *  see if there are still hostile NPC's around.  If not, it plays
 *  'victory' music.
 */

void Combat_schedule::monster_died(
) {
	if (battle_end_time >= battle_time)// Battle raging?
		return;         // No, it's over.
	Actor_vector nearby;        // Get all nearby NPC's.
	gwin->get_nearby_npcs(nearby);
	for (auto *actor : nearby) {
		if (!actor->is_dead() &&
		        actor->get_attack_mode() != Actor::flee &&
		        actor->get_effective_alignment() >= Actor::evil)
			return;     // Still possible enemies.
	}
	battle_end_time = Game::get_ticks();
	// Figure #seconds battle lasted.
	unsigned long len = (battle_end_time - battle_time) / 1000;
	bool hard = len > 15u && (rand() % 60u < len);
	Audio::get_ptr()->start_music_combat(hard ? CSBattle_Over
	                                     : CSVictory, false);
}

/*
 *  This (static) method is called to stop attacking a given NPC.
 *  This can happen because the NPC died, fell asleep or became
 *  invisible.
 */

void Combat_schedule::stop_attacking_npc(
    Game_object *npc
) {
	Actor_vector nearby;        // Get all nearby NPC's.
	gwin->get_nearby_npcs(nearby);
	for (auto *actor : nearby) {
		if (actor->get_target() == npc)
			actor->set_target(nullptr);
	}
}

/*
 *  This (static) method is called to stop attacking a given NPC.
 *  This can happen because the NPC died or fell asleep.
 */

void Combat_schedule::stop_attacking_invisible(
    Game_object *npc
) {
	Actor_vector nearby;        // Get all nearby NPC's.
	gwin->get_nearby_npcs(nearby);
	for (auto *actor : nearby) {
		if (actor->get_target() == npc && !actor->can_see_invisible())
			actor->set_target(nullptr);
	}
}

/*
 *  Can a given shape teleport? summon?  turn invisible?
 */

static bool Can_teleport(
    Actor *npc
) {
	if (npc->get_flag(Obj_flags::no_spell_casting))
		return false;
	return npc->get_info().can_teleport();
}

static bool Can_summon(
    Actor *npc
) {
	if (npc->get_flag(Obj_flags::no_spell_casting))
		return false;
	return npc->get_info().can_summon();
}

static bool Can_be_invisible(
    Actor *npc
) {
	if (npc->get_flag(Obj_flags::no_spell_casting))
		return false;
	return npc->get_info().can_be_invisible();
}

/*
 *  Certain monsters (wisps, mages) can teleport during battle.
 */

bool Combat_schedule::teleport(
) {
	Game_object *trg = npc->get_target();   // Want to get close to targ.
	if (!trg)
		return false;
	unsigned int curtime = SDL_GetTicks();
	if (curtime < teleport_time)
		return false;
	teleport_time = curtime + 2000 + rand() % 2000;
	Tile_coord dest = trg->get_tile();
	dest.tx += 4 - rand() % 8;
	dest.ty += 4 - rand() % 8;
	dest = Map_chunk::find_spot(dest, 3, npc, 1);
	if (dest.tx == -1)
		return false;       // No spot found.
	Tile_coord src = npc->get_tile();
	if (dest.distance(src) > 7 && rand() % 2 != 0)
		return false;       // Got to give Avatar a chance to
	//   get away.
	// Create fire-field where he was.
	src.tz = npc->get_chunk()->get_highest_blocked(src.tz,
	         src.tx % c_tiles_per_chunk, src.ty % c_tiles_per_chunk);
	if (src.tz < 0)
		src.tz = 0;
	eman->add_effect(std::make_unique<Fire_field_effect>(src));
	int sfx = Audio::game_sfx(43);
	Audio::get_ptr()->play_sound_effect(sfx, npc);  // The weird noise.
	npc->move(dest.tx, dest.ty, dest.tz);
	// Show the stars.
	eman->add_effect(std::make_unique<Sprites_effect>(7, npc, 0, 0, 0, 0));
	return true;
}

/*
 *  Some monsters can do the "summon" spell.
 */

bool Combat_schedule::summon(
) {
	ucmachine->call_usecode(SummonSpellUsecode,
	                        npc, Usecode_machine::double_click);
	npc->start_std();       // Back into queue.
	return true;
}

/*
 *  Some can turn invisible.
 */

bool Combat_schedule::be_invisible(
) {
	Object_sfx::Play(npc, Audio::game_sfx(44));
	gwin->get_effects()->add_effect(
	    std::make_unique<Sprites_effect>(12, npc, 0, 0, 0, 0, 0, -1));
	npc->set_flag(Obj_flags::invisible);
	npc->add_dirty();
	npc->start_std();       // Back into queue.
	return true;
}

/*
 *  Off-screen?
 */

inline bool Off_screen(
    Game_window *gwin,
    Game_object *npc
) {
	// See if off screen.
	Tile_coord t = npc->get_tile();
	Rectangle screen = gwin->get_win_tile_rect().enlarge(2);
	return !screen.has_world_point(t.tx, t.ty);
}

bool Combat_schedule::is_enemy(
    int align, int other
) {
	switch (align) {
	case Actor::good:
		return other == Actor::evil
		       || other == Actor::chaotic;
	case Actor::evil:
		return other == Actor::good
		       || other == Actor::chaotic;
	case Actor::neutral:
		return false;
	case Actor::chaotic:
		return other == Actor::evil
		       || other == Actor::good;
	}
	return true;    // This should never happen.
}

/*
 *  Find nearby opponents in the 9 surrounding chunks.
 */

void Combat_schedule::find_opponents(
) {
	opponents.clear();
	Game_window *gwin = Game_window::get_instance();
	Actor_vector nearby;
	Actor_vector sleeping;            // Get all nearby NPC's.
	gwin->get_nearby_npcs(nearby);
	Actor *avatar = gwin->get_main_actor();
	nearby.push_back(avatar);   // Incl. Avatar!
	bool charmed_avatar = Combat::charmed_more_difficult &&
	                      avatar->get_effective_alignment() !=  Actor::good;
	// See if we're a party member.
	bool in_party = npc->is_in_party() || npc == avatar;
	int npc_align = npc->get_effective_alignment();
	bool see_invisible = npc->can_see_invisible();
	for (auto *actor : nearby) {
		if (actor->is_dead() || (!see_invisible && actor->get_flag(Obj_flags::invisible)))
			continue;   // Dead or invisible.
		if (is_enemy(npc_align, actor->get_effective_alignment())) {
			if (actor->get_flag(Obj_flags::asleep)) {
				// sleeping
				sleeping.push_back(actor);
				if (combat_trace)
					cout << npc->get_name() << " pushed back(asleep,1) " << actor->get_name() << endl;
			} else {
				opponents.push_back(actor->weak_from_this());
				if (combat_trace)
					cout << npc->get_name() << " pushed back(1) " << actor->get_name() << endl;
			}
		} else if (in_party) {  // Attacking party member?
			Game_object *t = actor->get_target();
			if (!t)
				continue;
			// Is actor attacking a party member?
			if ((t->get_flag(Obj_flags::in_party) || t == avatar) &&
			        t->as_actor() && is_enemy(npc_align, t->as_actor()->get_effective_alignment())) {
				opponents.push_back(actor->weak_from_this());
				if (combat_trace)
					cout << npc->get_name() << " pushed back(2) " << actor->get_name() << endl;
			}
			int oppressor = actor->get_oppressor();
			if (oppressor < 0)
				continue;
			Actor *oppr = gwin->get_npc(oppressor);
			assert(oppr != nullptr);
			// Is actor being attacked by a party member?
			if ((oppr->get_flag(Obj_flags::in_party) || oppr == avatar) &&
			        is_enemy(npc_align, oppr->get_effective_alignment())) {
				opponents.push_back(actor->weak_from_this());
				if (combat_trace)
					cout << npc->get_name() << " pushed back(3) " << actor->get_name() << endl;
			}
		}
	}
	// None found?  Use Avatar's if same effective alignment
	if (opponents.empty() && in_party &&
	        avatar->get_effective_alignment() == npc_align) {
		Game_object *opp = avatar->get_target();
		Actor *oppnpc = opp ? opp->as_actor() : nullptr;
		if (oppnpc && oppnpc != npc
		        && oppnpc->get_schedule_type() == Schedule::combat) {
			opponents.push_back(oppnpc->weak_from_this());
			if (combat_trace)
				cout << npc->get_name() << " pushed back (4) " << oppnpc->get_name() << endl;
		}
	}
	// Still none? Try the sleeping/unconscious foes.
	if (opponents.empty() && !sleeping.empty()
	    && (npc != avatar || charmed_avatar)) {
		for (auto *act : sleeping) {
		    opponents.push_front(weak_from_obj(act));
		}
		if (combat_trace)
			cout << npc->get_name() << " pushed back asleep opponents" << endl;
	}
}

/*
 *  Find 'protected' party member's attackers.
 *
 *  Output: ->attacker in opponents, or opponents.end() if not found.
 */

list<Game_object_weak>::iterator Combat_schedule::find_protected_attacker(
) {
	if (!npc->is_in_party())    // Not in party?
		return opponents.end();
	Game_window *gwin = Game_window::get_instance();
	Actor *party[9];        // Get entire party, including Avatar.
	int cnt = gwin->get_party(party, 1);
	Actor *prot_actor = nullptr;
	for (int i = 0; i < cnt; i++)
		if (party[i]->is_combat_protected()) {
			prot_actor = party[i];
			break;
		}
	if (!prot_actor)        // Not found?
		return opponents.end();
	// Find closest attacker.
	int best_dist = 4 * c_tiles_per_chunk;
	auto best_opp = opponents.end();
	for (auto it = opponents.begin();
	        it != opponents.end(); ++it) {
		Actor_shared opp = std::static_pointer_cast<Actor>((*it).lock());
		int dist;
		if (opp && opp->get_target() == prot_actor &&
		        (dist = npc->distance(opp.get())) < best_dist) {
			best_dist = dist;
			best_opp = it;
		}
	}
	if (best_opp == opponents.end())
		return opponents.end();
	if (failures < 5 && yelled && rand() % 2 && npc != prot_actor) {
		if (npc->is_goblin())
			npc->say(goblin_will_help);
		else if (can_yell)
			npc->say(first_will_help, last_will_help);
	}
	return best_opp;
}

/*
 *  Find a foe.
 *
 *  Output: Opponent that was found.
 */

Game_object *Combat_schedule::find_foe(
    int mode            // Mode to use.
) {
	if (combat_trace) {
		cout << "'" << npc->get_name() << "' is looking for a foe" <<
		     endl;
	}
	int new_align = npc->get_effective_alignment();
	if (new_align != alignment) {
		// Alignment changed.
		opponents.clear();
		alignment = new_align;
	}
	Actor *avatar = gwin->get_main_actor();
	bool charmed_avatar = Combat::charmed_more_difficult &&
	                      avatar->get_effective_alignment() !=  Actor::good;
	// Remove any that died.
	opponents.erase(std::remove_if(opponents.begin(), opponents.end(),
		[this, avatar, charmed_avatar](const Game_object_weak& curr) {
			Actor_shared actor = std::static_pointer_cast<Actor>(curr.lock());
			return !actor || actor->is_dead() ||
			       (npc == avatar && !charmed_avatar &&
			        actor->get_flag(Obj_flags::asleep));
		}
		), opponents.end());
	if (opponents.empty()) { // No more from last scan?
		find_opponents();   // Find all nearby.
		if (practice_target)    // For dueling.
			return practice_target;
	}
	auto new_opp_link = opponents.end();
	switch (static_cast<Actor::Attack_mode>(mode)) {
	case Actor::weakest: {
		int str;
		int least_str = 100;
		for (auto it = opponents.begin();
		        it != opponents.end(); ++it) {
			Actor_shared opp = std::static_pointer_cast<Actor>((*it).lock());
			if (!opp)
			    continue;
			str = opp->get_property(Actor::strength);
			if (str < least_str) {
				least_str = str;
				new_opp_link = it;
			}
		}
		break;
	}
	case Actor::strongest: {
		int str;
		int best_str = -100;
		for (auto it = opponents.begin();
		        it != opponents.end(); ++it) {
			Actor_shared opp = std::static_pointer_cast<Actor>((*it).lock());
			if (!opp)
			    continue;
			str = opp->get_property(Actor::strength);
			if (str > best_str) {
				best_str = str;
				new_opp_link = it;
			}
		}
		break;
	}
	case Actor::nearest: {
		int best_dist = 4 * c_tiles_per_chunk;
		for (auto it = opponents.begin();
		        it != opponents.end(); ++it) {
			Actor_shared opp = std::static_pointer_cast<Actor>((*it).lock());
			if (!opp)
			    continue;
			int dist = npc->distance(opp.get());
			if (opp->get_attack_mode() == Actor::flee)
				dist += 16; // Avoid fleeing.
			if (dist < best_dist) {
				best_dist = dist;
				new_opp_link = it;
			}
		}
		break;
	}
	case Actor::protect:
		new_opp_link = find_protected_attacker();
		if (new_opp_link != opponents.end())
			break;      // Found one.
		// FALLTHROUGH
	case Actor::random:
	default:            // Default to random.
		if (!opponents.empty())
			new_opp_link = opponents.begin();
		break;
	}
	Actor_shared new_opponent;
	if (new_opp_link != opponents.end()) {
		new_opponent = std::static_pointer_cast<Actor>((*new_opp_link).lock());
		opponents.erase(new_opp_link);
	}
	return new_opponent ? new_opponent.get() : nullptr;
}

/*
 *  Find a foe.
 *
 *  Output: Opponent that was found.
 */

inline Game_object *Combat_schedule::find_foe(
) {
	if (npc->get_attack_mode() == Actor::manual)
		return nullptr;       // Find it yourself.
	return find_foe(static_cast<int>(npc->get_attack_mode()));
}

/*
 *  Back off if we're closer than our weapon's range.
 */

void Combat_schedule::back_off(
	Actor *npc,
    Game_object *attacker
) {
	int points;
	int weapon_shape;
	Game_object *weapon;
	const Weapon_info *winf = npc->get_weapon(points, weapon_shape, weapon);
	int weapon_dist = winf ? winf->get_range() : 3;
	int attacker_dist = npc->distance(attacker);
	Game_object *opponent = npc->get_target();
	if (opponent == attacker && weapon_dist <= attacker_dist)
	    return;	 			 			// Stay within our weapon's range.
	Tile_coord npc_tile = npc->get_tile();
	Tile_coord attacker_tile = attacker->get_tile();
	int dx = npc_tile.tx - attacker_tile.ty;
	int dy = npc_tile.ty - attacker_tile.ty;
	dx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
	dy = dy < 0 ? -1 : dy > 0 ? 1 : 0;
    Tile_coord spots[3];
	if (dx) {
	    if (dy) {
	        spots[0] = npc_tile + Tile_coord(dx, 0, 0);
		    spots[1] = npc_tile + Tile_coord(dx, dy, 0);
			spots[2] = npc_tile + Tile_coord(0, dy, 0);
		} else {
	        spots[0] = npc_tile + Tile_coord(dx, 0, 0);
		    spots[1] = npc_tile + Tile_coord(dx, -1, 0);
			spots[2] = npc_tile + Tile_coord(dx, 1, 0);
		}
    } else {							// dx == 0;
	    spots[0] = npc_tile + Tile_coord(0, dy, 0);
		spots[1] = npc_tile + Tile_coord(-1, dy, 0);
		spots[2] = npc_tile + Tile_coord(1, dy, 0);
    }
	int ind = rand()%3;
	if (npc->is_blocked(spots[ind])) {
	    ind = (ind + 1)%3;
		if (npc->is_blocked(spots[ind])) {
		    ind = (ind + 1)%3;
			if (npc->is_blocked(spots[ind]))
			   return;
		}
    }
	npc->move(spots[ind]);
    int dir = npc->get_facing_direction(attacker);
	npc->change_frame(npc->get_dir_framenum(dir, Actor::standing));
	cout << "***" << npc->get_name() << " is backing off" << endl;
}

/*
 *  Handle the 'approach' state.
 */

void Combat_schedule::approach_foe(
    bool for_projectile     // Want to attack with projectile.
    // FOR NOW:  Called as last resort,
    //  and we try to reach target.
) {
	int points;
	const Weapon_info *winf = npc->get_weapon(points, weapon_shape, weapon);
	int dist = for_projectile ? 1 : winf ? winf->get_range() : 3;
	Game_object *opponent = npc->get_target();
	// Find opponent.
	if (!opponent && !(opponent = find_foe())) {
		failures++;
		npc->start(200, 400);   // Try again in 2/5 sec.
		return;         // No one left to fight.
	}
	npc->set_target(opponent);
	Actor::Attack_mode mode = npc->get_attack_mode();
	Game_window *gwin = Game_window::get_instance();
	// Time to run?
	const Monster_info *minf = npc->get_info().get_monster_info();
	if ((!minf || !minf->cant_die()) &&
	        (mode == Actor::flee ||
	         (mode != Actor::berserk &&
	          (npc->get_type_flags()&MOVE_ALL) != 0 &&
	          npc != gwin->get_main_actor() &&
	          npc->get_property(Actor::health) < 3))) {
		run_away();
		return;
	}
	if (rand() % 4 == 0 && Can_teleport(npc) && // Try 1/4 to teleport.
	        teleport()) {
		start_battle();
		npc->start_std();
		return;
	}
	PathFinder *path = new Astar();
	// Try this for now:
	Monster_pathfinder_client cost(npc, dist, opponent);
	Tile_coord pos = npc->get_tile();
	if (!path->NewPath(pos, opponent->get_tile(), &cost)) {
		// Failed?  Try nearest opponent.
		failures++;
		bool retry_ok = false;
		if (npc->get_attack_mode() != Actor::manual) {
			Game_object *closest = find_foe(Actor::nearest);
			if (!closest) {
				// No one nearby.
				if (combat_trace)
					cout << npc->get_name() << " has no opponents nearby."
					     << endl;
				npc->set_target(nullptr);
				retry_ok = false;
			} else if (closest != opponent) {
				opponent = closest;
				npc->set_target(opponent);
				Monster_pathfinder_client cost(npc, dist,
				                               opponent);
				retry_ok = (opponent != nullptr && path->NewPath(
				                pos, opponent->get_tile(), &cost));
			}
		}
		if (!retry_ok) {
			delete path;    // Really failed.  Try again in
			//  after wandering.
			// Just try to walk towards opponent.
			Tile_coord pos = npc->get_tile();
			Tile_coord topos = opponent->get_tile();
			int dirx = topos.tx > pos.tx ? 2
			           : (topos.tx < pos.tx ? -2 : (rand() % 3 - 1));
			int diry = topos.ty > pos.ty ? 2
			           : (topos.ty < pos.ty ? -2 : (rand() % 3 - 1));
			pos.tx += dirx * (1 + rand() % 4);
			pos.ty += diry * (1 + rand() % 4);
			npc->walk_to_tile(pos, 2 * gwin->get_std_delay(),
			                  500 + rand() % 500);
			failures++;
			return;
		}
	}
	failures = 0;           // Clear count.  We succeeded.
	start_battle();         // Music if first time.
	if (combat_trace) {
		cout << npc->get_name() << " is pursuing " << opponent->get_name()
		     << endl;
	}
	// First time (or 256th), visible?
	if (!yelled && gwin->add_dirty(npc)) {
		yelled++;
		if (can_yell && rand() % 2) { // Half the time.
			if (npc->is_goblin())       // Goblin?
				npc->say(goblin_to_battle);
			else if (can_yell)
				npc->say(first_to_battle, last_to_battle);
		}
	}
	int extra_delay = 0;
	// Walk there, & check half-way.
	npc->set_action(new Approach_actor_action(path, opponent,
	                for_projectile));
	// Start walking.  Delay a bit if
	//   opponent is off-screen.
	npc->start(gwin->get_std_delay(), extra_delay +
	           (Off_screen(gwin, opponent) ?
	            5 * gwin->get_std_delay() : gwin->get_std_delay()));
}

/*
 *  Check for a useful weapon at a given ready-spot.
 */

static Game_object *Get_usable_weapon(
    Actor *npc,
    int index           // Ready-spot to check.
) {
	Game_object *bobj = npc->get_readied(index);
	if (!bobj)
		return nullptr;
	const Shape_info &info = bobj->get_info();
	const Weapon_info *winf = info.get_weapon_info();
	if (!winf)
		return nullptr;       // Not a weapon.
	Game_object *aobj;  // Check ranged first.
	int need_ammo = npc->get_weapon_ammo(bobj->get_shapenum(),
	                                     winf->get_ammo_consumed(), winf->get_projectile(),
	                                     true, &aobj);
	if (need_ammo) {
		if (!aobj)  // Try melee.
			need_ammo = npc->get_weapon_ammo(bobj->get_shapenum(),
			                                 winf->get_ammo_consumed(), winf->get_projectile(),
			                                 false, &aobj);
		if (need_ammo && !aobj)
			return nullptr;
	}
	if (info.get_ready_type() == both_hands &&
	        npc->get_readied(rhand) != nullptr)
		return nullptr;       // Needs two free hands.
	return bobj;
}

/*
 *  Swap weapon with the one in the belt.
 *
 *  Output: 1 if successful.
 */

static int Swap_weapons(
    Actor *npc
) {
	int index = belt;
	Game_object *bobj = Get_usable_weapon(npc, index);
	if (!bobj) {
		index = back_2h;
		bobj = Get_usable_weapon(npc, index);
		if (!bobj) {
			// Do thorough search for NPC's.
			if (!npc->is_in_party())
				return npc->ready_best_weapon();
			else
				return 0;
		}
	}
	Game_object *oldweap = npc->get_readied(lhand);
	if (oldweap)
		npc->remove(oldweap);
	npc->remove(bobj);
	npc->add(bobj, true);      // Should go into weapon hand.
	if (oldweap)            // Put old where new one was.
		npc->add_readied(oldweap, index, true, true);
	return 1;
}

// Just move around a bit.
void Combat_schedule::wander_for_attack(
) {
	Tile_coord pos = npc->get_tile();
	Game_object *opponent = npc->get_target();
	int dir = npc->get_direction(opponent);

	dir += rand()%2 ? 2 : 6;			// A perpendicular direction.
	dir %= 8;
	int tries = 3;
	while (tries--) {
	    int cnt = 2 + rand()%3;
	    while (cnt--)
	        pos = pos.get_neighbor(dir);
	    // Find a free spot.
	    Tile_coord dest = Map_chunk::find_spot(pos, 3, npc, 1);
	    if (dest.tx != -1 && npc->walk_path_to_tile(dest,
								gwin->get_std_delay(), rand() % 1000))
		    return;				// Success.
	}
	// Failed?  Try again a little later.
	npc->start(250, rand() % 3000);
}

/*
 *  Begin a strike at the opponent.
 */

void Combat_schedule::start_strike(
) {
	Game_object *opponent = npc->get_target();
	bool check_lof = !no_blocking;
	// Get difference in lift.
	const Weapon_info *winf = weapon_shape >= 0 ?
	                    ShapeID::get_info(weapon_shape).get_weapon_info() : nullptr;
	int dist = npc->distance(opponent);
	int reach;
	if (!winf) {
		const Monster_info *minf = npc->get_info().get_monster_info();
		reach = minf ? minf->get_reach() : Monster_info::get_default()->get_reach();
	} else
		reach = winf->get_range();
	bool ranged = not_in_melee_range(winf, dist, reach);
	// Out of range?
	if (!spellbook && npc->get_effective_range(winf, reach) < dist) {
		state = approach;
		approach_foe();     // Get a path.
		return;
	} else if (spellbook || ranged) {
		bool weapon_dead = false;
		if (spellbook)
			weapon_dead = !spellbook->can_do_spell(npc);
		else if (winf) {
			// See if we can fire spell/projectile.
			Game_object *ammo = nullptr;
			int need_ammo = npc->get_weapon_ammo(weapon_shape,
			                                     winf->get_ammo_consumed(), winf->get_projectile(),
			                                     ranged, &ammo);
			if (need_ammo && !ammo && !npc->ready_ammo())
				weapon_dead = true;
		}
		if (weapon_dead) {
			// Out of ammo/reagents/charges.
			if (npc->get_schedule_type() != Schedule::duel) {
				// Look in pack for ammo.
				if (Swap_weapons(npc))
					Combat_schedule::set_weapon();
				else
					set_hand_to_hand();
			}
			if (!npc->get_info().has_strange_movement())
				npc->change_frame(npc->get_dir_framenum(
				                      Actor::standing));
			state = approach;
			npc->set_target(nullptr);
			npc->start(200, 500);
			return;
		}
		state = fire;       // Clear to go.
	} else {
		check_lof = (reach > 1);
		state = strike;
	}
	// At this point, we're within range, with state set.
	if (check_lof &&
	        !Fast_pathfinder_client::is_straight_path(npc, opponent)) {
		state = approach;
		wander_for_attack();
		return;
	}
	if (!started_battle)
		start_battle(); // Play music if first time.
	// Some battle cries. Guessing at where to do it, and how often.
	if (yelled && !(rand() % 20)) {
		if (npc->is_goblin())
			npc->say(first_goblin_taunt, last_goblin_taunt);
		else if (can_yell)
			npc->say(first_taunt, last_taunt);
	}
	if (combat_trace) {
		cout << npc->get_name() << " attacks " << opponent->get_name() << endl;
	}
	int dir = npc->get_direction(opponent);
	signed char frames[12];     // Get frames to show.
	int cnt = npc->get_attack_frames(weapon_shape, ranged, dir, frames);
	if (cnt)
		npc->set_action(new Frames_actor_action(frames, cnt, gwin->get_std_delay()));
	npc->start();           // Get back into time queue.
	int sfx = -1;           // Play sfx.
	if (winf)
		sfx = winf->get_sfx();
	if (sfx < 0 || !winf) {
		const Monster_info *minf = ShapeID::get_info(
		                         npc->get_shapenum()).get_monster_info();
		if (minf)
			sfx = minf->get_hitsfx();
	}
	if (sfx >= 0) {
		int delay = ranged ? cnt : cnt / 2;
		Object_sfx::Play(npc, sfx, delay * gwin->get_std_delay());
	}
	dex_points -= dex_to_attack;
}

/*
 *  This static method causes the NPC to attack a given target/tile
 *  using the given weapon shape as weapon. This does not add
 *  an attack animation; rather, it is the actual strike attempt.
 *
 *  This function is called from (a) an intrinsic, (b) a script opcode
 *  or (c) the combat schedule.
 *
 *  Output: Returns false the attack cannot be realized (no ammo,
 *  out of range, etc.) or if a melee attack misses, true otherwise.
 */

bool Combat_schedule::attack_target(
    Game_object *attacker,      // Who/what is attacking.
    Game_object *target,        // Who/what is being attacked.
    Tile_coord const &tile,     // What tile is under fire, if no target.
    int weapon,                 // What is being used as weapon.
    // or < 0 for none.
    bool combat                 // We got here from combat schedule.
) {
	// Bail out if no attacker or if no target and no valid tile.
	if (!attacker || (!target && tile.tx == -1))
		return false;

	// Do not proceed if target is dead.
	Actor *att = attacker->as_actor();
	if (att && att->is_dead())
		return false;
	bool flash_mouse = !combat && att && gwin->get_main_actor() == att
	                   && att->get_attack_mode() != Actor::manual;

	const Shape_info &info = ShapeID::get_info(weapon);
	const Weapon_info *winf = weapon >= 0 ? info.get_weapon_info() : nullptr;

	int reach;
	int family = -1;    // Ammo, is needed, is the weapon itself.
	int proj = -1;  // This is what we will use as projectile sprite.
	if (!winf) {
		const Monster_info *minf = attacker->get_info().get_monster_info();
		reach = minf ? minf->get_reach() : Monster_info::get_default()->get_reach();
	} else {
		reach = winf->get_range();
		proj = winf->get_projectile();
		family = winf->get_ammo_consumed();
	}
	int dist = target ? attacker->distance(target) : attacker->distance(tile);
	bool ranged = not_in_melee_range(winf, dist, reach);
	// Out of range?
	if (attacker->get_effective_range(winf, reach) < dist) {
		// We are out of range.
		if (flash_mouse)
			Mouse::mouse->flash_shape(Mouse::outofrange);
		return false;
	}

	// See if we need ammo.
	Game_object *ammo = nullptr;
	int need_ammo = attacker->get_weapon_ammo(weapon, family,
	                proj, ranged, &ammo);
	Game_object_shared ammo_keep = shared_from_obj(ammo);
	if (need_ammo && !ammo) {
		if (flash_mouse)
			Mouse::mouse->flash_shape(Mouse::outofammo);
		// We don't have ammo, so bail out.
		return false;
	}

	// proj == -3 means use weapon shape for projectile sprite.
	if (proj == -3)
		proj = weapon;
	const Ammo_info *ainf;
	int basesprite;
	if (need_ammo && family >= 0) {
		// ammo should be nonzero here.
		ainf = ammo->get_info().get_ammo_info();
		basesprite = ammo->get_shapenum();
	} else {
		ainf = info.get_ammo_info();
		basesprite = weapon;
	}
	if (ainf) {
		int sprite = ainf->get_sprite_shape();
		if (sprite == -3)
			proj = basesprite;
		else if (sprite != -1 && sprite != ainf->get_family_shape())
			proj = sprite;
	} else
		ainf = Ammo_info::get_default();    // So we don't need to keep checking.

	// By now, proj should be >=0 or -1 for none.
	assert(proj >= -1);
	if (!winf)  // So we don't have to keep checking.
		winf = Weapon_info::get_default();
	if (need_ammo) {
		// We should only ever get here for containers and NPCs.
		// Also, ammo should never be zero in this branch.
		bool need_new_weapon = false;
		bool ready = att ? att->find_readied(ammo) >= 0 : false;

		// Time to use up ammo.
		if (winf->uses_charges()) {
			if (ammo->get_info().has_quality())
				ammo->set_quality(ammo->get_quality() - need_ammo);
			if (winf->delete_depleted() &&
			        (!ammo->get_quality() || !ammo->get_info().has_quality())) {
				// Call unready usecode if needed.
				if (att)
					att->remove(ammo);
				ammo->remove_this();
				need_new_weapon = true;
			}
		} else {
			int quant = ammo->get_quantity();
			// Call unready usecode if needed.
			if (att && quant == need_ammo)
				att->remove(ammo);
			ammo->modify_quantity(-need_ammo, &need_new_weapon);
		}

		if (att && need_new_weapon && ready) {
			// Readied weapon was depleted; we need a new one.
			if (winf->returns() || ainf->returns()) {
				// Weapon will return, so wait for it.
				if (combat) {
					// We got here due to combat schedule.
					auto *sched =
					    dynamic_cast<Combat_schedule *>(att->get_schedule());
					if (sched)  // May not need this check.
						sched->set_state(wait_return);
				}
			}
			// Try readying ammo first.
			else if (att && !att->ready_ammo()) {
				// Need new weapon.
				att->ready_best_weapon();
				// Tell schedule about it.
				att->get_schedule()->set_weapon(true);
			}
		}
	}

	Actor *trg = target ? target->as_actor() : nullptr;
	bool trg_party = trg ? trg->is_in_party() : false;
	bool att_party = att ? att->is_in_party() : false;
	int attval = att ? att->get_effective_prop(static_cast<int>(Actor::combat)) : 0;
	// These two give the correct statistics:
	attval += (winf->lucky() ? 3 : 0);
	attval += (ainf->lucky() ? 3 : 0);
	int bias = trg_party ? Combat::difficulty :
	           (att_party ? -Combat::difficulty : 0);
	attval += 2 * bias; // Apply all bias to the attack value.
	if (ranged) {
		int uses = winf->get_uses();
		attval += 6;
		// This seems reasonably close to how the originals do it,
		// although the error bands of the statistics are too wide here.
		if (uses == Weapon_info::poor_thrown)
			attval -= dist;
		else if (uses == Weapon_info::good_thrown)
			attval -= dist / 2;
		// We need to pass the attack value here to guard against
		// the possibility of the attacker's combat be lowered
		// (e.g., due to being paralyzed) while the projectile is
		// in flight and before it hits.
		std::unique_ptr<Projectile_effect> projectile;
		if (target)
			projectile = std::make_unique<Projectile_effect>(attacker, target, weapon,
			                                   ammo ? ammo->get_shapenum() : proj, proj, attval);
		else
			projectile = std::make_unique<Projectile_effect>(attacker, tile, weapon,
			                                   ammo ? ammo->get_shapenum() : proj, proj, attval);
		gwin->get_effects()->add_effect(std::move(projectile));
		return true;
	} else if (target) {
		// Do nothing when attacking tiles in melee.
		bool autohit = winf->autohits() || ainf->autohits();
		// godmode effects:
		if (cheat.in_god_mode())
			autohit = trg_party ? false : (att_party ? true : autohit);
		if (!autohit && !target->try_to_hit(attacker, attval))
			return false;   // Missed.
		target->play_hit_sfx(weapon, false);
		//if (weapon == 704)    // Powder keg.
		if (info.is_explosive()) {  // Powder keg.
			// Blow up *instead*.
			Tile_coord offset(0, 0, target->get_info().get_3d_height() / 2);
			eman->add_effect(std::make_unique<Explosion_effect>(target->get_tile() + offset,
			                                      target, 0, weapon, -1, attacker));
		} else {
		    Game_object_weak trg_check = weak_from_obj(trg);
			target->attacked(attacker, weapon,
			                 ammo ? ammo->get_shapenum() : -1, false);
            if (trg && !trg_check.expired())
			    back_off(trg, attacker);
		}
		return true;
	}
	return false;
}

/*
 *  Run away.
 */

void Combat_schedule::run_away(
) {
	Game_window *gwin = Game_window::get_instance();
	fleed++;
	// Might be nice to run from opp...
	int rx = rand();        // Get random position away from here.
	int ry = rand();
	int dirx = 2 * (rx % 2) - 1; // Get 1 or -1.
	int diry = 2 * (ry % 2) - 1;
	Tile_coord pos = npc->get_tile();
	pos.tx += dirx * (8 + rx % 8);
	pos.ty += diry * (8 + ry % 8);
	npc->walk_to_tile(pos, gwin->get_std_delay(), 0);
	if (fleed == 1 && !npc->get_flag(Obj_flags::tournament) &&
	        rand() % 3 && gwin->add_dirty(npc)) {
		yelled++;
		if (npc->is_goblin()) {
			if (rand() % 4)
				npc->say(goblin_flee_screaming);
			else
				npc->say(first_goblin_flee, last_goblin_flee);
		} else if (can_yell) {
			if (rand() % 4)
				npc->say(flee_screaming);
			else
				npc->say(first_flee, last_flee);
		}
	}
}

/*
 *  See if a spellbook is readied with a spell
 *  available.
 *
 *  Output: ->spellbook if so, else nullptr.
 */

Spellbook_object *Combat_schedule::readied_spellbook(
) {
	Spellbook_object *book = nullptr;
	// Check both hands.
	Game_object *obj = npc->get_readied(lhand);
	if (obj && obj->get_info().get_shape_class() == Shape_info::spellbook) {
		book = static_cast<Spellbook_object *>(obj);
		if (book->can_do_spell(npc))
			return book;
	}
	obj = npc->get_readied(rhand);
	if (obj && obj->get_info().get_shape_class() == Shape_info::spellbook) {
		book = static_cast<Spellbook_object *>(obj);
		if (book->can_do_spell(npc))
			return book;
	}
	return nullptr;
}

/*
 *  Set weapon 'max_range' and 'ammo'.  Ready a new weapon if needed.
 */

void Combat_schedule::set_weapon(
    bool removed            // The weapon was just removed.
) {
	int points;
	spellbook = nullptr;
	const Weapon_info *info = npc->get_weapon(points, weapon_shape, weapon);
	if (!removed &&
	        // Not dragging?
	        !gwin->is_dragging() &&
	        // And not dueling?
	        npc->get_schedule_type() != Schedule::duel &&
	        state != wait_return) { // And not waiting for boomerang.
		if (!info &&    // No weapon?
		        !(spellbook = readied_spellbook())) { // No spellbook?
			npc->ready_best_weapon();
			info = npc->get_weapon(points, weapon_shape, weapon);
		} else
			npc->ready_best_shield();
	}
	if (!info) {        // Still nothing.
		if (spellbook)      // Did we find a spellbook?
			no_blocking = true;
		else    // Don't do this if using spellbook.
			set_hand_to_hand();
	} else {
		no_blocking = false;
	}
	if (state == strike || state == fire)
		state = approach;   // Got to restart attack.
}


/*
 *  Set for hand-to-hand combat (no weapon).
 */

void Combat_schedule::set_hand_to_hand(
) {
	weapon = nullptr;
	weapon_shape = -1;
	no_blocking = false;
	// Put aside weapon.
	Game_object *weapon = npc->get_readied(lhand);
	if (weapon) {
	    Game_object_shared keep = weapon->shared_from_this();
	    npc->remove(weapon);
		if (!npc->add_readied(weapon, belt, true) &&
		        !npc->add_readied(weapon, back_2h, true) &&
		        !npc->add_readied(weapon, back_shield, true) &&
		        !npc->add_readied(weapon, rhand, true) &&
		        !npc->add_readied(weapon, backpack, true))
			npc->add(weapon, false, false, true);
	}
}

/*
 *  See if we need a new opponent.
 */

inline int Need_new_opponent(
    Game_window *gwin,
    Actor *npc
) {
	Game_object *opponent = npc->get_target();
	Actor *act;
	bool see_invisible = npc->can_see_invisible();
	// Nonexistent or dead?
	if (!opponent ||
	        ((act = opponent->as_actor()) != nullptr && act->is_dead()) ||
	        // Or invisible?
	        (!see_invisible && opponent->get_flag(Obj_flags::invisible)
	         && rand() % 4 == 0))
		return 1;
	// See if off screen.
	return Off_screen(gwin, opponent) && !Off_screen(gwin, npc);
}

/*
 *  Create.
 */

Combat_schedule::Combat_schedule(
    Actor *n,
    Schedule_types
    prev_sched
) : Schedule(n), state(initial), prev_schedule(prev_sched),
	practice_target(nullptr), weapon(nullptr), weapon_shape(-1), spellbook(nullptr),
	no_blocking(false), yelled(0), started_battle(false), fleed(0),
	failures(0), teleport_time(0), summon_time(0),
	dex_points(0), alignment(n->get_effective_alignment()) {
	Combat_schedule::set_weapon();
	// Cache some data.
	can_yell = npc->can_speak();
	unsigned int curtime = SDL_GetTicks();
	summon_time = curtime + 4000;
	invisible_time = curtime + 4500;
}


/*
 *  Previous action is finished.
 */

void Combat_schedule::now_what(
) {
	Game_window *gwin = Game_window::get_instance();
	if (state == initial) {     // Do NOTHING in initial state so
		//   usecode can, e.g., set opponent.
		// Way far away (50 tiles)?
		if (npc->distance(gwin->get_camera_actor()) > 50) {
			npc->set_dormant();
			return;     // Just go dormant.
		}
		state = approach;
		npc->start(200, 200);
		return;
	}
	if (npc->get_flag(Obj_flags::asleep)) {
		npc->start(200, 1000);  // Check again in a second.
		return;
	}
	// Running away?
	if (npc->get_attack_mode() == Actor::flee) {
		// If not in combat, stop running.
		const Monster_info *minf = npc->get_info().get_monster_info();
		if (minf && minf->cant_die())
			npc->set_attack_mode(Actor::nearest);
		else if (fleed > 2 && !gwin->in_combat() &&
		         npc->get_party_id() >= 0)
			// WARNING:  Destroys ourself.
			npc->set_schedule_type(Schedule::follow_avatar);
		else
			run_away();
		return;
	}
	// Check if opponent still breathes.
	if (Need_new_opponent(gwin, npc)) {
		npc->set_target(nullptr);
		state = approach;
	}
	Game_object *opponent = npc->get_target();
	switch (state) {        // Note:  state's action has finished.
	case approach:
		if (!opponent)
			approach_foe();
		else if (dex_points >= dex_to_attack) {
			int effint = npc->get_effective_prop(
			                 Actor::intelligence);
			if (!npc->get_flag(Obj_flags::invisible) &&
			        Can_be_invisible(npc) && rand() % 300 < effint) {
				(void) be_invisible();
				dex_points -= dex_to_attack;
			} else if (Can_summon(npc) && rand() % 600 < effint &&
			           summon())
				dex_points -= dex_to_attack;
			else
				start_strike();
		} else {
			dex_points += npc->get_property(Actor::dexterity);
			npc->start_std();
		}
		break;
	case strike: {          // He hasn't moved away?
		state = approach;
		// Back into queue.
		npc->start_std();
		Actor_shared safenpc = std::static_pointer_cast<Actor>(
											npc->shared_from_this());
		// Change back to ready frame.
		int delay = gwin->get_std_delay();
		// Neither slime nor BG sea serpent?
		if (!npc->get_info().has_strange_movement()) {
			auto frame =
			    static_cast<signed char>(npc->get_dir_framenum(Actor::ready_frame));
			npc->set_action(new Frames_actor_action(&frame, 1, delay));
		} else if (!npc->is_slime()) { // Sea serpent?
			signed char frames[] = {
				static_cast<signed char>(npc->get_dir_framenum(3)),
				static_cast<signed char>(npc->get_dir_framenum(2)),
				static_cast<signed char>(npc->get_dir_framenum(1))
			};
			npc->set_action(new Frames_actor_action(frames, sizeof(frames), delay));
		}
		npc->start(delay, delay);
		if (attack_target(npc, opponent, Tile_coord(-1, -1, 0), weapon_shape, true)) {
			// Strike but once at objects.
			Game_object *newtarg = safenpc->get_target();
			if (newtarg && !newtarg->as_actor())
				safenpc->set_target(nullptr);
			return;     // We may no longer exist!
		}
		break;
	}
	case fire: {        // Range weapon.
		failures = 0;
		state = approach;
		if (spellbook) {
			// Cast the spell.
			if (!spellbook->do_spell(npc, true))
				Combat_schedule::set_weapon();
		} else
			attack_target(npc, opponent, Tile_coord(-1, -1, 0), weapon_shape, true);

		int delay = 1;
		if (spellbook) {
			Usecode_script *scr = Usecode_script::find(npc);
			// Warning: assuming that the most recent script for the
			// actor is the spellcasting script.
			delay += (scr ? scr->get_length() : 0) + 2;
		}
		delay *= gwin->get_std_delay();
		// Change back to ready frame.
		// Neither slime nor BG sea serpent?
		if (!npc->get_info().has_strange_movement()) {
			auto frame =
			    static_cast<signed char>(npc->get_dir_framenum(Actor::ready_frame));
			npc->set_action(new Frames_actor_action(&frame, 1, delay));
		} else if (!npc->is_slime()) { // Sea serpent?
			signed char frames[] = {
				static_cast<signed char>(npc->get_dir_framenum(3)),
				static_cast<signed char>(npc->get_dir_framenum(2)),
				static_cast<signed char>(npc->get_dir_framenum(1))
			};
			npc->set_action(new Frames_actor_action(frames, sizeof(frames), delay));
		}
		npc->start(gwin->get_std_delay(), delay);

		// Strike but once at objects.
		Game_object *newtarg = npc->get_target();
		if (newtarg && !newtarg->as_actor()) {
			npc->set_target(nullptr);
			return;     // We may no longer exist!
		}
		break;
	}
	case wait_return:       // Boomerang should have returned.
		state = approach;
		dex_points += npc->get_property(Actor::dexterity);
		npc->start_std();
		break;
	default:
		break;
	}
	if (failures > 5 && npc != gwin->get_camera_actor()) {
		// Too many failures.  Give up for now.
		if (combat_trace) {
			cout << npc->get_name() << " is giving up" << endl;
		}
		if (npc->get_party_id() >= 0) {
			// Party member.
			npc->walk_to_tile(
			    gwin->get_main_actor()->get_tile(),
			    gwin->get_std_delay());
			// WARNING:  Destroys ourself.
			npc->set_schedule_type(Schedule::follow_avatar);
		} else if (!gwin->get_game_rect().intersects(
		               gwin->get_shape_rect(npc))) {
			// Off screen?  Stop trying.
			gwin->get_tqueue()->remove(npc);
			npc->set_dormant();
		} else if (npc->get_alignment() == Actor::good &&
		           prev_schedule != Schedule::combat) {
			// Return to normal schedule.
			npc->update_schedule(gclock->get_hour() / 3);
			if (npc->get_schedule_type() == Schedule::combat)
				npc->set_schedule_type(prev_schedule);
		} else {
			// Wander randomly.
			Tile_coord t = npc->get_tile();
			int dist = 2 + rand() % 3;
			int newx = t.tx - dist + rand() % (2 * dist);
			int newy = t.ty - dist + rand() % (2 * dist);
			// Wait a bit.
			npc->walk_to_tile(newx, newy, t.tz,
			                  2 * gwin->get_std_delay(), rand() % 1000);
		}
	}
}

/*
 *  Npc just went dormant (probably off-screen).
 */

void Combat_schedule::im_dormant(
) {
	if (npc->get_effective_alignment() == Actor::good &&
	        prev_schedule != npc->get_schedule_type() && npc->is_monster())
		// Good, so end combat.
		npc->set_schedule_type(prev_schedule);
}

/*
 *  Leaving combat.
 */

void Combat_schedule::ending(
    int /* newtype */
) {
	Game_window *gwin = Game_window::get_instance();
	if (gwin->get_main_actor() == npc &&
	        // Not if called from usecode.
	        !gwin->get_usecode()->in_usecode()) {
		// See if being a coward.
		find_opponents();
		bool found = false; // Find a close-by enemy.
		for (auto& opponent : opponents) {
			Actor_shared opp = std::static_pointer_cast<Actor>(opponent.lock());
			if (opp && opp->distance(npc) < (c_screen_tile_size / 2 - 2) &&
			        Fast_pathfinder_client::is_grabable(npc, opp.get())) {
				found = true;
				break;
			}
		}
		if (found)
			Audio::get_ptr()->start_music_combat(CSRun_Away,
			                                     false);
	}
}


/*
 *  Create duel schedule.
 */

Duel_schedule::Duel_schedule(
    Actor *n
) : Combat_schedule(n, duel), start(n->get_tile()),
	attacks(0) {
	started_battle = true;      // Avoid playing music.
}

/*
 *  Ready a bow-and-arrows.
 */

void Ready_duel_weapon(
    Actor *npc,
    int wshape,         // Weapon shape.
    int ashape          // Ammo shape, or -1.
) {
	Game_map *gmap = Game_window::get_instance()->get_map();
	Game_object *weap = npc->get_readied(lhand);
	if (!weap || weap->get_shapenum() != wshape) {
		// Need a bow.
		Game_object_shared newweap;
		Game_object_shared keep;
		Game_object_shared newkeep;
		Game_object *found =
		    npc->find_item(wshape, c_any_qual, c_any_framenum);
		if (found) {        // Have it?
		    newweap = found->shared_from_this();
			newweap->remove_this(&newkeep);
		} else            // Create new one.
			newweap = gmap->create_ireg_object(wshape, 0);
		if (weap)       // Remove old item.
			weap->remove_this(&keep);
		npc->add(newweap.get(), true);   // Should go in correct spot.
		if (weap)
			npc->add(weap, true);
	}
	if (ashape == -1)       // No ammo needed.
		return;
	// Now provide 1-3 arrows.
	Game_object *aobj = npc->get_readied(quiver);
	if (aobj)
		aobj->remove_this();    // Toss current ammo.
	Game_object_shared arrows = gmap->create_ireg_object(ashape, 0);
	int extra = rand() % 3;     // Add 1 or 2.
	if (extra)
		arrows->modify_quantity(extra);
	npc->add(arrows.get(), true);        // Should go to right spot.
}

/*
 *  Find dueling opponents.
 */

void Duel_schedule::find_opponents(
) {
	opponents.clear();
	attacks = 0;
	practice_target = nullptr;
	int r = rand() % 3;
	if (r == 0) {       // First look for practice targets.
		// Archery target:
		practice_target = npc->find_closest(735);
		if (practice_target)    // Need bow-and-arrows.
			Ready_duel_weapon(npc, 597, 722);
	}
	if (!practice_target) {     // Fencing dummy or dueling opponent.
		Ready_duel_weapon(npc, 602, -1);
		if (r == 1)
			practice_target = npc->find_closest(860);
	}
	Combat_schedule::set_weapon();
	if (practice_target) {
		npc->set_target(practice_target);
		return;         // Just use that.
	}
	Actor_vector vec;       // Find all nearby NPC's.
	npc->find_nearby_actors(vec, c_any_shapenum, 24);
	for (auto *opp : vec) {
		Game_object *oppopp = opp->get_target();
		if (opp != npc && opp->get_schedule_type() == duel &&
		        (!oppopp || oppopp == npc))
			opponents.push_back(opp->weak_from_this());
	}
}

/*
 *  Previous action is finished.
 */

void Duel_schedule::now_what(
) {
	if (state == strike || state == fire) {
		attacks++;
		// Practice target full?
		if (practice_target && practice_target->get_shapenum() == 735
		        && practice_target->get_framenum() > 0 &&
		        practice_target->get_framenum() % 3 == 0) {
			attacks = 0;    // Break off.
			//++++++Should walk there.
			practice_target->change_frame(0);
		}
	} else {
		Combat_schedule::now_what();
		return;
	}
	if (attacks % 8 == 0) { // Time to break off.
		npc->set_target(nullptr);
		Tile_coord pos = start;
		pos.tx += rand() % 24 - 12;
		pos.ty += rand() % 24 - 12;
		// Find a free spot.
		Tile_coord dest = Map_chunk::find_spot(pos, 3, npc, 1);
		if (dest.tx == -1 || !npc->walk_path_to_tile(dest,
		        gwin->get_std_delay(), rand() % 2000))
			// Failed?  Try again a little later.
			npc->start(250, rand() % 3000);
	} else
		Combat_schedule::now_what();
}

/*
 *  Pause/unpause while in combat.
 */

void Combat::toggle_pause(
) {
	if (!paused && mode == original)
		return;         // Not doing that sort of thing.
	if (paused) {
		resume();       // Text is probably for debugging.
		eman->center_text("Combat resumed");
	} else {
		gwin->get_tqueue()->pause(SDL_GetTicks());
		eman->center_text("Combat paused");
		paused = true;
	}
}

/*
 *  Resume.
 */

void Combat::resume(
) {
	if (!paused)
		return;
	gwin->get_tqueue()->resume(SDL_GetTicks());
	paused = false;
}
