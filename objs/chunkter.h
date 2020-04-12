/**
 ** Chunkter.h - Chunk terrain (16x16 flat tiles) on the map.
 **
 ** Written: 7/6/01 - JSF
 **/

/*
Copyright (C) 2001 The Exult Team

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

#ifndef CHUNKTER_H
#define CHUNKTER_H

#include "shapeid.h"

class Image_buffer8;

/*
 *  The flat landscape, 16x16 tiles:
 */
class Chunk_terrain : public Game_singletons {
	ShapeID shapes[256];        // Id's.  The flat (non-RLE's) are
	//   rendered here, the others are
	//   turned into Game_objects in the
	//   chunks that point to us.
	ShapeID *undo_shapes;       // Set to prev. values when editing.
	int num_clients;        // # of Chunk's that point to us.
	bool modified;          // Changed (by map-editor).
	Image_buffer8 *rendered_flats;  // Flats rendered for entire chunk.
	// Most-recently used circular queue
	//   for rendered_flats:
	static Chunk_terrain *render_queue;
	static int queue_size;
	Chunk_terrain *render_queue_next, *render_queue_prev;
	//   Kept only for nearby chunks.
	void insert_in_queue();     // Queue methods.
	void remove_from_queue();
	// Create rendered_flats.
	void paint_tile(int tilex, int tiley);
	Image_buffer8 *render_flats();
	void free_rendered_flats();
public:
	// Create from 16x16x2 data:
	Chunk_terrain(const unsigned char *data, bool v2_chunks);
	// Copy-constructor:
	Chunk_terrain(const Chunk_terrain &c2);
	~Chunk_terrain();
	inline void add_client() {
		num_clients++;
	}
	inline void remove_client() {
		num_clients--;
	}
	inline bool is_modified() {
		return modified;
	}
	inline void set_modified(bool tf = true) {
		modified = tf;
	}
	// Get tile's shape ID.
	inline ShapeID get_flat(int tilex, int tiley) const {
		return shapes[16 * tiley + tilex];
	}

	inline Shape_frame *get_shape(int tilex, int tiley) {
		return shapes[16 * tiley + tilex].get_shape();
	}

	// Set tile's shape.
	void set_flat(int tilex, int tiley, const ShapeID& id);
	bool commit_edits();        // Commit changes.  Rets. true if
	//   edited.
	void abort_edits();     // Undo changes.
	Image_buffer8 *get_rendered_flats() {
		if (render_queue != this)// Not already first in queue?
			// Move to front of queue.
			insert_in_queue();
		return rendered_flats
		       ? rendered_flats : render_flats();
	}
	void render_all(int cx, int cy);// Render (in terrain-editing mode).
	// Write out to chunk.
	int write_flats(unsigned char *chunk_data, bool v2_chunks);
};



#endif
