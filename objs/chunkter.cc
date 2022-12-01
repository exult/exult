/**
 ** Chunkter.cc - Chunk terrain (16x16 flat tiles) on the map.
 **
 ** Written: 7/6/01 - JSF
 **/

/*
Copyright (C) 2001-2022 The Exult Team

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
#include "chunkter.h"
#include "gamewin.h"

#include <cstring>

Chunk_terrain *Chunk_terrain::render_queue = nullptr;
int Chunk_terrain::queue_size = 0;

/*
 *  Insert at start of render queue.  It may already be there, but it's
 *  assumed that it's already tested as not being at the start.
 */

void Chunk_terrain::insert_in_queue(
) {
	if (render_queue_next) {    // In queue already?
		// !!Assuming it's not at head!!
		render_queue_next->render_queue_prev = render_queue_prev;
		render_queue_prev->render_queue_next = render_queue_next;
	} else
		queue_size++;       // Adding, so increment count.
	if (!render_queue)      // First?
		render_queue_next = render_queue_prev = this;
	else {
		render_queue_next = render_queue;
		render_queue_prev = render_queue->render_queue_prev;
		render_queue_prev->render_queue_next = this;
		render_queue->render_queue_prev = this;
	}
	render_queue = this;
}

/*
 *  Remove from render queue.
 */

void Chunk_terrain::remove_from_queue(
) {
	if (!render_queue_next)
		return;         // Not in queue.
	queue_size--;
	if (render_queue_next == this)  // Only element?
		render_queue = nullptr;
	else {
		if (render_queue == this)
			render_queue = render_queue_next;
		render_queue_next->render_queue_prev = render_queue_prev;
		render_queue_prev->render_queue_next = render_queue_next;
	}
	render_queue_next = render_queue_prev = nullptr;
}

/*
 *  Paint a flat tile into our cached buffer.
 */

inline void Chunk_terrain::paint_tile(
    int tilex, int tiley        // Tile within chunk.
) {
	Shape_frame *shape = get_shape(tilex, tiley);
	if (shape && !shape->is_rle())      // Only do flat tiles.
		rendered_flats->copy8(shape->get_data(),
		                      c_tilesize, c_tilesize, tilex * c_tilesize,
		                      tiley * c_tilesize);
}

/*
 *  Create list for a given chunk.
 */

Chunk_terrain::Chunk_terrain(
    const unsigned char *data,        // Chunk data.
    bool v2_chunks          // 3 bytes/shape.
) : undo_shapes(nullptr),
	num_clients(0), modified(false), rendered_flats(nullptr),
	render_queue_next(nullptr), render_queue_prev(nullptr) {
	for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++)
		for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++) {
			int shnum;
			int frnum;
			if (v2_chunks) {
				shnum = data[0] + 256 * data[1];
				frnum = data[2];
				data += 3;
			} else {
				shnum = data[0] + 256 * (data[1] & 3);
				frnum = (data[1] >> 2) & 0x1f;
				data += 2;
			}
			const ShapeID id(shnum, frnum);
			shapes[16 * tiley + tilex] = id;
		}
}

/*
 *  Copy another.  The 'modified' flag is set to true.
 */

Chunk_terrain::Chunk_terrain(
    const Chunk_terrain &c2
) : undo_shapes(nullptr),
	num_clients(0), modified(true), rendered_flats(nullptr),
	render_queue_next(nullptr), render_queue_prev(nullptr) {
	for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++)
		for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++)
			shapes[16 * tiley + tilex] = c2.shapes[16 * tiley + tilex];
}

/*
 *  Clean up.
 */

Chunk_terrain::~Chunk_terrain(
) {
	delete [] undo_shapes;
	delete rendered_flats;
	remove_from_queue();
}


/*
 *  Set tile's shape.
 *  NOTE:  Set's 'modified' flag.
 */

void Chunk_terrain::set_flat(
    int tilex, int tiley,
    const ShapeID& id
) {
	if (!undo_shapes) {     // Create backup.
		undo_shapes = new ShapeID[256];
		std::memcpy(reinterpret_cast<char *>(undo_shapes),
		            reinterpret_cast<char *>(&shapes[0]),
		            sizeof(shapes));
	}
	shapes[16 * tiley + tilex] = id;
	modified = true;
}

/*
 *  Commit changes.
 *
 *  Output: True if this was edited, else false.
 */

bool Chunk_terrain::commit_edits(
) {
	if (!undo_shapes)
		return false;
	delete [] undo_shapes;
	undo_shapes = nullptr;
	render_flats();         // Update with new data.
	return true;
}

/*
 *  Undo changes.   Note:  We don't clear 'modified', since this could
 *  still have been moved to a different position.
 */

void Chunk_terrain::abort_edits(
) {
	if (undo_shapes) {
		std::memcpy(reinterpret_cast<char *>(&shapes[0]),
		            reinterpret_cast<char *>(undo_shapes),
		            sizeof(shapes));
		delete [] undo_shapes;
		undo_shapes = nullptr;
	}
}

/*
 *  Figure max. queue size for given game window.
 */
static int Figure_queue_size(
) {
	//Game_window *gwin = Game_window::get_instance();
	//int w = gwin->get_width(), h = gwin->get_height();
	// Figure # chunks, rounding up.
	//int cw = (w + c_chunksize - 1)/c_chunksize,
	//    ch = (h + c_chunksize - 1)/c_chunksize;
	// Add extra in each dir.
	return 100;//(cw + 3)*(ch + 3);
}

/*
 *  Create rendered_flats buffer.
 */

Image_buffer8 *Chunk_terrain::render_flats(
) {
	if (!rendered_flats) {
		if (queue_size > Figure_queue_size()) {
			// Grown too big.  Remove last.
			Chunk_terrain *last = render_queue->render_queue_prev;
			last->free_rendered_flats();
			render_queue->render_queue_prev =
			    last->render_queue_prev;
			last->render_queue_prev->render_queue_next =
			    render_queue;
			last->render_queue_next = last->render_queue_prev = nullptr;
			queue_size--;
		}
		rendered_flats = new Image_buffer8(c_chunksize, c_chunksize);
	}
	// Go through array of tiles.
	for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++)
		for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++)
			paint_tile(tilex, tiley);
	return rendered_flats;
}

/*
 *  Free pre-rendered landscape.
 */

void Chunk_terrain::free_rendered_flats() {
	delete rendered_flats;
	rendered_flats = nullptr;
}

/*
 *  This method is only used in 'terrain-editor' mode, NOT in normal
 *  gameplay.
 */

void Chunk_terrain::render_all(
    int cx, int cy, int pass // Chunk rendering too.
) {
	Image_window8 *iwin = gwin->get_win();
	const int ctx = cx * c_tiles_per_chunk;
	const int cty = cy * c_tiles_per_chunk;
	const int scrolltx = gwin->get_scrolltx();
	const int scrollty = gwin->get_scrollty();
	// Go through array of tiles.
	for (int tiley = 0; tiley < c_tiles_per_chunk; tiley++)
		for (int tilex = 0; tilex < c_tiles_per_chunk; tilex++) {
			Shape_frame *shape = get_shape(tilex, tiley);
			if (!shape)
				continue;
			if (!shape->is_rle() && pass == 1)
				iwin->copy8(shape->get_data(), c_tilesize,
				            c_tilesize,
				            (ctx + tilex - scrolltx)*c_tilesize,
				            (cty + tiley - scrollty)*c_tilesize);
			else if (shape->is_rle() && pass == 2) {      // RLE.
				int x;
				int y;
				const Tile_coord tile(ctx + tilex, cty + tiley, 0);
				gwin->get_shape_location(tile, x, y);
				sman->paint_shape(x, y, shape);
			}
		}
}

/*
 *  Write out to a chunk.
 *
 *  Output: Length of data stored.
 */

int Chunk_terrain::write_flats(
    unsigned char *chunk_data,
    bool v2_chunks              // 3 bytes/entry.
) {
	unsigned char *start = chunk_data;
	for (int ty = 0; ty < c_tiles_per_chunk; ty++)
		for (int tx = 0; tx < c_tiles_per_chunk; tx++) {
			const ShapeID id = get_flat(tx, ty);
			const int shapenum = id.get_shapenum();
			const int framenum = id.get_framenum();
			if (v2_chunks) {
				*chunk_data++ = shapenum & 0xff;
				*chunk_data++ = (shapenum >> 8) & 0xff;
				*chunk_data++ = framenum;
			} else {
				*chunk_data++ = shapenum & 0xff;
				*chunk_data++ = ((shapenum >> 8) & 3) | (framenum << 2);
			}
		}
	return chunk_data - start;
}

