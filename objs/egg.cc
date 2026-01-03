/*
 *  Copyright (C) 1998-1999  Jeffrey S. Freedman
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

#include "egg.h"

#include "Audio.h"
#include "Gump_manager.h"
#include "animate.h"
#include "cheat.h"
#include "chunks.h"
#include "databuf.h"
#include "effects.h"
#include "exult.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "monsters.h"
#include "npctime.h"
#include "paths.h"
#include "shapeinf.h"
#include "tqueue.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "ucscriptop.h"
#include "weaponinf.h"

#include <memory>

#ifdef USE_EXULTSTUDIO
#	include "mouse.h"
#	include "objserial.h"
#	include "servemsg.h"
#	include "server.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::ostream;
using std::rand;
using std::string;

Game_object_shared Egg_object::editing;

/*
 *  Timer for a missile egg (type-6 egg).
 */
class Missile_launcher : public Time_sensitive, public Game_singletons {
	Egg_object* egg;         // Egg this came from.
	int         weapon;      // Shape for weapon.
	int         shapenum;    // Shape for missile.
	int         dir;         // Direction (0-7).  (8==??).
	int         delay;       // Delay (msecs) between launches.
	int         range;
	bool        chk_range;    // For party near and avatar near.
public:
	Missile_launcher(Egg_object* e, int weap, int shnum, int di, int del)
			: egg(e), weapon(weap), shapenum(shnum), dir(di), delay(del),
			  range(20) {
		const Weapon_info* winfo = ShapeID::get_info(weapon).get_weapon_info();
		// Guessing.
		if (winfo) {
			range = winfo->get_range();
		}
		const int crit = e->get_criteria();
		chk_range      = crit == Egg_object::party_near
					|| crit == Egg_object::avatar_near;
	}

	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Launch a missile.
 */

void Missile_launcher::handle_event(unsigned long curtime, uintptr udata) {
	const Tile_coord src = egg->get_tile();
	// Is egg a ways off the screen?
	if (!gwin->get_win_tile_rect().enlarge(10).has_world_point(
				src.tx, src.ty)) {
		return;    // Return w'out adding back to queue.
	}
	// Add back to queue for next time.
	if (chk_range) {
		Actor* main_actor = gwin->get_main_actor();
		if (!main_actor || main_actor->distance(egg) > range) {
			return;
		}
	}
	std::unique_ptr<Projectile_effect> proj{nullptr};
	if (dir < 8) {    // Direction given?
					  // Get adjacent tile in direction.
		const Tile_coord adj = src.get_neighbor(dir % 8);
		// Make it go (range) tiles.
		const int  dx   = adj.tx - src.tx;
		const int  dy   = adj.ty - src.ty;
		Tile_coord dest = src;
		dest.tx += range * dx;
		dest.ty += range * dy;
		proj = std::make_unique<Projectile_effect>(
				egg, dest, weapon, shapenum, shapenum);
	} else {    // Target a party member.
		Actor*    party[9];
		const int psize = gwin->get_party(party, 1);
		int       cnt   = psize;
		const int n     = rand() % psize;    // Pick one at random.
		// Find one we can hit.
		for (int i = n; !proj && cnt; cnt--, i = (i + 1) % psize) {
			if (Fast_pathfinder_client::is_straight_path(
						src, party[i]->get_tile())) {
				proj = std::make_unique<Projectile_effect>(
						src, party[i], weapon, shapenum, shapenum);
			}
		}
	}
	if (proj) {
		eman->add_effect(std::move(proj));
	}
	if (chk_range) {
		gwin->get_tqueue()->add(curtime + (delay > 0 ? delay : 1), this, udata);
	}
}

/*
 *  Each egg type:
 */
class Jukebox_egg : public Egg_object, public Time_sensitive {
protected:
	unsigned char score;
	bool          continuous;

public:
	Jukebox_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, 0),
			  score(d1 & 0xff), continuous(((d1 >> 8) & 1) != 0) {}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
#ifdef DEBUG
		cout << "Jukebox_egg::hatch_now Audio parameters might be: "
			 << (data1 & 0xff) << " and " << ((data1 >> 8) & 0x01) << endl;
#endif
		// palette shift the shape as we are active
		set_palette_transform(
				147);    // an offset of 147 will makes the jukebox egg palette
						 // cycle instead of be plain blue
		MyMidiPlayer* player = Audio::get_ptr()->get_midi();
		if (player && continuous) {
			// create the Jukeboxfar_egg to stop music repeat
			if (player && player->get_current_track() == score) {
				if (continuous) {
					// increment egg count and set repeat to true if we are
					// continuous
					player->set_egg_count(player->get_egg_count() + 1);
					player->set_repeat(true);
				}
				return;
			}
		}
		Audio::get_ptr()->start_music(score, continuous);
		// set the egg count to 1 if we are continous otherwise to 0
		if (player) {
			player->set_egg_count(continuous ? 1 : 0);
		}
	}

	bool can_unhatch() override {
		return true;
	}

	bool unhatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		MyMidiPlayer* player = Audio::get_ptr()->get_midi();
		// only do anything if the currently playing track is ours
		if (player && player->get_current_track() == score) {
			auto count = player->get_egg_count();
#ifdef DEBUG
			cout << "Jukebox_egg::unhatch_now Audio parameters might be: "
				 << (data1 & 0xff) << " and " << ((data1 >> 8) & 0x01)
				 << ", count " << count << endl;
#endif

			// No eggs left so stop repeat
			if (count <= 1) {
				switch (Audio::get_ptr()->get_music_looping()) {
					// if in endless mode we do not do anything
				case Audio::LoopingType::Endless:
					break;
				// if in limited mode we stop the music but wait a tick in case
				// other eggs are going to be hatched that might change the
				// cxounter
				case Audio::LoopingType::Limited:

#ifdef DEBUG
					cout << "Jukebox_egg::unhatch_now adding to timequeue "
						 << endl;
#endif
					Time_sensitive::set_always(true);
					gwin->get_tqueue()->add(
							0, std::static_pointer_cast<Time_sensitive>(
									   std::static_pointer_cast<Jukebox_egg>(
											   shared_from_this())));
					break;
				default:
					player->set_repeat(false);
				}
				player->set_egg_count(0);
			} else {
				player->set_egg_count(count - 1);
			}
		}
		// set the egg back to normal palette so blue
		set_palette_transform(0);
		return true;
	}

	void handle_event(unsigned long curtime, uintptr udata) override {
		ignore_unused_variable_warning(curtime, udata);
		MyMidiPlayer* player = Audio::get_ptr()->get_midi();
		// only do anything if the currently playing track is ours
		if (player && player->get_current_track() == score) {
			auto count = player->get_egg_count();
#ifdef DEBUG
			cout << "Jukebox_egg::handle_event Audio parameters might be: "
				 << (data1 & 0xff) << " and " << ((data1 >> 8) & 0x01)
				 << ", count " << count << endl;
#endif
			// No eggs left so stop repeat
			if (count < 1) {
				switch (Audio::get_ptr()->get_music_looping()) {
					// if in endless mode we do not do anything
				case Audio::LoopingType::Endless:
					break;
				// if in Stop mode we immediately stop the music
				case Audio::LoopingType::Limited:
					player->stop_music();
					break;
				default:
					player->set_repeat(false);
				}
			}
		}
	}

	void paint() override {
#ifdef DEBUG
		if (gwin->paint_eggs) {
			// paint outline of jukebox egg area
			// 80 is the norm colour of the jukebox egg shapebut also apply the
			// palette shift if we hve one
			const int pix = 80 + get_palette_transform();
			int       rx;
			int       by;
			int       lx;
			int       ty;
			auto      shaperect = gwin->get_shape_rect(this);
			// Centrepoint of egg on screen
			int ex = shaperect.x + shaperect.w / 2;
			int ey = shaperect.y + shaperect.h / 2;
			// draw area at lift of player
			gwin->get_shape_location(
					Tile_coord(
							area.x - 1, area.y - 1,
							gwin->get_main_actor()->get_lift()),
					lx, ty);
			rx = lx + area.w * c_tilesize;
			by = ty + area.h * c_tilesize;
			// Horiz. line along top, bottom.
			gwin->get_win()->draw_line8(pix, lx, ty, rx, ty);
			gwin->get_win()->draw_line8(pix, lx, by, rx, by);
			// Vert. line to left, right.
			gwin->get_win()->draw_line8(pix, lx, ty, lx, by);
			gwin->get_win()->draw_line8(pix, rx, ty, rx, by);
			// Egg to corners
			gwin->get_win()->draw_line8(pix, ex, ey, rx, ty);
			gwin->get_win()->draw_line8(pix, ex, ey, rx, by);
			gwin->get_win()->draw_line8(pix, ex, ey, lx, by);
			gwin->get_win()->draw_line8(pix, ex, ey, lx, ty);
			// Egg to midpoints
			gwin->get_win()->draw_line8(pix, ex, ey, (lx + rx) / 2, ty);
			gwin->get_win()->draw_line8(pix, ex, ey, rx, (ty + by) / 2);
			gwin->get_win()->draw_line8(pix, ex, ey, (lx + rx) / 2, by);
			gwin->get_win()->draw_line8(pix, ex, ey, lx, (ty + by) / 2);

			auto footprint = gwin->get_main_actor()->get_footprint();
			gwin->get_shape_location(
					gwin->get_main_actor(), footprint.x, footprint.y);
			footprint.x -= c_tilesize;
			footprint.y -= c_tilesize;
			footprint.h *= c_tilesize;
			footprint.w *= c_tilesize;

			gwin->get_win()->draw_line8(
					40, footprint.x, footprint.y, footprint.x + footprint.w,
					footprint.y);
			gwin->get_win()->draw_line8(
					40, footprint.x + footprint.w, footprint.y,
					footprint.x + footprint.w, footprint.y + footprint.h);
			gwin->get_win()->draw_line8(
					40, footprint.x + footprint.w, footprint.y + footprint.h,
					footprint.x, footprint.y + footprint.h);
			gwin->get_win()->draw_line8(
					40, footprint.x + footprint.w, footprint.y + footprint.h,
					footprint.x, footprint.y + footprint.h);
			gwin->get_win()->draw_line8(
					40, footprint.x, footprint.y, footprint.x,
					footprint.y + footprint.h);
		}
#endif    // DEBUG
		Egg_object::paint();
	}
};

class Soundsfx_egg : public Jukebox_egg {
public:
	Soundsfx_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1)
			: Jukebox_egg(shnum, frnum, tx, ty, tz, itype, prob, d1) {}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		Audio::get_ptr()->play_sound_effect(
				score, this, AUDIO_MAX_VOLUME, continuous);
	}

	bool unhatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		//  Might want to do something here to stop repeating sounds like
		//  Jukebox_egg
		return false;
	}
};

class Voice_egg : public Egg_object {
public:
	Voice_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, 0) {}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		ucmachine->do_speech(data1 & 0xff);
	}
};

class Monster_egg : public Egg_object {
	unsigned short mshape;    // For monster.
	unsigned char  mframe;
	unsigned char  sched, align, cnt;

	void create_monster() const {
		const Tile_coord dest
				= Map_chunk::find_spot(get_tile(), 5, mshape, 0, 1);
		if (dest.tx != -1) {
			const Game_object_shared new_monster
					= Monster_actor::create(mshape, dest, sched, align);
			Game_object* monster = new_monster.get();
			monster->change_frame(mframe);
			gwin->add_dirty(monster);
			gwin->add_nearby_npc(monster->as_npc());
		}
	}

public:
	Monster_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2, uint16 d3)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2, d3),
			  sched(d1 >> 8), align(d1 & 3), cnt((d1 & 0xff) >> 2) {
		if (d3 > 0) {    // Exult extension.
			mshape = d3;
			mframe = d2 & 0xff;
		} else {
			mshape = d2 & 1023;
			mframe = d2 >> 10;
		}
	}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		const Shape_info& info = ShapeID::get_info(mshape);
		if (info.is_npc()) {
			if (gwin->armageddon) {
				return;
			}
			int num = cnt;
			if (num > 1) {    // Randomize.
				num = 1 + (rand() % num);
			}
			while (num--) {
				create_monster();
			}
		} else {    // Create item.
			const Game_object_shared nobj = get_map()->create_ireg_object(
					info, mshape, mframe, get_tx(), get_ty(), get_lift());
			if (nobj->is_egg()) {
				chunk->add_egg(nobj->as_egg());
			} else {
				chunk->add(nobj.get());
			}
			gwin->add_dirty(nobj.get());
			nobj->set_flag(Obj_flags::okay_to_take);
			// Objects are created temporary
			nobj->set_flag(Obj_flags::is_temporary);
		}
	}
};

class Usecode_egg : public Egg_object {
	short  fun;
	string fun_name;    // Actual name in usecode source.
public:
	Usecode_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2, const char* fnm)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2),
			  fun(d2) {
		Game_object::set_quality(d1 & 0xff);
		Usecode_egg::set_str1(fnm);
	}

	void set_quality(int q) override {
		Game_object::set_quality(q & 0xff);
		data1 = (data1 & 0xFF00) | get_quality();
	}

	void set_str1(const char* s) override {
		fun_name = s ? s : "";
		if (s && *s) {
			fun = 0;    // Want to look this up.
		}
	}

	const char* get_str1() const override {
		return fun_name.c_str();
	}

	int get_usecode() const override {
		return fun;
	}

	bool set_usecode(int funid, const char* nm = nullptr) override {
		fun      = funid;
		fun_name = nm;
		return true;
	}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj);
		if (!fun && !fun_name.empty()) {
			fun = ucmachine->find_function(fun_name.c_str());
		}
		if (must) {    // From script?  Do immediately.
			ucmachine->call_usecode(fun, this, Usecode_machine::egg_proximity);
		} else {    // Do on next animation frame.
			auto* scr = new Usecode_script(this);
			scr->add(Ucscript::usecode, fun);
			if (flags & (1 << static_cast<int>(once))) {
				// Don't remove until done.
				scr->add(Ucscript::remove);
				flags &= ~(1 << static_cast<int>(once));
			}
			scr->start(gwin->get_std_delay());
		}
	}
};

class Missile_egg : public Egg_object {
	short             weapon;
	unsigned char     dir, delay;
	Missile_launcher* launcher;

public:
	Missile_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2),
			  weapon(d1), dir(d2 & 0xff), delay(d2 >> 8), launcher(nullptr) {}

	~Missile_egg() override {
		if (launcher) {
			gwin->get_tqueue()->remove(launcher);
			delete launcher;
		}
	}

	void remove_this(Game_object_shared* keep) override {
		if (launcher) {    // Stop missiles.
			gwin->get_tqueue()->remove(launcher);
			delete launcher;
			launcher = nullptr;
		}
		Egg_object::remove_this(keep);
	}

	void paint() override {
		// Make sure launcher is active.
		if (launcher && !launcher->in_queue()
			&& (criteria == party_near || criteria == avatar_near)) {
			gwin->get_tqueue()->add(0L, launcher);
		}
		Egg_object::paint();
	}

	void set(int crit, int dist) override {
		if (crit == external_criteria && launcher) {    // Cancel trap.
			gwin->get_tqueue()->remove(launcher);
			delete launcher;
			launcher = nullptr;
		}
		Egg_object::set(crit, dist);
	}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		const Shape_info&  info = ShapeID::get_info(weapon);
		const Weapon_info* winf = info.get_weapon_info();
		int                proj;
		if (winf && winf->get_projectile()) {
			proj = winf->get_projectile();
		} else {
			proj = 856;    // Fireball.  Shouldn't get here.
		}
		if (!launcher) {
			launcher = new Missile_launcher(
					this, weapon, proj, dir, gwin->get_std_delay() * delay);
		}
		if (!launcher->in_queue()) {
			gwin->get_tqueue()->add(0L, launcher);
		}
	}
};

class Teleport_egg : public Egg_object {
	short          mapnum;    // If not -1.
	unsigned short destx, desty, destz;

public:
	Teleport_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2, uint16 d3)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2, d3),
			  mapnum(-1) {
		if (type == intermap) {
			mapnum = d1 & 0xff;
		} else {
			Game_object::set_quality(d1 & 0xff);    // Teleport egg.
		}
		const int schunk = d1 >> 8;
		destx            = (schunk % 12) * c_tiles_per_schunk + (d2 & 0xff);
		desty            = (schunk / 12) * c_tiles_per_schunk + (d2 >> 8);
		destz            = d3 & 0xff;
	}

	void set_quality(int q) override {
		Game_object::set_quality(q & 0xff);
		data1 = (data1 & 0xFF00) | get_quality();
	}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(must);
		Tile_coord pos(-1, -1, -1);    // Get position to jump to.
		int        eggnum = 255;
		if (mapnum == -1) {
			eggnum = get_quality();
		}
		if (eggnum == 255) {    // Jump to coords.
			pos = Tile_coord(destx, desty, destz);
		} else {
			Egg_vector vec;    // Look for dest. egg (frame == 6).
			if (find_nearby_eggs(vec, 275, 256, eggnum, 6)) {
				Egg_object* path = vec[0];
				pos              = path->get_tile();
			}
		}
		cout << "Should teleport to map " << mapnum << ", (" << pos.tx << ", "
			 << pos.ty << ')' << endl;
		if (pos.tx != -1 && obj && obj->get_flag(Obj_flags::in_party)) {
			// Teleport everyone!!!
			gwin->teleport_party(pos, false, mapnum, false);
		}
	}
};

class Weather_egg : public Egg_object {
	unsigned char weather;    // 0-6
	unsigned char len;        // In game minutes.
public:
	Weather_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2),
			  weather(d1 & 0xff), len(d1 >> 8) {}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(obj, must);
		set_weather(weather, len, this);
	}
};

class Button_egg : public Egg_object {
	unsigned char dist;

public:
	Button_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2),
			  dist(d1 & 0xff) {}

	void hatch_now(Game_object* obj, bool must) override {
		ignore_unused_variable_warning(must);
		Egg_vector eggs;
		find_nearby_eggs(eggs, 275, dist);
		find_nearby_eggs(eggs, 200, dist);
		for (auto* egg : eggs) {
			if (egg != this && egg->criteria == external_criteria &&
				// Attempting to fix problem in Silver Seed
				!(egg->flags & (1 << static_cast<int>(hatched)))) {
				egg->hatch(obj, false);
			}
		}
	}
};

class Path_egg : public Egg_object {
public:
	Path_egg(
			int shnum, int frnum, unsigned int tx, unsigned int ty,
			unsigned int tz, unsigned short itype, unsigned char prob,
			uint16 d1, uint16 d2)
			: Egg_object(shnum, frnum, tx, ty, tz, itype, prob, d1, d2) {
		Game_object::set_quality(d1 & 0xff);
	}

	void set_quality(int q) override {
		Game_object::set_quality(q & 0xff);
		data1 = (data1 & 0xFF00) | get_quality();
	}
};

/*
 *  Create an "egg" from Ireg data.
 */

Egg_object_shared Egg_object::create_egg(
		const unsigned char* entry,    // 12+ byte ireg entry.
		int entlen, bool animated, int shnum, int frnum, unsigned int tx,
		unsigned int ty, unsigned int tz) {
	const unsigned short type  = entry[4] + 256 * entry[5];
	const int            prob  = entry[6];    // Probability (1-100).
	const int            data1 = entry[7] + 256 * entry[8];
	const int            data2 = entry[10] + 256 * entry[11];
	const int data3 = entlen >= 14 ? (entry[12] + 256 * entry[13]) : 0;
	return create_egg(
			animated, shnum, frnum, tx, ty, tz, type, prob, data1, data2,
			data3);
}

Egg_object_shared Egg_object::create_egg(
		bool animated, int shnum, int frnum, unsigned int tx, unsigned int ty,
		unsigned int   tz,
		unsigned short itype,    // Type + flags, etc.
		unsigned char prob, short data1, short data2, short data3,
		const char* str1) {
	int type = itype & 0xf;
	// Teleport destination?
	if (type == teleport && frnum == 6 && shnum == 275) {
		type = path;    // (Mountains N. of Vesper).
	}

	Egg_object_shared obj;
	switch (type) {    // The type:
	case monster:
		obj = std::make_shared<Monster_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2, data3);
		break;
	case jukebox:
		obj = std::make_shared<Jukebox_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1);
		break;
	case soundsfx:
		obj = std::make_shared<Soundsfx_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1);
		break;
	case voice:
		obj = std::make_shared<Voice_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1);
		break;
	case usecode:
		obj = std::make_shared<Usecode_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2, str1);
		break;
	case missile:
		obj = std::make_shared<Missile_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2);
		break;
	case teleport:
	case intermap:
		obj = std::make_shared<Teleport_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2, data3);
		break;
	case weather:
		obj = std::make_shared<Weather_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2);
		break;
	case path:
		obj = std::make_shared<Path_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2);
		break;
	case button:
		obj = std::make_shared<Button_egg>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2);
		break;
	default:
		cerr << "Illegal egg itype:  " << type << endl;
		obj = std::make_shared<Egg_object>(
				shnum, frnum, tx, ty, tz, itype, prob, data1, data2, data3);
	}
	if (animated) {
		obj->set_animator(new Frame_animator(obj.get()));
	}
	return obj;
}

/*
 *  Paint at given spot in world.
 */

void Egglike_game_object::paint() {
	if (gwin->paint_eggs) {
		Game_object::paint();
	}
}

/*
 *  Can this be clicked on?
 */

bool Egglike_game_object::is_findable() {
	return gwin->paint_eggs && Ireg_game_object::is_findable();
}

/*
 *  Create an egg from IREG data.
 */

Egg_object::Egg_object(
		int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
		unsigned int lft, unsigned short itype, unsigned char prob, short d1,
		short d2, short d3)
		: Egglike_game_object(shapenum, framenum, tilex, tiley, lft),
		  probability(prob), data1(d1), data2(d2), data3(d3),
		  area(TileRect(0, 0, 0, 0)), animator(nullptr) {
	type = itype & 0xf;
	// Teleport destination?
	if (type == teleport && framenum == 6 && shapenum == 275) {
		type = path;    // (Mountains N. of Vesper).
	}
	criteria                    = (itype & (7 << 4)) >> 4;
	distance                    = (itype >> 10) & 0x1f;
	const unsigned char noct    = (itype >> 7) & 1;
	const unsigned char do_once = (itype >> 8) & 1;
	// Missile eggs can be rehatched
	const unsigned char htch = (type == missile) ? 0 : ((itype >> 9) & 1);
	solid_area = (criteria == something_on || criteria == cached_in ||
				  // Teleports need solid area.
				  type == teleport || type == intermap)
						 ? 1
						 : 0;
	const unsigned char ar = (itype >> 15) & 1;
	flags = (noct << nocturnal) + (do_once << once) + (htch << hatched)
			+ (ar << auto_reset);
	// Party_near & auto_reset don't mix
	//   well.
	if (criteria == party_near && (flags & (1 << auto_reset))) {
		criteria = avatar_near;
	}
}

/*
 *  Init. for a field.
 */

inline void Egg_object::init_field(unsigned char ty    // Egg (field) type.
) {
	type        = ty;
	probability = 100;
	data1 = data2 = 0;
	area          = TileRect(0, 0, 0, 0);
	criteria      = party_footpad;
	distance      = 0;
	solid_area    = 0;
	animator      = nullptr;
	flags         = (1 << auto_reset);
}

/*
 *  Create an egg representing a field.
 */

Egg_object::Egg_object(
		int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
		unsigned int  lft,
		unsigned char ty    // Egg (field) type.
		)
		: Egglike_game_object(shapenum, framenum, tilex, tiley, lft) {
	init_field(ty);
}

/*
 *  Destructor:
 */

Egg_object::~Egg_object() {
	delete animator;
}

/*
 *  Clone this egg with all its data.
 */

Egg_object_shared Egg_object::clone_egg(int tilex, int tiley, int lift) const {
	// Reconstruct itype from egg properties
	unsigned short itype = type & 0xf;
	itype |= ((criteria & 7) << 4);
	itype |= (((flags >> nocturnal) & 1) << 7);
	itype |= (((flags >> once) & 1) << 8);
	itype |= (((flags >> hatched) & 1) << 9);
	itype |= ((distance & 0x1f) << 10);
	itype |= (((flags >> auto_reset) & 1) << 15);

	// Check if this egg has an animator
	const bool animated = (animator != nullptr);

	// Create the new egg using the static factory method
	return create_egg(
			animated, get_shapenum(), get_framenum(), tilex, tiley, lift, itype,
			probability, data1, data2, data3, get_str1());
}

/*
 *  Set active area after being added to its chunk.
 */

void Egg_object::set_area() {
	if (!probability || type == path) {    // No chance of normal activation?
		area = TileRect(0, 0, 0, 0);
		return;
	}
	const Tile_coord t = get_tile();    // Get absolute tile coords.
	switch (criteria) {                 // Set up active area.
	case cached_in:                     // Make it really large.
		area = TileRect(t.tx - 32, t.ty - 32, 64, 64);
		break;
	case avatar_footpad:
	case party_footpad: {
		const Shape_info& info   = get_info();
		const int         frame  = get_framenum();
		const int         xtiles = info.get_3d_xtiles(frame);
		const int         ytiles = info.get_3d_ytiles(frame);
		area = TileRect(t.tx - xtiles + 1, t.ty - ytiles + 1, xtiles, ytiles);
		break;
	}
	case avatar_far:    // Make it 1 tile bigger each dir.
		area = TileRect(
				t.tx - distance - 1, t.ty - distance - 1, 2 * distance + 3,
				2 * distance + 3);
		break;
	default: {
		int width = 2 * distance;
		width++;                // Added 8/1/01.
		if (distance <= 1) {    // Small?
			// More guesswork:
			if (criteria == external_criteria) {
				width += 2;
			}
		}
		area = TileRect(t.tx - distance, t.ty - distance, width, width);
		break;
	}
	}
	// Don't go outside the world.
	const TileRect world(
			0, 0, c_num_chunks * c_tiles_per_chunk,
			c_num_chunks * c_tiles_per_chunk);
	area = area.intersect(world);
}

/*
 *  Can this be clicked on?
 */
bool Egg_object::is_findable() {
	if (animator) {
		return Ireg_game_object::is_findable();
	} else {
		return Egglike_game_object::is_findable();
	}
}

/*
 *  Change the criteria and distance.
 */

void Egg_object::set(int crit, int dist) {
	Map_chunk* echunk = get_chunk();
	echunk->remove_egg(this);    // Got to add it back.
	criteria = crit;
	distance = dist;
	echunk->add_egg(this);
}

/*
 *  Is the egg active when stepping onto a given spot, or placing an obj.
 *  on the spot?
 */

bool Egg_object::is_active(
		Game_object* obj,           // Object placed (or Actor).
		int tx, int ty, int tz,     // Tile stepped onto.
		int from_tx, int from_ty    // Tile stepped from.
) {
	if (cheat.in_map_editor()) {
		return false;    // Disable in map-editor.
	}
	if ((flags & (1 << static_cast<int>(hatched)))
		&& !(flags & (1 << static_cast<int>(auto_reset)))) {
		return false;    // For now... Already hatched.
	}
	if (flags & (1 << static_cast<int>(nocturnal))) {
		// Nocturnal.
		const int hour = gclock->get_hour();
		if (hour < 21 && hour > 4) {
			return false;    // It's not night.
		}
	}
	auto cri = static_cast<Egg_criteria>(get_criteria());

	const int deltaz = tz - get_lift();
	switch (cri) {
	case cached_in: {    // Anywhere in square.
		// This seems to be true for SI in general. It has the side effect
		// of "fixing" Fawn Tower goblins.
		// It does NOT happen in BG, though.
		if (GAME_SI && deltaz / 5 != 0 && type == monster) {
			// Mark hatched if not auto-reset.
			if (!(flags & (1 << static_cast<int>(auto_reset)))) {
				flags |= (1 << static_cast<int>(hatched));
			}
			return false;
		}
		if (obj != gwin->get_main_actor() || !area.has_world_point(tx, ty)) {
			return false;    // Not in square.
		}
		if (!(flags & (1 << static_cast<int>(hatched)))) {
			return true;    // First time.
		}
		// Must have autoreset.
		// Just activate when reentering.
		return !area.has_world_point(from_tx, from_ty);
	}
	case avatar_near:
		if (obj != gwin->get_main_actor()) {
			return false;
		}
#ifdef DEBUG
		print_debug();
#endif
		[[fallthrough]];
	case party_near:    // Avatar or party member.
		if (!obj->get_flag(Obj_flags::in_party)) {
			return false;
		}
		if (type == teleport ||    // Teleports:  Any tile, exact lift.
			type == intermap) {
			return deltaz == 0 && area.has_world_point(tx, ty);
		} else if (type == jukebox || type == soundsfx || type == voice) {
			// Guessing. Fixes shrine of Spirituality and Sacrifice.
			return area.has_world_point(tx, ty);
		}
		return (deltaz / 2 == 0 ||
				// Using trial&error here:
				(Game::get_game_type() == SERPENT_ISLE && type != missile)
				|| (type == missile && deltaz / 5 == 0))
			   &&
			   // New tile is in, old is out.
			   area.has_world_point(tx, ty)
			   && !area.has_world_point(from_tx, from_ty);
	case avatar_far: {    // New tile is outside, old is inside.
		if (obj != gwin->get_main_actor() || !area.has_world_point(tx, ty)) {
			return false;
		}
		const TileRect inside(area.x + 1, area.y + 1, area.w - 2, area.h - 2);
		return inside.has_world_point(from_tx, from_ty)
			   && !inside.has_world_point(tx, ty);
	}
	case avatar_footpad:
		return obj == gwin->get_main_actor() && deltaz == 0
			   && area.has_world_point(tx, ty);
	case party_footpad:
		// Our field eggs need to hurt every actor
		if (type >= fire_field) {
			return area.has_world_point(tx, ty) && deltaz == 0
				   && obj->as_actor();
		} else {
			return area.has_world_point(tx, ty) && deltaz == 0
				   && obj->get_flag(Obj_flags::in_party);
		}
	case something_on:
		// force close gumps when gumps pause game
		if (!gumpman->gumps_dont_pause_game()) {
			gumpman->close_all_gumps();
		}
		return    // Guessing.  At SI end, deltaz == -1.
				deltaz / 4 == 0 && area.has_world_point(tx, ty)
				&& !obj->as_actor();
	case external_criteria:
	default:
		return false;
	}
}

// test_unhatch pretty much inverts all the criteria compared to is_active
// thiswas added for improve jukebox_egg behaviour to give the eggs a proximity
// near and far trigger unless other egg types are modified this code should not
// effect anything except jukebox_eggs

bool Egg_object::test_unhatch(
		Game_object* obj,           // Object placed (or Actor).
		int tx, int ty, int tz,     // Tile stepped onto.
		int from_tx, int from_ty    // Tile stepped from.
) {
	// Only avatar can unhatch
	if (obj != gwin->get_main_actor()) {
		return false;
	}
	// if the subclass doesn't support unhatch then we do not do anything
	if (!can_unhatch()) {
		return false;
	}
	// cannot unhatch unless already hatched
	if (!is_hatched()) {
		return false;
	}
	if (cheat.in_map_editor()) {
		return false;    // Disable in map-editor.
	}
	// As this is an Exult only feature I am making nocturnal eggs always
	// unhatch if it becomes day this sort of thing might have some use to
	// automatically clean up things once it becomes day if someone in the
	// future wants to implement that. Right now unhatching is only relevate to
	// jukebox_eggs and as far as I know they are not nocturnal
	if (flags & (1 << static_cast<int>(nocturnal))) {
		// Nocturnal.
		const int hour = gclock->get_hour();
		if (hour < 21 && hour > 4) {
			return true;    // It's not night.
		}
	}
	auto cri = static_cast<Egg_criteria>(get_criteria());

	const int deltaz = tz - get_lift();
	switch (cri) {
	case cached_in: {    // Anywhere in square.
		if (GAME_SI && deltaz / 5 != 0 && type == monster) {
			return true;
		}

		// return true  moving from in to out
		return area.has_world_point(from_tx, from_ty)
			   && !area.has_world_point(tx, ty);
	}
	case avatar_near:
		[[fallthrough]];
	case party_near:    // Avatar or party member.
		if (!obj->get_flag(Obj_flags::in_party)) {
			return false;
		}
		if (type == teleport ||    // Teleports:  Any tile, exact lift.
			type == intermap) {
			return false;    // this makes no sense for teleports
		} else if (type == jukebox || type == soundsfx || type == voice) {
			return !area.has_world_point(tx, ty)
				   && area.has_world_point(from_tx, from_ty);
		}
		return !(
				(deltaz / 2 == 0 ||
				 // Using trial&error here:
				 (Game::get_game_type() == SERPENT_ISLE && type != missile)
				 || (type == missile && deltaz / 5 == 0))
				&&
				// New tile is in, old is out.
				area.has_world_point(tx, ty)
				&& !area.has_world_point(from_tx, from_ty));
	case avatar_far: {    // New tile is outside, old is inside.
		if (obj != gwin->get_main_actor()
			|| !area.has_world_point(from_tx, from_ty)) {
			return false;
		}
		const TileRect inside(area.x + 1, area.y + 1, area.w - 2, area.h - 2);
		return !inside.has_world_point(from_tx, from_ty)
			   && inside.has_world_point(tx, ty);
	}
	case avatar_footpad:
		return obj == gwin->get_main_actor()
			   && (deltaz != 0 || !area.has_world_point(tx, ty));
	case party_footpad:
		// This is a basic inversion of the logic from is_active
		// if this is needed it might be necessary to adjust this logic
		if (type >= fire_field) {
			return (!area.has_world_point(tx, ty) || deltaz != 0)
				   && obj->as_actor();
		} else {
			return (!area.has_world_point(tx, ty) || deltaz != 0)
				   && obj->get_flag(Obj_flags::in_party);
		}
	case something_on:
		return (deltaz / 4 != 0 || !area.has_world_point(tx, ty))
			   && !obj->as_actor();
	case external_criteria:
	default:
		return false;
	}
}

/*
 *  Animate.
 */
void Egg_object::set_animator(Animator* a) {
	delete animator;
	animator = a;
}

/*
 *  Stop animation.
 */

void Egg_object::stop_animation() {
	if (animator) {
		animator->stop_animation();
	}
}

/*
 *  Paint at given spot in world.
 */

void Egg_object::paint() {
	if (animator) {
		animator->want_animation();    // Be sure animation is on.
		Ireg_game_object::paint();     // Always paint these.
	} else {
		Egglike_game_object::paint();
	}
}

/*
 *  Run usecode when double-clicked.
 */

void Egg_object::activate(int event) {
	// If event==2, open basic properties instead of egg editor
	if (event == 2 && edit_basic_properties()) {
		return;
	}
	if (!edit()) {
		hatch(nullptr, false);
	}
	if (animator) {
		flags &= ~(
				1 << static_cast<int>(hatched));    // Moongate:  reset always.
	}
}

/*
 *  Edit in ExultStudio.
 *
 *  Output: True if map-editing & ES is present.
 */

bool Egg_object::edit() {
#ifdef USE_EXULTSTUDIO
	if (client_socket >= 0 &&    // Talking to ExultStudio?
		cheat.in_map_editor()) {
		editing.reset();
		const Tile_coord t = get_tile();
		// Usecode function name.
		const char* str1 = get_str1();
		if (Egg_object_out(
					client_socket, this, t.tx, t.ty, t.tz, get_shapenum(),
					get_framenum(), type, criteria, probability, distance,
					(flags >> nocturnal) & 1, (flags >> once) & 1,
					(flags >> hatched) & 1, (flags >> auto_reset) & 1, data1,
					data2, data3, str1)
			!= -1) {
			cout << "Sent egg data to ExultStudio" << endl;
			editing = shared_from_this();
		} else {
			cout << "Error sending egg data to ExultStudio" << endl;
		}
		return true;
	}
#endif
	return false;
}

bool Egg_object::edit_basic_properties() {
	// Call the base Game_object::edit() to open basic properties
	// instead of the Egg-specific editor
	return Game_object::edit();
}

/*
 *  Message to update from ExultStudio.
 */

void Egg_object::update_from_studio(unsigned char* data, int datalen) {
#ifdef USE_EXULTSTUDIO
	int         x;
	int         y;    // Mouse click for new egg.
	Egg_object* oldegg;
	int         tx;
	int         ty;
	int         tz;
	int         shape;
	int         frame;
	int         type;
	int         criteria;
	int         probability;
	int         distance;
	bool        nocturnal;
	bool        once;
	bool        hatched;
	bool        auto_reset;
	int         data1;
	int         data2;
	int         data3;
	string      str1;
	if (!Egg_object_in(
				data, datalen, oldegg, tx, ty, tz, shape, frame, type, criteria,
				probability, distance, nocturnal, once, hatched, auto_reset,
				data1, data2, data3, str1)) {
		cout << "Error decoding egg" << endl;
		return;
	}
	if (oldegg && oldegg != editing.get()) {
		cout << "Egg from ExultStudio is not being edited" << endl;
		return;
	}
	// Keeps NPC alive until end of function
	const Game_object_shared keep = std::move(editing);
	if (!oldegg) {    // Creating a new one?  Get loc.
		if (!Get_click(x, y, Mouse::greenselect, nullptr)) {
			if (client_socket >= 0) {
				Exult_server::Send_data(client_socket, Exult_server::cancel);
			}
			return;
		}
		if (shape == -1) {
			shape = 275;    // FOR NOW.
		}
	} else if (shape == -1) {
		shape = oldegg->get_shapenum();
	}
	if (frame == -1) {
		switch (type) {
			// (These aren't perfect.)
		case monster:
			frame = 0;
			break;
		case jukebox:
			frame = 2;
			break;
		case soundsfx:
			frame = 1;
			break;
		case voice:
			frame = 3;
			break;
		case weather:
			frame = 4;
			break;
		case intermap:
		case teleport:
			frame = 5;
			break;
		case path:
			frame = 6;
			break;
		case missile:
			shape = 200;
			if ((data2 & 0xFF) < 8) {
				frame = 2 + ((data2 & 0xFF) / 2);
			} else {
				frame = 1;
			}
			break;
		default:
			frame = 7;
			break;
		}
	}

	const Shape_info&       info = ShapeID::get_info(shape);
	const bool              anim = info.is_animated() || info.has_sfx();
	const Egg_object_shared egg  = create_egg(
            anim, shape, frame, tx, ty, tz, type, probability, data1, data2,
            data3, str1.c_str());
	if (!oldegg) {
		int lift;    // Try to drop at increasing hts.
		for (lift = 0; lift < 12; lift++) {
			if (gwin->drop_at_lift(egg.get(), x, y, lift) == 1) {
				break;
			}
		}
		if (lift == 12) {
			if (client_socket >= 0) {
				Exult_server::Send_data(client_socket, Exult_server::cancel);
			}
			return;
		}
		if (client_socket >= 0) {
			Exult_server::Send_data(
					client_socket, Exult_server::user_responded);
		}
	} else {
		const Tile_coord pos = oldegg->get_tile();
		egg->move(pos.tx, pos.ty, pos.tz);
	}
	gwin->add_dirty(egg.get());
	egg->criteria    = criteria & 7;
	egg->distance    = distance & 31;
	egg->probability = probability;
	egg->flags       = ((nocturnal ? 1 : 0) << Egg_object::nocturnal)
				 + ((once ? 1 : 0) << Egg_object::once)
				 + ((hatched ? 1 : 0) << Egg_object::hatched)
				 + ((auto_reset ? 1 : 0) << Egg_object::auto_reset);

	if (oldegg) {
		oldegg->remove_this();
	}
	Map_chunk* echunk = egg->get_chunk();
	echunk->remove_egg(egg.get());    // Got to add it back.
	echunk->add_egg(egg.get());
	cout << "Egg updated" << endl;
#else
	ignore_unused_variable_warning(data, datalen);
#endif
}

/*
 *  Hatch egg.
 */

void Egg_object::hatch(
		Game_object* obj,    // Object (actor) that came near it.
		bool         must    // If 1, skip dice roll & execute
							 //   usecode eggs immediately.
) {
#ifdef DEBUG
	print_debug();
#endif
	/*
	  MAJOR HACK!
	  This is an attempt at a work-around of a potential bug in the original
	  Serpent Isle. See SourceForge bug #879253

	  Prevent the Serpent Staff egg from hatching only once
	*/
	const Tile_coord eggpos = get_tile();
	if (GAME_SI && eggpos.tx == 1287 && eggpos.ty == 2568 && eggpos.tz == 0) {
		flags &= ~(1 << static_cast<int>(hatched));
	}
	// Fixing some stairs in SS bug # 3115182 by making them auto_reset
	if (GAME_SS && type == teleport
		&& ((eggpos.tx == 2844 && eggpos.ty == 2934 && eggpos.tz == 2)
			|| (eggpos.tx == 2843 && eggpos.ty == 2934 && eggpos.tz == 1)
			|| (eggpos.tx == 3015 && eggpos.ty == 2840 && eggpos.tz == 0)
			|| (eggpos.tx == 2950 && eggpos.ty == 2887 && eggpos.tz == 0)
			|| (eggpos.tx == 2859 && eggpos.ty == 3048 && eggpos.tz == 2)
			|| (eggpos.tx == 2884 && eggpos.ty == 3047 && eggpos.tz == 2)
			|| (eggpos.tx == 2983 && eggpos.ty == 2887 && eggpos.tz == 0))) {
		flags |= (1 << static_cast<int>(auto_reset));
	}

	/* end hack */

	// Must not try to hatch an unhatchble egg unless unhatch() was called and
	// removedthe hatched flag
	if (can_unhatch() && is_hatched()) {
		return;
	}

	const int roll = must ? 0 : 1 + rand() % 100;
	if (roll <= probability) {
		// Time to hatch the egg.
		// Watch it in case it gets deleted.
		const Game_object_weak watch = weak_from_this();
		hatch_now(obj, must);
		if (watch.expired()) {
			// We have been deleted, so just leave.
			return;
		}
		// unhatchble eggs must not be removed here
		if (!can_unhatch() && flags & (1 << static_cast<int>(once))) {
			remove_this(nullptr);
			return;
		}
	}
	// Flag it as done, whether or not it has been hatched.
	flags |= (1 << static_cast<int>(hatched));
}

void Egg_object::unhatch(Game_object* obj, bool must) {
	// if unhatch_now returns true the hatched flag is cleared
	if (unhatch_now(obj, must)) {
		if (is_auto_reset()) {
			flags &= ~(1 << static_cast<int>(hatched));
		}
	}
	// once only eggs are safe to remove here
	if (is_once()) {
		remove_this(nullptr);
	}
}

/*
 *  Print debug information.
 */

void Egg_object::print_debug() {
	cout << "Egg type is " << static_cast<int>(type)
		 << ", prob = " << static_cast<int>(probability)
		 << ", distance = " << static_cast<int>(distance)
		 << ", crit = " << static_cast<int>(criteria)
		 << ", once = " << ((flags & (1 << static_cast<int>(once))) != 0)
		 << ", hatched = " << ((flags & (1 << static_cast<int>(hatched))) != 0)
		 << ", areset = "
		 << ((flags & (1 << static_cast<int>(auto_reset))) != 0)
		 << ", data1 = " << data1 << ", data2 = " << data2
		 << ", data3 = " << data3 << endl;
}

/*
 *  Set the weather (static).
 */

void Egg_object::set_weather(
		int          weather,    // 0-6.
		int          len,        // In game minutes (I think).
		Game_object* egg         // Egg this came from, or null.
) {
	if (!len) {        // Means continuous.
		len = 6000;    // Confirmed from originals.
	}
	const int cur = eman->get_weather();
	cout << "Current weather is " << cur << "; setting " << weather << endl;
	// Experimenting.
	if (weather != 4 && (weather == 3 || cur != weather)) {
		eman->remove_weather_effects();
	}

	switch (weather) {
	case 0:    // Back to normal.
		eman->remove_weather_effects();
		break;
	case 1:    // Snow.
		eman->add_effect(std::make_unique<Snowstorm_effect>(len, 0, egg));
		break;
	case 2:    // Storm.
		eman->add_effect(std::make_unique<Storm_effect>(len, 0, egg));
		break;
	case 3:    // (On Ambrosia and in Generators).
		eman->remove_weather_effects();
		eman->add_effect(std::make_unique<Sparkle_effect>(len, 0, egg));
		break;
	case 4:    // Fog.
		eman->add_effect(std::make_unique<Fog_effect>(len, 0, egg));
		break;
	case 5:    // Overcast.
	case 6:    // Clouds.
		eman->add_effect(std::make_unique<Clouds_effect>(len, 0, egg, weather));
		break;
	default:
		break;
	}
}

/*
 *  Move to a new absolute location.  This should work even if the old
 *  location is invalid (cx=cy=255).
 */

void Egg_object::move(int newtx, int newty, int newlift, int newmap) {
	// Figure new chunk.
	const int newcx  = newtx / c_tiles_per_chunk;
	const int newcy  = newty / c_tiles_per_chunk;
	Game_map* eggmap = newmap >= 0 ? gwin->get_map(newmap) : get_map();
	if (!eggmap) {
		eggmap = gmap;
	}
	Map_chunk* newchunk = eggmap->get_chunk_safely(newcx, newcy);
	if (!newchunk) {
		return;    // Bad loc.
	}
	Game_object_shared keep;
	remove_this(&keep);    // Remove from old.
	set_lift(newlift);     // Set new values.
	set_shape_pos(newtx % c_tiles_per_chunk, newty % c_tiles_per_chunk);
	newchunk->add_egg(this);    // Updates cx, cy.
	gwin->add_dirty(this);      // And repaint new area.
}

/*
 *  This is needed since it calls remove_egg().
 */

void Egg_object::remove_this(
		Game_object_shared* keep    // Non-null to not delete.
) {
	if (keep) {
		*keep = shared_from_this();
	}
	if (get_owner()) {    // Watch for this.
		get_owner()->remove(this);
	} else {
		if (chunk) {
			gwin->add_dirty(this);    // (Make's ::move() simpler.).
			chunk->remove_egg(this);
		}
	}
}

/*
 *  Write out.
 */

void Egg_object::write_ireg(ODataSource* out) {
	unsigned char  buf[30];    // 12-14 byte entry.
	const int      sz    = data3 > 0 ? 14 : 12;
	uint8*         ptr   = write_common_ireg(sz, buf);
	unsigned short tword = type & 0xf;    // Set up 'type' word.
	tword |= ((criteria & 7) << 4);
	tword |= (((flags >> nocturnal) & 1) << 7);
	tword |= (((flags >> once) & 1) << 8);
	tword |= (((flags >> hatched) & 1) << 9);
	tword |= ((distance & 0x1f) << 10);
	tword |= (((flags >> auto_reset) & 1) << 15);
	little_endian::Write2(ptr, tword);
	Write1(ptr, probability);
	little_endian::Write2(ptr, data1);
	Write1(ptr, nibble_swap(get_lift()));
	little_endian::Write2(ptr, data2);
	if (data3 > 0) {
		little_endian::Write2(ptr, data3);
	}
	out->write(reinterpret_cast<char*>(buf), ptr - buf);
	const char* str1 = get_str1();
	if (*str1) {    // This will be usecode fun. name.
		Game_map::write_string(out, str1);
	}
	// Write scheduled usecode.
	Game_map::write_scheduled(out, this);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Egg_object::get_ireg_size() {
	// These shouldn't ever happen, but you never know
	if (gumpman->find_gump(this) || Usecode_script::find(this)) {
		return -1;
	}
	const char* str1 = get_str1();
	return 8 + get_common_ireg_size() + ((data3 > 0) ? 2 : 0)
		   + (*str1 ? Game_map::write_string(nullptr, str1) : 0);
}

/*
 *  Create.
 */
Field_object::Field_object(
		int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
		unsigned int lft, unsigned char ty)
		: Egg_object(shapenum, framenum, tilex, tiley, lft, ty) {
	const Shape_info& info = get_info();
	if (info.is_animated()) {
		set_animator(new Field_frame_animator(this));
	}
}

/*
 *  Apply field.
 *
 *  Output: True to delete field.
 */

bool Field_object::field_effect(Actor* actor) {
	if (!actor) {
		return false;
	}
	bool      del   = false;    // Only delete poison, sleep fields.
	const int frame = get_framenum();
	switch (type) {
	case poison_field:
		if (rand() % 2 && !actor->get_flag(Obj_flags::poisoned)) {
			actor->set_flag(Obj_flags::poisoned);
			del = true;
		}
		break;
	case sleep_field:
		if (rand() % 2 && !actor->get_flag(Obj_flags::asleep)) {
			actor->set_flag(Obj_flags::asleep);
			del = true;
		}
		break;
	case campfire_field:
		// Campfire (Fire in SI) doesn't hurt when burnt out
		if (frame == 0) {
			return false;
		}
		[[fallthrough]];
	case fire_field:
		actor->reduce_health(2 + rand() % 3, Weapon_data::fire_damage);
		// But no sleeping here.
		if (actor->get_flag(Obj_flags::asleep) && !actor->is_knocked_out()) {
			actor->clear_flag(Obj_flags::asleep);
		}
		break;
	case caltrops_field:
		if (actor->get_effective_prop(Actor::intelligence) < rand() % 40) {
			// actor->reduce_health(2 + rand()%3, Weapon_info::normal_damage);
			//  Caltrops don't seem to cause much damage.
			actor->reduce_health(1 + rand() % 2, Weapon_data::normal_damage);
		}
		return false;
	}

	if (!del && animator) {
		// Tell animator to keep checking.
		animator->activate_animator();
	}
	return del;
}

/*
 *  Render.
 */

void Field_object::paint() {
	if (animator) {
		animator->want_animation();    // Be sure animation is on.
	}
	Ireg_game_object::paint();    // Always paint these.
}

/*
 *  Run usecode when double-clicked or when activated by proximity.
 *  (Generally, nothing will happen.)
 */

void Field_object::activate(int event) {
	// Field_frame_animator calls us with
	//   event==0 to check for damage.
	if (event != Usecode_machine::npc_proximity) {
		Ireg_game_object::activate(event);
		return;
	}
	Actor_vector npcs;    // Find all nearby NPC's.
	gwin->get_nearby_npcs(npcs);
	npcs.push_back(gwin->get_main_actor());    // Include Avatar.
	const TileRect eggfoot = get_footprint();
	// Clear flag to check.
	if (animator) {
		animator->deactivate_animator();
	}
	for (auto* actor : npcs) {
		if (actor->is_dead() || Game_object::distance(actor) > 4) {
			continue;
		}
		if (actor->get_footprint().intersects(eggfoot)) {
			Field_object::hatch(actor);
		}
	}
}

/*
 *  Someone stepped on it.
 */

void Field_object::hatch(
		Game_object* obj,    // Object (actor) that came near it.
		bool /* must */      // If 1, skip dice roll.
) {
	if (field_effect(obj->as_actor())) {    // Apply field.
		remove_this(nullptr);               // Delete sleep/poison if applied.
	}
}

/*
 *  Write out.  These are stored as normal game objects.
 */

void Field_object::write_ireg(ODataSource* out) {
	Ireg_game_object::write_ireg(out);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Field_object::get_ireg_size() {
	return Ireg_game_object::get_ireg_size();
}

/*
 *  It's a Mirror
 */

Mirror_object::Mirror_object(
		int shapenum, int framenum, unsigned int tilex, unsigned int tiley,
		unsigned int lft)
		: Egg_object(
				  shapenum, framenum, tilex, tiley, lft,
				  Egg_object::mirror_object) {
	solid_area = 1;
}

void Mirror_object::activate(int event) {
	Ireg_game_object::activate(event);
}

void Mirror_object::hatch(Game_object* obj, bool must) {
	ignore_unused_variable_warning(obj, must);
	// These are broken, so dont touch
	if ((get_framenum() % 3) == 2) {
		return;
	}

	int wanted_frame = get_framenum() / 3;
	wanted_frame *= 3;

	// Find upperleft or our area
	Tile_coord t = get_tile();

	// To left or above?
	if (get_shapenum() == 268) {    // Left
		t.tx++;
		t.ty--;
	} else {    // Above
		t.tx--;
		t.ty++;
	}

	// We just want to know if the area is blocked
	int nl = 0;
	if (Map_chunk::is_blocked(1, t.tz, t.tx, t.ty, 2, 2, nl, MOVE_WALK, 0)) {
		wanted_frame++;
	}

	// Only if it changed update the shape
	if (get_framenum() != wanted_frame) {
		change_frame(wanted_frame);
	}
}

// Can it be activated?
bool Mirror_object::is_active(
		Game_object* obj, int tx, int ty, int tz, int from_tx, int from_ty) {
	ignore_unused_variable_warning(obj, tx, ty, tz, from_tx, from_ty);
	// These are broken, so dont touch
	const int frnum = get_framenum();
	if (frnum % 3 == 2) {
		return false;
	}
	if (frnum >= 3 && GAME_BG) {    // Demon mirror in FOV.
		return false;
	}

	return true;
}

// Set up active area.
void Mirror_object::set_area() {
	// These are broken, so dont touch
	if ((get_framenum() % 3) == 2) {
		area = TileRect(0, 0, 0, 0);
	}

	const Tile_coord t = get_tile();    // Get absolute tile coords.

	// To left or above?
	if (get_shapenum() == 268) {
		area = TileRect(t.tx - 1, t.ty - 3, 6, 6);
	} else {
		area = TileRect(t.tx - 3, t.ty - 1, 6, 6);
	}
}

void Mirror_object::paint() {
	Ireg_game_object::paint();    // Always paint these.
}

/*
 *  Write out.  These are stored as normal game objects.
 */

void Mirror_object::write_ireg(ODataSource* out) {
	Ireg_game_object::write_ireg(out);
}

// Get size of IREG. Returns -1 if can't write to buffer
int Mirror_object::get_ireg_size() {
	// TODO!!!!!!!
	return Ireg_game_object::get_ireg_size();
}
