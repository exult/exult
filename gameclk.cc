/*
 *  gameclk.cc - Keep track of time.
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

#include "gameclk.h"

#include "actors.h"
#include "cheat.h"
#include "game.h"
#include "gamewin.h"

#include <iostream> /* Debugging. */

static inline bool is_dark_palette(int pal) {
	return pal == PALETTE_DUSK || pal == PALETTE_NIGHT;
}

static inline bool is_day_palette(int pal) {
	return pal == PALETTE_DAWN || pal == PALETTE_DAY;
}

static inline int get_time_palette(int hour, bool dungeon) {
	if (dungeon || hour < 5) {
		return PALETTE_NIGHT;
	} else if (hour == 5) {
		return PALETTE_DAWN;
	} else if (hour < 20) {
		return PALETTE_DAY;
	} else if (hour == 20) {
		return PALETTE_DUSK;
	} else {
		return PALETTE_NIGHT;
	}
}

static inline int get_final_palette(int pal, bool cloudy, bool foggy, int light, bool special, bool infravision, bool invisible) {
	if (invisible) {
		return PALETTE_INVISIBLE;
	}
	if (infravision) {
		return PALETTE_DAY;
	}
	if ((light || special) && is_dark_palette(pal)) {
		// Gump mode, or light spell?
		if (special) {
			return PALETTE_SPELL;
		} else if (light < 224) {    // Seems to match original
			return PALETTE_CANDLE;
		} else if (light < 640) {    // Seems to match original
			return PALETTE_SINGLE_LIGHT;
		} else {
			return PALETTE_MANY_LIGHTS;
		}
	} else if (is_day_palette(pal)) {
		if (foggy) {
			return PALETTE_FOG;
		} else if (cloudy) {
			return PALETTE_OVERCAST;
		}
	}
	return pal;
}

/*
 *  Set palette.
 */

void Game_clock::set_time_palette(bool force) {
	const Game_window* gwin       = Game_window::get_instance();
	const Actor*       main_actor = gwin->get_main_actor();

	// This is the information we will need to determine what palette/transition
	// we want.
	const bool invis           = main_actor && main_actor->get_flag(Obj_flags::invisible);
	const bool infra           = gwin->in_infravision();
	const bool invis_change    = invis && (force || !old_invisible);
	const bool infra_change    = !main_actor || (infra && (force || !old_infravision));
	const int  new_dungeon     = gwin->is_in_dungeon();
	int        new_palette     = get_time_palette(hour + 1, new_dungeon != 0);
	int        old_palette     = get_time_palette(hour, (dungeon != 255 ? dungeon : new_dungeon) != 0);
	const bool cloudy          = overcast > 0;
	const bool foggy           = fog > 0;
	const bool weather_change  = force || (cloudy != was_overcast) || (foggy != was_foggy);
	const bool light_sensitive = force || is_dark_palette(new_palette) || is_dark_palette(old_palette);
	const bool light_change    = light_sensitive
							  && (force || (light_source_level != old_light_level)
								  || (gwin->is_special_light() != old_special_light) || (new_dungeon != dungeon));

	// These are the final palettes we will use for checking.
	new_palette = get_final_palette(new_palette, cloudy, foggy, light_source_level, gwin->is_special_light(), infra, invis);
	old_palette = get_final_palette(
			old_palette, was_overcast, was_foggy, old_light_level, old_special_light, old_infravision, old_invisible);

	// Always update these variables.
	was_overcast      = cloudy;
	was_foggy         = foggy;
	old_light_level   = light_source_level;
	old_special_light = gwin->is_special_light();
	dungeon           = new_dungeon;
	old_invisible     = invis;
	old_infravision   = infra;

	auto apply_palette = [gwin](int palette) {
		auto* pal = gwin->get_pal();
		pal->set(palette);
		if (!pal->is_faded_out()) {
			pal->apply(false);
		}
	};
	if (invis_change || infra_change || gwin->get_pal()->is_faded_out()) {
		transition.reset();
		apply_palette(new_palette);
		return;
	}
	if (weather_change) {
		// TODO: Maybe implement smoother transition from
		// weather to/from dawn/sunrise/sundown/dusk.
		// Right now, it works like the original.
		transition
				= std::make_unique<Palette_transition>(old_palette, new_palette, hour, minute, ticks, 1, 20, hour, minute, ticks);
		return;
	}
	if (light_change) {
		transition.reset();
		apply_palette(new_palette);
		return;
	}
	if (transition) {
		if (transition->set_step(hour, minute, ticks)) {
			return;
		}
		transition.reset();
	}
	if (force || old_palette != new_palette) {    // Do we have a transition?
		transition = std::make_unique<Palette_transition>(
				old_palette, new_palette, hour, minute, ticks, ticks_per_minute, 60, hour, 0, 0);
	} else {
		apply_palette(new_palette);
	}
}

/*
 *  Set palette. Used for restoring a game.
 */

void Game_clock::set_palette() {
	// Update palette to new time.
	set_time_palette(false);
}

/*
 *  Resets palette.
 */

void Game_clock::reset_palette() {
	// Forcibly update palette to new time.
	set_time_palette(true);
}

/*
 *  Set the palette for a changed light source level.
 */

void Game_clock::set_light_source_level(int lev) {
	light_source_level = lev;
	set_time_palette(false);
}

/*
 *  Cloud cover.
 */

void Game_clock::set_overcast(bool onoff) {
	overcast += (onoff ? 1 : -1);
	set_time_palette(false);    // Update palette.
}

/*
 *  Fog.
 */

void Game_clock::set_fog(bool onoff) {
	fog += (onoff ? 1 : -1);
	if (hour < 6 || hour > 20) {
		fog = 0;    // Disable fog at night???
	}
	set_time_palette(false);    // Update palette.
}

/*
 *  Decrement food level and check hunger of the party members.
 */

void Game_clock::check_hunger() const {
	Game_window* gwin = Game_window::get_instance();
	Actor*       party[9];    // Get party + Avatar.
	const int    cnt = gwin->get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		party[i]->use_food();
	}
}

static void Check_freezing() {
	Game_window* gwin = Game_window::get_instance();
	// Avatar's flag applies to party.
	const bool freeze = gwin->get_main_actor()->get_flag(Obj_flags::freeze);
	Actor*     party[9];    // Get party + Avatar.
	const int  cnt = gwin->get_party(party, 1);
	for (int i = 0; i < cnt; i++) {
		party[i]->check_temperature(freeze);
	}
}

/*
 *  Increment clock.
 *
 *  FOR NOW, skipping call to mend_npcs().  Not sure...
 */

void Game_clock::increment(int num_minutes    // # of minutes to increment.
) {
	Game_window* gwin     = Game_window::get_instance();
	const int    old_hour = hour;    // Remember current 3-hour period.
	num_minutes += 7;                // Round to nearest 15 minutes.
	num_minutes -= num_minutes % 15;
	const long new_min = minute + num_minutes;
	hour += static_cast<short>(new_min / 60);    // Update hour.
	minute = static_cast<short>(new_min % 60);
	ticks  = 0;
	day += hour / 24;    // Update day.
	hour %= 24;

	// Update palette to new time.
	set_time_palette(false);
	// Check to see if we need to update the NPC schedules.
	if (hour != old_hour) {    // Update NPC schedules.
		gwin->schedule_npcs(hour);
	}
}

/*
 *  Advance clock.
 */

void Game_clock::handle_event(
		unsigned long curtime,    // Current time of day.
		uintptr       udata       // ->game window.
) {
	auto* gwin = reinterpret_cast<Game_window*>(udata);

	const int min_old  = minute;
	const int hour_old = hour;
	// Time stopped?  Don't advance.
	if (!gwin->is_time_stopped() && !cheat.in_map_editor()) {
		ticks += time_rate;
		minute += (ticks / ticks_per_minute);
		ticks %= ticks_per_minute;
		// ++++ TESTING
		// if (Game::get_game_type() == SERPENT_ISLE)
		// Only do every minute?
		if (min_old != minute) {
			Check_freezing();
		}
	}

	while (minute >= 60) {    // advance to the correct hour (and day)
		minute -= 60;
		if (++hour >= 24) {
			hour -= 24;
			day++;
		}
		// Testing.
		// set_time_palette(false);
		gwin->mend_npcs();    // Restore HP's each hour.
		check_hunger();       // Use food, and print complaints.
		gwin->schedule_npcs(hour);
	}

	if (transition && !transition->set_step(hour, minute, ticks)) {
		transition.reset();
		set_time_palette(false);
	} else if (hour != hour_old) {
		set_time_palette(false);
	}

	if ((hour != hour_old) || (minute / 15 != min_old / 15)) {
		COUT("Clock updated to " << hour << ':' << minute);
	}
	curtime += gwin->get_std_delay();
	tqueue->add(curtime, this, udata);
}

/*
 *  Fake an update to the next 3-hour period.
 */

void Game_clock::fake_next_period() {
	ticks  = 0;
	minute = 0;
	hour   = ((hour / 3 + 1) * 3);
	day += hour / 24;    // Update day.
	hour %= 24;
	Game_window* gwin = Game_window::get_instance();
	set_time_palette(false);
	check_hunger();
	gwin->schedule_npcs(hour);
	gwin->mend_npcs();    // Just do it once, cheater.
	COUT("The hour is now " << hour);
}
