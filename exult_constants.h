/*
 *  exult_constants.h - Some constants/macros that are used all over the code.
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

#ifndef EXULT_CONSTANTS_H
#define EXULT_CONSTANTS_H

/*
 *  Sizes:
 */
constexpr const int c_basetilesize = 8;       // A tile (shape) is 8x8 pixels.
constexpr const int c_tilesize = 8;   // A tile (shape) is 8x8 pixels.
constexpr const int c_num_tile_bytes = c_tilesize * c_tilesize;   // Total pixels per tile.
constexpr const int c_screen_tile_size = 320 / c_basetilesize; // Number of tiles in a 'screen'.
constexpr const int c_tiles_per_chunk = 16;   // A chunk is 16x16 tiles.
constexpr const int c_chunksize = 16 * 8;     // A chunk has 16 8x8 shapes.
constexpr const int c_num_schunks = 12;
constexpr const int c_num_chunks = 12 * 16;   // Total # of chunks in each dir.
constexpr const int c_chunks_per_schunk = 16; // # chunks in each superchunk.
constexpr const int c_tiles_per_schunk = 16 * 16; // # tiles in each superchunk.
// Total # tiles in each dir.:
constexpr const int c_num_tiles = c_tiles_per_chunk * c_num_chunks;

constexpr const int c_fade_in_time = 30;  // Time for fade in
constexpr const int c_fade_out_time = 30; // Time for fade out
constexpr const int c_std_delay = 200;    // Standard animation delay.  May want to
//   make this settable!

constexpr const int c_any_shapenum = -359;
constexpr const int c_any_qual = -359;
constexpr const int c_any_framenum = -359;
constexpr const int c_any_quantity = -359;

// Maximum number of shapes:
constexpr const int c_max_shapes = 2048;
constexpr const int c_occsize = c_max_shapes / 8 + ((c_max_shapes % 8) != 0 ? 1 : 0);

// Maximum number of global flags:
constexpr const int c_last_gflag = 2047;

constexpr const int MOVE_NODROP = (1<<3);
constexpr const int MOVE_FLY = (1<<4);
constexpr const int MOVE_LEVITATE = (MOVE_FLY|MOVE_NODROP);
constexpr const int MOVE_WALK = (1<<5);
constexpr const int MOVE_SWIM = (1<<6);
constexpr const int MOVE_ALL_TERRAIN = ((1<<5)|(1<<6));
constexpr const int MOVE_ETHEREAL = (1<<7);
constexpr const int MOVE_ALL = (MOVE_FLY|MOVE_WALK|MOVE_SWIM|MOVE_ETHEREAL);
constexpr const int MOVE_MAPEDIT = (1<<8);

//	Wrapping:
constexpr inline int INCR_CHUNK(int x) {
	return (x + 1) % c_num_chunks;
}
constexpr inline int DECR_CHUNK(int x) {
	return (x - 1 + c_num_chunks) % c_num_chunks;
}
constexpr inline int INCR_TILE(int x) {
	return (x + 1) % c_num_tiles;
}
constexpr inline int DECR_TILE(int x, int amt = 1) {
	return (x - amt + c_num_tiles) % c_num_tiles;
}
// Return x - y with wrapping.
constexpr inline int SUB_TILE(int x, int y) {
	int delta = x - y;
	return delta < -c_num_tiles / 2 ? delta + c_num_tiles :
	       delta >= c_num_tiles / 2 ? delta - c_num_tiles : delta;
}

// Debug
#ifdef DEBUG
#  define COUT(x)       do { std::cout << x << std::endl; std::cout.flush(); } while (0)
#  define CERR(x)       do { std::cerr << x << std::endl; std::cerr.flush(); } while (0)
#else
#  define COUT(x)       do { } while(0)
#  define CERR(x)       do { } while(0)
#endif

enum Exult_Game {
    NONE,
    BLACK_GATE,
    SERPENT_ISLE,
    EXULT_DEVEL_GAME,       // One that we develop.
    EXULT_MENU_GAME         // Game type for the exult menu
};

#endif
