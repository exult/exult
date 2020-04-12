/*
Copyright (C) 2001-2013 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "Face_stats.h"
#include "Face_button.h"
#include "Paperdoll_gump.h"
#include "actors.h"
#include "party.h"
#include "exult_flx.h"
#include "gamewin.h"
#include "game.h"
#include "Gump_manager.h"
#include "combat_opts.h"
#include "ignore_unused_variable_warning.h"

#define PALETTE_INDEX_RED   22
#define PALETTE_INDEX_GREEN 64
#define PALETTE_INDEX_BLUE  79

#define PORTRAIT_NUM_MODES  3

#define PORTRAIT_WIDTH      40

class Stat_bar : public Gump_button {
	Actor       *actor;
	int     prop;
	int     prop_max;
	unsigned char   colour;

	int     val;
	int     max_val;
public:
	Stat_bar(Gump *par, int px, int py, Actor *a, int s, int m, unsigned char c);
	void double_clicked(int x, int y) override;
	void paint() override;

	bool activate(int button) override {
		return button == 1;
	}
	bool push(int button) override {
		return button == 1;
	}
	void unpush(int button) override {
		ignore_unused_variable_warning(button);
	}
	// update dirty region, if required
	void update_widget() override;

	bool is_draggable() override {
		return false;
	}
};

Stat_bar::Stat_bar(Gump *par, int px, int py, Actor *a, int s, int m, unsigned char c)
	: Gump_button(par, EXULT_FLX_HP_BAR_SHP, px, py, SF_EXULT_FLX),
	  actor(a), prop(s), prop_max(m), colour(c), val(-256), max_val(-256) {

	gwin->add_dirty(get_rect());
	val = actor->get_effective_prop(prop);
	max_val = actor->get_effective_prop(prop_max);
}

void Stat_bar::double_clicked(int x, int y) {
	ignore_unused_variable_warning(x, y);
	gumpman->add_gump(actor, game->get_shape("gumps/statsdisplay"));
}



/*
 *  Paint on screen.
 */

void Stat_bar::paint(
) {
	Gump_button::paint();

	int width = (val * 32) / (max_val ? max_val : 1);

	if (width > 0) {
		if (width > 32) width = 32;

		int px = x;
		int py = y;

		if (parent) {
			px += parent->get_x();
			py += parent->get_y();
		}

		gwin->get_win()->fill8(colour, width, 3, px, py);
	}
}

/*
 *  Update Dirty Region
 */

void Stat_bar::update_widget() {
	if (val != actor->get_effective_prop(prop) || max_val != actor->get_effective_prop(prop_max))
		gwin->add_dirty(get_rect());

	val = actor->get_effective_prop(prop);
	max_val = actor->get_effective_prop(prop_max);
}

/*
 *  Portrait_button
 */

class Portrait_button : public Face_button {
protected:
	Stat_bar    *hp;        // Bar for HP
	Stat_bar    *mana;      // Bar for MANA
	bool        hit;
	bool        pois;
	bool        prot;
	bool        para;
	bool        charm;
	bool        curse;
public:
	Portrait_button(Gump *par, int px, int py, Actor *a);
	~Portrait_button() override;
	void double_clicked(int x, int y) override;
	void paint() override;

	// update dirty region, if required
	void update_widget() override;

	bool on_button(int mx, int my) const override;

	Rectangle get_rect() override;

	bool is_draggable() override {
		return false;
	}
};


Portrait_button::Portrait_button(Gump *par, int px, int py, Actor *a)
	: Face_button(par, px + 14, py - 20, a), hp(nullptr), mana(nullptr) {
	hp = new Stat_bar(par, px + 4, py - 10, a, Actor::health, Actor::strength, PALETTE_INDEX_RED);

	if (actor->get_effective_prop(Actor::magic) > 0)
		mana = new Stat_bar(par, px + 4, py - 5, a, Actor::mana, Actor::magic, PALETTE_INDEX_BLUE);

	hit = actor->was_hit();
	pois = actor->get_flag(Obj_flags::poisoned);
	prot = actor->get_flag(Obj_flags::protection);
	para = actor->get_flag(Obj_flags::paralyzed);
	charm = actor->get_flag(Obj_flags::charmed);
	curse = actor->get_flag(Obj_flags::cursed);

	gwin->add_dirty(get_rect());
}

Portrait_button::~Portrait_button() {
	delete hp;
	delete mana;
}

void Portrait_button::double_clicked(int x, int y) {
	if (hp && hp->on_button(x, y))
		hp->double_clicked(x, y);
	else if (mana && mana->on_button(x, y))
		mana->double_clicked(x, y);
	else if (actor->can_act_charmed())
		actor->show_inventory();
}

bool Portrait_button::on_button(int x, int y) const {
	if (hp && hp->on_button(x, y))
		return true;
	else if (mana && mana->on_button(x, y))
		return true;
	else if (Face_button::on_button(x, y))
		return true;

	return false;
}

void Portrait_button::update_widget() {
	Face_button::update_widget();
	if (hp)
		hp->update_widget();
	if (mana)
		mana->update_widget();

	if (hit != actor->was_hit() ||
	        pois != actor->get_flag(Obj_flags::poisoned) ||
	        prot != actor->get_flag(Obj_flags::protection) ||
	        para != actor->get_flag(Obj_flags::paralyzed) ||
	        charm != actor->get_flag(Obj_flags::charmed) ||
	        curse != actor->get_flag(Obj_flags::cursed)) {
		Rectangle r = get_rect();
		gwin->add_dirty(r);
		hit = actor->was_hit();
		pois = actor->get_flag(Obj_flags::poisoned);
		prot = actor->get_flag(Obj_flags::protection);
		para = actor->get_flag(Obj_flags::paralyzed);
		charm = actor->get_flag(Obj_flags::charmed);
		curse = actor->get_flag(Obj_flags::cursed);
		r = get_rect();
		gwin->add_dirty(r);
	}
}

/*
 *  Paint on screen.
 */

void Portrait_button::paint() {
	Face_button::paint();

	Shape_frame *s = get_shape();

	if (s) {
		int px = x;
		int py = y;

		if (parent) {
			px += parent->get_x();
			py += parent->get_y();
		}

		if (hit) {
			sman->paint_outline(px, py, s, HIT_PIXEL);
		} else if (charm)
			sman->paint_outline(px, py, s, CHARMED_PIXEL);
		else if (para) {
			sman->paint_outline(px, py, s, PARALYZE_PIXEL);
		} else if (prot) {
			sman->paint_outline(px, py, s, PROTECT_PIXEL);
		} else if (curse)
			sman->paint_outline(px, py, s, CURSED_PIXEL);
		else if (pois) {
			sman->paint_outline(px, py, s, POISON_PIXEL);
		}
	}

	if (hp)
		hp->paint();
	if (mana)
		mana->paint();
}

Rectangle Portrait_button::get_rect() {
	Rectangle rect = Face_button::get_rect();
	if (hit || pois || prot || para || charm || curse)
		rect.enlarge(2);

	if (hp) {
		Rectangle r = hp->get_rect();
		rect = rect.add(r);
	}
	if (mana) {
		Rectangle r = mana->get_rect();
		rect = rect.add(r);
	}

	return rect;
}

/*
 *  Face_stats
 */

Face_stats::Face_stats() : Gump(nullptr, 0, 0, 0, SF_GUMPS_VGA) {
	for (int i = 1; i < 8; i++) {
		npc_nums[i] = -1;
		party[i] = nullptr;
	}


	create_buttons();
}

Face_stats::~Face_stats() {
	delete_buttons();

	gwin->set_all_dirty();
	self = nullptr;
}

/*
 *  Paint on screen.
 */

void Face_stats::paint(
) {
	for (int i = 0; i < 8; i++)
		if (party[i]) party[i]->paint();
}

/*
 *  On a Button?
 */

Gump_button *Face_stats::on_button(int mx, int my) {
	for (int i = 0; i < 8; i++)
		if (party[i] && party[i]->on_button(mx, my))
			return party[i];

	return nullptr;
}

// add dirty region, if dirty
void Face_stats::update_gump() {
	for (int i = 0; i < 8; i++)
		if (party[i]) party[i]->update_widget();
}

// Delete all the buttons
void Face_stats::delete_buttons() {
	gwin->add_dirty(get_rect());

	for (int i = 0; i < 8; i++) {
		if (party[i]) {
			delete party[i];
			party[i] = nullptr;
		}
		npc_nums[i] = -1;
	}

	resx = 0;
	resy = 0;
	gamex = 0;
	gamey = 0;
}

void Face_stats::create_buttons() {
	int i;
	int pos = 0;
	int width = PORTRAIT_WIDTH;

	resx = gwin->get_win()->get_full_width();
	resy = gwin->get_win()->get_full_height();
	gamex = gwin->get_game_width();
	gamey = gwin->get_game_height();
	x = 0;
	y = gwin->get_win()->get_end_y();

	party_size = partyman->get_count();

	int num_to_paint = 0;

	for (i = 0; i < party_size; i++) {
		int num = partyman->get_member(i);
		Actor *act = gwin->get_npc(num);
		assert(act != nullptr);
		// Show faces if in SI, or if paperdolls are allowed
		if (sman->can_use_paperdolls() ||
		        // Otherwise, show faces also if the character
		        // has paperdoll information
		        act->get_info().get_npc_paperdoll())
			++num_to_paint;
	}

	if (mode == 0)
		pos = 0;
	else if (mode == 1)
		pos = (resx - (num_to_paint + 1) * PORTRAIT_WIDTH) / 2;
	else if (mode == 2) {
		pos = resx - PORTRAIT_WIDTH;
		width = - PORTRAIT_WIDTH;
	}

	pos += gwin->get_win()->get_start_x();

	std::memset(party, 0, sizeof(party));

	party[0] = new Portrait_button(this, pos, 0, gwin->get_main_actor());

	for (i = 0; i < party_size; i++) {
		npc_nums[i + 1] = partyman->get_member(i);
		Actor *act = gwin->get_npc(npc_nums[i + 1]);
		assert(act != nullptr);
		// Show faces if in SI, or if paperdolls are allowed
		if (sman->can_use_paperdolls() ||
		        // Otherwise, show faces also if the character
		        // has paperdoll information
		        act->get_info().get_npc_paperdoll()) {
			pos += width;
			party[i + 1] = new Portrait_button(this, pos, 0, gwin->get_npc(npc_nums[i + 1]));
		} else {
			party[i + 1] = nullptr;
		}
	}

	region.x = region.y = region.w = region.h = 0;

	for (i = 0; i < 8; i++)
		if (party[i]) {
			Rectangle r = party[i]->get_rect();
			region = region.add(r);
		}
}

bool Face_stats::has_point(int x, int y) const {
	for (int i = 0; i < 8; i++)
		if (party[i] && party[i]->on_button(x, y))
			return true;

	return false;
}

/*
 *  Add an object.  If mx, my, sx, sy are all -1, the object's position
 *  is calculated by 'paint()'.  If they're all -2, it's assumed that
 *  obj->cx, obj->cy are already correct.
 *
 *  Output: false if cannot add it.
 */

bool Face_stats::add(
    Game_object *obj,
    int mx, int my,         // Mouse location.
    int sx, int sy,         // Screen location of obj's hotspot.
    bool dont_check,        // Skip volume check.
    bool combine            // True to try to combine obj.  MAY
    //   cause obj to be deleted.
) {
	if (sx < 0 && sy < 0 && my < 0 && mx < 0)
		return false;

	for (int i = 0; i < 8; i++)
		if (party[i] && party[i]->on_button(mx, my))
			return party[i]->get_actor()->add(obj, dont_check, combine);

	return false;
}

Container_game_object *Face_stats::find_actor(int mx, int my) {
	for (int i = 0; i < 8; i++) if (party[i] && party[i]->on_button(mx, my))
		return party[i]->get_actor();

	return nullptr;
}

// Statics

int Face_stats::mode = 0;
Face_stats *Face_stats::self = nullptr;

// Creates if doesn't already exist
void Face_stats::CreateGump() {
	if (!self) {
		self = new Face_stats();
		gumpman->add_gump(self);
	}
}

// Removes is exists
void Face_stats::RemoveGump() {
	if (self)
		gumpman->close_gump(self);
}

// Increments the state of the gump
void Face_stats::AdvanceState() {
	if (!self)
		CreateGump();
	else {
		RemoveGump();

		mode++;
		mode %= PORTRAIT_NUM_MODES;
		if (mode)
			CreateGump();
	}
}

// Used for updating mana bar when magic value changes and when gwin resizes
void Face_stats::UpdateButtons() {
	if (!self) {
		return;
	} else {
		self->delete_buttons();
		self->create_buttons();
	}
}

void Face_stats::save_config(Configuration *config) {
	config->set("config/gameplay/facestats", self ? mode : -1, true);
}

void Face_stats::load_config(Configuration *config) {
	int nmode;
	config->value("config/gameplay/facestats", nmode, -1);
	if (self)
		RemoveGump();
	if (nmode >= 0) {
		mode = nmode % PORTRAIT_NUM_MODES;
		CreateGump();
	}
}
