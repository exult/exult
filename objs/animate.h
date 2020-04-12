/**
 ** Animate.h - Animated game objects.
 **
 ** Written: 7/27/2000 - JSF
 **/

/*
Copyright (C) 2000  Jeffrey S. Freedman

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

#ifndef INCL_ANIMATE
#define INCL_ANIMATE    1

#include "iregobjs.h"
#include "miscinf.h"

/*
 *  A class for playing sound effects that get updated by position
 *  and distance. Adds itself to time-queue, deletes itself when done.
 */
class Object_sfx : public Time_sensitive, public Game_singletons {
	Game_object_weak obj;   // Object that caused the sound.
	Tile_coord last_pos;
	int sfx;            // ID of sound effect being played.
	int channel;        // Channel of sfx being played.
	void stop_playing();
	Object_sfx(Game_object *o, int sfx);
protected:
	void dequeue() override;
public:
	static void Play(Game_object *o, int sfx, int delay = 20);
	void stop();
	int get_sfxnum() {
		return sfx;
	}
	void handle_event(unsigned long time, uintptr udata) override;
};

/*
 *  A class for playing sound effects when certain objects are nearby.
 */
class Shape_sfx : public Game_singletons {
	const Game_object *obj;       // Object that caused the sound.
	const SFX_info *sfxinf;
	int channel[2];         // ID of sound effect being played.
	int distance;           // Distance in tiles from Avatar.
	int dir;            // Direction (0-15) from Avatar.
	int last_sfx;       // For playing sequential sfx ranges.
	bool looping;       // If the SFX should loop until stopped.
public:
	// Create & start playing sound.
	Shape_sfx(Game_object *o)
		: obj(o), distance(0), last_sfx(-1) {
		channel[0] = channel[1] = -1;
		sfxinf = obj->get_info().get_sfx_info();
		if (sfxinf)
			last_sfx = 0;
		set_looping();  // To avoid including sfxinf.h.
	}
	int get_sfxnum() {
		return last_sfx;
	}
	int get_distance() {
		return distance;
	}
	void update(bool play); // Set to new object.
	void set_looping();
	void stop();
};

/*
 *  An animator:
 */
class Animator : public Time_sensitive, public Game_singletons {
protected:
	Game_object *obj;       // Object we're controlling.
	unsigned char deltax, deltay;   // If wiggling, deltas from
	//   original position.
	bool animating;         // 1 if animation turned on.
	Shape_sfx *objsfx;
	void start_animation();
public:
	Animator(Game_object *o)
		: obj(o), deltax(0), deltay(0), animating(false) {
		objsfx = new Shape_sfx(obj);
	}
	static Animator *create(Game_object *ob);
	~Animator() override;
	void want_animation() {     // Want animation on.
		if (!animating)
			start_animation();
	}
	void stop_animation() {
		animating = false;
	}
	int get_deltax() {
		return deltax;
	}
	int get_deltay() {
		return deltay;
	}
	virtual int get_framenum();
};

/*
 *  Animate by going through frames.
 */
class Frame_animator : public Animator {
	const Animation_info *aniinf;
	unsigned short first_frame; // Initial frame of animation cycle
	unsigned short currpos;         // Current position in the animation.
	unsigned short nframes;     // Number of frames in cycle.
	unsigned short frame_counter;       // When to increase frame.
	unsigned int created;       // Time created
	unsigned short last_shape;  // To check if we need to re init
	unsigned short last_frame;  // To check if we need to re init
	void Initialize();
public:
	Frame_animator(Game_object *o);
	int get_next_frame();
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
	int get_framenum() override {
		return obj->get_framenum();
	}
};

/*
 *  Just play SFX.
 */
class Sfx_animator : public Animator {
public:
	Sfx_animator(Game_object *o);
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
};

/*
 *  Animate by going through frames, but only do the lower frames once.
 */
class Field_frame_animator : public Frame_animator {
	bool activated;         // Time to check for damage.
public:
	friend class Field_object;
	Field_frame_animator(Game_object *o);
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
};

/*
 *  Animate by wiggling.
 */
class Wiggle_animator : public Animator {
public:
	Wiggle_animator(Game_object *o) : Animator(o)
	{  }
	// For Time_sensitive:
	void handle_event(unsigned long time, uintptr udata) override;
};

/*
 *  An object that cycles through its frames, or wiggles if just one
 *  frame.  The base class is for those in U7chunks.
 */
class Animated_object : public Terrain_game_object {
	Animator *animator;     // Controls animation.
public:
	Animated_object(int shapenum, int framenum, unsigned int tilex,
	                unsigned int tiley, unsigned int lft = 0);
	~Animated_object() override;
	// Render.
	void paint() override;
	// +++++Needed on this one:
	// Get coord. where this was placed.
	Tile_coord get_original_tile_coord() const override {
		return get_tile() +
		       Tile_coord(-animator->get_deltax(),
		                  -animator->get_deltay(), 0);
	}
};

/*
 *  An object that cycles through its frames, or wiggles if just one
 *  frame.  This is the IREG version.
 */
class Animated_ireg_object : public Ireg_game_object {
	Animator *animator;     // Controls animation.
public:
	Animated_ireg_object(int shapenum, int framenum, unsigned int tilex,
	                     unsigned int tiley, unsigned int lft = 0);
	~Animated_ireg_object() override;
	// Render.
	void paint() override;
	// Get coord. where this was placed.
	Tile_coord get_original_tile_coord() const override {
		return get_tile() +
		       Tile_coord(-animator->get_deltax(),
		                  -animator->get_deltay(), 0);
	}

	// Write out to IREG file.
	void write_ireg(ODataSource *out) override;
};

/*
 *  An object that cycles through its frames, or wiggles if just one
 *  frame.  This is the IFIX version.
 */
class Animated_ifix_object : public Ifix_game_object {
	Animator *animator;     // Controls animation.
public:
	Animated_ifix_object(int shapenum, int framenum, unsigned int tilex,
	                     unsigned int tiley, unsigned int lft = 0);
	~Animated_ifix_object() override;
	// Render.
	void paint() override;
	// Get coord. where this was placed.
	Tile_coord get_original_tile_coord() const override {
		return get_tile() +
		       Tile_coord(-animator->get_deltax(),
		                  -animator->get_deltay(), 0);
	}

	void write_ifix(ODataSource *ifix, bool v2) override;
};
#endif


