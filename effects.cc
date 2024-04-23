/*
 *  effects.cc - Special effects.
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

#include "effects.h"

#include "Audio.h"
#include "AudioMixer.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "Zombie.h"
#include "actors.h"
#include "ammoinf.h"
#include "chunks.h"
#include "dir.h"
#include "egg.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "shapeinf.h"
#include "ucmachine.h"
#include "weaponinf.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

using namespace Pentagram;

using std::rand;
using std::string;
using std::strlen;
using std::vector;

int Cloud::randcnt           = 0;
int Lightning_effect::active = 0;

/**
 *  Clean up.
 */

Effects_manager::~Effects_manager() {
	remove_all_effects(false);
}

/**
 *  Add text over a given item.
 *  @param msg      text to add
 *  @param item     Item text ID's, or null.
 */

void Effects_manager::add_text(const char* msg, Game_object* item) {
	if (!msg) {    // Happens with edited games.
		return;
	}
	// Don't duplicate for item.
	if (std::find_if(
				texts.cbegin(), texts.cend(),
				[item](const auto& el) {
					return el->is_text(item);
				})
		!= texts.cend()) {
		return;    // Already have text on this.
	}

	//	txt->paint(this);        // Draw it.
	//	painted = 1;
	texts.emplace_front(std::make_unique<Text_effect>(msg, item, gwin));
}

/**
 *  Add a text object at a given spot.
 *  @param msg      text to add
 *  @param x        x coord. on screen.
 *  @param y        y coord. on screen.
 */

void Effects_manager::add_text(const char* msg, int x, int y) {
	texts.emplace_front(std::make_unique<Text_effect>(
			msg, gwin->get_scrolltx() + x / c_tilesize,
			gwin->get_scrollty() + y / c_tilesize, gwin));
}

/**
 *  Add a text object in the center of the screen
 */

void Effects_manager::center_text(const char* msg) {
	remove_text_effects();
	Shape_manager* sman = Shape_manager::get_instance();
	add_text(
			msg, (gwin->get_width() - sman->get_text_width(0, msg)) / 2,
			gwin->get_height() / 2);
}

/**
 *  Add an effect at the start of the chain.
 */

void Effects_manager::add_effect(std::unique_ptr<Special_effect> effect) {
	effects.emplace_front(std::move(effect));
}

/**
 *  Remove a given object's text effect.
 */

void Effects_manager::remove_text_effect(
		Game_object* item    // Item text was added for.
) {
	auto effectToDelete
			= std::find_if(texts.begin(), texts.end(), [item](const auto& el) {
				  return el->is_text(item);
			  });
	if (effectToDelete == texts.end()) {
		return;
	} else {
		texts.erase(effectToDelete);
		gwin->paint();
	}
}

/**
 *  Remove a sprite from the chain and return it to caller
 */

std::unique_ptr<Special_effect> Effects_manager::remove_effect(
		Special_effect* effect) {
	auto toDelete = std::find_if(
			std::begin(effects), std::end(effects), [effect](const auto& ef) {
				return ef.get() == effect;
			});
	decltype(effects)::value_type result;
	if (toDelete != effects.end()) {
		result = std::move(*toDelete);
		effects.erase(toDelete);
	}
	return result;
}

/**
 *  Remove text from the chain and delete it.
 */

void Effects_manager::remove_text_effect(Text_effect* txt) {
	texts.remove_if([txt](const auto& el) {
		return el.get() == txt;
	});
}

/**
 *  Remove all text items.
 */

void Effects_manager::remove_all_effects(bool repaint) {
	if (effects.empty() && texts.empty()) {
		return;
	}
	effects.clear();
	texts.clear();
	if (repaint) {
		gwin->paint();    // Just paint whole screen.
	}
}

/**
 *  Remove text effects.
 */

void Effects_manager::remove_text_effects() {
	texts.clear();
	gwin->set_all_dirty();
}

/**
 *  Remove weather effects.
 *  @param  dist    Only remove those from eggs at least this far away.
 */

void Effects_manager::remove_weather_effects(int dist) {
	Actor*     main_actor = gwin->get_main_actor();
	Tile_coord apos
			= main_actor ? main_actor->get_tile() : Tile_coord(-1, -1, -1);
	effects.remove_if([&](const auto& ef) {
		return ef->is_weather()
			   && (!dist
				   || static_cast<Weather_effect*>(ef.get())->out_of_range(
						   apos, dist));
	});
	gwin->set_all_dirty();
}

/**
 *  Remove lightning effects.
 */

void Effects_manager::remove_usecode_lightning() {
	effects.remove_if([](const auto& ef) {
		return ef->is_usecode_lightning();
	});
	gwin->set_all_dirty();
}

/**
 *  Find last numbered weather effect added.
 */

int Effects_manager::get_weather() {
	auto found = std::find_if(
			effects.cbegin(), effects.cend(), [](const auto& ef) {
				return ef->is_weather()
					   && (static_cast<Weather_effect*>(ef.get())->get_num()
						   >= 0);
			});
	return found != effects.cend()
				   ? static_cast<Weather_effect*>(found->get())->get_num()
				   : 0;
}

/**
 * Construct and store game window singleton
 * TODO check if passing it from other scope
 * won't be better
 */

Special_effect::Special_effect() : gwin(Game_window::get_instance()) {}

/**
 * Destructor
 * Takes advantage of the fact that gwin
 * is stored.
 */
Special_effect::~Special_effect() {
	if (in_queue()) {
		gwin->get_tqueue()->remove(this);
	}
}

/**
 *  Paint them all.
 */

void Effects_manager::paint() {
	for (auto& effect : effects) {
		effect->paint();
	}
}

/**
 *  Paint all text.
 */

void Effects_manager::paint_text() {
	for (auto& txt : texts) {
		txt->paint();
	}
}

/**
 *  Paint all text.
 */
void Effects_manager::update_dirty_text() {
	for (auto& txt : texts) {
		txt->update_dirty();
	}
}

/**
 *  Some special effects may not need painting.
 */

void Special_effect::paint() {}

/**
 *  Create an animation from the 'sprites.vga' file.
 */

Sprites_effect::Sprites_effect(
		int               num,    // Index.
		const Tile_coord& p,      // Position within world.
		int dx, int dy,           // Add to offset for each frame.
		int delay,                // Delay (msecs) before starting.
		int frm,                  // Starting frame.
		int rps                   // Reps, or <0 to go through frames.
		)
		: sprite(num, frm, SF_SPRITES_VGA), pos(p), xoff(0), yoff(0),
		  deltax(dx), deltay(dy), reps(rps) {
	frames = sprite.get_num_frames();
	// Start.
	gwin->get_tqueue()->add(Game::get_ticks() + delay, this);
}

/**
 *  Create an animation on an object.
 */

Sprites_effect::Sprites_effect(
		int          num,    // Index.
		Game_object* it,     // Item to put effect by.
		int xf, int yf,      // Offset from actor in pixels.
		int dx, int dy,      // Add to offset on each frame.
		int frm,             // Starting frame.
		int rps              // Reps, or <0 to go through frames.
		)
		: sprite(num, frm, SF_SPRITES_VGA), item(weak_from_obj(it)), xoff(xf),
		  yoff(yf), deltax(dx), deltay(dy), reps(rps) {
	pos    = it->get_tile();
	frames = sprite.get_num_frames();
	// Start immediately.
	gwin->get_tqueue()->add(Game::get_ticks(), this);
}

/**
 *  Add a dirty rectangle for the current position and frame.
 */

inline void Sprites_effect::add_dirty(int frnum) {
	if (pos.tx == -1 || frnum == -1) {
		return;    // Already at destination.
	}
	Shape_frame* shape = sprite.get_shape();
	const int    lp    = pos.tz / 2;

	gwin->add_dirty(gwin->clip_to_win(
			gwin->get_shape_rect(
						shape,
						xoff
								+ (pos.tx - lp - gwin->get_scrolltx())
										  * c_tilesize,
						yoff
								+ (pos.ty - lp - gwin->get_scrollty())
										  * c_tilesize)
					.enlarge((3 * c_tilesize) / 2)));
}

/**
 *  Animation.
 */

void Sprites_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	int       frame_num = sprite.get_framenum();
	const int delay
			= gwin->get_std_delay();    // Delay between frames.  Needs to
	//   match usecode animations.
	if (!reps || (reps < 0 && frame_num == frames)) {    // At end?
		// Remove & delete this.
		gwin->set_all_dirty();
		auto ownHandle = eman->remove_effect(this);
		return;
	}
	add_dirty(frame_num);    // Clear out old.
	gwin->set_painted();
	const Game_object_shared item_obj = item.lock();
	if (item_obj) {    // Following actor?
		pos = item_obj->get_tile();
	}
	xoff += deltax;    // Add deltas.
	yoff += deltay;
	frame_num++;       // Next frame.
	if (reps > 0) {    // Given a count?
		--reps;
		frame_num %= frames;
	}
	add_dirty(frame_num);    // Want to paint new frame.
	sprite.set_frame(frame_num);
	// Add back to queue for next time.
	gwin->get_tqueue()->add(curtime + delay, this, udata);
}

/**
 *  Render.
 */

void Sprites_effect::paint() {
	if (sprite.get_framenum() >= frames) {
		return;
	}
	const int lp = pos.tz / 2;    // Account for lift.
	sprite.paint_shape(
			xoff + (pos.tx - lp - gwin->get_scrolltx()) * c_tilesize
					- gwin->get_scrolltx_lo(),
			yoff + (pos.ty - lp - gwin->get_scrollty()) * c_tilesize
					- gwin->get_scrolltx_lo());
}

static inline int get_explosion_shape(int weap, int proj) {
	const int shp = proj >= 0 ? proj : (weap >= 0 ? weap : 704);
	return ShapeID::get_info(shp).get_explosion_sprite();
}

static inline int get_explosion_sfx(int weap, int proj) {
	const int shp = proj >= 0 ? proj : (weap >= 0 ? weap : 704);
	return ShapeID::get_info(shp).get_explosion_sfx();
}

/**
 *  Start explosion.
 */

Explosion_effect::Explosion_effect(
		const Tile_coord& p, Game_object* exp,
		int delay,    // Delay before starting (msecs).
		int weap,     // Weapon to use for damage calcs., or
		//   -1 for default(704 = powder keg).
		int          proj,    // Projectile for e.g., burst arrows, 0 otherwise
		Game_object* att      // who is responsible for the explosion
		//  or nullptr for default
		// Different sprites for different explosion types
		)
		: Sprites_effect(get_explosion_shape(weap, proj), p, 0, 0, delay),
		  explode(weak_from_obj(exp)),
		  weapon(weap >= 0 ? weap : (proj >= 0 ? proj : 704)), projectile(proj),
		  exp_sfx(get_explosion_sfx(weap, proj)), attacker(weak_from_obj(att)) {
	// if (exp && exp->get_shapenum() == 704)  // powderkeg
	if (exp && exp->get_info().is_explosive()) {    // powderkeg
		exp->set_quality(1);                        // mark as detonating
	}

	if (!att || !att->as_actor()) {
		// Blame avatar: if we have no living attacker.
		attacker = weak_from_obj(gwin->get_main_actor());
	}
}

void Explosion_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const int frnum = sprite.get_framenum();
	if (!frnum) {    // Max. volume, with stereo position.
		Audio::get_ptr()->play_sound_effect(exp_sfx, pos, AUDIO_MAX_VOLUME);
	}
	if (frnum == frames / 4) {
		// this was in ~Explosion_effect before
		const Game_object_shared exp_obj = explode.lock();
		if (exp_obj && !exp_obj->is_pos_invalid()) {
			Game_window::get_instance()->add_dirty(exp_obj.get());
			exp_obj->remove_this();
			explode = Game_object_weak();
		}
		Shape_frame* shape = sprite.get_shape();
		const int    width = shape->get_width();    // Get the sprite's width
		Game_object_vector vec;    // Find objects near explosion.
		Game_object::find_nearby(
				vec, pos, c_any_shapenum, width / (2 * c_tilesize), 0);
		// Objects could disappear due to Avatar being killed and teleported.
		vector<Game_object_weak> cvec(vec.size());
		Game_object::obj_vec_to_weak(cvec, vec);
		for (auto& it : cvec) {
			const Game_object_shared obj = it.lock();
			if (obj) {
				const Game_object_shared att_obj = attacker.lock();
				obj->attacked(att_obj.get(), weapon, projectile, true);
			}
		}
	}
	Sprites_effect::handle_event(curtime, udata);
}

/**
 *  Get direction in 1/16's starting from North.
 */

inline int Get_dir16(Tile_coord& t1, Tile_coord& t2) {
	// Treat as cartesian coords.
	return Get_direction16(t1.ty - t2.ty, t2.tx - t1.tx);
}

/**
 *  Initialize path & add to time queue.
 */

void Projectile_effect::init(
		const Tile_coord& s,    // Source,
		const Tile_coord& d     // Destination.
) {
	no_blocking              = false;    // We'll check the ammo & weapon.
	const Weapon_info* winfo = ShapeID::get_info(weapon).get_weapon_info();
	if (winfo) {
		no_blocking = no_blocking || winfo->no_blocking();
		if (speed < 0) {
			speed = winfo->get_missile_speed();
		}
		autohit = winfo->autohits();
	}
	if (speed < 0) {
		speed = 4;
	}
	const Ammo_info* ainfo
			= ShapeID::get_info(projectile_shape).get_ammo_info();
	if (ainfo) {
		no_blocking = no_blocking || ainfo->no_blocking();
		autohit     = autohit || ainfo->autohits();
	}
	const Game_object_shared att_obj = attacker.lock();
	const Game_object_shared tgt_obj = target.lock();
	if (att_obj) {    // Try to set start better.
		const int dir = tgt_obj ? att_obj->get_direction(tgt_obj.get())
								: att_obj->get_direction(d);
		pos           = att_obj->get_missile_tile(dir);
	} else {
		pos = s;    // Get starting position.
	}

	Tile_coord dst = d;
	if (tgt_obj) {    // Try to set end better.
		dst = tgt_obj->get_center_tile();
	} else {
		dst.tz = pos.tz;
	}
	path = new Zombie();    // Create simple pathfinder.
	// Find path.  Should never fail.
	const bool explodes
			= (winfo && winfo->explodes()) || (ainfo && ainfo->explodes());
	if (explodes && ainfo && ainfo->is_homing()) {
		path->NewPath(pos, pos, nullptr);    // A bit of a hack, I know...
	} else {
		path->NewPath(pos, dst, nullptr);
		if (att_obj) {
			// Getprojectile  out of shooter's volume.
			const Block vol = att_obj->get_block();
			Tile_coord  t;
			bool        done;
			while (path->GetNextStep(t, done)) {
				if (!vol.has_world_point(t.tx, t.ty, t.tz)) {
					break;
				}
			}
			pos = t;
		}
	}
	const int sprite_shape = sprite.get_shapenum();
	set_sprite_shape(sprite_shape);
	// Start after a slight delay.
	gwin->get_tqueue()->add(Game::get_ticks(), this, gwin->get_std_delay() / 2);
}

void Projectile_effect::set_sprite_shape(int s) {
	if (s < 0) {
		skip_render = true;
		sprite.set_shape(s);
		sprite.set_frame(0);
		return;
	}
	sprite.set_shape(s);
	frames = sprite.get_num_frames();
	if (frames >= 24) {    // Use frames 8-23, for direction
		//   going clockwise from North.
		Tile_coord src  = path->get_src();
		Tile_coord dest = path->get_dest();
		const int  dir  = Get_dir16(src, dest);
		sprite.set_frame(8 + dir);
	}
	// else if (frames == 1 && sprite.get_shapenum() != 704)
	else if (frames == 1 && sprite.get_info().is_explosive()) {
		sprite.set_frame(0);    // (Don't show powder keg!)
	} else {
		skip_render = true;    // We just won't show it.
	}
	add_dirty();    // Paint immediately.
}

/**
 *  Create a projectile animation.
 */
Projectile_effect::Projectile_effect(
		Game_object* att,       // Source of spell/attack.
		Game_object* to,        // End here, 'attack' it with shape.
		int          weap,      // Weapon (bow, gun, etc.) shape.
		int          proj,      // Projectile shape # in 'shapes.vga'.
		int          spr,       // Shape to render on-screen or -1 for none.
		int          attpts,    // Attack points of projectile.
		int          spd        // Projectile speed, or -1 to use default.
		)
		: attacker(weak_from_obj(att)), target(weak_from_obj(to)), weapon(weap),
		  projectile_shape(proj), sprite(spr, 0), return_path(false),
		  skip_render(spr < 0), speed(spd), attval(attpts), autohit(false) {
	init(att->get_tile(), to->get_tile());
}

/**
 *  Constructor used by missile eggs & fire_projectile intrinsic.
 */

Projectile_effect::Projectile_effect(
		Game_object*      att,     // Source of spell/attack.
		const Tile_coord& d,       // End here.
		int               weap,    // Weapon (bow, gun, etc.) shape.
		int               proj,    // Projectile shape # in 'shapes.vga'.
		int               spr,     // Shape to render on-screen or -1 for none.
		int               attpts,    // Attack points of projectile.
		int               spd,       // Projectile speed, or -1 to use default.
		bool              retpath    // Return of a boomerang.
		)
		: attacker(weak_from_obj(att)), weapon(weap), projectile_shape(proj),
		  sprite(spr, 0), return_path(retpath), skip_render(spr < 0),
		  speed(spd), attval(attpts), autohit(false) {
	init(att->get_tile(), d);
}

/*
 *  Another used by missile eggs and for 'boomerangs'.
 */

Projectile_effect::Projectile_effect(
		const Tile_coord& s,       // Start here.
		Game_object*      to,      // End here, 'attack' it with shape.
		int               weap,    // Weapon (bow, gun, etc.) shape.
		int               proj,    // Projectile shape # in 'shapes.vga'.
		int               spr,     // Shape to render on-screen or -1 for none.
		int               attpts,    // Attack points of projectile.
		int               spd,       // Projectile speed, or -1 to use default.
		bool              retpath    // Return of a boomerang.
		)
		: target(weak_from_obj(to)), weapon(weap), projectile_shape(proj),
		  sprite(spr, 0), return_path(retpath), skip_render(spr < 0),
		  speed(spd), attval(attpts), autohit(false) {
	init(s, to->get_tile());
}

/**
 *  Delete.
 */

Projectile_effect::~Projectile_effect() {
	delete path;
}

/**
 *  Add a dirty rectangle for the current position and frame.
 */

inline void Projectile_effect::add_dirty() {
	if (skip_render) {
		return;
	}
	Shape_frame* shape = sprite.get_shape();
	// Force repaint of prev. position.
	const int liftpix = pos.tz * c_tilesize / 2;
	gwin->add_dirty(gwin->clip_to_win(
			gwin->get_shape_rect(
						shape,
						(pos.tx - gwin->get_scrolltx()) * c_tilesize - liftpix,
						(pos.ty - gwin->get_scrollty()) * c_tilesize - liftpix)
					.enlarge(c_tilesize / 2)));
}

/**
 *  See if something was hit.
 *
 *  @return target hit, or 0.
 */

inline Game_object_shared Find_target(Game_window* gwin, Tile_coord pos) {
	ignore_unused_variable_warning(gwin);
	if (pos.tz % 5 == 0) {    // On floor?
		pos.tz++;             // Look up 1 tile.
	}
	const Tile_coord dest = pos;    // This gets modified.
	if (!Map_chunk::is_blocked(pos, 1, MOVE_FLY, 0) && dest == pos) {
		return nullptr;
	}
	Game_object* ptr = Game_object::find_blocking(pos);
	return shared_from_obj(ptr);
}

/**
 *  Animation.
 */

void Projectile_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const int delay = gwin->get_std_delay() / 2;
	add_dirty();    // Force repaint of old pos.
	const Weapon_info* winf = ShapeID::get_info(weapon).get_weapon_info();
	if (winf && winf->get_rotation_speed()) {
		// The missile rotates (such as axes/boomerangs)
		const int new_frame
				= sprite.get_framenum() + winf->get_rotation_speed();
		sprite.set_frame(
				new_frame > 23 ? ((new_frame - 8) % 16) + 8 : new_frame);
	}
	bool                     path_finished = false;
	const Game_object_shared att_obj       = attacker.lock();
	Game_object_shared       tgt_obj       = target.lock();
	for (int i = 0; i < speed; i++) {
		// This speeds up the missile.
		path_finished = !path->GetNextStep(pos)
						||    // Get next spot.
							  // If missile egg, detect target.
						(!tgt_obj && !no_blocking
						 && (tgt_obj = Find_target(gwin, pos)) != nullptr);
		if (path_finished) {
			target = Game_object_weak(tgt_obj);
			break;
		}
	}
	const Ammo_info* ainf = ShapeID::get_info(projectile_shape).get_ammo_info();
	if (path_finished) {
		// Done?
		const bool explodes
				= (winf && winf->explodes()) || (ainf && ainf->explodes());
		if (return_path) {    // Returned a boomerang?
			const Ireg_game_object_shared obj
					= gmap->create_ireg_object(sprite.get_shapenum(), 0);
			if (!tgt_obj || !tgt_obj->add(obj.get())) {
				obj->set_flag(Obj_flags::okay_to_take);
				obj->set_flag(Obj_flags::is_temporary);
				obj->move(pos.tx, pos.ty, pos.tz, -1);
			}
		} else if (explodes) {    // Do this here (don't want to explode
			// returning weapon).
			Tile_coord offset;
			if (tgt_obj) {
				offset = Tile_coord(
						0, 0, tgt_obj->get_info().get_3d_height() / 2);
			} else {
				offset = Tile_coord(0, 0, 0);
			}
			if (ainf && ainf->is_homing()) {
				eman->add_effect(std::make_unique<Homing_projectile>(
						weapon, att_obj.get(), tgt_obj.get(), pos,
						pos + offset));
			} else {
				eman->add_effect(std::make_unique<Explosion_effect>(
						pos + offset, nullptr, 0, weapon, projectile_shape,
						att_obj.get()));
			}
			target = Game_object_weak();    // Takes care of attack.
		} else {
			// Not teleported away ?
			const bool returns
					= (winf && winf->returns()) || (ainf && ainf->returns());
			bool hit = false;
			if (tgt_obj && att_obj != tgt_obj &&
				// Aims for center tile, so check center tile.
				tgt_obj->get_center_tile().distance(pos) < 3) {
				hit = autohit || tgt_obj->try_to_hit(att_obj.get(), attval);
				if (hit) {
					tgt_obj->play_hit_sfx(weapon, true);
					tgt_obj->attacked(
							att_obj.get(), weapon, projectile_shape, false);
				}
			} else {
				// Hack warning: this exists solely to make Mind Blast (SI)
				// work as it does in the original when you target the
				// avatar with the spell.
				if (winf && winf->get_usecode() > 0) {
					ucmachine->call_usecode(
							winf->get_usecode(), nullptr,
							Usecode_machine::weapon);
				}
			}
			if (returns && att_obj &&    // boomerangs
				att_obj->distance(pos) < 50) {
				// not teleported away
				auto proj = std::make_unique<Projectile_effect>(
						pos, att_obj.get(), weapon, projectile_shape,
						sprite.get_shapenum(), attval, speed, true);
				proj->set_speed(speed);
				proj->set_sprite_shape(sprite.get_shapenum());
				eman->add_effect(std::move(proj));
			} else {
				// See if we should drop projectile.
				bool drop = false;
				// Seems to match originals quite well.
				if (!winf) {
					drop = true;
				} else if (ainf) {
					const int ammo = winf->get_ammo_consumed();
					const int type = ainf->get_drop_type();
					drop           = (ammo >= 0 || ammo == -3)
						   && (type == Ammo_info::always_drop
							   || (!hit && type != Ammo_info::never_drop));
				}
				if (drop) {
					const Tile_coord dpos = Map_chunk::find_spot(
							pos, 3, sprite.get_shapenum(), 0, 1);
					if (dpos.tx != -1) {
						const Game_object_shared aobj
								= gmap->create_ireg_object(
										sprite.get_shapenum(), 0);
						if (!att_obj
							|| att_obj->get_flag(Obj_flags::is_temporary)) {
							aobj->set_flag(Obj_flags::is_temporary);
						}
						aobj->set_flag(Obj_flags::okay_to_take);
						aobj->move(dpos);
					}
				}
			}
		}
		add_dirty();
		skip_render    = true;
		auto ownHandle = eman->remove_effect(this);
		return;
	}
	add_dirty();    // Paint new spot/frame.
	// Add back to queue for next time.
	gwin->get_tqueue()->add(curtime + delay, this, udata);
}

/**
 *  Render.
 */

void Projectile_effect::paint() {
	if (skip_render) {
		return;
	}
	const int liftpix = pos.tz * c_tilesize / 2;
	sprite.paint_shape(
			(pos.tx - gwin->get_scrolltx()) * c_tilesize - liftpix
					- gwin->get_scrolltx_lo(),
			(pos.ty - gwin->get_scrollty()) * c_tilesize - liftpix
					- gwin->get_scrollty_lo());
}

/**
 *  Create a 'death vortex' or an 'energy mist'.
 */

Homing_projectile::Homing_projectile(    // A better name is welcome...
		int               shnum,         // The projectile shape.
		Game_object*      att,           // Who cast the spell.
		Game_object*      trg,           // What to aim for.
		const Tile_coord& sp,            // Where to start.
		const Tile_coord& tp             // Target pos, if trg isn't an actor.
		)
		: sprite(ShapeID::get_info(shnum).get_explosion_sprite(), 0,
				 SF_SPRITES_VGA),
		  next_damage_time(0),
		  sfx(ShapeID::get_info(shnum).get_explosion_sfx()), channel(-1) {
	weapon     = shnum;
	attacker   = weak_from_obj(att);
	pos        = sp;
	dest       = tp;
	target     = trg ? trg->as_actor() : nullptr;
	stationary = target == nullptr;    // If true, the sprite will 'park' at
									   // dest
	frames = sprite.get_num_frames();
	// Go for 20 seconds.
	stop_time = Game::get_ticks() + 20 * 1000;
	// Start immediately.
	gwin->get_tqueue()->add(Game::get_ticks(), this);
	channel = Audio::get_ptr()->play_sound_effect(sfx, pos, -1);
}

/**
 *  Add a dirty rectangle for the current position and frame.
 *
 *  @return     Width in pixels.
 */

inline int Homing_projectile::add_dirty() {
	Shape_frame* shape   = sprite.get_shape();
	const int    liftpix = pos.tz * c_tilesize / 2;
	gwin->add_dirty(gwin->clip_to_win(
			gwin->get_shape_rect(
						shape,
						(pos.tx - gwin->get_scrolltx()) * c_tilesize - liftpix,
						(pos.ty - gwin->get_scrollty()) * c_tilesize - liftpix)
					.enlarge(c_tilesize / 2)));
	return shape->get_width();
}

/**
 *  Animation.
 */

void Homing_projectile::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const int width = add_dirty();    // Repaint old area.

	if ((target && !target->is_dead()) || stationary) {
		// Move to target/destination if needed
		const Tile_coord tpos   = stationary ? dest : target->get_tile();
		const int        deltax = tpos.tx - pos.tx;
		const int        deltay = tpos.ty - pos.ty;
		const int        deltaz
				= tpos.tz
				  + (stationary ? 0 : target->get_info().get_3d_height() / 2)
				  - pos.tz;
		const int    absx = deltax >= 0 ? deltax : -deltax;
		const int    absy = deltay >= 0 ? deltay : -deltay;
		const int    absz = deltaz >= 0 ? deltaz : -deltaz;
		const uint32 dist = absx * absx + absy * absy + absz * absz;
		if (dist > 1) {
			if (deltax) {
				pos.tx += deltax / absx;
			}
			if (deltay) {
				pos.ty += deltay / absy;
			}
			if (deltaz) {
				pos.tz += deltaz / absz;
			}
		}
	} else {
		// The target has been killed; find another one
		Actor_vector npcs;    // Find NPC's.
		Game_object::find_nearby_actors(npcs, pos, -1, 30);
		Actor* nearest  = nullptr;
		uint32 bestdist = 100000;
		for (auto* npc : npcs) {
			if (!npc->is_in_party() && !npc->is_dead()
				&& (npc->get_effective_alignment() >= Actor::evil)) {
				const Tile_coord npos = npc->get_tile();
				const int        dx   = npos.tx - pos.tx;
				const int        dy   = npos.ty - pos.ty;
				const int        dz   = npos.tz - pos.tz;
				const uint32     dist = dx * dx + dy * dy + dz * dz;
				if (dist < bestdist) {
					bestdist = dist;
					nearest  = npc;
				}
			}
		}
		target = nearest;
	}
	if (curtime > next_damage_time) {    // Time to cause damage.
		// Do it every second.
		next_damage_time = curtime + 1000;
		Actor_vector npcs;    // Find NPC's.
		Game_object::find_nearby_actors(
				npcs, pos, -1, width / (2 * c_tilesize));
		for (auto* npc : npcs) {
			if (!npc->is_in_party()) {
				// Still powerful, but no longer overkill...
				// also makes the enemy react, which is good
				const Game_object_shared att_obj = attacker.lock();
				npc->attacked(att_obj.get(), weapon, weapon, true);
			}
		}
	}
	sprite.set_frame((sprite.get_framenum() + 1) % frames);

	add_dirty();                  // Paint new.
	if (curtime < stop_time) {    // Keep going?
		gwin->get_tqueue()->add(curtime + 100, this, udata);
		if (channel < 0) {
			return;
		}
		channel = Audio::get_ptr()->update_sound_effect(channel, pos);
	} else {
		if (channel >= 0) {
			AudioMixer::get_instance()->stopSample(channel);
			channel = -1;
		}
		gwin->set_all_dirty();
		auto ownHandle = eman->remove_effect(this);
		return;
	}
}

/**
 *  Render.
 */

void Homing_projectile::paint() {
	const int liftpix = pos.tz * c_tilesize / 2;
	sprite.paint_shape(
			(pos.tx - gwin->get_scrolltx()) * c_tilesize - liftpix
					- gwin->get_scrolltx_lo(),
			(pos.ty - gwin->get_scrollty()) * c_tilesize - liftpix
					- gwin->get_scrollty_lo());
}

/**
 *  Figure text position.
 */

TileRect Text_effect::Figure_text_pos() {
	const Game_object_shared item_obj = item.lock();
	if (item_obj) {
		Gump_manager* gumpman = gwin->get_gump_man();
		// See if it's in a gump.
		Gump* gump = gumpman->find_gump(item_obj.get());
		if (gump) {
			return gump->get_shape_rect(item_obj.get());
		} else {
			Game_object* outer = item_obj->get_outermost();
			if (!outer->get_chunk()) {
				return pos;
			}
			TileRect r = gwin->get_shape_rect(outer);
			r.x -= gwin->get_scrolltx_lo();
			r.y -= gwin->get_scrollty_lo();
			return r;
		}
	} else {
		int x;
		int y;
		gwin->get_shape_location(tpos, x, y);
		return TileRect(x, y, c_tilesize, c_tilesize);
	}
}

/**
 *  Add dirty rectangle for current position.
 */

void Text_effect::add_dirty() {
	// Repaint slightly bigger rectangle.
	const TileRect rect(
			pos.x - c_tilesize, pos.y - c_tilesize, width + 2 * c_tilesize,
			height + 2 * c_tilesize);
	gwin->add_dirty(gwin->clip_to_win(rect));
}

/**
 *  Initialize.
 */

void Text_effect::init() {
	set_always(true);    // Always execute in time queue, even
	//   when paused.
	width  = 8 + sman->get_text_width(0, msg.c_str());
	height = 8 + sman->get_text_height(0);
	add_dirty();    // Force first paint.
	// Start immediately.
	gwin->get_tqueue()->add(Game::get_ticks(), this);
	if (msg[0] == '@') {
		msg[0] = '"';
	}
	const int len = msg.size();
	if (msg[len - 1] == '@') {
		msg[len - 1] = '"';
	}
}

/**
 *  Create a text effect for a given object.
 */

Text_effect::Text_effect(
		const string& m,       // A copy is made.
		Game_object*  it,      // Item text is on, or null.
		Game_window*  gwin_    // Back-reference to gwin from Effects_manager
		)
		: gwin(gwin_), msg(m), item(weak_from_obj(it)), pos(Figure_text_pos()),
		  num_ticks(0) {
	init();
}

/**
 *  Create a text object.
 */

Text_effect::Text_effect(
		const string& m,      // A copy is made.
		int t_x, int t_y,     // Abs. tile coords.
		Game_window* gwin_    // Back-reference to gwin from Effects_manager
		)
		: gwin(gwin_), msg(m), tpos(t_x, t_y, 0), pos(Figure_text_pos()),
		  num_ticks(0) {
	init();
}

/**
 *
 */

void Text_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata       // Ignored.
) {
	ignore_unused_variable_warning(curtime, udata);
	if (++num_ticks == 10) {    // About 1-2 seconds.
		// All done.
		add_dirty();
		eman->remove_text_effect(this);
		return;
	}
	// Back into queue.
	gwin->get_tqueue()->add(Game::get_ticks() + gwin->get_std_delay(), this);

	update_dirty();
}

/**
 *  Reposition if necessary.
 */

void Text_effect::update_dirty() {
	// See if moved.
	const TileRect npos = Figure_text_pos();
	if (npos == pos) {    // No change?
		return;
	}
	add_dirty();    // Force repaint of old area.
	pos = npos;     // Then set to repaint new.
	add_dirty();
}

Text_effect::~Text_effect() {
	if (in_queue()) {
		gwin->get_tqueue()->remove(this);
	}
}

/**
 *  Render.
 */

void Text_effect::paint() {
	const char* ptr = msg.c_str();
	const int   len = strlen(ptr);
	sman->paint_text(0, ptr, len, pos.x, pos.y);
}

/**
 *  Init. a weather effect.
 */

Weather_effect::Weather_effect(
		int          duration,    // Length in game minutes.
		int          delay,       // Delay before starting.
		int          n,           // Weather number.
		Game_object* egg          // Egg that started it, or null.
		)
		: num(n) {
	if (egg) {
		eggloc = egg->get_tile();
	} else {
		eggloc = Tile_coord(-1, -1, -1);
	}
	stop_time = Game::get_ticks() + delay
				+ duration * gwin->get_std_delay() * ticks_per_minute;
	// Start immediately.
	gwin->get_tqueue()->add(Game::get_ticks() + delay, this);
}

/**
 *  Are we far enough away from this to cancel it?
 *  @param  avpos   Avatar's position.
 *  @param  dist    Distance in tiles to cancel at.
 */

bool Weather_effect::out_of_range(Tile_coord& avpos, int dist) {
	if (eggloc.tx == -1) {    // Not created by an egg?
		return false;
	}
	return eggloc.distance(avpos) >= dist;
}

/*
 *  A generic raindrop/snowflake/magic sparkle particle:
 */
class Particle : public ShapeID {
	long ax = -1, ay = -1;    // Coords. where drawn in abs. pixels.
	bool forward = true;

public:
	Particle() : ShapeID(0, -1, SF_SPRITES_VGA) {}

	// Move to next position.
	void move(long dx, long dy) {
		ax = dx;
		ay = dy;
	}

	long get_ax() const {
		return ax;
	}

	long get_ay() const {
		return ay;
	}

	bool get_forward() const {
		return forward;
	}

	void toggle_forward() {
		forward = !forward;
	}
};

class Particledrop {
protected:
	virtual void do_move(
			Particle& drop, int x, int y, int w, int h, int ascrollx,
			int ascrolly) {
		ignore_unused_variable_warning(drop, x, y, w, h, ascrollx, ascrolly);
	}

public:
	virtual ~Particledrop() = default;

	void move(Particle& drop, int scrolltx, int scrollty, int w, int h) {
		const int    frame    = drop.get_framenum();
		const uint32 ascrollx = scrolltx * static_cast<uint32>(c_tilesize);
		const uint32 ascrolly = scrollty * static_cast<uint32>(c_tilesize);
		const int    ax       = drop.get_ax();
		const int    ay       = drop.get_ay();
		const int    x        = ax - ascrollx;
		const int    y        = ay - ascrolly;
		// Still on screen?  Restore pix.
		if (frame >= 0 && x >= 0 && y >= 0 && x < w && y < h) {
			Game_window* gwin = Game_window::get_instance();
			gwin->add_dirty(gwin->clip_to_win(
					gwin->get_shape_rect(drop.get_shape(), x, y)
							.enlarge(c_tilesize / 2)));
		}
		do_move(drop, x, y, w, h, ascrollx, ascrolly);
	}

	void paint(Particle& drop, int scrolltx, int scrollty, int w, int h) {
		const uint32 ascrollx = scrolltx * static_cast<uint32>(c_tilesize);
		const uint32 ascrolly = scrollty * static_cast<uint32>(c_tilesize);
		const int    ax       = drop.get_ax();
		const int    ay       = drop.get_ay();
		const int    x        = ax - ascrollx;
		const int    y        = ay - ascrolly;
		// Still on screen?  Restore pix.
		if (x >= 0 && y >= 0 && x < w && y < h) {
			Game_window* gwin = Game_window::get_instance();
			drop.paint_shape(x, y);
			gwin->add_dirty(gwin->clip_to_win(
					gwin->get_shape_rect(drop.get_shape(), x, y)
							.enlarge(c_tilesize / 2)));
		}
	}
};

template <int fra0, int fraN, bool randomize>
static inline void set_frame(Particle& drop) {
	int frame = drop.get_framenum();
	if (frame < 0) {
		if (randomize) {
			const int dir = rand() % 2;
			if (dir) {
				drop.toggle_forward();
			}
			frame = fra0 + rand() % (fraN - fra0) + dir;
		} else {
			frame = fra0;
		}
	} else if (drop.get_forward()) {
		if (++frame == fraN) {
			drop.toggle_forward();
		}
	} else if (--frame == fra0) {
		drop.toggle_forward();
	}
	drop.set_frame(frame);
}

template <int fra0, int fraN, int delta, bool randomize>
class Basicdrop : public Particledrop {
protected:
	void do_move(
			Particle& drop, int x, int y, int w, int h, int ascrollx,
			int ascrolly) override {
		set_frame<fra0, fraN, randomize>(drop);
		// Time to restart?
		if (x < 0 || x >= w || y < 0 || y >= h
			|| (delta == 0 && drop.get_framenum() == fraN)) {
			const int r = rand();
			drop.move(ascrollx + r % (w - w / 8), ascrolly + r % (h - h / 4));
		} else {    // Next spot.
			drop.move(drop.get_ax() + delta, drop.get_ay() + delta);
		}
	}
};

// This looks slightly cooler:
// using Raindrop = Basicdrop< 3, 6, 6, false>;
using Raindrop  = Basicdrop<3, 7, 6, false>;
using Snowflake = Basicdrop<13, 20, 1, false>;
using Sparkle   = Basicdrop<21, 27, 0, true>;    // delta is zero as sparkles
												 // should not fall

/*
 *  Raining.
 */
#define MAXDROPS 200

template <typename Functor>
class Rain_effect : public Weather_effect {
protected:
	Particle drops[MAXDROPS];    // Drops moving down the screen.
	int      num_drops;          // # to actually use.
	bool     gradual;
	Functor  do_drop;        // Controls how drops move.
	int      weatherType;    // check weather type to show sparkle indoors

	void change_ndrops(unsigned long curtime) {
		if (!gradual) {
			return;
		}
		if ((curtime > stop_time - 2500) && num_drops) {
			// End gradually.
			num_drops -= (rand() % 15);
			if (num_drops < 0) {
				num_drops = 0;
			}
		} else if (gradual && curtime < stop_time) {    // Keep going?
			// Start gradually.
			if (num_drops < MAXDROPS) {
				num_drops += (rand() % 5);
			}
			if (num_drops > MAXDROPS) {
				num_drops = MAXDROPS;
			}
		}
	}

public:
	Rain_effect(
			int duration, int delay = 0, int ndrops = MAXDROPS, int n = -1,
			Game_object* egg = nullptr)
			: Weather_effect(duration, delay, n, egg), num_drops(ndrops),
			  gradual(ndrops == 0) {
		weatherType = n;
	}

	// Execute when due.
	void handle_event(
			unsigned long curtime,    // Current time of day.
			uintptr       udata) override {
		// Gradual start/end.
		change_ndrops(curtime);

		// we want to display the magic sparkle effect indoors however
		if ((!gwin->is_main_actor_inside() && !gumpman->showing_gumps(true))
			|| weatherType == 3) {
			// Don't show rain inside buildings!
			Image_window8* win = gwin->get_win();
			const int      w   = win->get_game_width();
			const int      h   = win->get_game_height();
			// Get transform table.
			const int scrolltx = gwin->get_scrolltx();
			const int scrollty = gwin->get_scrollty();
			// Move drops.
			for (int i = 0; i < num_drops; i++) {
				do_drop.move(drops[i], scrolltx, scrollty, w, h);
			}
			gwin->set_painted();
		}
		if (curtime >= stop_time) {
			gwin->set_all_dirty();
			eman->remove_effect(this);
			return;
		}
		gwin->get_tqueue()->add(curtime + 100, this, udata);
	}

	// Render.
	void paint() override {
		if (gwin->is_main_actor_inside() && weatherType != 3) {
			return;    // Inside.
		}
		// Get transform table.
		const int      scrolltx = gwin->get_scrolltx();
		const int      scrollty = gwin->get_scrollty();
		Image_window8* win      = gwin->get_win();
		const int      w        = win->get_game_width();
		const int      h        = win->get_game_height();
		for (int i = 0; i < num_drops; i++) {
			do_drop.paint(drops[i], scrolltx, scrollty, w, h);
		}
		gwin->set_painted();
	}
};

/**
 *  End of lightning.
 */

Lightning_effect::~Lightning_effect() {
	if (flashing) {    // Be sure palette is restored.
		Game_window::get_instance()->get_clock()->set_palette();
	}
}

/**
 *  Lightning.
 */

void Lightning_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const int r     = rand();    // Get a random #.
	int       delay = 100;       // Delay for next time.
	if (flashing) {              // Just turned white?  Restore.
		gclock->set_palette();
		flashing = false;
		active   = false;
		if (curtime >= stop_time) {
			// Time to stop.
			eman->remove_effect(this);
			return;
		}
		if (r % 50 == 0) {    // Occassionally flash again.
			delay = (1 + r % 7) * 40;
		} else {    // Otherwise, wait several secs.
			delay = (4000 + r % 3000);
		}
	} else if (
			(fromusecode || !gwin->is_in_dungeon())
			&& !active) {    // Time to flash.
		// Play thunder.
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(62));
		active   = true;
		flashing = true;
		gwin->get_pal()->set(PALETTE_LIGHTNING);
		delay = (1 + r % 2) * 25;
	}
	gwin->get_tqueue()->add(curtime + delay, this, udata);
}

/**
 *  Start a storm.
 */

Storm_effect::Storm_effect(
		int          duration,    // In game minutes.
		int          delay,       // In msecs.
		Game_object* egg          // Egg that caused it, or null.
		)
		: Weather_effect(duration, delay, 2, egg), start(true) {
	// Start raining soon.
	eman->add_effect(std::make_unique<Clouds_effect>(duration + 1, delay));
	const int rain_delay = 20 + rand() % 1000;
	eman->add_effect(std::make_unique<Rain_effect<Raindrop>>(
			duration + 2, rain_delay, 0));
	const int lightning_delay = rain_delay + rand() % 500;
	eman->add_effect(
			std::make_unique<Lightning_effect>(duration - 2, lightning_delay));
}

/**
 *  Start/end of storm.
 */
void Storm_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	ignore_unused_variable_warning(curtime);
	if (start) {
		start = false;
		// Nothing more to do but end.
		gwin->get_tqueue()->add(stop_time, this, udata);
	} else {    // Must be time to stop.
		auto ownHandle = eman->remove_effect(this);
		return;
	}
}

/**
 *  Start a snowstorm.
 */

Snowstorm_effect::Snowstorm_effect(
		int          duration,    // In game minutes.
		int          delay,       // In msecs.
		Game_object* egg          // Egg that caused it, or null.
		)
		: Weather_effect(duration, delay, 1, egg), start(true) {
	// Start snowing soon.
	eman->add_effect(std::make_unique<Clouds_effect>(duration + 1, delay));
	eman->add_effect(std::make_unique<Rain_effect<Snowflake>>(
			duration + 2, 20 + rand() % 1000, 0));
}

/**
 *  Start/end of snowstorm.
 */
void Snowstorm_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	ignore_unused_variable_warning(curtime);
	if (start) {
		start = false;
		// Nothing more to do but end.
		gwin->get_tqueue()->add(stop_time, this, udata);
	} else {    // Must be time to stop.
		auto ownHandle = eman->remove_effect(this);
		return;
	}
}

/**
 *  Start anti magic storm.
 */

Sparkle_effect::Sparkle_effect(
		int          duration,    // In game minutes.
		int          delay,       // In msecs.
		Game_object* egg          // Egg that caused it, or null.
		)
		: Weather_effect(duration, delay, 3, egg), start(true) {
	// Start snowing soon.

	eman->add_effect(std::make_unique<Rain_effect<Sparkle>>(
			duration, delay, MAXDROPS / 6, 3, egg));
}

/**
 *  Sparkles (in Ambrosia and generators).
 */

void Sparkle_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	ignore_unused_variable_warning(curtime);
	if (start) {
		start = false;
		// Nothing more to do but end.
		gwin->get_tqueue()->add(stop_time, this, udata);
	} else {    // Must be time to stop.
		auto ownHandle = eman->remove_effect(this);
		return;
	}
}

/**
 *  Fog.
 */

Fog_effect::~Fog_effect() {
	gclock->set_fog(false);
}

Fog_effect::Fog_effect(
		int          duration,    // In game minutes.
		int          delay,       // In msecs.
		Game_object* egg          // Egg that caused it, or null.
		)
		: Weather_effect(duration, delay, 4, egg), start(true) {
	// SI adds sparkle/raindrops to the fog palette shift
	// let's do that for all games
	const int rain_delay = 250 + rand() % 1000;
	eman->add_effect(std::make_unique<Rain_effect<Sparkle>>(
			duration, rain_delay, MAXDROPS / 2));
}

void Fog_effect::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(curtime);
	if (start) {
		start = false;
		// Nothing more to do but end.
		gwin->get_tqueue()->add(stop_time, this, udata);
		gclock->set_fog(true);
	} else {    // Must be time to stop.
		auto ownHandle = eman->remove_effect(this);
		return;
	}
}

/**
 *  Create a cloud.
 */

const int CLOUD = 2;    // Shape #.

Cloud::Cloud(
		short dx, short dy    // Deltas for movement.
		)
		: cloud(CLOUD, 0, SF_SPRITES_VGA), wx(0), wy(0), deltax(dx), deltay(dy),
		  count(-1) {
	Game_window* gwin = Game_window::get_instance();
	// Get abs. values.
	const int adx = deltax > 0 ? deltax : -deltax;
	const int ady = deltay > 0 ? deltay : -deltay;
	if (adx < ady) {
		max_count = 2 * gwin->get_height() / ady;
	} else {
		max_count = 2 * gwin->get_width() / adx;
	}
	start_time = 0;
}

/**
 *  Set starting screen position according to direction.
 */

void Cloud::set_start_pos(
		Shape_frame* shape, int w, int h,    // Dims. of window.
		int& x, int& y                       // Screen pos. returned.
) {
	if (!deltax) {    // Special cases first.
		x = rand() % w;
		y = deltay > 0 ? -shape->get_ybelow() : h + shape->get_yabove();
		return;
	}
	if (!deltay) {
		y = rand() % h;
		x = deltax > 0 ? -shape->get_xright() : w + shape->get_xleft();
		return;
	}
	const int halfp = w + h;             // 1/2 perimeter.
	const int r     = rand() % halfp;    // Start on one of two sides.
	if (r > h) {                         // Start on top/bottom.
		x = r - h;
		y = deltay > 0 ? -shape->get_ybelow() : h + shape->get_yabove();
		return;
	}
	y = r;               // On left or right side.
	if (deltax > 0) {    // Going right?
		x = -shape->get_xright();
	} else {    // Going left?
		x = w + shape->get_xleft();
	}
}

/**
 *  Move cloud
 */

inline void Cloud::next(
		Game_window*  gwin,       // Game window.
		unsigned long curtime,    // Current time of day.
		int w, int h              // Dims. of window.
) {
	if (curtime < start_time) {
		return;    // Not yet.
	}
	// Get top-left world pos.
	const long   scrollx = gwin->get_scrolltx() * c_tilesize;
	const long   scrolly = gwin->get_scrollty() * c_tilesize;
	Shape_frame* shape   = cloud.get_shape();
	gwin->add_dirty(gwin->clip_to_win(
			gwin->get_shape_rect(shape, wx - scrollx, wy - scrolly)
					.enlarge(c_tilesize / 2)));
	if (count <= 0) {    // Time to restart?
		// Set start time randomly.
		start_time = curtime + 2000 * randcnt + rand() % 2000;
		randcnt    = (randcnt + 1) % 4;
		start_time = Game::get_ticks() + 2000 * randcnt + rand() % 500;
		count      = max_count;
		cloud.set_frame(rand() % cloud.get_num_frames());
		int x;
		int y;    // Get screen pos.
		set_start_pos(shape, w, h, x, y);
		wx = x + scrollx;
		wy = y + scrolly;
	} else {
		wx += deltax;
		wy += deltay;
		count--;
	}
	gwin->add_dirty(gwin->clip_to_win(
			gwin->get_shape_rect(shape, wx - scrollx, wy - scrolly)
					.enlarge(c_tilesize / 2)));
}

/**
 *  Paint cloud.
 */

void Cloud::paint() {
	Game_window* gwin = Game_window::get_instance();
	if (count > 0) {    // Might not have been started.
		cloud.paint_shape(
				wx - gwin->get_scrolltx() * c_tilesize
						- gwin->get_scrolltx_lo(),
				wy - gwin->get_scrollty() * c_tilesize
						- gwin->get_scrollty_lo());
	}
}

/**
 *  Create a few clouds to float across the screen.
 */

Clouds_effect::Clouds_effect(
		int          duration,    // In game minutes.
		int          delay,       // In msecs.
		Game_object* egg,         // Egg that caused it, or null.
		int          n)
		: Weather_effect(duration, delay, n, egg), overcast(n != 6) {
	Game_clock* gclock = Game_window::get_instance()->get_clock();
	if (overcast) {
		gclock->set_overcast(true);
	} else {
		gclock->set_overcast(false);
	}

	num_clouds = 2 + rand() % 5;    // Pick #.
	if (overcast) {
		num_clouds += rand() % 2;
	}
	// Figure wind direction.
	int dx = rand() % 5 - 2;
	int dy = rand() % 5 - 2;
	if (!dx && !dy) {
		dx = 1 + rand() % 2;
		dy = 1 - rand() % 3;
	}
	for (int i = 0; i < num_clouds; i++) {
		// Modify speed of some.
		int deltax = dx;
		int deltay = dy;
		if (rand() % 2 == 0) {
			deltax += deltax / 2;
			deltay += deltay / 2;
		}
		clouds.emplace_back(std::make_unique<Cloud>(deltax, deltay));
	}
}

/**
 *  Cloud drift.
 */

void Clouds_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const int delay = 100;
	if (curtime >= stop_time) {
		// Time to stop.
		auto ownHandle = eman->remove_effect(this);
		gwin->set_all_dirty();
		return;
	}
	const int w = gwin->get_width();
	const int h = gwin->get_height();
	for (auto& cloud : clouds) {
		cloud->next(gwin, curtime, w, h);
	}
	gwin->get_tqueue()->add(curtime + delay, this, udata);
}

/**
 *  Render.
 */

void Clouds_effect::paint() {
	if (!gwin->is_main_actor_inside() && !gumpman->showing_gumps(true)) {
		for (auto& cloud : clouds) {
			cloud->paint();
		}
	}
}

/**
 *  Destructor.
 */

Clouds_effect::~Clouds_effect() {
	if (overcast) {
		Game_window::get_instance()->get_clock()->set_overcast(false);
	}
}

/**
 *  Shake the screen.
 */

void Earthquake::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	static int eqsoundonce;

	if (eqsoundonce != 1) {
		eqsoundonce = 1;
		// Play earthquake SFX once
		Audio::get_ptr()->play_sound_effect(Audio::game_sfx(60));
	}

	Game_window*  gwin = Game_window::get_instance();
	Image_window* win  = gwin->get_win();
	int           w    = win->get_game_width();
	int           h    = win->get_game_height();
	int           sx   = 0;
	int           sy   = 0;
	int           dx   = rand() % 9 - 4;
	int           dy   = rand() % 9 - 4;
	if (dx > 0) {
		w -= dx;
	} else {
		w += dx;
		sx -= dx;
		dx = 0;
	}
	if (dy > 0) {
		h -= dy;
	} else {
		h += dy;
		sy -= dy;
		dy = 0;
	}
	win->copy(sx, sy, w, h, dx, dy);
	gwin->set_painted();
	gwin->show();
	// Shake back.
	win->copy(dx, dy, w, h, sx, sy);
	if (++i < len) {    // More to do?  Put back in queue.
		gwin->get_tqueue()->add(curtime + 100, this, udata);
	} else {
		eqsoundonce = 0;
		delete this;
	}
}

/**
 *  Create a fire field that will last for about 4 seconds.
 */

Fire_field_effect::Fire_field_effect(
		const Tile_coord& t,    // Where to create it.
		int base_lifespan, bool endless) {
	const Game_object_shared field_obj = gmap->create_ireg_object(895, 0);
	field                              = Game_object_weak(field_obj);
	field_obj->set_flag(Obj_flags::is_temporary);
	field_obj->move(t.tx, t.ty, t.tz);
	if (endless || base_lifespan == 0) {
		remaining_ticks = 10000;    // Guess
	} else {
		remaining_ticks = (rand() % base_lifespan) + base_lifespan + 10;
	}
	constexpr static const int dir_frames[] = {2, 4, 1, 3};
	for (int dir = 0; dir < 4; dir++) {
		const Tile_coord         pos = t.get_neighbor(dir * 2);
		const Game_object_shared dust_obj
				= gmap->create_ireg_object(224, dir_frames[dir]);
		dust_obj->set_flag(Obj_flags::is_temporary);
		dust_obj->move(pos.tx, pos.ty, pos.tz);
	}
	const Game_object_shared dust_obj = gmap->create_ireg_object(224, 5);
	dust_obj->set_flag(Obj_flags::is_temporary);
	dust_obj->move(t.tx, t.ty, t.tz);
	gwin->get_tqueue()->add(Game::get_ticks() + 3000 + rand() % 2000, this);
}

/**
 *  Remove the field.
 */

void Fire_field_effect::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata) {
	const Game_object_shared field_obj = field.lock();
	if (remaining_ticks-- <= 0) {    // All done?
		if (field_obj) {
			field_obj->remove_this();
		}
		auto ownHandle = eman->remove_effect(this);
	} else {
		gwin->get_tqueue()->add(curtime + gwin->get_std_delay(), this, udata);
	}
}
