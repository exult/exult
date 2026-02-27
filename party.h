/**
 ** Party.h - Manage the party.
 **
 ** Written: 4/8/02 - JSF
 **/
/*  Copyright (C) 2002-2022  The Exult Team
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

#ifndef INCL_PARTY_H
#define INCL_PARTY_H 1

#include "singles.h"
#include "tiles.h"

#include <array>
#include <cstddef>

class Actor;

#define EXULT_PARTY_MAX 8

/*
 *  Manage the party.
 */
class Party_manager : public Game_singletons {
	// NPC #'s of party members.
	std::array<int, EXULT_PARTY_MAX> party;
	// # of NPC's in party.
	size_t party_count;
	// NPC #'s of dead party members.
	std::array<int, 16> dead_party;
	// # of dead NPC's in party.
	size_t dead_party_count;
	// NPC's able to walk with Avatar.
	std::array<Actor*, EXULT_PARTY_MAX> valid;
	// # of valid NPC's in party.
	size_t validcnt;
	// Formation-walking:
	void move_followers(Actor* npc, int vindex, int dir) const;
	int  step(Actor* npc, Actor* leader, int dir, const Tile_coord& dest) const;

public:
	Party_manager();

	void set_count(int n) {    // For initializing from file.
		party_count = n;
	}

	void set_member(int i, int npcnum) {
		party[i] = npcnum;
	}

	int get_count() const {    // Get # party members.
		return party_count;
	}

	int get_member(int i) const {    // Get npc# of i'th party member.
		return party[i];
	}

	int get_dead_count() const {    // Same for dead party members.
		return dead_party_count;
	}

	int get_dead_member(int i) const {
		return dead_party[i];
	}

	// A Bidirectional Iterator that iterates the Party as Actors
	class party_actor_iterator {
		using value_type = Actor*;
		int num;

	public:
		party_actor_iterator(int num) : num(num) {}

		Actor* operator*() {
			return partyman->get_actor(num);
		}

		Actor* operator->() {
			return partyman->get_actor(num);
		}

		party_actor_iterator& operator++(int) {
			++num;
			return *this;
		}

		party_actor_iterator operator++() {
			return party_actor_iterator(num++);
		}

		party_actor_iterator& operator--(int) {
			--num;
			return *this;
		}

		party_actor_iterator operator--() {
			return party_actor_iterator(num--);
		}

		bool operator==(const party_actor_iterator& other) {
			return num == other.num;
		}

		bool operator!=(const party_actor_iterator& other) {
			return num != other.num;
		}

		bool operator<(const party_actor_iterator& other) {
			return num < other.num;
		}

		bool operator<=(const party_actor_iterator& other) {
			return num < other.num;
		}
	};

	party_actor_iterator begin() const {
		return party_actor_iterator(0);
	}

	party_actor_iterator end() const {
		return party_actor_iterator(get_count());
	}

	// Iterate the party including the MainActor
	struct {
		party_actor_iterator begin() const {
			return party_actor_iterator(-1);
		}

		party_actor_iterator end() const {
			return party_actor_iterator(partyman->get_count());
		}

	} IterateWithMainActor;

	// Get Party Member as Actor*, -1 will return Main Actor. Out of range
	// returns nullptr
	Actor* get_actor(int i) const;

	// Add/remove party member.
	bool add_to_party(Actor* npc);
	bool remove_from_party(Actor* npc);
	int  in_dead_party(Actor* npc) const;
	bool add_to_dead_party(Actor* npc);
	bool remove_from_dead_party(Actor* npc);
	// Update status of NPC that died or
	//   was resurrected.
	void update_party_status(Actor* npc);
	void link_party();    // Set party's id's.
	// Formation-walking:
	void get_followers(int dir);
};

#endif
