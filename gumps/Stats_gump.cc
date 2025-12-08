/*
Copyright (C) 2000-2024 The Exult Team

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
#	include <config.h>
#endif

#include "Stats_gump.h"

#include "Gump_manager.h"
#include "actors.h"
#include "exult_flx.h"
#include "game.h"
#include "gamewin.h"
#include "misc_buttons.h"

/*
 *  Some gump shape numbers:
 */

const int ASLEEP = 0, POISONED = 1, CHARMED = 2, HUNGRY = 3, PROTECTED = 4,
		  CURSED = 5, PARALYZED = 6;

/*
 *  Statics:
 */

short     Stats_gump::textx     = 123;
short     Stats_gump::texty[10] = {17, 26, 35, 46, 55, 67, 76, 86, 95, 104};
const int num_extra_spots       = 10;
const int att_name_textx        = 32;

/*
 *  A secondary gump for showing custom attributes (for mods, new games).
 */
class Stats_extra_gump : public Stats_gump {
	int                order;    // 1, 2, 3 for the NPC.
	Actor::Atts_vector atts;

public:
	Stats_extra_gump(
			Container_game_object* cont, int initx, int inity,
			Actor::Atts_vector& allatts, int first, int ord)
			: Stats_gump(
					  cont, initx, inity, EXULT_FLX_STATS_EXTRA_SHP,
					  SF_EXULT_FLX),
			  order(ord) {
		int cnt = allatts.size() - first;
		if (cnt > num_extra_spots) {
			cnt = num_extra_spots;
		}
		atts.resize(cnt);
		for (int i = first; i < cnt; ++i) {
			atts[i] = allatts[first + i];
		}
	}

	void paint() override;
};

/*
 *  Paint on screen.
 */

void Stats_extra_gump::paint() {
	const int     font = 2;
	Gump_manager* gman = gumpman;

	// Area to print name in.
	const int namex = 30;
	const int namey = 6;
	const int namew = 95;
	Actor*    act   = get_actor();
	// Paint the gump itself.
	paint_shape(x, y);
	// Paint red "checkmark".
	paint_elems();
	// Show statistics.
	std::string nm = act->get_name();
	char        buf[20];
	snprintf(buf, sizeof(buf), "(%d)", order + 1);
	nm += buf;
	sman->paint_text(
			2, nm.c_str(),
			x + namex + (namew - sman->get_text_width(2, nm.c_str())) / 2,
			y + namey);
	const int cnt = atts.size();
	for (int i = 0; i < cnt; ++i) {
		std::string attnm(atts[i].first);
		attnm += ':';
		sman->paint_text(font, attnm.c_str(), x + att_name_textx, y + texty[i]);
		gman->paint_num(atts[i].second, x + textx, y + texty[i]);
	}
}

/*
 *  Show one of the atts.
 *
 *  Output: Amount to increment x-pos for the next one.
 */

static int Show_atts(
		int x, int y,    // Pos. on screen.
		int framenum) {
	Shape_manager* sman = Shape_manager::get_instance();
	const ShapeID  sid(
            game->get_shape("gumps/statatts"), framenum, SF_GUMPS_VGA);
	Shape_frame* s = sid.get_shape();
	sman->paint_shape(x + s->get_xleft(), y + s->get_ybelow(), s, false);
	return s->get_width() + 2;
}

/*
 *  Create stats display.
 */
Stats_gump::Stats_gump(Container_game_object* cont, int initx, int inity)
		: Gump(cont, initx, inity, game->get_shape("gumps/statsdisplay")) {
	set_object_area(TileRect(0, 0, 0, 0), 22, 124);
}

Stats_gump::Stats_gump(
		Container_game_object* cont, int initx, int inity, int shnum,
		ShapeFile shfile)
		: Gump(cont, initx, inity, shnum, shfile) {
	set_object_area(TileRect(0, 0, 0, 0), 22, 124);
}

/*
 *  Create for given NPC after first creating one of more for the 'extra'
 *  stats.
 */

Stats_gump* Stats_gump::create(Game_object* npc_obj, int x, int y) {
	Actor* npc = npc_obj->as_actor();
	assert(npc != nullptr);
	Actor::Atts_vector atts;
	npc->get_attributes(atts);
	const int sz  = atts.size();
	const int num = (sz + num_extra_spots - 1) / num_extra_spots;
	// Create going backwards.
	for (int i = num - 1; i >= 0; --i) {
		const int first = i * num_extra_spots;
		auto*     egmp  = new Stats_extra_gump(
                npc, x - 5 * (i + 1), y - 5 * (i + 1), atts, first, i + 1);
		gumpman->add_gump(egmp);
	}
	return new Stats_gump(npc, x, y);
}

/*
 *  Paint on screen.
 */

void Stats_gump::paint() {
	Gump_manager* gman = gumpman;

	// Area to print name in.
	const int namex = 30;
	const int namey = 6;
	const int namew = 95;
	Actor*    act   = get_actor();    // Check for freezing (SI).
	if (gwin->get_main_actor()->get_flag(Obj_flags::freeze)) {
		int frame = act->get_temperature_zone();
		if (get_num_frames() < frame + 1) {    // Prevent problems in BG.
			frame = get_num_frames() - 1;
		}
		if (frame < 0) {
			frame = 0;
		}
		set_frame(frame);
	}
	// Paint the gump itself.
	paint_shape(x, y);
	// Paint red "checkmark".
	paint_elems();
	// Show statistics.
	const std::string nm = act->get_name();
	sman->paint_text(
			2, nm.c_str(),
			x + namex + (namew - sman->get_text_width(2, nm.c_str())) / 2,
			y + namey);
	if (gwin->failed_copy_protection()) {
		const int oinkx = 96;
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[0]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[1]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[2]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[3]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[4]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[5]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[6]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[7]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[8]);
		sman->paint_text(2, "Oink!", x + oinkx, y + texty[9]);
	} else {
		gman->paint_num(
				act->get_effective_prop(Actor::strength), x + textx,
				y + texty[0]);
		gman->paint_num(
				act->get_effective_prop(Actor::dexterity), x + textx,
				y + texty[1]);
		gman->paint_num(
				act->get_effective_prop(Actor::intelligence), x + textx,
				y + texty[2]);
		gman->paint_num(
				act->get_effective_prop(Actor::combat), x + textx,
				y + texty[3]);
		gman->paint_num(
				act->get_property(Actor::magic), x + textx, y + texty[4]);
		gman->paint_num(
				act->get_property(Actor::health), x + textx, y + texty[5]);
		gman->paint_num(
				act->get_property(Actor::mana), x + textx, y + texty[6]);
		gman->paint_num(act->get_property(Actor::exp), x + textx, y + texty[7]);
		gman->paint_num(act->get_level(), x + textx, y + texty[8]);
		gman->paint_num(
				act->get_property(Actor::training), x + textx, y + texty[9]);
	}
	// Now show atts. at bottom.
	const int attsy  = 130;
	const int attsx0 = 29;
	int       attsx  = attsx0;
	if (act->get_flag(Obj_flags::asleep)) {
		attsx += Show_atts(x + attsx, y + attsy, ASLEEP);
	}
	if (act->get_flag(Obj_flags::poisoned)) {
		attsx += Show_atts(x + attsx, y + attsy, POISONED);
	}
	if (act->get_flag(Obj_flags::charmed)) {
		attsx += Show_atts(x + attsx, y + attsy, CHARMED);
	}
	if (act->get_property(static_cast<int>(Actor::food_level)) <= 9) {
		attsx += Show_atts(x + attsx, y + attsy, HUNGRY);
	}
	if (act->get_flag(Obj_flags::protection)) {
		attsx += Show_atts(x + attsx, y + attsy, PROTECTED);
	}
	if (act->get_flag(Obj_flags::cursed)) {
		attsx += Show_atts(x + attsx, y + attsy, CURSED);
	}
	if (act->get_flag(Obj_flags::paralyzed)) {
		Show_atts(x + attsx, y + attsy, PARALYZED);
	}
}
