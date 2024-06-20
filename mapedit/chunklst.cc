/**
 ** A GTK widget showing the chunks from 'u7chunks'.
 **
 ** Written: 7/8/01 - JSF
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
#	include <config.h>
#endif

#include "chunklst.h"

#include "endianio.h"
#include "exult_constants.h"
#include "ibuf8.h"
#include "shapegroup.h"
#include "u7drag.h"
#include "vgafile.h"

#include <glib.h>

#include <iosfwd>

using EStudio::Add_menu_item;
using std::cout;
using std::endl;

const int border = 2;    // Border at bottom, sides of each

//   chunk.

/*
 *  Blit onto screen.
 */

void Chunk_chooser::show(
		int x, int y, int w, int h    // Area to blit.
) {
	Shape_draw::show(x, y, w, h);
	if ((selected >= 0) && (drawgc != nullptr)) {    // Show selected.
		const int      zoom_scale = ZoomGet();
		const TileRect b          = info[selected].box;
		// Draw yellow box.
		cairo_set_line_width(drawgc, zoom_scale / 2.0);
		cairo_set_source_rgb(
				drawgc, ((drawfg >> 16) & 255) / 255.0,
				((drawfg >> 8) & 255) / 255.0, (drawfg & 255) / 255.0);
		cairo_rectangle(
				drawgc, (b.x * zoom_scale) / 2, (b.y * zoom_scale) / 2,
				(b.w * zoom_scale) / 2, (b.h * zoom_scale) / 2);
		cairo_stroke(drawgc);
	}
}

/*
 *  Send selected chunk# to Exult.
 */

void Chunk_chooser::tell_server() {
	if (selected < 0) {
		return;
	}
	unsigned char  buf[Exult_server::maxlength];
	unsigned char* ptr = &buf[0];
	little_endian::Write2(ptr, info[selected].num);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::set_edit_chunknum, buf, ptr - buf);
}

/*
 *  Select an entry.  This should be called after rendering
 *  the chunk.
 */

void Chunk_chooser::select(int new_sel) {
	if (new_sel < 0 || new_sel >= info_cnt) {
		return;    // Bad value.
	}
	selected = new_sel;
	tell_server();    // Tell Exult.
	enable_controls();
	const int chunknum = info[selected].num;
	// Remove prev. selection msg.
	// gtk_statusbar_pop(GTK_STATUSBAR(sbar), sbar_sel);
	char buf[150];    // Show new selection.
	g_snprintf(buf, sizeof(buf), "Chunk %d", chunknum);
	gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
}

/*
 *  Render as many chunks as fit in the chunk chooser window.
 */

void Chunk_chooser::render() {
	// Look for selected frame.
	int selchunk     = -1;
	int new_selected = -1;
	if (selected >= 0) {    // Save selection info.
		selchunk = info[selected].num;
	}
	// Remove "selected" message.
	// gtk_statusbar_pop(GTK_STATUSBAR(sbar), sbar_sel);
	delete[] info;    // Delete old info. list.
	// Get drawing area dimensions.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	const gint winw = ZoomDown(alloc.width);
	const gint winh = ZoomDown(alloc.height);
	// Provide more than enough room.
	info     = new Chunk_info[256];
	info_cnt = 0;    // Count them.
	// Clear window first.
	iwin->fill8(255);    // Background color.
	int index = index0;
	// 16x16 tiles, each 8x8 pixels.
	const int chunkw    = 128;
	const int chunkh    = 128;
	const int total_cnt = get_count();
	int       y         = border - voffset;
	while (index < total_cnt && y < winh) {
		int       x     = border;
		const int cliph = y + chunkh <= winh ? chunkh : (winh - y);
		while (index < total_cnt
			   && (x + chunkw + border <= winw || x == border)) {
			const int clipw = x + chunkw <= winw ? chunkw : (winw - x);
			iwin->set_clip(x, y, clipw, cliph);
			const int chunknum = group ? (*group)[index] : index;
			render_chunk(chunknum, iwin, x, y);
			iwin->clear_clip();
			// Store info. about where drawn.
			info[info_cnt].set(chunknum, x, y, chunkw, chunkh);
			if (chunknum == selchunk) {
				// Found the selected chunk.
				new_selected = info_cnt;
			}
			info_cnt++;
			index++;    // Next chunk.
			x += chunkw + border;
		}
		y += chunkh + border;
	}
	if (new_selected == -1) {
		unselect(false);
	} else {
		select(new_selected);
	}
	gtk_widget_queue_draw(draw);
}

/*
 *  Read in desired chunk if not already read.
 *
 *  Output: ->chunk, stored in chunklist.
 */

unsigned char* Chunk_chooser::get_chunk(int chunknum) {
	unsigned char* data = chunklist[chunknum];
	if (data) {
		return data;    // Already have it.
	}
	// Get from server.
	unsigned char        buf[Exult_server::maxlength];
	unsigned char*       ptr    = &buf[0];
	const unsigned char* newptr = &buf[0];
	little_endian::Write2(ptr, chunknum);
	ExultStudio*           studio        = ExultStudio::get_instance();
	int                    server_socket = studio->get_server_socket();
	Exult_server::Msg_type id;    // Expect immediate answer.
	int                    datalen;
	if (!studio->send_to_server(Exult_server::send_terrain, buf, ptr - buf)
		|| !Exult_server::wait_for_response(server_socket, 100)
		|| (datalen
			= Exult_server::Receive_data(server_socket, id, buf, sizeof(buf)))
				   == -1
		|| id != Exult_server::send_terrain
		|| little_endian::Read2(newptr) != chunknum) {
		// No server?  Get from file.
		data                = new unsigned char[chunksz];
		chunklist[chunknum] = data;
		chunkfile.seekg(headersz + chunknum * chunksz);
		chunkfile.read(reinterpret_cast<char*>(data), chunksz);
		if (!chunkfile.good()) {
			memset(data, 0, chunksz);
			cout << "Error reading chunk file" << endl;
		}
	} else {
		set_chunk(buf, datalen);
	}
	return chunklist[chunknum];
}

/*
 *  Update #chunks.
 */

void Chunk_chooser::update_num_chunks(int new_num_chunks) {
	num_chunks         = new_num_chunks;
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gtk_adjustment_set_upper(
			adj, (((128 + border) * num_chunks) + per_row - 1) / per_row);
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
}

/*
 *  Set chunk with data from 'Exult'.
 *
 *  NOTE:  Don't call 'show()' or 'render()' here, since this gets called
 *      from 'render()'.
 */

void Chunk_chooser::set_chunk(
		const unsigned char* data,    // Message from server.
		int                  datalen) {
	const int tnum = little_endian::Read2(data);    // First the terrain #.
	const int new_num_chunks
			= little_endian::Read2(data);    // Always sends total.
	datalen -= 4;
	if (datalen != chunksz) {
		cout << "Set_chunk:  Wrong data length" << endl;
		return;
	}
	if (tnum < 0 || tnum >= new_num_chunks) {
		cout << "Set_chunk:  Bad terrain # (" << tnum << ") received" << endl;
		return;
	}
	if (new_num_chunks != num_chunks) {
		// Update total #.
		if (new_num_chunks > num_chunks) {
			chunklist.resize(new_num_chunks);
		}
		update_num_chunks(new_num_chunks);
	}
	unsigned char* chunk = chunklist[tnum];
	if (!chunk) {    // Not read yet?
		chunk = chunklist[tnum] = new unsigned char[datalen];
	}
	memcpy(chunk, data, datalen);    // Copy it in.
}

/*
 *  Render one chunk.
 */

void Chunk_chooser::render_chunk(
		int            chunknum,    // # to render.
		Image_buffer8* rwin,        // Where to draw it
		int xoff, int yoff          // Where to draw it in rwin.
) {
	for (int pass = 1; pass <= 2; pass++) {
		unsigned char* data = get_chunk(chunknum);
		int            y    = c_tilesize;
		for (int ty = 0; ty < c_tiles_per_chunk; ty++, y += c_tilesize) {
			int x = c_tilesize;
			for (int tx = 0; tx < c_tiles_per_chunk; tx++, x += c_tilesize) {
				int                 shapenum;
				int                 framenum;
				const unsigned char l = Read1(data);
				const unsigned char h = Read1(data);
				if (!headersz) {
					shapenum = l + 256 * (h & 0x3);
					framenum = h >> 2;
				} else {    // Version 2.
					shapenum = l + 256 * h;
					framenum = Read1(data);
				}
				Shape_frame* s = ifile->get_shape(shapenum, framenum);
				// First pass over non RLE flats :
				//  8x8 tiles, shapenum < 150,
				//  Default origin -1, -1 to be reset,
				//  Not clickable nor Hack-Movable.
				if (s && pass == 1 && !s->is_rle()) {
					s->paint(rwin, xoff + x, yoff + y);
				}
				// Second pass over RLE flats :
				//  Larger than 8*8 tiles, shapenum >= 150,
				//  Valid origin,
				//  Clikable show Name and Outline, Hack-Movable.
				if (s && pass == 2 && s->is_rle()) {
					s->paint(rwin, xoff + x - 1, yoff + y - 1);
				}
			}
		}
	}
}

/*
 *  Get # shapes we can display.
 */

int Chunk_chooser::get_count() {
	return group ? group->size() : num_chunks;
}

/*
 *  Configure the viewing window.
 */

gint Chunk_chooser::configure(
		GtkWidget*         widget,    // The drawing area.
		GdkEventConfigure* event,
		gpointer           data    // ->Chunk_chooser
) {
	ignore_unused_variable_warning(widget, event);
	auto* chooser = static_cast<Chunk_chooser*>(data);
	chooser->Shape_draw::configure();
	chooser->render();
	chooser->setup_info(true);
	if (chooser->group) {          // Filtering?
		chooser->enable_drop();    // Can drop chunks here.
	}

	return true;
}

void Chunk_chooser::setup_info(bool savepos    // Try to keep current position.
) {
	// Set new scroll amounts.
	GtkAllocation alloc = {0, 0, 0, 0};
	gtk_widget_get_allocation(draw, &alloc);
	const int w           = ZoomDown(alloc.width);
	const int h           = ZoomDown(alloc.height);
	const int per_row_old = per_row;
	per_row               = std::max((w - border) / (128 + border), 1);
	GtkAdjustment* adj    = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gtk_adjustment_set_upper(
			adj, (((128 + border) * num_chunks) + per_row - 1) / per_row);
	gtk_adjustment_set_step_increment(adj, ZoomDown(16));
	gtk_adjustment_set_page_increment(adj, h - border);
	gtk_adjustment_set_page_size(adj, h - border);
	if (savepos && selected >= 0) {
		gtk_adjustment_set_value(
				adj, ((128 + border) * info[selected].num) / per_row);
	} else if (savepos) {
		gtk_adjustment_set_value(
				adj, (gtk_adjustment_get_value(adj) * per_row_old / per_row));
	}
	if (gtk_adjustment_get_value(adj)
		> (gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj))) {
		gtk_adjustment_set_value(
				adj, (gtk_adjustment_get_upper(adj)
					  - gtk_adjustment_get_page_size(adj)));
	}
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
}

/*
 *  Handle an expose event.
 */

gint Chunk_chooser::expose(
		GtkWidget* widget,    // The view window.
		cairo_t*   cairo,
		gpointer   data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(widget);
	auto* chooser = static_cast<Chunk_chooser*>(data);
	chooser->set_graphic_context(cairo);
	GdkRectangle area = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	chooser->show(
			ZoomDown(area.x), ZoomDown(area.y), ZoomDown(area.width),
			ZoomDown(area.height));
	chooser->set_graphic_context(nullptr);
	return true;
}

/*
 *  Handle a mouse button press event.
 */

gint Chunk_chooser::drag_motion(
		GtkWidget*      widget,    // The view window.
		GdkEventMotion* event,
		gpointer        data    // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget);
	auto* chooser = static_cast<Chunk_chooser*>(data);
	if (!chooser->dragging && chooser->selected >= 0) {
		chooser->start_drag(
				U7_TARGET_CHUNKID_NAME, U7_TARGET_CHUNKID,
				reinterpret_cast<GdkEvent*>(event));
	}
	return true;
}

gint Chunk_chooser::mouse_press(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(widget);
	auto* chooser = static_cast<Chunk_chooser*>(data);

#ifdef DEBUG
	cout << "Chunks : Clicked to " << (event->x) << " * " << (event->y)
		 << " by " << (event->button) << endl;
#endif
	if (event->button == 4) {
		chooser->scroll(true);
		return true;
	} else if (event->button == 5) {
		chooser->scroll(false);
		return true;
	}

	// int old_selected = chooser->selected;
	int i;    // Search through entries.
	for (i = 0; i < chooser->info_cnt; i++) {
		if (chooser->info[i].box.has_point(
					ZoomDown(static_cast<int>(event->x)),
					ZoomDown(static_cast<int>(event->y)))) {
			// Found the box?
			//			if (i == old_selected)
			//				return true;
			// Indicate we can dra.
			chooser->selected  = i;
			chooser->locate_cx = chooser->locate_cy = -1;
			chooser->render();
			// Tell client.
			if (chooser->sel_changed) {
				(*chooser->sel_changed)();
			}
			break;
		}
	}
	if (i == chooser->info_cnt && event->button == 1) {
		chooser->unselect(true);    // Nothing under mouse.
	} else if (event->button == 3) {
		gtk_menu_popup_at_pointer(
				GTK_MENU(chooser->create_popup()),
				reinterpret_cast<GdkEvent*>(event));
	}
	return true;
}

/*
 *  Handle a mouse button-release event.
 */
static gint Mouse_release(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, event);
	auto* chooser = static_cast<Chunk_chooser*>(data);
	chooser->mouse_up();
	return true;
}

/*
 *  Someone wants the dragged chunk.
 */

void Chunk_chooser::drag_data_get(
		GtkWidget*        widget,    // The view window.
		GdkDragContext*   context,
		GtkSelectionData* seldata,    // Fill this in.
		guint info, guint time,
		gpointer data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(widget, context, time);
	cout << "In DRAG_DATA_GET of Chunk for " << info << " and '"
		 << gdk_atom_name(gtk_selection_data_get_target(seldata)) << "'"
		 << endl;
	auto* chooser = static_cast<Chunk_chooser*>(data);
	if (chooser->selected < 0
		|| (info != U7_TARGET_CHUNKID && info != U7_TARGET_CHUNKID + 100
			&& info != U7_TARGET_CHUNKID + 200)) {
		return;    // Not sure about this.
	}
	guchar            buf[U7DND_DATA_LENGTH(1)];
	const Chunk_info& shinfo = chooser->info[chooser->selected];
	const int         len    = Store_u7_chunkid(buf, shinfo.num);
	cout << "Setting selection data (" << shinfo.num << ')' << " (" << len
		 << ") '" << buf << "'" << endl;
	// Set data.
	gtk_selection_data_set(
			seldata, gtk_selection_data_get_target(seldata), 8, buf, len);
}

/*
 *  Beginning of a drag.
 */

gint Chunk_chooser::drag_begin(
		GtkWidget*      widget,    // The view window.
		GdkDragContext* context,
		gpointer        data    // ->Chunk_chooser.
) {
	ignore_unused_variable_warning(widget, context);
	cout << "In DRAG_BEGIN of Chunk" << endl;
	auto* chooser = static_cast<Chunk_chooser*>(data);
	if (chooser->selected < 0) {
		return false;    // ++++Display a halt bitmap.
	}
	// Get ->chunk.
	Chunk_info&   shinfo = chooser->info[chooser->selected];
	const int     w      = c_tiles_per_chunk * c_tilesize;
	const int     h      = c_tiles_per_chunk * c_tilesize;
	Image_buffer8 tbuf(w, h);    // Create buffer to render to.
	tbuf.fill8(0xff);            // Fill with 'transparent' pixel.
	unsigned char* tbits = tbuf.get_bits();
	chooser->render_chunk(shinfo.num, &tbuf, 0, 0);
	// Put shape on a pixmap.
	GdkPixbuf* pixbuf  = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, w, h);
	guchar*    pixels  = gdk_pixbuf_get_pixels(pixbuf);
	const int  rstride = gdk_pixbuf_get_rowstride(pixbuf);
	const int  pstride = gdk_pixbuf_get_n_channels(pixbuf);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			guchar*       t = pixels + y * rstride + x * pstride;
			const guchar  s = tbits[y * w + x];
			const guint32 c = chooser->palette->colors[s];
			t[0]            = (s == 255 ? 0 : (c >> 16) & 255);
			t[1]            = (s == 255 ? 0 : (c >> 8) & 255);
			t[2]            = (s == 255 ? 0 : (c >> 0) & 255);
			t[3]            = (s == 255 ? 0 : 255);
		}
	}
	// This will be the chunk dragged.
	gtk_drag_set_icon_pixbuf(context, pixbuf, w - 2, h - 2);
	g_object_unref(pixbuf);
	return true;
}

/*
 *  Chunk was dropped here.
 */

void Chunk_chooser::drag_data_received(
		GtkWidget* widget, GdkDragContext* context, gint x, gint y,
		GtkSelectionData* seldata, guint info, guint time,
		gpointer udata    // -> Chunk_chooser.
) {
	ignore_unused_variable_warning(widget, context, x, y, info, time);
	auto* chooser = static_cast<Chunk_chooser*>(udata);
	cout << "In DRAG_DATA_RECEIVED of Chunk for '"
		 << gdk_atom_name(gtk_selection_data_get_data_type(seldata)) << "'"
		 << endl;
	if ((gtk_selection_data_get_data_type(seldata)
				 == gdk_atom_intern(U7_TARGET_CHUNKID_NAME, 0)
		 || gtk_selection_data_get_data_type(seldata)
					== gdk_atom_intern(U7_TARGET_DROPTEXT_NAME_MIME, 0)
		 || gtk_selection_data_get_data_type(seldata)
					== gdk_atom_intern(U7_TARGET_DROPTEXT_NAME_MACOSX, 0))
		&& Is_u7_chunkid(gtk_selection_data_get_data(seldata))
		&& gtk_selection_data_get_format(seldata) == 8
		&& gtk_selection_data_get_length(seldata) > 0) {
		int cnum;
		Get_u7_chunkid(gtk_selection_data_get_data(seldata), cnum);
		chooser->group->add(cnum);
		chooser->render();
		//		chooser->adjust_scrollbar(); ++++++Probably need to do this.
	}
}

/*
 *  Set to accept drops from drag-n-drop of a chunk.
 */

void Chunk_chooser::enable_drop() {
	if (drop_enabled) {    // More than once causes warning.
		return;
	}
	drop_enabled = true;
	gtk_widget_realize(draw);    //???????
	GtkTargetEntry tents[3];
	tents[0].target = const_cast<char*>(U7_TARGET_CHUNKID_NAME);
	tents[1].target = const_cast<char*>(U7_TARGET_DROPTEXT_NAME_MIME);
	tents[2].target = const_cast<char*>(U7_TARGET_DROPTEXT_NAME_MACOSX);
	tents[0].flags  = 0;
	tents[1].flags  = 0;
	tents[2].flags  = 0;
	tents[0].info   = U7_TARGET_CHUNKID;
	tents[1].info   = U7_TARGET_CHUNKID + 100;
	tents[2].info   = U7_TARGET_CHUNKID + 200;
	gtk_drag_dest_set(
			draw, GTK_DEST_DEFAULT_ALL, tents, 3,
			static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE));

	g_signal_connect(
			G_OBJECT(draw), "drag-data-received",
			G_CALLBACK(drag_data_received), this);
}

/*
 *  Scroll to a new chunk/frame.
 */

void Chunk_chooser::scroll(int newpixel    // Abs. index of leftmost to show.
) {
	const int total     = get_count();
	const int newindex  = (newpixel / (128 + border)) * per_row;
	const int newoffset = newpixel % (128 + border);
	voffset             = newindex >= 0 ? newoffset : 0;
	if (index0 < newindex) {    // Going forwards?
		index0 = newindex < total ? newindex : total;
	} else if (index0 > newindex) {    // Backwards?
		index0 = newindex >= 0 ? newindex : 0;
	}
	render();
}

/*
 *  Scroll up/down by one row.
 */

void Chunk_chooser::scroll(bool upwards) {
	GtkAdjustment* adj   = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gdouble        delta = 128 + border;
	if (upwards) {
		delta = -delta;
	}
	gtk_adjustment_set_value(adj, gtk_adjustment_get_value(adj) + delta);
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
	scroll(static_cast<gint>(gtk_adjustment_get_value(adj)));
}

/*
 *  Handle a scrollbar event.
 */

void Chunk_chooser::scrolled(
		GtkAdjustment* adj,    // The adjustment.
		gpointer       data    // ->Chunk_chooser.
) {
	auto* chooser = static_cast<Chunk_chooser*>(data);
#ifdef DEBUG
	cout << "Chunks : VScrolled to " << gtk_adjustment_get_value(adj)
		 << " of [ " << gtk_adjustment_get_lower(adj) << ", "
		 << gtk_adjustment_get_upper(adj) << " ] by "
		 << gtk_adjustment_get_step_increment(adj) << " ( "
		 << gtk_adjustment_get_page_increment(adj) << ", "
		 << gtk_adjustment_get_page_size(adj) << " )" << endl;
#endif
	const gint newindex = static_cast<gint>(gtk_adjustment_get_value(adj));
	chooser->scroll(newindex);
}

/*
 *  Enable/disable controls after selection changed.
 */

void Chunk_chooser::enable_controls() {
	if (selected == -1) {    // No selection.
		gtk_widget_set_sensitive(loc_down, false);
		gtk_widget_set_sensitive(loc_up, false);
		if (!group) {
			gtk_widget_set_sensitive(move_down, false);
			gtk_widget_set_sensitive(move_up, false);
		}
		return;
	}
	gtk_widget_set_sensitive(loc_down, true);
	gtk_widget_set_sensitive(loc_up, true);
	if (!group) {
		gtk_widget_set_sensitive(
				move_down, info[selected].num < num_chunks - 1);
		gtk_widget_set_sensitive(move_up, info[selected].num > 0);
	}
}

/*
 *  Handle popup menu items.
 */

static void on_insert_empty(GtkMenuItem* item, gpointer udata) {
	ignore_unused_variable_warning(item);
	auto* chooser = static_cast<Chunk_chooser*>(udata);
	chooser->insert(false);
}

static void on_insert_dup(GtkMenuItem* item, gpointer udata) {
	ignore_unused_variable_warning(item);
	auto* chooser = static_cast<Chunk_chooser*>(udata);
	chooser->insert(true);
}

static void on_delete(GtkMenuItem* item, gpointer udata) {
	ignore_unused_variable_warning(item);
	auto* chooser = static_cast<Chunk_chooser*>(udata);
	chooser->del();
}

/*
 *  Set up popup menu.
 */

GtkWidget* Chunk_chooser::create_popup() {
	create_popup_internal(true);    // Create popup with groups, files.
	if (group != nullptr) {         // Filtering?  Skip the rest.
		return popup;
	}
	GtkWidget* mitem    = Add_menu_item(popup, "New...");
	GtkWidget* new_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(mitem), new_menu);
	Add_menu_item(new_menu, "Empty", G_CALLBACK(on_insert_empty), this);
	if (selected >= 0) {
		Add_menu_item(new_menu, "Duplicate", G_CALLBACK(on_insert_dup), this);
		Add_menu_item(popup, "Delete", G_CALLBACK(on_delete), this);
	}
	return popup;
}

const int V2_CHUNK_HDR_SIZE = 4 + 4 + 2;    // 0xffff, "exlt", vers.

/*
 *  Create the list.
 */

Chunk_chooser::Chunk_chooser(
		Vga_file*      i,         // Where they're kept.
		std::istream&  cfile,     // Chunks file.
		unsigned char* palbuf,    // Palette, 3*256 bytes (rgb triples).
		int w, int h,             // Dimensions.
		Shape_group* g            // Filter, or null.
		)
		: Object_browser(g), Shape_draw(i, palbuf, gtk_drawing_area_new()),
		  chunkfile(cfile), chunksz(c_tiles_per_chunk * c_tiles_per_chunk * 2),
		  headersz(0), info(nullptr), info_cnt(0), locate_cx(-1), locate_cy(-1),
		  drop_enabled(false), to_del(-1), sel_changed(nullptr), voffset(0),
		  per_row(1) {
	static char v2hdr[]
			= {'\xff', '\xff', '\xff', '\xff', 'e', 'x', 'l', 't', 0, 0};
	char v2buf[V2_CHUNK_HDR_SIZE];    // Check for V2 chunks.
	chunkfile.seekg(0);
	chunkfile.read(v2buf, sizeof(v2buf));
	if (!chunkfile.good()) {
		cout << "Chunkfile has an error!" << endl;
	}
	if (memcmp(v2hdr, v2buf, sizeof(v2buf)) == 0) {
		headersz = V2_CHUNK_HDR_SIZE;
		chunksz  = c_tiles_per_chunk * c_tiles_per_chunk * 3;
	}
	chunkfile.seekg(0, std::ios::end);    // Figure total #chunks.
	num_chunks = (static_cast<int>(chunkfile.tellg()) - headersz) / chunksz;
	chunklist.resize(num_chunks);    // Init. list of ->'s to chunks.

	// Put things in a vert. box.
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(vbox), false);
	set_widget(vbox);    // This is our "widget"
	gtk_widget_set_visible(vbox, true);

	GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox), false);
	gtk_widget_set_visible(hbox, true);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, true, true, 0);

	// A frame looks nice.
	GtkWidget* frame = gtk_frame_new(nullptr);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	widget_set_margins(
			frame, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(frame, true);
	gtk_box_pack_start(GTK_BOX(hbox), frame, true, true, 0);

	// NOTE:  draw is in Shape_draw.
	// Indicate the events we want.
	gtk_widget_set_events(
			draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
						  | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON1_MOTION_MASK);
	// Set "configure" handler.
	g_signal_connect(
			G_OBJECT(draw), "configure-event", G_CALLBACK(configure), this);
	// Set "expose-event" - "draw" handler.
	g_signal_connect(G_OBJECT(draw), "draw", G_CALLBACK(expose), this);
	// Set mouse click handler.
	g_signal_connect(
			G_OBJECT(draw), "button-press-event", G_CALLBACK(mouse_press),
			this);
	g_signal_connect(
			G_OBJECT(draw), "button-release-event", G_CALLBACK(Mouse_release),
			this);
	// Mouse motion.
	g_signal_connect(
			G_OBJECT(draw), "drag-begin", G_CALLBACK(drag_begin), this);
	g_signal_connect(
			G_OBJECT(draw), "motion-notify-event", G_CALLBACK(drag_motion),
			this);
	g_signal_connect(
			G_OBJECT(draw), "drag-data-get", G_CALLBACK(drag_data_get), this);
	gtk_container_add(GTK_CONTAINER(frame), draw);
	widget_set_margins(
			draw, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_size_request(draw, w, h);
	gtk_widget_set_visible(draw, true);
	// Want a scrollbar for the chunks.
	GtkAdjustment* chunk_adj = GTK_ADJUSTMENT(
			gtk_adjustment_new(0, 0, (128 + border) * num_chunks, 1, 4, 1.0));
	vscroll = gtk_scrollbar_new(
			GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT(chunk_adj));
	gtk_box_pack_start(GTK_BOX(hbox), vscroll, false, true, 0);
	// Set scrollbar handler.
	g_signal_connect(
			G_OBJECT(chunk_adj), "value-changed", G_CALLBACK(scrolled), this);
	gtk_widget_set_visible(vscroll, true);
	// Scroll events.
	enable_draw_vscroll(draw);

	// At the bottom, status bar:
	GtkWidget* hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(hbox1), false);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, false, false, 0);
	gtk_widget_set_visible(hbox1, true);
	// At left, a status bar.
	sbar     = gtk_statusbar_new();
	sbar_sel = gtk_statusbar_get_context_id(GTK_STATUSBAR(sbar), "selection");
	gtk_box_pack_start(GTK_BOX(hbox1), sbar, true, true, 0);
	widget_set_margins(
			sbar, 2 * HMARGIN, 2 * HMARGIN, 2 * VMARGIN, 2 * VMARGIN);
	gtk_widget_set_visible(sbar, true);
	// Add locate/move controls to bottom.
	gtk_box_pack_start(
			GTK_BOX(vbox),
			create_controls(locate_controls | (!group ? move_controls : 0)),
			false, false, 0);
}

/*
 *  Delete.
 */

Chunk_chooser::~Chunk_chooser() {
	gtk_widget_destroy(get_widget());
	delete[] info;
	int i;
	for (i = 0; i < num_chunks; i++) {    // Delete all the chunks.
		delete chunklist[i];
	}
}

/*
 *  Handle response from server.
 *
 *  Output: true if handled here.
 */

bool Chunk_chooser::server_response(int id, unsigned char* data, int datalen) {
	switch (static_cast<Exult_server::Msg_type>(id)) {
	case Exult_server::locate_terrain:
		locate_response(data, datalen);
		return true;
	case Exult_server::insert_terrain:
		insert_response(data, datalen);
		return true;
	case Exult_server::delete_terrain:
		delete_response(data, datalen);
		return true;
	case Exult_server::swap_terrain:
		swap_response(data, datalen);
		return true;
	case Exult_server::send_terrain:
		set_chunk(data, datalen);
		render();
		return true;
	default:
		return false;
	}
}

/*
 *  Done with terrain editing.
 */

void Chunk_chooser::end_terrain_editing() {
	// Clear out cache of chunks.
	for (int i = 0; i < num_chunks; i++) {
		delete chunklist[i];
		chunklist[i] = nullptr;
	}
	render();
}

/*
 *  Unselect.
 */

void Chunk_chooser::unselect(bool need_render    // 1 to render and show.
) {
	if (selected >= 0) {
		selected  = -1;
		locate_cx = locate_cy = -1;
		if (need_render) {
			render();
		}
		if (sel_changed) {    // Tell client.
			(*sel_changed)();
		}
	}
	enable_controls();    // Enable/disable controls.
	if (info_cnt > 0) {
		char buf[150];    // Show new selection.
		g_snprintf(
				buf, sizeof(buf), "Chunks %d to %d", info[0].num,
				info[info_cnt - 1].num);
		gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
	}
}

/*
 *  Locate terrain on game map.
 */

void Chunk_chooser::locate(int dir    // 1=downwards, -1=upwards, 0=from top.
) {
	if (selected < 0) {
		return;    // Shouldn't happen.
	}
	bool           upwards = false;
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr  = &data[0];
	const int      tnum = info[selected].num;    // Terrain #.
	int            cx   = locate_cx;
	int            cy   = locate_cy;
	if (dir == 0) {
		cx = cy = -1;
	} else if (dir == -1) {
		upwards = true;
	}
	little_endian::Write2(ptr, tnum);
	little_endian::Write2(ptr, cx);    // Current chunk, or -1.
	little_endian::Write2(ptr, cy);
	Write1(ptr, upwards ? 1 : 0);
	ExultStudio* studio = ExultStudio::get_instance();
	if (!studio->send_to_server(
				Exult_server::locate_terrain, data, ptr - data)) {
		to_del = -1;    // In case we're deleting.
	}
}

void Chunk_chooser::locate(bool upwards) {
	locate(upwards ? -1 : 1);
}

/*
 *  Response from server to a 'locate'.
 */

void Chunk_chooser::locate_response(const unsigned char* data, int datalen) {
	ignore_unused_variable_warning(datalen);
	const unsigned char* ptr  = data;
	const int            tnum = little_endian::Read2(ptr);
	if (selected < 0 || tnum != info[selected].num) {
		to_del = -1;
		return;    // Not the current selection.
	}
	auto cx = little_endian::Read2s(ptr);    // Get chunk found.
	auto cy = little_endian::Read2s(ptr);
	ptr++;    // Skip upwards flag.
	if (!*ptr) {
		if (to_del >= 0 && to_del == tnum) {
			unsigned char  data[Exult_server::maxlength];
			unsigned char* ptr = &data[0];
			little_endian::Write2(ptr, tnum);
			ExultStudio* studio = ExultStudio::get_instance();
			studio->send_to_server(
					Exult_server::delete_terrain, data, ptr - data);
		} else {
			EStudio::Alert("Terrain %d not found.", tnum);
		}
	} else {
		locate_cx = cx;    // Save new chunk.
		locate_cy = cy;
		if (to_del >= 0) {
			EStudio::Alert("Terrain %d is still in use", tnum);
		}
	}
	to_del = -1;
}

/*
 *  Insert a new chunk terrain into the list.
 */

void Chunk_chooser::insert(bool dup) {
	if (dup && selected < 0) {
		return;    // Shouldn't happen.
	}
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr  = &data[0];
	const int      tnum = selected >= 0 ? info[selected].num : -1;
	little_endian::Write2(ptr, tnum);
	Write1(ptr, dup ? 1 : 0);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::insert_terrain, data, ptr - data);
}

/*
 *  Delete currently selected chunk if it's not being used.
 */

void Chunk_chooser::del() {
	if (selected < 0) {
		return;
	}
	to_del = info[selected].num;
	locate(0);    // See if it exists.
}

/*
 *  Response from server to an 'insert'.
 */

void Chunk_chooser::insert_response(const unsigned char* data, int datalen) {
	ignore_unused_variable_warning(datalen);
	const unsigned char* ptr  = data;
	const int            tnum = little_endian::Read2s(ptr);
	const bool           dup  = Read1(ptr) != 0;
	if (!*ptr) {
		EStudio::Alert("Terrain insert failed.");
	} else {
		// Insert in our list.
		auto* data = new unsigned char[chunksz];
		if (dup && tnum >= 0 && tnum < num_chunks && chunklist[tnum]) {
			memcpy(data, chunklist[tnum], chunksz);
		} else {
			memset(data, 0, chunksz);
		}
		// FIXME - for now only append to the end which prevents ordering errors 
		// in chunk groups. Ideally this should update existing chunk groups as well.
		// if (tnum >= 0 && tnum < num_chunks - 1) {
		//	chunklist.insert(chunklist.begin() + tnum + 1, data);
		// } else    // If -1, append to end.*/
		chunklist.push_back(data);

		update_num_chunks(num_chunks + 1);
		render();
	}
}

/*
 *  Response from server to an 'delete'.
 */

void Chunk_chooser::delete_response(const unsigned char* data, int datalen) {
	ignore_unused_variable_warning(datalen);
	const unsigned char* ptr  = data;
	const int            tnum = little_endian::Read2s(ptr);
	if (!*ptr) {
		EStudio::Alert("Terrain delete failed.");
	} else {
		// Remove from our list.
		delete chunklist[tnum];
		chunklist.erase(chunklist.begin() + tnum);
		update_num_chunks(num_chunks - 1);
		render();
	}
}

/*
 *  Move currently-selected chunk up or down.
 */

void Chunk_chooser::move(bool upwards) {
	if (selected < 0) {
		return;    // Shouldn't happen.
	}
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr  = &data[0];
	int            tnum = info[selected].num;
	if ((tnum == 0 && upwards) || (tnum == num_chunks - 1 && !upwards)) {
		return;
	}
	if (upwards) {    // Going to swap tnum & tnum+1.
		tnum--;
	}
	little_endian::Write2(ptr, tnum);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::swap_terrain, data, ptr - data);
}

/*
 *  Response from server to a 'swap'.
 */

void Chunk_chooser::swap_response(const unsigned char* data, int datalen) {
	ignore_unused_variable_warning(datalen);
	const unsigned char* ptr  = data;
	const int            tnum = little_endian::Read2s(ptr);
	if (!*ptr) {
		cout << "Terrain insert failed." << endl;
	} else if (tnum >= 0 && tnum < num_chunks - 1) {
		unsigned char* tmp  = get_chunk(tnum);
		chunklist[tnum]     = get_chunk(tnum + 1);
		chunklist[tnum + 1] = tmp;
		if (selected >= 0) {    // Update selected.
			if (info[selected].num == tnum) {
				// Moving downwards.
				if (selected >= info_cnt - 1) {
					scroll(false);
				}
				select(selected + 1);
			} else if (info[selected].num == tnum + 1) {
				if (selected <= 0) {
					scroll(true);
				}
				select(selected - 1);
			}
		}
		render();
	}
}
