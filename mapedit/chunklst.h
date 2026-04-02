/**
 ** A GTK widget showing the chunks from 'u7chunks'.
 **
 ** Written: 7/8/01 - JSF
 **/

#ifndef INCL_CHUNKLST
#define INCL_CHUNKLST 1

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

#include "objbrowse.h"
#include "rect.h"
#include "shapedraw.h"

#include <iosfwd>
#include <vector>

class Image_buffer8;
class Shape_group;

/*
 *  Store information about an individual chunk shown in the list.
 */
class Chunk_entry {
	friend class Chunk_chooser;
	int      num;
	TileRect box;    // Box where drawn.

	void set(int n, int rx, int ry, int rw, int rh) {
		num = n;
		box = TileRect(rx, ry, rw, rh);
	}
};

/*
 *  One row.
 */
class Chunk_row {
	friend class Chunk_chooser;
	short    height = 0;    // In pixels.
	long     y      = 0;    // Absolute y-coord. in pixels.
	unsigned index0 = 0;    // Index of 1st Npc_entry in row.
};

/*
 *  This class manages the list of chunks.
 */
class Chunk_chooser : public Object_browser, public Shape_draw {
	std::istream& chunkfile;     // Where chunks are read from (each is 256 shape ID's (2 or 3 bytes).
	GtkWidget*    sbar;          // Status bar.
	guint         sbar_sel;      // Status bar context for selection.
	int           num_chunks;    // Total # of chunks.
	// List of chunks we've read in.
	std::vector<unsigned char*> chunklist;
	int                         chunksz;     // # bytes/chunk.
	int                         headersz;    // # bytes in header.
	std::vector<Chunk_entry>    info;        // Pos. of each shape/frame.
	std::vector<Chunk_row>      rows;
	unsigned                    row0;                    // Row # at top of window.
	int                         row0_voffset;            // Vert. pos. (in pixels) of top row.
	long                        total_height;            // In pixels, for all rows.
	int                         locate_cx, locate_cy;    // Last chunk found by 'locate'.
	bool                        drop_enabled;            // So we only do it once.
	int                         to_del;                  // Terrain # to delete, or -1.
	void (*sel_changed)();                               // Called when selection changes.
	// Blit onto screen.
	int  voffset;
	void show(int x, int y, int w, int h) override;
	void tell_server();
	void select(int new_sel) override;    // Show new selection.
	void render() override;               // Draw list.
	void setup_info(bool savepos = true) override;
	void setup_shapes_info();

	void set_background_color(guint32 c) override {
		Shape_draw::set_background_color(c);
	}

	int get_selected_id() override {
		return selected < 0 ? -1 : info[selected].num;
	}

	unsigned char* get_chunk(int chunknum);
	void           update_num_chunks(int new_num_chunks);
	void           set_chunk(const unsigned char* data, int datalen);
	void           render_chunk(int chunknum, Image_buffer8* rwin, int xoff, int yoff);
	void           scroll_row_vertical(unsigned newrow);
	void           scroll_vertical(int newoffset);    // Scroll.
	void           setup_vscrollbar();                // Set new scroll amounts.
	void           goto_index(unsigned index);        // Get desired index in view.
	void           enable_controls();                 // Enable/disable controls after selection has changed.
	GtkWidget*     create_popup() override;           // Popup menu.
public:
	Chunk_chooser(Vga_file* i, std::istream& cfile, unsigned char* palbuf, int w, int h, Shape_group* g = nullptr);
	~Chunk_chooser() override;
	bool server_response(int id, unsigned char* data, int datalen) override;
	void end_terrain_editing() override;
	// Turn off selection.
	void unselect(bool need_render = true);

	bool is_selected() {    // Is a chunk selected?
		return selected >= 0;
	}

	void set_selected_callback(void (*fun)()) {
		sel_changed = fun;
	}

	unsigned get_num_cols(unsigned rownum) {
		return ((rownum < rows.size() - 1) ? rows[rownum + 1].index0 : info.size()) - rows[rownum].index0;
	}

	int get_count();    // Get # chunks we can display.
	// Configure when created/resized.
	gint configure(GdkEventConfigure* event);
	// Blit to screen.
	static gint expose(GtkWidget* widget, cairo_t* cairo, gpointer data);
	// Handle mouse press.
	gint mouse_press(GtkWidget* widget, GdkEventButton* event);
	// Give dragged chunk.
	static void drag_data_get(
			GtkWidget* widget, GdkDragContext* context, GtkSelectionData* seldata, guint info, guint time, gpointer data);
	static gint drag_begin(GtkWidget* widget, GdkDragContext* context, gpointer data);
	// Handler for drop.
	static void drag_data_received(
			GtkWidget* widget, GdkDragContext* context, gint x, gint y, GtkSelectionData* seldata, guint info, guint time,
			gpointer udata);
	void enable_drop();
	// Handle scrollbar.
	static void vscrolled(GtkAdjustment* adj, gpointer data);
	void        locate(int dir);    // Locate terrain on game map.
	void        locate(bool upwards) override;
	void        locate_response(const unsigned char* data, int datalen);
	void        insert(bool dup);    // Insert new chunk.
	void        del();               // Delete current chunk.
	void        insert_response(const unsigned char* data, int datalen);
	void        delete_response(const unsigned char* data, int datalen);
	void        move(bool upwards) override;    // Move current selected chunk.
	void        swap_response(const unsigned char* data, int datalen);
	static gint drag_motion(GtkWidget* widget, GdkEventMotion* event, gpointer data);
};

#endif
