/*
 *  effects.h - Special effects.
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

#ifndef EFFECTS_H
#define EFFECTS_H 1

#include "rect.h"
#include "shapeid.h"
#include "singles.h"
#include "tiles.h"
#include "tqueue.h"

#include <list>
#include <memory>
#include <string>

class Xform_palette;
class PathFinder;
class Game_object;
class Game_window;
class Image_window8;
class Shape_frame;
class Actor;
class Special_effect;
class Text_effect;

using Game_object_weak = std::weak_ptr<Game_object>;

/*
 *  Manage special effects.
 */
class Effects_manager {
	Game_window* gwin;    // Handy pointer.
	std::list<std::unique_ptr<Special_effect>>
			effects;    // Sprite effects, projectiles, etc.
	std::list<std::unique_ptr<Text_effect>> texts;    // Text snippets.
public:
	Effects_manager(Game_window* g) : gwin(g) {}

	~Effects_manager();
	// Add text item.
	void add_text(const char* msg, Game_object* item);
	void add_text(const char* msg, int x, int y);
	void center_text(const char* msg);
	void add_effect(std::unique_ptr<Special_effect> effect);
	void remove_text_effect(Game_object* item);
	// Remove text item & delete it.
	std::unique_ptr<Special_effect> remove_effect(Special_effect* effect);
	void                            remove_text_effect(Text_effect* txt);
	void                            remove_all_effects(bool repaint = false);
	void                            remove_text_effects();
	void                            update_dirty_text();
	// Remove just the weather.
	void remove_weather_effects(int dist = 0);
	void remove_usecode_lightning();
	int  get_weather();    // Get # of last weather added.
	void paint();          // Draw all sprites/proj./weather.
	void paint_text();     // Draw text.
};

/*
 *  Base class for special-effects:
 */
class Special_effect : public Time_sensitive, public Game_singletons {
protected:
	Game_window* gwin;

public:
	Special_effect();
	~Special_effect() override;
	// Render.
	virtual void paint();

	virtual bool is_weather() const {    // Need to distinguish weather.
		return false;
	}

	virtual bool is_usecode_lightning() const {
		return false;
	}
};

/*
 *  An animation from 'sprites.vga':
 */
class Sprites_effect : public Special_effect {
protected:
	ShapeID sprite;
	// int sprite_num;       // Which one.
	// int frame_num;        // Current frame.
	int              frames;            // # frames.
	Game_object_weak item;              // Follows this around if not null.
	Tile_coord       pos;               // Position within world.
	int              xoff, yoff;        // Offset from position in pixels.
	int              deltax, deltay;    // Add to xoff, yoff on each frame.
	int              reps;              // Repetitions, or -1.
	void             add_dirty(int frnum);

public:
	Sprites_effect(
			int num, const Tile_coord& p, int dx = 0, int dy = 0, int delay = 0,
			int frm = 0, int rps = -1);
	Sprites_effect(
			int num, Game_object* it, int xf, int yf, int dx, int dy,
			int frm = 0, int rps = -1);
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
	// Render.
	void paint() override;
};

/*
 *  An explosion.
 */
class Explosion_effect : public Sprites_effect {
	Game_object_weak explode;       // What's exploding, or nullptr.
	int              weapon;        // Weapon to use for attack values.
	int              projectile;    // The projectile, for e.g., burst arrows
	int              exp_sfx;       // Explosion SFX.
	Game_object_weak attacker;      // Who is responsible for the explosion;
	// otherwise, explosion and delayed blast spells
	// would not trigger a response from target
public:
	Explosion_effect(
			const Tile_coord& p, Game_object* exp, int delay = 0, int weap = -1,
			int proj = -1, Game_object* att = nullptr);
	void handle_event(unsigned long time, uintptr udata) override;
};

/*
 *  A moving animation, followed by an 'attack' at the end, to
 *  implement Usecode intrinsic 0x41:
 */
class Projectile_effect : public Special_effect {
	Game_object_weak attacker;            // Source of attack/spell.
	Game_object_weak target;              // Target of path.
	int              weapon;              // Shape # of firing weapon.
	int              projectile_shape;    // Shape # of projectile/spell.
	ShapeID          sprite;              // Sprite shape to display.
	int              frames;              // # frames.
	PathFinder*      path;                // Determines path.
	Tile_coord       pos;                 // Current position.
	bool             return_path;         // Returning a boomerang.
	bool             no_blocking;         // Don't get blocked by things.
	bool             skip_render;         // For delayed blast.
	// Add dirty rectangle.
	int  speed;     // Missile speed.
	int  attval;    // Attack value of projectile.
	bool autohit;
	void add_dirty();
	void init(const Tile_coord& s, const Tile_coord& d);

public:
	Projectile_effect(
			Game_object* att, Game_object* to, int weap, int proj, int spr,
			int attpts = 60, int spd = -1);
	// For missile traps:
	Projectile_effect(
			Game_object* att, const Tile_coord& d, int weap, int proj, int spr,
			int attpts = 60, int spd = -1, bool retpath = false);
	Projectile_effect(
			const Tile_coord& s, Game_object* to, int weap, int proj, int spr,
			int attpts = 60, int spd = -1, bool retpath = false);
	~Projectile_effect() override;
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
	// Render.
	void paint() override;

	void set_speed(int s) {
		speed = s;
	}

	void set_sprite_shape(int s);
};

/*
 *  'Death Vortex' and 'Energy Mist', as the spells.  (Maybe this could be a
 *  Sprites_effect.)
 *  A better name is welcome...
 */
class Homing_projectile : public Special_effect {
	ShapeID          sprite;
	int              weapon;      // The weapon's shape number.
	Game_object_weak attacker;    // Who is responsible for the attack.
	Actor*           target;      // We'll follow this around if not nullptr.
	Tile_coord       pos;         // Current position.
	Tile_coord       dest;    // Destination pos for when there is no target.
	bool             stationary;    // If the effect should seek new targets.
	int              frames;        // # frames.
	uint32           stop_time;     // Time in 1/1000 secs. to stop.
	uint32           next_damage_time;    // When to check for NPC's beneath us.
	int              sfx;
	int              channel;
	int              add_dirty();

public:
	Homing_projectile(
			int shnum, Game_object* att, Game_object* trg, const Tile_coord& sp,
			const Tile_coord& tp);
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
	// Render.
	void paint() override;
};

/*
 *  A text object is a message that stays on the screen for just a couple
 *  of seconds.  These are all kept in a single list, and managed by
 *  Game_window.
 */
class Text_effect : public Time_sensitive, public Game_singletons {
	Game_window*     gwin;
	std::string      msg;     // What to print.
	Game_object_weak item;    // Item text is on.  May be null.
	Tile_coord       tpos;    // Position to display it at.
	TileRect         pos;
	short            width, height;    // Dimensions of rectangle.
	int              num_ticks;        // # ticks passed.
	void             add_dirty();
	void             init();
	TileRect         Figure_text_pos();

public:
	Text_effect(const std::string& m, Game_object* it, Game_window* gwin_);
	Text_effect(const std::string& m, int t_x, int t_y, Game_window* gwin_);
	// At timeout, remove from screen.
	void handle_event(unsigned long curtime, uintptr udata) override;
	// Render.
	virtual void paint();

	// Check for matching item.
	bool is_text(Game_object* it) {
		return it == item.lock().get();
	}

	virtual void update_dirty();
	~Text_effect() override;
};

/*
 *  Weather.
 */
class Weather_effect : public Special_effect {
protected:
	uint32     stop_time;    // Time in 1/1000 secs. to stop.
	int        num;          // Weather ID (0-6), or -1.
	Tile_coord eggloc;       // Location of egg that started this.
public:
	Weather_effect(int duration, int delay, int n, Game_object* egg = nullptr);
	// Avatar out of range?
	bool out_of_range(Tile_coord& avpos, int dist);

	bool is_weather() const override {
		return true;
	}

	int get_num() {
		return num;
	}
};

/*
 *  Fog.
 */
class Fog_effect : public Weather_effect {
	bool start;

public:
	Fog_effect(int duration, int delay = 0, Game_object* egg = nullptr);
	~Fog_effect() override;
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Lightning.
 */
class Lightning_effect : public Weather_effect {
	static int active;      // Just want one at a time.
	bool       flashing;    // Lightning palette is set.
	bool       fromusecode;
	friend class Storm_effect;

public:
	Lightning_effect(int duration, int delay = 0, bool uc = false)
			: Weather_effect(duration, delay, -1), flashing(false),
			  fromusecode(uc) {}

	bool from_usecode() const {
		return fromusecode;
	}

	bool is_usecode_lightning() const override {
		return from_usecode();
	}

	~Lightning_effect() override;
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Storm.
 */
class Storm_effect : public Weather_effect {
	bool start;    // 1 to start storm.
public:
	Storm_effect(int duration, int delay = 0, Game_object* egg = nullptr);
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Snow storm.
 */
class Snowstorm_effect : public Weather_effect {
	bool start;    // 1 to start storm.
public:
	Snowstorm_effect(int duration, int delay = 0, Game_object* egg = nullptr);
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Ambrosia and Generators Sparkles that seem to hang in the air.
 */
class Sparkle_effect : public Weather_effect {
	bool start;    // 1 to start storm.
public:
	Sparkle_effect(int duration, int delay = 0, Game_object* egg = nullptr);
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  A single cloud (sprite shape 2):
 */
class Cloud {
	ShapeID    cloud;
	long       wx, wy;            // Position within world.
	short      deltax, deltay;    // How to move.
	int        count;             // Counts down to 0.
	int        max_count;
	uint32     start_time;    // When to start.
	static int randcnt;       // For generating random times.
	void       set_start_pos(Shape_frame* shape, int w, int h, int& x, int& y);

public:
	Cloud(short dx, short dy);
	// Move to next position & paint.
	void next(Game_window* gwin, unsigned long curtime, int w, int h);
	void paint();
};

/*
 *  Clouds.
 */
class Clouds_effect : public Weather_effect {
	int                                 num_clouds;
	std::vector<std::unique_ptr<Cloud>> clouds;
	bool                                overcast;

public:
	Clouds_effect(
			int duration, int delay = 0, Game_object* egg = nullptr,
			int n = -1);
	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
	// Render.
	void paint() override;
	~Clouds_effect() override;
};

/*
 *  Earthquakes.  +++++Might make this a Weather_effect.
 */
class Earthquake : public Time_sensitive {
	int len;    // From Usecode intrinsic.
	int i;      // Current index.
public:
	Earthquake(int l) : len(l), i(0) {}

	// Execute when due.
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  A fire field that dies out after a few seconds.
 */
class Fire_field_effect : public Special_effect {
	Game_object_weak field;    // What we create.
	int              remaining_ticks;

public:
	Fire_field_effect(const Tile_coord& t, int base_lifespan, bool endless);
	void handle_event(unsigned long curtime, uintptr udata) override;
};

#endif
