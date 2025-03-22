/*
 *  path.cc - Pathfinding algorithms.
 *
 *  Copyright (C) 2000-2022  The Exult Team
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
#	include <config.h>
#endif

#include "path.h"

#include "PathFinder.h"
#include "exult_constants.h"
#include "hash_utils.h"

#include <iostream>
#include <vector>

using std::cout;
using std::endl;
using std::size_t;
using std::vector;

/*
 *  Iterate through neighbors of a tile (in 2 dimensions).
 */
class Neighbor_iterator {
	Tile_coord tile;          // Original tile.
	static int coords[16];    // Coords to go through ((x,y) pairs)
	int        index{};       // 0-7.
public:
	Neighbor_iterator(const Tile_coord& t) : tile(t) {}

	// Get next neighbor.
	int operator()(Tile_coord& newt) noexcept {
		if (index < 8) {
			newt = Tile_coord(
					tile.tx + coords[2 * index],
					tile.ty + coords[2 * index + 1], tile.tz);
			index++;
			// Handle world-wrapping.
			newt.tx = (newt.tx + c_num_tiles) % c_num_tiles;
			newt.ty = (newt.ty + c_num_tiles) % c_num_tiles;
			return 1;
		}
		return 0;
	}
};

/*
 *  Statics:
 */
int Neighbor_iterator::coords[16]
		= {-1, -1, 0, -1, 1, -1, -1, 0, 1, 0, -1, 1, 0, 1, 1, 1};

/*
 *  A node for our search:
 */
class Search_node {
	Tile_coord   tile;             // The coords (x, y, z) in tiles.
	short        start_cost;       // Actual cost from start.
	short        goal_cost;        // Estimated cost to goal.
	short        total_cost;       // Sum of the two above.
	Search_node* parent;           // Prev. in path.
	Search_node* priority_next;    // ->next with same total_cost, or
								   //   nullptr if not in 'open' set.
public:
	Search_node(const Tile_coord& t, short scost, short gcost, Search_node* p)
			: tile(t), start_cost(scost), goal_cost(gcost), parent(p),
			  priority_next(nullptr) {
		total_cost = gcost + scost;
	}

	// For creating a key to search for.
	Search_node(const Tile_coord& t) : tile(t) {}

	Tile_coord get_tile() const {
		return tile;
	}

	int get_start_cost() {
		return start_cost;
	}

	int get_goal_cost() {
		return goal_cost;
	}

	int get_total_cost() {
		return total_cost;
	}

	bool is_open() {    // In 'open' priority queue?
		return priority_next != nullptr;
	}

	void update(short scost, short gcost, Search_node* p) {
		start_cost = scost;
		goal_cost  = gcost;
		total_cost = gcost + scost;
		parent     = p;
	}

	// Create path back to start.
	std::vector<Tile_coord> create_path() {
		size_t pathlen = 0;    // Start at 0 as we don't want starting tile.
		// Count back to start.
		Search_node* each = this;
		while ((each = each->parent) != nullptr) {
			pathlen++;
		}
		std::vector<Tile_coord> result(pathlen);
		each = this;
		for (size_t i = pathlen; i > 0; i--) {
			result[i - 1] = each->tile;
			each          = each->parent;
		}
		return result;
	}
#ifdef VERIFYCHAIN
	// Returns false if bad chain.
	bool verify_chain(Search_node* last, bool removed = false) {
		if (last == nullptr) {
			return true;
		}
		bool         found = false;
		Search_node* prev  = last;
		size_t       cnt   = 0;
		do {
			Search_node* next = prev->priority_next;
			if (next == this) {
				found = true;
			}
			prev = next;
			if (cnt > 10000) {
				break;
			}
		} while (prev != last);
		if (!found && !removed) {
			return false;
		}
		if (cnt == 10000) {
			return false;
		}
		return true;
	}
#endif
	// Add to chain of same priorities.
	void add_to_chain(Search_node*& last) {
		if (last) {
			priority_next       = last->priority_next;
			last->priority_next = this;
		} else {
			last          = this;
			priority_next = this;
		}
#ifdef VERIFYCHAIN
		if (!verify_chain(last)) {
			cout << "Bad chain after adding." << endl;
		}
#endif
	}

	// Remove this from its chain.
	void remove_from_chain(Search_node*& last) {
#ifdef VERIFYCHAIN
		if (!verify_chain(last)) {
			cout << "Bad chain before removing." << endl;
		}
#endif
		if (priority_next == this) {
			// Only one in chain?
			last = nullptr;
		} else {
			// Got to find prev. to this.
			Search_node* prev = last;
			do {
				Search_node* next = prev->priority_next;
				if (next == this) {
					break;
				}
				prev = next;
			} while (prev != last);
			if (prev) {
				prev->priority_next = priority_next;
				if (last == this) {
					last = priority_next;
				}
			}
		}
		priority_next = nullptr;    // No longer in 'open'.
#ifdef VERIFYCHAIN
		if (!verify_chain(last, 1)) {
			cout << "Bad chain after removing." << endl;
		}
#endif
	}

	// Remove 1st from a priority chain.
	static Search_node* remove_first_from_chain(Search_node*& last) {
		Search_node* first = last->priority_next;
		if (first == last) {    // Last entry?
			last = nullptr;
		} else {
			last->priority_next = first->priority_next;
		}
		first->priority_next = nullptr;
		return first;
	}
};

/*
 *  Hash function for nodes:
 */
class Hash_node {
public:
	size_t operator()(const Search_node* a) const noexcept {
		const Tile_coord t = a->get_tile();
		return (t.tz << 24) + (t.ty << 12) + t.tx;
	}
};

/*
 *  For testing if two nodes match.
 */
class Equal_nodes {
public:
	bool operator()(const Search_node* a, const Search_node* b) const noexcept {
		const Tile_coord ta = a->get_tile();
		const Tile_coord tb = b->get_tile();
		return ta == tb;
	}
};

/*
 *  The priority queue for the A* algorithm:
 */
class A_star_queue {
	vector<Search_node*> open;    // Nodes to be done, by priority. Each
	//   is a ->last node in chain.
	int best;    // Index of 1st non-null ent. in open.
	// For finding each tile's node:
	using Lookup_set = std::unordered_set<Search_node*, Hash_node, Equal_nodes>;
	Lookup_set lookup;

public:
	A_star_queue() : open(256), lookup(1000) {
		open.insert(open.begin(), 256, nullptr);
		best = open.size();    // Best is past end.
	}

	~A_star_queue() {
		/*
		This _should_ work, but might hang some hash_set implementations.
		The problem is that on deleting the Search_node, the hash_set can
		no longer properly evaluate the hash value (since the hash function
		dereferences the Search_node* stored). This might cause an endless
		loop.
		*/
		for (auto X = lookup.begin(); X != lookup.end();) {
			Search_node* sn = *X;
			++X;
			delete sn;    // only delete this _after_ iterating
		}
		lookup.clear();    // Remove all nodes.
	}

	void add_open(int pri, Search_node* nd) {
		if (pri >= static_cast<int>(open.size())) {
			open.resize(pri + 2);
		}
		open[pri] = nd;
	}

	void add_back(Search_node* nd) {    // Add an existing node back to 'open'.
		const int    total_cost = nd->get_total_cost();
		Search_node* last       = total_cost < static_cast<int>(open.size())
										  ? open[total_cost]
										  : nullptr;
		nd->add_to_chain(last);    // Add node to this chain.
		add_open(total_cost, last);
		if (total_cost < best) {
			best = total_cost;
		}
	}

	void add(Search_node* nd) {    // Add new node to 'open' set.
		lookup.insert(nd);
		add_back(nd);
	}

	// Remove node from 'open' set.
	void remove_from_open(Search_node* nd) {
		if (!nd->is_open()) {
			return;    // Nothing to do.
		}
		const int    total_cost = nd->get_total_cost();
		Search_node* last       = total_cost < static_cast<int>(open.size())
										  ? open[total_cost]
										  : nullptr;
		if (last) {
			nd->remove_from_chain(last);
			// Store updated 'last'.
			add_open(total_cost, last);
		}
		if (!last) {    // Last in chain?
			if (total_cost == best) {
				const int cnt = open.size();
				for (best++; best < cnt; best++) {
					if (open[best] != nullptr) {
						break;
					}
				}
			}
		}
	}

	Search_node* pop() {    // Pop best from priority queue.
		Search_node* last
				= best < static_cast<int>(open.size()) ? open[best] : nullptr;
		if (!last) {
			return nullptr;
		}
		// Return 1st in list.
		Search_node* node = Search_node::remove_first_from_chain(last);
		// Store updated 'last'.
		add_open(best, last);
		if (!last) {    // List now empty?
			const int cnt = open.size();
			for (best++; best < cnt; best++) {
				if (open[best] != nullptr) {
					break;
				}
			}
		}
		return node;
	}

	// Find node for given tile.
	Search_node* find(const Tile_coord& tile) {
		Search_node        key(tile);
		Search_node* const pkey = &key;
		auto               it   = lookup.find(pkey);
		if (it != lookup.end()) {
			return *it;
		} else {
			return nullptr;
		}
	}
};

static bool tracing = false;

/*
 *  First cut at using the A* pathfinding algorithm.
 *
 *  Output: pair<path vector, flag> where flag is true if path found.
 */

std::pair<std::vector<Tile_coord>, bool> Find_path(
		const Tile_coord&        start,    // Where to start from.
		const Tile_coord&        goal,     // Where to end up.
		const Pathfinder_client* client    // Provides costs.
) {
	A_star_queue nodes;    // The priority queue & hash table.
	int          max_cost = client->estimate_cost(start, goal);
	// Create start node.
	nodes.add(new Search_node(start, 0, max_cost, nullptr));
	// Figure when to give up.
	max_cost = client->get_max_cost(max_cost);
	Search_node* node;    // Try 'best' node each iteration.
	while ((node = nodes.pop()) != nullptr) {
		if (tracing) {
			cout << "Goal: (" << goal.tx << ", " << goal.ty << ", " << goal.tz
				 << "), Node: (" << node->get_tile().tx << ", "
				 << node->get_tile().ty << ", " << node->get_tile().tz << ")"
				 << endl;
		}
		const Tile_coord curtile = node->get_tile();
		if (client->at_goal(curtile, goal)) {
			// Success.
			return {node->create_path(), true};
		}
		// Go through surrounding tiles.
		Neighbor_iterator get_next(curtile);
		Tile_coord        ntile(0, 0, 0);
		while (get_next(ntile)) {
			// Get cost to next tile.
			const int step_cost = client->get_step_cost(curtile, ntile);
			// Blocked?
			if (step_cost == -1) {
				continue;
			}
			// Get cost from start to ntile.
			const int new_cost = node->get_start_cost() + step_cost;
			// See if next tile already seen.
			Search_node* next = nodes.find(ntile);
			// Already there, and cheaper?
			if (next && next->get_start_cost() <= new_cost) {
				continue;
			}
			const int new_goal_cost = client->estimate_cost(ntile, goal);
			// Skip nodes too far away.
			if (new_cost + new_goal_cost >= max_cost) {
				continue;
			}
			if (!next) {    // Create if necessary.
				next = new Search_node(ntile, new_cost, new_goal_cost, node);
				nodes.add(next);
			} else {
				// It's going to move.
				nodes.remove_from_open(next);
				next->update(new_cost, new_goal_cost, node);
				nodes.add_back(next);
			}
		}
	}
	// Failed if here.
	return {{}, false};
}
