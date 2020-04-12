/**
 ** A GTK widget showing a list of shapes from an image file.
 **
 ** Written: 7/25/99 - JSF
 **/

/*
Copyright (C) 1999  Jeffrey S. Freedman

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

#ifdef _WIN32
#include "windrag.h"
#include <windows.h>
#endif

#include <iostream>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wparentheses"
#if !defined(__llvm__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wuseless-cast"
#else
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
#endif  // __GNUC__
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#ifdef XWIN
#include <gdk/gdkx.h>
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__
#include "gtk_redefines.h"

#include <glib.h>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#include "shapelst.h"
#include "shapevga.h"
#include "ibuf8.h"
#include "u7drag.h"
#include "studio.h"
#include "utils.h"
#include "shapegroup.h"
#include "shapefile.h"
#include "pngio.h"
#include "fontgen.h"
#include "utils.h"
#include "databuf.h"
#include "items.h"
#include "frnameinf.h"
#include "ignore_unused_variable_warning.h"

using std::cout;
using std::endl;
using std::strlen;
using std::string;
using std::ifstream;
using std::make_unique;
using std::unique_ptr;
using EStudio::Prompt;
using EStudio::Alert;
using EStudio::Add_menu_item;

std::vector<std::unique_ptr<Editing_file>> Shape_chooser::editing_files;
int Shape_chooser::check_editing_timer = -1;

#define IS_FLAT(shnum)  ((shnum) < c_first_obj_shape)

/*
 *  Here's a description of a file being edited by an external program
 *  like Gimp or Photoshop.
 */
class Editing_file {
	string vga_basename;        // Name of image file this comes from.
	string pathname;        // Full path to file.
	time_t mtime;           // Last modification time.
	int shapenum, framenum;     // Shape/frame.
	int tiles;          // If > 0, #8x8 tiles per row or col.
	bool bycolumns;         // If true tile by column first.
public:
	friend class Shape_chooser;
	// Create for single frame:
	Editing_file(const char *vganm, const char *pnm, time_t m,
	             int sh, int fr)
		: vga_basename(vganm), pathname(pnm),
		  mtime(m), shapenum(sh), framenum(fr), tiles(0)
	{  }
	// Create tiled:
	Editing_file(const char *vganm, const char *pnm, time_t m, int sh,
	             int ts, bool bycol)
		: vga_basename(vganm), pathname(pnm), mtime(m), shapenum(sh),
		  framenum(0), tiles(ts), bycolumns(bycol)
	{  }
};

/*
 *  Callback for when a shape is dropped on our draw area.
 */

static void Shape_dropped_here(
    int file,           // U7_SHAPE_SHAPES.
    int shape,
    int frame,
    void *udata
) {
	static_cast<Shape_chooser *>(udata)->shape_dropped_here(file, shape, frame);
}

/*
 *  Blit onto screen.
 */

void Shape_chooser::show(
    int x, int y, int w, int h  // Area to blit.
) {
	Shape_draw::show(draw->window, x, y, w, h);
	if (selected >= 0) {    // Show selected.
		Rectangle b = info[selected].box;
		// Draw yellow box.
		gdk_draw_rectangle(draw->window, drawgc, FALSE,
		                   b.x - hoffset, b.y - voffset, b.w, b.h);
	}
}

/*
 *  Send selected shape/frame to Exult.
 */

void Shape_chooser::tell_server_shape(
) {
	int shnum = -1;
	int frnum = 0;
	if (selected >= 0) {
		shnum = info[selected].shapenum;
		frnum = info[selected].framenum;
	}
	unsigned char buf[Exult_server::maxlength];
	unsigned char *ptr = &buf[0];
	Write2(ptr, shnum);
	Write2(ptr, frnum);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::set_edit_shape, buf, ptr - buf);
}

/*
 *  Select an entry.  This should be called after rendering
 *  the shape.
 */

void Shape_chooser::select(
    int new_sel
) {
	selected = new_sel;
	tell_server_shape();        // Tell Exult.
	int shapenum = info[selected].shapenum;
	// Update spin-button value, range.
	gtk_widget_set_sensitive(fspin, true);
	int nframes = ifile->get_num_frames(shapenum);
	frame_adj->upper = nframes - 1;
	gtk_adjustment_changed(frame_adj);
	gtk_adjustment_set_value(frame_adj, info[selected].framenum);
	gtk_widget_set_sensitive(fspin, true);
	update_statusbar();
}

const int border = 4;           // Border at bottom, sides.
/*
 *  Render as many shapes as fit in the shape chooser window.
 */

void Shape_chooser::render(
) {
	// Get drawing area dimensions.
	gint winh = draw->allocation.height;
	// Clear window first.
	iwin->fill8(255);       // Set to background_color.
	int curr_y = -row0_voffset;
	//int total_cnt = get_count();
	//   filter (group).
	for (unsigned rownum = row0; curr_y  < winh && rownum < rows.size();
	        ++rownum) {
		Shape_row &row = rows[rownum];
		unsigned cols = get_num_cols(rownum);
		for (unsigned index = row.index0; cols; --cols, ++index) {
			int shapenum = info[index].shapenum;
			int framenum = info[index].framenum;
			Shape_frame *shape = ifile->get_shape(shapenum,
			                                      framenum);
			if (shape) {
				int sx = info[index].box.x - hoffset;
				int sy = info[index].box.y - voffset;
				shape->paint(iwin, sx + shape->get_xleft(),
				             sy + shape->get_yabove());
				last_shape = shapenum;
			}
		}
		curr_y += rows[rownum].height;
	}
}

/*
 *  Get maximum shape height for all its frames.
 */

static int Get_max_height(
    Shape *shape
) {
	int cnt = shape->get_num_frames();
	int maxh = 0;
	for (int i = 0; i < cnt; i++) {
		Shape_frame *frame = shape->get_frame(i);
		int ht =  frame ? frame->get_height() : -1;
		if (ht > maxh)
			maxh = ht;
	}
	return maxh;
}

/*
 *  Get the x-offset in pixels where a frame will be drawn.
 *
 *  Output: Offset from left edge of (virtual) drawing area.
 */

static int Get_x_offset(
    Shape *shape,
    int framenum
) {
	if (!shape)
		return 0;
	int nframes = shape->get_num_frames();
	if (framenum >= nframes)
		framenum = nframes - 1;
	int xoff = 0;
	for (int i = 0; i < framenum; i++) {
		Shape_frame *fr = shape->get_frame(i);
		assert(fr);
		xoff += fr->get_width() + border;
	}
	return xoff;
}

/*
 *  Find where everything goes.
 */

void Shape_chooser::setup_info(
    bool savepos            // Try to keep current position.
) {
	unsigned oldind = rows[row0].index0;
	info.resize(0);
	rows.resize(0);
	row0 = row0_voffset = 0;
	last_shape = 0;
	/* +++++NOTE:  index0 is always 0 for the shape browse.  It should
	    probably be removed from the base Obj_browser class */
	index0 = 0;
	voffset = 0;
	total_height = 0;
	if (frames_mode)
		setup_frames_info();
	else
		setup_shapes_info();
	setup_vscrollbar();
	if (savepos)
		goto_index(oldind);
}

/*
 *  Setup info when not in 'frames' mode.
 */

void Shape_chooser::setup_shapes_info(
) {
	int selshape = -1;
	int selframe = -1;
	if (selected >= 0) {    // Save selection info.
		selshape = info[selected].shapenum;
		selframe = info[selected].framenum;
	}
	// Get drawing area dimensions.
	gint winw = draw->allocation.width;
	int x = 0;
	int curr_y = 0;
	int row_h = 0;
	int total_cnt = get_count();
	//   filter (group).
	rows.resize(1);         // Start 1st row.
	rows[0].index0 = 0;
	rows[0].y = 0;
	for (int index = 0; index < total_cnt; index++) {
		int shapenum = group ? (*group)[index] : index;
		int framenum = shapenum == selshape ? selframe : framenum0;
		Shape_frame *shape = ifile->get_shape(shapenum, framenum);
		if (!shape)
			continue;
		int sh = shape->get_height();
		int sw = shape->get_width();
		// Check if we've exceeded max width
		if (x + sw > winw && x) {   // But don't leave row empty.
			// Next line.
			rows.back().height = row_h + border;
			curr_y += row_h + border;
			row_h = 0;
			x = 0;
			rows.push_back(Shape_row());
			rows.back().index0 = info.size();
			rows.back().y = curr_y;
		}
		if (sh > row_h)
			row_h = sh;
		int sy = curr_y + border; // Get top y-coord.
		// Store info. about where drawn.
		info.push_back(Shape_entry());
		info.back().set(shapenum, framenum, x, sy, sw, sh);
		x += sw + border;
	}
	rows.back().height = row_h + border;
	total_height = curr_y + rows.back().height + border;
}

/*
 *  Setup one shape per row, showing its frames from left to right.
 */

void Shape_chooser::setup_frames_info(
) {
	// Get drawing area dimensions.
	int curr_y = 0;
	int maxw = 0;
	unsigned total_cnt = get_count();
	//   filter (group).
	for (unsigned index = index0; index < total_cnt; index++) {
		int shapenum = group ? (*group)[index] : index;
		// Get all frames.
		Shape *shape = ifile->extract_shape(shapenum);
		int nframes = shape ? shape->get_num_frames() : 0;
		if (!nframes)
			continue;
		int row_h = Get_max_height(shape);
		rows.push_back(Shape_row());
		rows.back().index0 = info.size();
		rows.back().y = curr_y;
		rows.back().height = row_h + border;
		int x = 0;
		int sw;
		int sh;
		for (int framenum = 0; framenum < nframes; framenum++,
		        x += sw + border) {
			Shape_frame *frame = shape->get_frame(framenum);
			if (!frame) {
				sw = sh = 0;
				continue;
			}
			sh = frame->get_height();
			sw = frame->get_width();
			int sy = curr_y + border; // Get top y-coord.
			// Store info. about where drawn.
			info.push_back(Shape_entry());
			info.back().set(shapenum, framenum, x, sy, sw, sh);
		}
		if (x > maxw)
			maxw = x;
		// Next line.
		curr_y += row_h + border;
	}
	total_height = curr_y + border;
	setup_hscrollbar(maxw);
}

/*
 *  Horizontally scroll so that the selected frame is visible (in frames
 *  mode).
 */

void Shape_chooser::scroll_to_frame(
) {
	if (selected >= 0) {    // Save selection info.
		int selshape = info[selected].shapenum;
		int selframe = info[selected].framenum;
		Shape *shape = ifile->extract_shape(selshape);
		int xoff = Get_x_offset(shape, selframe);
		if (xoff < hoffset) // Left of visual area?
			hoffset = xoff > border ? xoff - border : 0;
		else {
			gint winw = draw->allocation.width;
			Shape_frame *fr = shape->get_frame(selframe);
			assert(fr);
			int sw = fr->get_width();
			if (xoff + sw + border - hoffset > winw)
				hoffset = xoff + sw + border - winw;
		}
		GtkAdjustment *adj = gtk_range_get_adjustment(
		                         GTK_RANGE(hscroll));
		gtk_adjustment_set_value(adj, hoffset);
	}
}

/*
 *  Scroll so a desired index is in view.
 */

void Shape_chooser::goto_index(
    unsigned index           // Desired index in 'info'.
) {
	if (index >= info.size())
		return;         // Illegal index or empty chooser.
	Shape_entry &inf = info[index]; // Already in view?
	unsigned midx = inf.box.x + inf.box.w / 2;
	unsigned midy = inf.box.y + inf.box.h / 2;
	Rectangle winrect(hoffset, voffset, config_width, config_height);
	if (winrect.has_point(midx, midy))
		return;
	unsigned start = 0;
	unsigned count = rows.size();
	while (count > 1) {     // Binary search.
		unsigned mid = start + count / 2;
		if (index < rows[mid].index0)
			count = mid - start;
		else {
			count = (start + count) - mid;
			start = mid;
		}
	}
	if (start < rows.size()) {
		// Get to right spot again!
		GtkAdjustment *adj = gtk_range_get_adjustment(
		                         GTK_RANGE(vscroll));
		gtk_adjustment_set_value(adj, rows[start].y);
	}
}

/*
 *  Find an index (not necessarily the 1st) for a given shape #.
 */

int Shape_chooser::find_shape(
    int shnum
) {
	if (group) {        // They're not ordered.
		unsigned cnt = info.size();
		for (unsigned i = 0; i < cnt; ++i)
			if (info[i].shapenum == shnum)
				return i;
		return -1;
	}
	unsigned start = 0;
	unsigned count = info.size();
	while (count > 1) {     // Binary search.
		unsigned mid = start + count / 2;
		if (shnum < info[mid].shapenum)
			count = mid - start;
		else {
			count = (start + count) - mid;
			start = mid;
		}
	}
	if (start < info.size())
		return start;
	else
		return -1;
}

/*
 *  Configure the viewing window.
 */

static gint Configure_chooser(
    GtkWidget *widget,      // The drawing area.
    GdkEventConfigure *event,
    gpointer data           // ->Shape_chooser
) {
	ignore_unused_variable_warning(widget);
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	return chooser->configure(event);
}
gint Shape_chooser::configure(
    GdkEventConfigure *event
) {
	Shape_draw::configure();
	// Did the size change?
	if (event->width != config_width || event->height != config_height) {
		config_width = event->width;
		config_height = event->height;
		setup_info(true);
		render();
		update_statusbar();
	} else
		render();       // Same size?  Just render it.
	// Set handler for shape dropped here,
	//   BUT not more than once.
	if (drop_callback != Shape_dropped_here)
		enable_drop(Shape_dropped_here, this);
	return TRUE;
}

/*
 *  Handle an expose event.
 */

gint Shape_chooser::expose(
    GtkWidget *widget,      // The view window.
    GdkEventExpose *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget);
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	chooser->show(event->area.x, event->area.y, event->area.width,
	              event->area.height);
	return TRUE;
}

/*
 *  Handle a mouse drag event.
 */

#ifdef _WIN32

static bool win32_button = false;

gint Shape_chooser::win32_drag_motion(
    GtkWidget *widget,      // The view window.
    GdkEventMotion *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, event);
	if (win32_button) {
		win32_button = false;

		// prepare the dragged data
		windragdata wdata;

		// This call allows us to recycle the data transfer initialization code.
		//  It's clumsy, but far easier to maintain.
		drag_data_get(nullptr, nullptr, reinterpret_cast<GtkSelectionData*>(&wdata),
		              U7_TARGET_SHAPEID, 0, data);

		POINT pnt;
		GetCursorPos(&pnt);

		Windropsource idsrc(nullptr, pnt.x, pnt.y);
		LPDATAOBJECT idobj = new Winstudioobj(wdata);
		DWORD dndout;

		HRESULT res = DoDragDrop(idobj, &idsrc, DROPEFFECT_COPY, &dndout);
		if (FAILED(res)) {
			g_warning("Oops! Something is wrong with OLE2 DnD..");
		}

		idobj->Release();   // Not sure if we really need this. However, it doesn't hurt either.
	}

	return true;
}
#else
gint Shape_chooser::drag_motion(
    GtkWidget *widget,      // The view window.
    GdkEventMotion *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget);
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	if (!chooser->dragging && chooser->selected >= 0)
		chooser->start_drag(U7_TARGET_SHAPEID_NAME,
		                    U7_TARGET_SHAPEID, reinterpret_cast<GdkEvent *>(event));
	return true;
}
#endif

/*
 *  Handle a mouse button-press event.
 */
gint Shape_chooser::mouse_press(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event
) {
	gtk_widget_grab_focus(widget);

	if (event->button == 4) {
		if (row0 > 0)
			scroll_row_vertical(row0 - 1);
		return TRUE;
	} else if (event->button == 5) {
		scroll_row_vertical(row0 + 1);
		return TRUE;
	}
	int old_selected = selected;
	int new_selected = -1;
	unsigned i;              // Search through entries.
	unsigned infosz = info.size();
	int absx = static_cast<int>(event->x) + hoffset;
	int absy = static_cast<int>(event->y) + voffset;
	for (i = rows[row0].index0; i < infosz; i++) {
		if (info[i].box.distance(absx, absy) <= 2) {
			// Found the box?
			// Indicate we can drag.
#ifdef _WIN32
// Here, we have to override GTK+'s Drag and Drop, which is non-OLE and
// usually stucks outside the program window. I think it's because
// the dragged shape only receives mouse motion events when the new mouse pointer
// position is *still* inside the shape. So if you move the mouse too fast,
// we are stuck.
			win32_button = true;
#endif
			new_selected = i;
			break;
		} else if (info[i].box.y - voffset >= config_height)
			break;      // Past bottom of screen.
	}
	if (new_selected >= 0) {
		select(new_selected);
		render();
		show();
		if (sel_changed)    // Tell client.
			(*sel_changed)();
	}
	if (new_selected < 0 && event->button == 1)
		unselect(true);     // No selection.
	else if (selected == old_selected && old_selected >= 0) {
		// Same square.  Check for dbl-click.
		if (reinterpret_cast<GdkEvent *>(event)->type == GDK_2BUTTON_PRESS)
			edit_shape_info();
	}
	if (event->button == 3)
		gtk_menu_popup(GTK_MENU(create_popup()),
		               nullptr, nullptr, nullptr, nullptr, event->button, event->time);
	return TRUE;
}

/*
 *  Handle mouse button press/release events.
 */
static gint Mouse_press(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event,
    gpointer data           // ->Shape_chooser.
) {
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	return chooser->mouse_press(widget, event);
}
static gint Mouse_release(
    GtkWidget *widget,      // The view window.
    GdkEventButton *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, event);
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	chooser->mouse_up();
	return true;
}

/*
 *  Keystroke in draw-area.
 */
C_EXPORT gboolean
on_draw_key_press(GtkEntry   *entry,
                  GdkEventKey    *event,
                  gpointer    user_data) {
	ignore_unused_variable_warning(entry);
	Shape_chooser *chooser = static_cast<Shape_chooser *>(user_data);
	switch (event->keyval) {
	case GDK_Delete:
		chooser->del_frame();
		return TRUE;
	case GDK_Insert:
		chooser->new_frame();
		return TRUE;
	}
	return FALSE;           // Let parent handle it.
}

const unsigned char transp = 255;

/*
 *  Export the currently selected frame as a .png file.
 *
 *  Output: Modification time of file written, or 0 if error.
 */

time_t Shape_chooser::export_png(
    const char *fname       // File to write out.
) {
	int shnum = info[selected].shapenum;
	int frnum = info[selected].framenum;
	Shape_frame *frame = ifile->get_shape(shnum, frnum);
	int w = frame->get_width();
	int h = frame->get_height();
	Image_buffer8 img(w, h);    // Render into a buffer.
	img.fill8(transp);      // Fill with transparent pixel.
	frame->paint(&img, frame->get_xleft(), frame->get_yabove());
	int xoff = 0;
	int yoff = 0;
	if (frame->is_rle()) {
		xoff = -frame->get_xright();
		yoff = -frame->get_ybelow();
	}
	return export_png(fname, img, xoff, yoff);
}

/*
 *  Convert a GDK color map to a 3*256 byte RGB palette.
 */

static void Get_rgb_palette(
    GdkRgbCmap *palette,
    unsigned char *buf      // 768 bytes (3*256).
) {
	for (int i = 0; i < 256; i++) {
		buf[3 * i] = (palette->colors[i] >> 16) & 0xff;
		buf[3 * i + 1] = (palette->colors[i] >> 8) & 0xff;
		buf[3 * i + 2] = palette->colors[i] & 0xff;
	}
}

/*
 *  Export an image as a .png file.
 *
 *  Output: Modification time of file written, or 0 if error.
 */

time_t Shape_chooser::export_png(
    const char *fname,      // File to write out.
    Image_buffer8 &img,     // Image.
    int xoff, int yoff      // Offset (from bottom-right).
) {
	unsigned char pal[3 * 256]; // Set up palette.
	Get_rgb_palette(palette, pal);
	int w = img.get_width();
	int h = img.get_height();
	struct stat fs;         // Write out to the .png.
	// (Rotate transp. pixel to 0 for the
	//   Gimp's sake.)
	if (!Export_png8(fname, transp, w, h, w, xoff, yoff, img.get_bits(),
	                 &pal[0], 256, true) ||
	        stat(fname, &fs) != 0) {
		Alert("Error creating '%s'", fname);
		return 0;
	}
	return fs.st_mtime;
}

/*
 *  Export the current shape (which better be 8x8 flat) as a tiled PNG.
 *
 *  Output: modification time of file, or 0 if error.
 */

time_t Shape_chooser::export_tiled_png(
    const char *fname,      // File to write out.
    int tiles,          // If #0, write all frames as tiles,
    //   this many in each row (col).
    bool bycols         // Write tiles columns-first.
) {
	assert(selected >= 0);
	int shnum = info[selected].shapenum;
	ExultStudio *studio = ExultStudio::get_instance();
	// Low shape in 'shapes.vga'?
	assert(IS_FLAT(shnum) && file_info == studio->get_vgafile());
	Shape *shape = ifile->extract_shape(shnum);
	assert(shape != nullptr);
	cout << "Writing " << fname << " tiled"
	     << (bycols ? ", by cols" : ", by rows") << " first" << endl;
	int nframes = shape->get_num_frames();
	// Figure #tiles in other dim.
	int dim1_cnt = (nframes + tiles - 1) / tiles;
	int w;
	int h;
	if (bycols) {
		h = tiles * c_tilesize;
		w = dim1_cnt * c_tilesize;
	} else {
		w = tiles * c_tilesize;
		h = dim1_cnt * c_tilesize;
	}
	Image_buffer8 img(w, h);
	img.fill8(transp);      // Fill with transparent pixel.
	for (int f = 0; f < nframes; f++) {
		Shape_frame *frame = shape->get_frame(f);
		if (!frame)
			continue;   // We'll just leave empty ones blank.
		if (frame->is_rle() || frame->get_width() != c_tilesize ||
		        frame->get_height() != c_tilesize) {
			char buf[250];
			snprintf(buf, sizeof(buf), "Can only tile %dx%d flat shapes",
			         c_tilesize, c_tilesize);
			Alert("%s", buf);
			return 0;
		}
		int x;
		int y;
		if (bycols) {
			y = f % tiles;
			x = f / tiles;
		} else {
			x = f % tiles;
			y = f / tiles;
		}
		frame->paint(&img, x * c_tilesize + frame->get_xleft(),
		             y * c_tilesize + frame->get_yabove());
	}
	// Write out to the .png.
	return export_png(fname, img, 0, 0);
}

/*
 *  Bring up the shape-info editor for the selected shape.
 */

void Shape_chooser::edit_shape_info(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	int shnum = info[selected].shapenum;
	int frnum = info[selected].framenum;
	Shape_info *info = nullptr;
	const char *name = nullptr;
	if (shapes_file) {
		// Read info. the first time.
		shapes_file->read_info(studio->get_game_type(), true);
		if (!shapes_file->has_info(shnum))
			shapes_file->set_info(shnum, Shape_info());
		info = &shapes_file->get_info(shnum);
		name = studio->get_shape_name(shnum);
	}
	studio->open_shape_window(shnum, frnum, file_info, name, info);
}

/*
 *  Bring up the user's image-editor for the selected shape.
 */

void Shape_chooser::edit_shape(
    int tiles,          // If #0, write all frames as tiles,
    //   this many in each row (col).
    bool bycols         // Write tiles columns-first.
) {
	ExultStudio *studio = ExultStudio::get_instance();
	int shnum = info[selected].shapenum;
	int frnum = info[selected].framenum;
	string filestr("<SAVEGAME>");   // Set up filename.
	filestr += "/itmp";     // "Image tmp" directory.
	U7mkdir(filestr.c_str(), 0755); // Create if not already there.
	// Lookup <SAVEGAME>.
	filestr = get_system_path(filestr);
	char *ext;
	if (!tiles)         // Create name from file,shape,frame.
		ext = g_strdup_printf("/%s.s%d_f%d.png",
		                      file_info->get_basename(), shnum, frnum);
	else                // Tiled.
		ext = g_strdup_printf("/%s.s%d_%c%d.png",
		                      file_info->get_basename(), shnum,
		                      (bycols ? 'c' : 'r'), tiles);
	filestr += ext;
	g_free(ext);
	const char *fname = filestr.c_str();
	cout << "Writing image '" << fname << "'" << endl;
	time_t mtime;
	if (!tiles) {       // One frame?
		mtime = export_png(fname);
		if (!mtime)
			return;
		// Store info. about file.
		editing_files.push_back(std::make_unique<Editing_file>(
		                            file_info->get_basename(), fname, mtime, shnum, frnum));
	} else {
		mtime = export_tiled_png(fname, tiles, bycols);
		if (!mtime)
			return;
		editing_files.push_back(std::make_unique<Editing_file>(
		                            file_info->get_basename(), fname, mtime, shnum,
		                            tiles, bycols));
	}
	string cmd(studio->get_image_editor());
	cmd += ' ';
	string imgpath;
#ifdef _WIN32
	if (fname[0] == '.' && (fname[1] == '\\' || fname[1] == '/')) {
		char currdir[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, currdir);
		imgpath += currdir;
		if (imgpath[imgpath.length() - 1] != '\\')
			imgpath += '\\';
	}
#endif
	imgpath += fname;
	// handle spaces in path.
	if (imgpath.find(' ') != string::npos)
		imgpath = "\"" + imgpath + "\"";
	cmd += imgpath;
#ifndef _WIN32
	cmd += " &";            // Background.
	int ret = system(cmd.c_str());
	if (ret == 127 || ret == -1)
		Alert("Can't launch '%s'", studio->get_image_editor());
#else
	for (string::iterator it = cmd.begin(); it != cmd.end(); ++it)
		if (*it == '/')
			*it =  '\\';
	PROCESS_INFORMATION pi;
	STARTUPINFO     si;
	std::memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	int ret = CreateProcess(nullptr, const_cast<char *>(cmd.c_str()),
	                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
	if (!ret)
		Alert("Can't launch '%s'", studio->get_image_editor());
#endif
	if (check_editing_timer == -1)  // Monitor files every 6 seconds.
		check_editing_timer = gtk_timeout_add(6000,
		                                      Shape_chooser::check_editing_files_cb, nullptr);
}

/*
 *  Check the list of files being edited externally, and read in any that
 *  have changed.
 *
 *  Output: 1 always.
 */

gint Shape_chooser::check_editing_files_cb(
    gpointer data
) {
	ignore_unused_variable_warning(data);
	ExultStudio *studio = ExultStudio::get_instance();
	// Is focus in main window?
	if (studio->has_focus())
		check_editing_files();
	return 1;           // Keep repeating.
}
// This one doesn't check focus.
gint Shape_chooser::check_editing_files(
) {
	bool modified = false;
	for (auto& ed : editing_files) {
		struct stat fs;     // Check mod. time of file.
		if (stat(ed->pathname.c_str(), &fs) != 0) {
			// Gone?
			ed.reset();
			continue;
		}
		if (fs.st_mtime <= ed->mtime)
			continue;   // Still not changed.
		ed->mtime = fs.st_mtime;
		read_back_edited(ed.get());
		modified = true;    // Did one.
	}
	editing_files.erase(std::remove_if(editing_files.begin(), editing_files.end(),
		[](const auto& ed) {
			return ed == nullptr;
		}), editing_files.end());
	if (modified) {         // Repaint if modified.
		ExultStudio *studio = ExultStudio::get_instance();
		Object_browser *browser = studio->get_browser();
		if (browser) {
			// Repaint main window.
			browser->render();
			browser->show();
		}
		studio->update_group_windows(nullptr);
	}
	return 1;           // Continue timeouts.
}

/*
 *  Find the closest color in a palette to a given one.
 *
 *  Output: 0-254, whichever has closest color.
 */

static int Find_closest_color(
    const unsigned char *pal,     // 3*255 bytes.
    int r, int g, int b     // Color to match.
) {
	int best_index = -1;
	long best_distance = 0xfffffff;
	// Be sure to search rotating colors too.
	for (int i = 0; i < 0xff; i++) {
		// Get deltas.
		long dr = r - pal[3 * i];
		long dg = g - pal[3 * i + 1];
		long db = b - pal[3 * i + 2];
		// Figure distance-squared.
		long dist = dr * dr + dg * dg + db * db;
		if (dist < best_distance) {
			// Better than prev?
			best_index = i;
			best_distance = dist;
		}
	}
	return best_index;
}

/*
 *  Convert an 8-bit image in one palette to another.
 */

static void Convert_indexed_image(
    unsigned char *pixels,      // Pixels.
    int count,          // # pixels.
    unsigned char *oldpal,      // Palette pixels currently uses.
    int oldpalsize,         // Size of old palette.
    unsigned char *newpal       // Palette (255 bytes) to convert to.
) {
	if (memcmp(oldpal, newpal, oldpalsize) == 0)
		return;         // Old palette matches new.
	int map[256];           // Set up old->new map.
	int i;
	for (i = 0; i < 256; i++)   // Set to 'unknown'.
		map[i] = -1;
	map[transp] = transp;       // But leave transparent pix. alone.
	// Go through pixels.
	for (i = 0; i < count; i++) {
		unsigned char pix = *pixels;
		if (map[pix] == -1) // New one?
			map[pix] = Find_closest_color(newpal, oldpal[3 * pix],
			                              oldpal[3 * pix + 1], oldpal[3 * pix + 2]);
		*pixels++ = map[pix];
	}
}

/*
 *  Import a PNG file into a given shape,frame.
 */

static void Import_png(
    const char *fname,      // Filename.
    Shape_file_info *finfo,     // What we're updating.
    unsigned char *pal,     // 3*255 bytes game palette.
    int shapenum, int framenum  // Shape, frame to update
) {
	ExultStudio *studio = ExultStudio::get_instance();
	Vga_file *ifile = finfo->get_ifile();
	if (!ifile)
		return;         // Shouldn't happen.
	Shape *shape = ifile->extract_shape(shapenum);
	if (!shape)
		return;
	int w;
	int h;
	int rowsize;
	int xoff;
	int yoff;
	int palsize;
	unsigned char *pixels;
	unsigned char *oldpal;
	// Import, with 255 = transp. index.
	if (!Import_png8(fname, 255, w, h, rowsize, xoff, yoff,
	                 pixels, oldpal, palsize))
		return;         // Just return if error, for now.
	// Convert to game palette.
	Convert_indexed_image(pixels, h * rowsize, oldpal, palsize, pal);
	delete [] oldpal;
	// Low shape in 'shapes.vga'?
	bool flat = IS_FLAT(shapenum) && finfo == studio->get_vgafile();
	int xleft;
	int yabove;
	if (flat) {
		xleft = yabove = c_tilesize;
		if (w != c_tilesize || h != c_tilesize || rowsize != c_tilesize) {
			char *msg = g_strdup_printf(
			                "Shape %d must be %dx%d", shapenum, c_tilesize, c_tilesize);
			studio->prompt(msg, "Continue");
			g_free(msg);
			delete [] pixels;
			return;
		}
	} else {            // RLE. xoff,yoff are neg. from bottom.
		xleft = w + xoff - 1;
		yabove = h + yoff - 1;
	}
	shape->set_frame(make_unique<Shape_frame>(pixels,
	                                 w, h, xleft, yabove, !flat), framenum);
	delete [] pixels;
	finfo->set_modified();
}

/*
 *  Import a tiled PNG file into a given shape's frames.
 */

static void Import_png_tiles(
    const char *fname,      // Filename.
    Shape_file_info *finfo,     // What we're updating.
    unsigned char *pal,     // 3*255 bytes game palette.
    int shapenum,
    int tiles,          // #tiles per row/col.
    bool bycols         // Write tiles columns-first.
) {
	Vga_file *ifile = finfo->get_ifile();
	if (!ifile)
		return;         // Shouldn't happen.
	Shape *shape = ifile->extract_shape(shapenum);
	if (!shape)
		return;
	int nframes = shape->get_num_frames();
	cout << "Reading " << fname << " tiled"
	     << (bycols ? ", by cols" : ", by rows") << " first" << endl;
	// Figure #tiles in other dim.
	int dim0_cnt = tiles;
	int dim1_cnt = (nframes + dim0_cnt - 1) / dim0_cnt;
	int needw;
	int needh;       // Figure min. image dims.
	if (bycols) {
		needh = dim0_cnt * c_tilesize;
		needw = dim1_cnt * c_tilesize;
	} else {
		needw = dim0_cnt * c_tilesize;
		needh = dim1_cnt * c_tilesize;
	}
	int w;
	int h;
	int rowsize;
	int xoff;
	int yoff;
	int palsize;
	unsigned char *pixels;
	unsigned char *oldpal;
	// Import, with 255 = transp. index.
	if (!Import_png8(fname, 255, w, h, rowsize, xoff, yoff,
	                 pixels, oldpal, palsize)) {
		Alert("Error reading '%s'", fname);
		return;
	}
	// Convert to game palette.
	Convert_indexed_image(pixels, h * rowsize, oldpal, palsize, pal);
	delete [] oldpal;
	if (w < needw || h < needh) {
		Alert("File '%s' image is too small.  %dx%d required.",
		      fname, needw, needh);
		delete [] pixels;
		return;
	}
	for (int frnum = 0; frnum < nframes; frnum++) {
		int x;
		int y;
		if (bycols) {
			y = frnum % dim0_cnt;
			x = frnum / dim0_cnt;
		} else {
			x = frnum % dim0_cnt;
			y = frnum / dim0_cnt;
		}
		unsigned char *src = pixels + w * c_tilesize * y + c_tilesize * x;
		unsigned char buf[c_num_tile_bytes];    // Move tile to buffer.
		unsigned char *ptr = &buf[0];
		for (int row = 0; row < c_tilesize; row++) {
			// Write it out.
			memcpy(ptr, src, c_tilesize);
			ptr += c_tilesize;
			src += w;
		}
		shape->set_frame(make_unique<Shape_frame>(&buf[0], c_tilesize, c_tilesize,
		                                 c_tilesize, c_tilesize, false), frnum);
	}
	delete [] pixels;
	finfo->set_modified();
}

/*
 *  Read in a shape that was changed by an external program (i.e., Gimp).
 */

void Shape_chooser::read_back_edited(
    Editing_file *ed
) {
	ExultStudio *studio = ExultStudio::get_instance();
	Shape_file_info *finfo = studio->open_shape_file(
	                             ed->vga_basename.c_str());
	if (!finfo)
		return;
	unsigned char pal[3 * 256]; // Convert to 0-255 RGB's.
	unsigned char *palbuf = studio->get_palbuf();
	for (int i = 0; i < 3 * 256; i++)
		pal[i] = palbuf[i] * 4;
	if (!ed->tiles)
		Import_png(ed->pathname.c_str(), finfo, pal,
		           ed->shapenum, ed->framenum);
	else
		Import_png_tiles(ed->pathname.c_str(), finfo, pal,
		                 ed->shapenum, ed->tiles, ed->bycolumns);
}

/*
 *  Delete all the files being edited after doing a final check.
 */

void Shape_chooser::clear_editing_files(
) {
	check_editing_files();      // Import any that changed.
	for (auto& ed : editing_files) {
		unlink(ed->pathname.c_str());
	}
	editing_files.clear();
	if (check_editing_timer != -1)
		gtk_timeout_remove(check_editing_timer);
	check_editing_timer = -1;
}

/*
 *  Export current frame.
 */

void Shape_chooser::export_frame(
    char *fname,
    gpointer user_data
) {
	Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
	if (U7exists(fname)) {
		char *msg = g_strdup_printf(
		                "'%s' already exists.  Overwrite?", fname);
		int answer = Prompt(msg, "Yes", "No");
		g_free(msg);
		if (answer != 0)
			return;
	}
	if (ed->selected < 0)
		return;         // Shouldn't happen.
	ed->export_png(fname);
}

/*
 *  Import current frame.
 */

void Shape_chooser::import_frame(
    char *fname,
    gpointer user_data
) {
	Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
	if (ed->selected < 0)
		return;         // Shouldn't happen.
	int shnum = ed->info[ed->selected].shapenum;
	int frnum = ed->info[ed->selected].framenum;
	unsigned char pal[3 * 256]; // Get current palette.
	Get_rgb_palette(ed->palette, pal);
	Import_png(fname, ed->file_info, pal, shnum, frnum);
	ed->render();
	ed->show();
	ExultStudio *studio = ExultStudio::get_instance();
	studio->update_group_windows(nullptr);
}

/*
 *  Export all frames of current shape.
 */

void Shape_chooser::export_all_pngs(
    char *fname,
    int shnum
) {
	for (int i = 0; i < ifile->get_num_frames(shnum); i++) {
		char *fullname = new char[strlen(fname) + 30];
		sprintf(fullname, "%s%02d.png", fname, i);
		cout << "Writing " << fullname << endl;
		Shape_frame *frame = ifile->get_shape(shnum, i);
		int w = frame->get_width();
		int h = frame->get_height();
		Image_buffer8 img(w, h);    // Render into a buffer.
		img.fill8(transp);      // Fill with transparent pixel.
		frame->paint(&img, frame->get_xleft(), frame->get_yabove());
		int xoff = 0;
		int yoff = 0;
		if (frame->is_rle()) {
			xoff = -frame->get_xright();
			yoff = -frame->get_ybelow();
		}
		export_png(fullname, img, xoff, yoff);
		delete [] fullname;
	}
}

void Shape_chooser::export_all_frames(
    char *fname,
    gpointer user_data
) {
	Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
	int shnum = ed->info[ed->selected].shapenum;
	ed->export_all_pngs(fname, shnum);
}

void Shape_chooser::export_shape(
    char *fname,
    gpointer user_data
) {
	Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
	int shnum = ed->info[ed->selected].shapenum;
	Shape *shp = ed->ifile->extract_shape(shnum);
	Image_file_info::write_file(fname, &shp, 1, true);
}

/*
 *  Import all frames into current shape.
 */

void Shape_chooser::import_all_pngs(
    char *fname,
    int shnum
) {
	char *fullname = new char[strlen(fname) + 30];
	sprintf(fullname, "%s%02d.png", fname, 0);
	if (!U7exists(fullname)) {
		std::cerr << "Invalid base file name for import of all frames!" << std::endl;
		delete [] fullname;
		return;
	}
	int i = 0;
	unsigned char pal[3 * 256]; // Get current palette.
	Get_rgb_palette(palette, pal);
	Shape *shape = ifile->extract_shape(shnum);
	if (!shape) {
		delete [] fullname;
		return;
	}
	ExultStudio *studio = ExultStudio::get_instance();
	while (U7exists(fullname)) {
		int w;
		int h;
		int rowsize;
		int xoff;
		int yoff;
		int palsize;
		unsigned char *pixels;
		unsigned char *oldpal;
		// Import, with 255 = transp. index.
		if (!Import_png8(fullname, 255, w, h, rowsize, xoff, yoff,
		                 pixels, oldpal, palsize)) {
			delete [] fullname;
			return;         // Just return if error, for now.
		}
		// Convert to game palette.
		Convert_indexed_image(pixels, h * rowsize, oldpal, palsize, pal);
		delete [] oldpal;
		int xleft = w + xoff - 1;
		int yabove = h + yoff - 1;
		auto frame = make_unique<Shape_frame>(pixels,
		                                     w, h, xleft, yabove, true);
		if (i < ifile->get_num_frames(shnum))
			shape->set_frame(std::move(frame), i);
		else
			shape->add_frame(std::move(frame), i);
		delete [] pixels;

		i++;
		sprintf(fullname, "%s%02d.png", fname, i);
	}
	render();
	show();
	file_info->set_modified();
	studio->update_group_windows(nullptr);
	delete [] fullname;
}

void Shape_chooser::import_all_frames(
    char *fname,
    gpointer user_data
) {
	Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
	if (ed->selected < 0)
		return;         // Shouldn't happen.
	int shnum = ed->info[ed->selected].shapenum;
	int len = strlen(fname);
	// Ensure we have a valid file name.
	if (fname[len - 4] == '.')
		fname[len - 6] = 0;
	ed->import_all_pngs(fname, shnum);
}

void Shape_chooser::import_shape(
    char *fname,
    gpointer user_data
) {
	if (U7exists(fname)) {
		Shape_chooser *ed = static_cast<Shape_chooser *>(user_data);
		if (ed->selected < 0)
			return;         // Shouldn't happen.
		IFileDataSource ds(fname);
		// Check to see if it is a valid shape file.
		// We never get here through a flat, so we don't deal
		// with that case. These tests aren't perfect!
		int size = ds.getSize();
		int len = ds.read4();
		int first;
		if (len != size || (first = ds.read4()) > size || (first % 4) != 0)
			return;
		int shnum = ed->info[ed->selected].shapenum;
		Shape *shp = ed->ifile->extract_shape(shnum);
		ds.seek(0);
		shp->load(&ds);
		shp->set_modified();
		ed->render();
		ed->show();
		ed->file_info->set_modified();
	}
}

/*
 *  Add a frame.
 */

void Shape_chooser::new_frame(
) {
	if (selected < 0)
		return;
	int shnum = info[selected].shapenum;
	int frnum = info[selected].framenum;
	Vga_file *ifile = file_info->get_ifile();
	// Read entire shape.
	Shape *shape = ifile->extract_shape(shnum);
	if (!shape ||           // Shouldn't happen.
	        // We'll insert AFTER frnum.
	        frnum > shape->get_num_frames())
		return;
	// Low shape in 'shapes.vga'?
	ExultStudio *studio = ExultStudio::get_instance();
	bool flat = IS_FLAT(shnum) && file_info == studio->get_vgafile();
	int w = 0;
	int h = 0;
	int xleft;
	int yabove;
	if (flat)
		w = h = xleft = yabove = c_tilesize;
	else {              // Find largest frame.
		int cnt = shape->get_num_frames();
		for (int i = 0; i < cnt; i++) {
			Shape_frame *fr = shape->get_frame(i);
			assert(fr);
			int ht = fr->get_height();
			if (ht > h)
				h = ht;
			int wd = fr->get_width();
			if (wd > w)
				w = wd;
		}
		if (h == 0)
			h = c_tilesize;
		if (w == 0)
			w = c_tilesize;
		xleft = w - 1;
		yabove = h - 1;
	}
	Image_buffer8 img(w, h);
	img.fill8(1);           // Just use color #1.
	if (w > 2 && h > 2)
		img.fill8(2, w - 2, h - 2, 1, 1);
	shape->add_frame(make_unique<Shape_frame>(img.get_bits(),
	                                     w, h, xleft, yabove, !flat), frnum + 1);
	file_info->set_modified();
	Object_browser *browser = studio->get_browser();
	if (browser) {
		// Repaint main window.
		if (frames_mode)
			browser->setup_info(true);
		browser->render();
		browser->show();
	}
	studio->update_group_windows(nullptr);
}

/*
 *  Callback for new-shape 'okay'.
 */
C_EXPORT void
on_new_shape_okay_clicked(GtkButton       *button,
                          gpointer         user_data) {
	ignore_unused_variable_warning(user_data);
	GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(button));
	Shape_chooser *chooser = static_cast<Shape_chooser *>(
	                         gtk_object_get_user_data(GTK_OBJECT(win)));
	chooser->create_new_shape();
	gtk_widget_hide(win);
}
// Toggled 'From font' button:
C_EXPORT void on_new_shape_font_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(btn));
	Shape_chooser *chooser = static_cast<Shape_chooser *>(
	                         gtk_object_get_user_data(GTK_OBJECT(win)));
	chooser->from_font_toggled(on);
}
C_EXPORT gboolean on_new_shape_font_color_draw_expose_event(
    GtkWidget *widget,      // The draw area.
    GdkEventExpose *event,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	ExultStudio *studio = ExultStudio::get_instance();
	int index = studio->get_spin("new_shape_font_color");
	Shape_chooser *ed = static_cast<Shape_chooser *>(
	                    gtk_object_get_user_data(GTK_OBJECT(widget)));
	guint32 color = ed->get_color(index);
	GdkGC *gc = static_cast<GdkGC *>(
	            gtk_object_get_data(GTK_OBJECT(widget), "color_gc"));
	if (!gc) {
		gc = gdk_gc_new(widget->window);
		gtk_object_set_data(GTK_OBJECT(widget), "color_gc", gc);
	}
	gdk_rgb_gc_set_foreground(gc, color);
	gdk_draw_rectangle(widget->window, gc, TRUE, event->area.x,
	                   event->area.y, event->area.width, event->area.height);
	return TRUE;
}
C_EXPORT void on_new_shape_font_color_changed(
    GtkSpinButton *button,
    gpointer user_data
) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	// Show new color.
	GtkWidget *draw = glade_xml_get_widget(studio->get_xml(),
	                                       "new_shape_font_color_draw");
	GdkRectangle area = {0, 0, draw->allocation.width,
	                     draw->allocation.height
	                    };
	gtk_widget_draw(draw, &area);
}

/*
 *  Font file was selected.
 */

static void font_file_chosen(
    const char *fname,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_entry("new_shape_font_name", fname);
	studio->set_spin("new_shape_nframes", 128);
}

/*
 *  'From font' toggled in 'New shape' dialog.
 */

void Shape_chooser::from_font_toggled(
    bool on
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("new_shape_font_name", on);
	if (!on)
		return;
	studio->set_sensitive("new_shape_font_color", true);
	studio->set_sensitive("new_shape_font_height", true);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Choose font file", font_file_chosen, nullptr);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}

/*
 *  Add a new shape.
 */

void Shape_chooser::new_shape(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	GladeXML *xml = studio->get_xml();
	GtkWidget *win = glade_xml_get_widget(xml, "new_shape_window");
	gtk_window_set_modal(GTK_WINDOW(win), true);
	gtk_object_set_user_data(GTK_OBJECT(win), this);
	// Get current selection.
	int shnum = selected >= 0 ? info[selected].shapenum : 0;
	GtkWidget *spin = glade_xml_get_widget(xml, "new_shape_num");
	GtkAdjustment *adj = gtk_spin_button_get_adjustment(
	                         GTK_SPIN_BUTTON(spin));
	adj->upper = c_max_shapes - 1;
	gtk_adjustment_changed(adj);
	Vga_file *ifile = file_info->get_ifile();
	int shstart;            // Find an unused shape.
	for (shstart = shnum; shstart <= adj->upper; shstart++)
		if (shstart >= ifile->get_num_shapes() ||
		        !ifile->get_num_frames(shstart))
			break;
	if (shstart > adj->upper) {
		for (shstart = shnum - 1; shstart >= 0; shstart--)
			if (!ifile->get_num_frames(shstart))
				break;
		if (shstart < 0)
			shstart = shnum;
	}
	gtk_adjustment_set_value(adj, shstart);
	spin = glade_xml_get_widget(xml, "new_shape_nframes");
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	bool flat = IS_FLAT(shnum) && file_info == studio->get_vgafile();
	if (flat)
		adj->upper = 31;
	else
		adj->upper = 255;
	gtk_adjustment_changed(adj);
	spin = glade_xml_get_widget(xml, "new_shape_font_height");
	studio->set_sensitive("new_shape_font_height", false);
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	adj->lower = 4;
	adj->upper = 64;
	gtk_adjustment_changed(adj);
	spin = glade_xml_get_widget(xml, "new_shape_font_color");
	studio->set_sensitive("new_shape_font_color", false);
	adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	adj->lower = 0;
	adj->upper = 255;
	gtk_adjustment_changed(adj);
	// Unset 'From font:'.
	studio->set_toggle("new_shape_font", false);
#ifndef HAVE_FREETYPE2          /* No freetype?  No fonts.  */
	studio->set_sensitive("new_shape_font", false);
#endif
	// Store our pointer in color drawer.
	GtkWidget *draw = glade_xml_get_widget(xml,
	                                       "new_shape_font_color_draw");
	gtk_object_set_user_data(GTK_OBJECT(draw), this);
	gtk_widget_show(win);
}

/*
 *  Add a new shape after the user has clicked 'Okay' in the new-shape
 *  dialog.
 */

void Shape_chooser::create_new_shape(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	int shnum = studio->get_spin("new_shape_num");
	int nframes = studio->get_spin("new_shape_nframes");
	if (nframes <= 0)
		nframes = 1;
	else if (nframes > 256)
		nframes = 256;
	Vga_file *ifile = file_info->get_ifile();
	if (shnum < ifile->get_num_shapes() && ifile->get_num_frames(shnum)) {
		if (Prompt("Replace existing shape?", "Yes", "No") != 0)
			return;
	}
	Shape *shape = ifile->new_shape(shnum);
	if (!shape) {
		Alert("Can't create shape %d", shnum);
		return;
	}
	// Create frames.
	bool flat = IS_FLAT(shnum) && file_info == studio->get_vgafile();
	bool use_font = false;
#ifdef HAVE_FREETYPE2
	// Want to create from a font?
	use_font = studio->get_toggle("new_shape_font");
	const char *fontname = studio->get_text_entry("new_shape_font_name");
	use_font = use_font && (fontname != nullptr) && *fontname != 0;
	if (use_font) {
		if (flat) {
			Alert("Can't load font into a 'flat' shape");
			return;
		}
		int ht = studio->get_spin("new_shape_font_height");
		int fg = studio->get_spin("new_shape_font_color");
		if (!Gen_font_shape(shape, fontname, nframes,
		                    // Use transparent color for bgnd.
		                    ht, fg, 255))
			Alert("Error loading font file '%s'", fontname);
	}
#endif
	if (!use_font) {
		int w = c_tilesize;
		int h = c_tilesize;
		int xleft = flat ? c_tilesize : w - 1;
		int yabove = flat ? c_tilesize : h - 1;
		Image_buffer8 img(w, h);
		img.fill8(1);       // Just use color #1.
		img.fill8(2, w - 2, h - 2, 1, 1);
		// Include some transparency.
		img.fill8(255, w / 2, h / 2, w / 4, h / 4);
		for (int i = 0; i < nframes; i++)
			shape->add_frame(make_unique<Shape_frame>(img.get_bits(),
			                                 w, h, xleft, yabove, !flat), i);
	}
	file_info->set_modified();
	Object_browser *browser = studio->get_browser();
	if (browser) {
		// Repaint main window.
		browser->setup_info(true);
		browser->render();
		browser->show();
	}
	studio->update_group_windows(nullptr);
}

/*
 *  Delete a frame, and the shape itself if this is its last.
 */

void Shape_chooser::del_frame(
) {
	if (selected < 0)
		return;
	int shnum = info[selected].shapenum;
	int frnum = info[selected].framenum;
	Vga_file *ifile = file_info->get_ifile();
	// Read entire shape.
	Shape *shape = ifile->extract_shape(shnum);
	if (!shape ||           // Shouldn't happen.
	        frnum > shape->get_num_frames() - 1)
		return;
	// 1-shape file & last frame?
	if (!ifile->is_flex() && shape->get_num_frames() == 1)
		return;
	shape->del_frame(frnum);
	file_info->set_modified();
	ExultStudio *studio = ExultStudio::get_instance();
	Object_browser *browser = studio->get_browser();
	if (browser) {
		// Repaint main window.
		browser->render();
		browser->show();
	}
	studio->update_group_windows(nullptr);
}

/*
 *  Someone wants the dragged shape.
 */

void Shape_chooser::drag_data_get(
    GtkWidget *widget,      // The view window.
    GdkDragContext *context,
    GtkSelectionData *seldata,  // Fill this in.
    guint info,
    guint time,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, context, time);
	cout << "In DRAG_DATA_GET" << endl;
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	if (chooser->selected < 0 || info != U7_TARGET_SHAPEID)
		return;         // Not sure about this.
	guchar buf[30];
	int file = chooser->ifile->get_u7drag_type();
	if (file == U7_SHAPE_UNK)
		file = U7_SHAPE_SHAPES; // Just assume it's shapes.vga.
	Shape_entry &shinfo = chooser->info[chooser->selected];
	int len = Store_u7_shapeid(buf, file, shinfo.shapenum,
	                           shinfo.framenum);
	cout << "Setting selection data (" << shinfo.shapenum <<
	     '/' << shinfo.framenum << ')' << endl;
#ifdef _WIN32
	windragdata *wdata = reinterpret_cast<windragdata*>(seldata);
	wdata->assign(info, len, buf);
#else
	// Make us owner of xdndselection.
	//gtk_selection_owner_set(widget, gdk_atom_intern("XdndSelection", 0),
	//                          time);
	// Set data.
	gtk_selection_data_set(seldata,
	                       gdk_atom_intern(U7_TARGET_SHAPEID_NAME, 0),
	                       8, buf, len);
#endif
}

/*
 *  Another app. has claimed the selection.
 */

gint Shape_chooser::selection_clear(
    GtkWidget *widget,      // The view window.
    GdkEventSelection *event,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, event, data);
//	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	cout << "SELECTION_CLEAR" << endl;
	return TRUE;
}

/*
 *  Beginning of a drag.
 */

gint Shape_chooser::drag_begin(
    GtkWidget *widget,      // The view window.
    GdkDragContext *context,
    gpointer data           // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget);
	cout << "In DRAG_BEGIN" << endl;
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	if (chooser->selected < 0)
		return FALSE;       // ++++Display a halt bitmap.
	// Get ->shape.
	Shape_entry &shinfo = chooser->info[chooser->selected];
	Shape_frame *shape = chooser->ifile->get_shape(shinfo.shapenum,
	                     shinfo.framenum);
	if (!shape)
		return FALSE;
	chooser->set_drag_icon(context, shape); // Set icon for dragging.
	return TRUE;
}

/*
 *  Scroll to a new shape/frame.
 */

void Shape_chooser::scroll_row_vertical(
    unsigned newrow          // Abs. index of row to show.
) {
	if (newrow >= rows.size())
		return;
	row0 = newrow;
	row0_voffset = 0;
	render();
	show();
}

/*
 *  Scroll to new pixel offset.
 */

void Shape_chooser::scroll_vertical(
    int newoffset
) {
	int delta = newoffset - voffset;
	while (delta > 0 && row0 < rows.size() - 1) {
		// Going down.
		int rowh = rows[row0].height - row0_voffset;
		if (delta < rowh) {
			// Part of current row.
			voffset += delta;
			row0_voffset += delta;
			delta = 0;
		} else {
			// Go down to next row.
			voffset += rowh;
			delta -= rowh;
			++row0;
			row0_voffset = 0;
		}
	}
	while (delta < 0) {
		if (-delta <= row0_voffset) {
			voffset += delta;
			row0_voffset += delta;
			delta = 0;
		} else if (row0_voffset) {
			voffset -= row0_voffset;
			delta += row0_voffset;
			row0_voffset = 0;
		} else {
			if (row0 == 0)
				break;
			--row0;
			row0_voffset = 0;
			voffset -= rows[row0].height;
			delta += rows[row0].height;
			if (delta > 0) {
				row0_voffset = delta;
				voffset += delta;
				delta = 0;
			}
		}
	}
	render();
	show();
	update_statusbar();
}

/*
 *  Adjust vertical scroll amounts after laying out shapes.
 */

void Shape_chooser::setup_vscrollbar(
) {
	GtkAdjustment *adj = gtk_range_get_adjustment(
	                         GTK_RANGE(vscroll));
	adj->value = 0;
	adj->lower = 0;
	adj->upper = total_height;
	adj->step_increment = 16;   // +++++FOR NOW.
	adj->page_increment = config_height;
	adj->page_size = config_height;
	gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
}

/*
 *  Adjust horizontal scroll amounts.
 */

void Shape_chooser::setup_hscrollbar(
    int newmax          // New max., or -1 to leave alone.
) {
	GtkAdjustment *adj = gtk_range_get_adjustment(
	                         GTK_RANGE(hscroll));
	if (newmax > 0)
		adj->upper = newmax;
	adj->page_increment = draw->allocation.width;
	adj->page_size = draw->allocation.width;
	if (adj->page_size > adj->upper)
		adj->upper = adj->page_size;
	gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
}

/*
 *  Handle a scrollbar event.
 */

void Shape_chooser::vscrolled(      // For vertical scrollbar.
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Shape_chooser.
) {
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	cout << "Scrolled to " << adj->value << '\n';
	gint newindex = static_cast<gint>(adj->value);
	chooser->scroll_vertical(newindex);
}
void Shape_chooser::hscrolled(      // For horizontal scrollbar.
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Shape_chooser.
) {
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	chooser->hoffset = static_cast<gint>(adj->value);
	chooser->render();
	chooser->show();
}

/*
 *  Handle a change to the 'frame' spin button.
 */

void Shape_chooser::frame_changed(
    GtkAdjustment *adj,     // The adjustment.
    gpointer data           // ->Shape_chooser.
) {
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	gint newframe = static_cast<gint>(adj->value);
	if (chooser->selected >= 0) {
		Shape_entry &shinfo = chooser->info[chooser->selected];
		int nframes = chooser->ifile->get_num_frames(shinfo.shapenum);
		if (newframe >= nframes)    // Just checking
			return;
		shinfo.framenum = newframe;
		if (chooser->frames_mode)   // Get sel. frame in view.
			chooser->scroll_to_frame();
		chooser->render();
		chooser->show();
		chooser->update_statusbar();
	}
}

/*
 *  'All frames' toggled.
 */

void Shape_chooser::all_frames_toggled(
    GtkToggleButton *btn,
    gpointer data
) {
	Shape_chooser *chooser = static_cast<Shape_chooser *>(data);
	bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	chooser->frames_mode = on;
	if (on)             // Frame => show horiz. scrollbar.
		gtk_widget_show(chooser->hscroll);
	else
		gtk_widget_hide(chooser->hscroll);
	// The old index is no longer valid, so we need to remember the shape.
	int indx = chooser->selected >= 0 ? chooser->selected
	           : static_cast<int>(chooser->rows[chooser->row0].index0);
	int shnum = chooser->info[indx].shapenum;
	chooser->selected = -1;
	chooser->setup_info();
	indx = chooser->find_shape(shnum);
	if (indx >= 0)          // Get back to given shape.
		chooser->goto_index(indx);
}

/*
 *  Handle popup menu items.
 */

void Shape_chooser::on_shapes_popup_info_activate(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	static_cast<Shape_chooser *>(udata)->edit_shape_info();
}

void Shape_chooser::on_shapes_popup_edit_activate(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	static_cast<Shape_chooser *>(udata)->edit_shape();
}

void Shape_chooser::on_shapes_popup_edtiles_activate(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	Shape_chooser *ch = static_cast<Shape_chooser *>(udata);
	if (ch->selected < 0)
		return;         // Shouldn't happen.
	ExultStudio *studio = ExultStudio::get_instance();
	GladeXML *xml = studio->get_xml();
	GtkWidget *win = glade_xml_get_widget(xml, "export_tiles_window");
	gtk_window_set_modal(GTK_WINDOW(win), true);
	gtk_object_set_user_data(GTK_OBJECT(win), ch);
	// Get current selection.
	int shnum = ch->info[ch->selected].shapenum;
	Vga_file *ifile = ch->file_info->get_ifile();
	int nframes = ifile->get_num_frames(shnum);
	GtkWidget *spin = glade_xml_get_widget(xml, "export_tiles_count");
	GtkAdjustment *adj = gtk_spin_button_get_adjustment(
	                         GTK_SPIN_BUTTON(spin));
	adj->lower = 1;
	adj->upper = nframes;
	gtk_adjustment_changed(adj);
	gtk_widget_show(win);
}

static void on_shapes_popup_import(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Import frame from a .png file",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::import_frame),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_export(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Export frame to a .png file",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::export_frame),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_export_all(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Choose the base .png file name for all frames",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::export_all_frames),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_import_all(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Choose the one of the .png sprites to import",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::import_all_frames),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_export_shape(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Choose the shp file name",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::export_shape),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_import_shape(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	GtkFileSelection *fsel = Create_file_selection(
	                             "Choose the shp file to import",
	                             reinterpret_cast<File_sel_okay_fun>(Shape_chooser::import_shape),
	                             udata);
	if (is_system_path_defined("<PATCH>")) {
		// Default to a writable location.
		string patch = get_system_path("<PATCH>/");
		gtk_file_selection_set_filename(fsel, patch.c_str());
	}
	gtk_widget_show(GTK_WIDGET(fsel));
}
static void on_shapes_popup_new_frame(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	static_cast<Shape_chooser *>(udata)->new_frame();
}
static void on_shapes_popup_new_shape(
    GtkMenuItem *item,
    gpointer udata
) {
	ignore_unused_variable_warning(item);
	static_cast<Shape_chooser *>(udata)->new_shape();
}

/*
 *  Callback for edit-tiles 'okay'.
 */
C_EXPORT void
on_export_tiles_okay_clicked(GtkButton       *button,
                             gpointer         user_data) {
	ignore_unused_variable_warning(user_data);
	GtkWidget *win = gtk_widget_get_toplevel(GTK_WIDGET(button));
	Shape_chooser *chooser = static_cast<Shape_chooser *>(
	                         gtk_object_get_user_data(GTK_OBJECT(win)));
	ExultStudio *studio = ExultStudio::get_instance();
	int tiles = studio->get_spin("export_tiles_count");
	bool bycol = studio->get_toggle("tiled_by_columns");
	chooser->edit_shape(tiles, bycol);
	gtk_widget_hide(win);
}

/*
 *  Handle a shape dropped on our draw area.
 */

void Shape_chooser::shape_dropped_here(
    int file,           // U7_SHAPE_SHAPES.
    int shape,
    int frame
) {
	ignore_unused_variable_warning(frame);
	// Got to be from same file type.
	if (ifile->get_u7drag_type() == file && group != nullptr) {
		// Add to group.
		if (group->is_builtin()) {
			Alert("Can't modify builtin group.");
			return;
		}
		group->add(shape);
		// Update all windows for this group.
		ExultStudio::get_instance()->update_group_windows(group);
	}
}

/*
 *  Get # shapes we can display.
 */

int Shape_chooser::get_count(
) {
	return group ? group->size() : ifile->get_num_shapes();
}

/*
 *  Search for an entry.
 */

void Shape_chooser::search(
    const char *search,     // What to search for.
    int dir             // 1 or -1.
) {
	if (!shapes_file)       // Not 'shapes.vga'.
		return;         // In future, maybe find shape #?
	int total = get_count();
	if (!total)
		return;         // Empty.
	// Read info if not read.
	ExultStudio *studio = ExultStudio::get_instance();
	shapes_file->read_info(studio->get_game_type(), true);
	// Start with selection, or top.
	int start = selected >= 0 ? selected : static_cast<int>(rows[row0].index0);
	int i;
	start += dir;
	int stop = dir == -1 ? -1 : static_cast<int>(info.size());
	codepageStr srch(search);
	for (i = start; i != stop; i += dir) {
		int shnum = info[i].shapenum;
		const char *nm = studio->get_shape_name(shnum);
		if (nm && search_name(nm, srch))
			break;      // Found it.
		const Shape_info &info = shapes_file->get_info(shnum);
		if (info.has_frame_name_info()) {
			bool found = false;
			const std::vector<Frame_name_info> &nminf = info.get_frame_name_info();
			for (std::vector<Frame_name_info>::const_iterator it = nminf.begin();
			        it != nminf.end(); ++it) {
				int type = it->get_type();
				int msgid = it->get_msgid();
				if (type == -255 || type == -1 || msgid >= get_num_misc_names()
				        || !get_misc_name(msgid))
					continue;   // Keep looking.
				if (search_name(get_misc_name(msgid), srch)) {
					found = true;
					break;      // Found it.
				}
			}
			if (found)
				break;
		}
	}
	if (i == stop)
		return;         // Not found.
	goto_index(i);
	select(i);
	show();
}

/*
 *  Locate shape on game map.
 */

void Shape_chooser::locate(
    bool upwards
) {
	if (selected < 0)
		return;         // Shouldn't happen.
	unsigned char data[Exult_server::maxlength];
	unsigned char *ptr = &data[0];
	int qual = ExultStudio::get_num_entry(get_loc_q(), -1);
	int frnum = ExultStudio::get_num_entry(get_loc_f(), -1);
	Write2(ptr, info[selected].shapenum);
	Write2(ptr, frnum < 0 ? c_any_framenum : frnum);
	Write2(ptr, qual < 0 ? c_any_qual : qual);
	*ptr++ = upwards ? 1 : 0;
	ExultStudio *studio = ExultStudio::get_instance();
	studio->send_to_server(
	    Exult_server::locate_shape, data, ptr - data);
}

/*
 *  Set up popup menu for shape browser.
 */

GtkWidget *Shape_chooser::create_popup(
) {
	ExultStudio *studio = ExultStudio::get_instance();
	create_popup_internal(true); // Create popup with groups, files.
	if (selected >= 0) {    // Add editing choices.
		Add_menu_item(popup, "Info...",
		              GTK_SIGNAL_FUNC(on_shapes_popup_info_activate), this);
		if (studio->get_image_editor()) {
			Add_menu_item(popup, "Edit...",
			              GTK_SIGNAL_FUNC(on_shapes_popup_edit_activate),
			              this);
			if (IS_FLAT(info[selected].shapenum) &&
			        file_info == studio->get_vgafile())
				Add_menu_item(popup, "Edit tiled...",
				              GTK_SIGNAL_FUNC(
				                  on_shapes_popup_edtiles_activate), this);
		}
		// Separator.
		Add_menu_item(popup);
		// Add/del.
		Add_menu_item(popup, "New frame",
		              GTK_SIGNAL_FUNC(on_shapes_popup_new_frame), this);
		// Export/import.
		Add_menu_item(popup, "Export frame...",
		              GTK_SIGNAL_FUNC(on_shapes_popup_export), this);
		Add_menu_item(popup, "Import frame...",
		              GTK_SIGNAL_FUNC(on_shapes_popup_import), this);
		if (!IS_FLAT(info[selected].shapenum) ||
		        file_info != studio->get_vgafile()) {
			// Separator.
			Add_menu_item(popup);
			// Export/import all frames.
			Add_menu_item(popup, "Export all frames...",
			              GTK_SIGNAL_FUNC(on_shapes_popup_export_all), this);
			Add_menu_item(popup, "Import all frames...",
			              GTK_SIGNAL_FUNC(on_shapes_popup_import_all), this);
		}
	}
	if (ifile->is_flex()) {     // Multiple-shapes file (.vga)?
		if (selected >= 0 && (!IS_FLAT(info[selected].shapenum) ||
		                      file_info != studio->get_vgafile())) {
			// Separator.
			Add_menu_item(popup);
			// Export/import shape.
			Add_menu_item(popup, "Export shape...",
			              GTK_SIGNAL_FUNC(on_shapes_popup_export_shape), this);
			Add_menu_item(popup, "Import shape...",
			              GTK_SIGNAL_FUNC(on_shapes_popup_import_shape), this);
		}
		// Separator.
		Add_menu_item(popup);
		Add_menu_item(popup, "New shape",
		              GTK_SIGNAL_FUNC(on_shapes_popup_new_shape), this);
	}
	return popup;
}

/*
 *  Create the list.
 */

Shape_chooser::Shape_chooser(
    Vga_file *i,            // Where they're kept.
    unsigned char *palbuf,      // Palette, 3*256 bytes (rgb triples).
    int w, int h,           // Dimensions.
    Shape_group *g,
    Shape_file_info *fi
) : Object_browser(g, fi),
	Shape_draw(i, palbuf, gtk_drawing_area_new()),
	shapes_file(nullptr), framenum0(0),
	info(0), rows(0), row0(0),
	row0_voffset(0), total_height(0),
	frames_mode(false), hoffset(0), voffset(0), status_id(-1), sel_changed(nullptr) {
	rows.reserve(40);
	// Put things in a vert. box.
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	set_widget(vbox); // This is our "widget"
	gtk_widget_show(vbox);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	// A frame looks nice.
	GtkWidget *frame = gtk_frame_new(nullptr);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
	// NOTE:  draw is in Shape_draw.
	// Indicate the events we want.
	gtk_widget_set_events(draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
	                      | GDK_BUTTON_RELEASE_MASK
	                      | GDK_POINTER_MOTION_HINT_MASK |
	                      GDK_BUTTON1_MOTION_MASK | GDK_KEY_PRESS_MASK);
	// Set "configure" handler.
	gtk_signal_connect(GTK_OBJECT(draw), "configure_event",
	                   GTK_SIGNAL_FUNC(Configure_chooser), this);
	// Set "expose" handler.
	gtk_signal_connect(GTK_OBJECT(draw), "expose_event",
	                   GTK_SIGNAL_FUNC(expose), this);
	// Keystroke.
	gtk_signal_connect(GTK_OBJECT(draw), "key-press-event",
	                   GTK_SIGNAL_FUNC(on_draw_key_press),
	                   this);
	GTK_WIDGET_SET_FLAGS(draw, GTK_CAN_FOCUS);
	// Set mouse click handler.
	gtk_signal_connect(GTK_OBJECT(draw), "button_press_event",
	                   GTK_SIGNAL_FUNC(Mouse_press), this);
	gtk_signal_connect(GTK_OBJECT(draw), "button_release_event",
	                   GTK_SIGNAL_FUNC(Mouse_release), this);
	// Mouse motion.
	gtk_signal_connect(GTK_OBJECT(draw), "drag_begin",
	                   GTK_SIGNAL_FUNC(drag_begin), this);
#ifdef _WIN32
// required to override GTK+ Drag and Drop
	gtk_signal_connect(GTK_OBJECT(draw), "motion_notify_event",
	                   GTK_SIGNAL_FUNC(win32_drag_motion), this);
#else
	gtk_signal_connect(GTK_OBJECT(draw), "motion_notify_event",
	                   GTK_SIGNAL_FUNC(drag_motion), this);
#endif
	gtk_signal_connect(GTK_OBJECT(draw), "drag_data_get",
	                   GTK_SIGNAL_FUNC(drag_data_get), this);
	gtk_signal_connect(GTK_OBJECT(draw), "selection_clear_event",
	                   GTK_SIGNAL_FUNC(selection_clear), this);
	gtk_container_add(GTK_CONTAINER(frame), draw);
	gtk_drawing_area_size(GTK_DRAWING_AREA(draw), w, h);
	gtk_widget_show(draw);
	// Want vert. scrollbar for the shapes.
	GtkObject *shape_adj = gtk_adjustment_new(0, 0,
	                       get_count() / 4, 1, 1, 1);
	vscroll = gtk_vscrollbar_new(GTK_ADJUSTMENT(shape_adj));
	gtk_box_pack_start(GTK_BOX(hbox), vscroll, FALSE, TRUE, 0);
	// Set scrollbar handler.
	gtk_signal_connect(GTK_OBJECT(shape_adj), "value_changed",
	                   GTK_SIGNAL_FUNC(vscrolled), this);
	gtk_widget_show(vscroll);
	// Horizontal scrollbar.
	shape_adj = gtk_adjustment_new(0, 0, 1600, 8, 16, 16);
	hscroll = gtk_hscrollbar_new(GTK_ADJUSTMENT(shape_adj));
	gtk_box_pack_start(GTK_BOX(vbox), hscroll, FALSE, TRUE, 0);
	// Set scrollbar handler.
	gtk_signal_connect(GTK_OBJECT(shape_adj), "value_changed",
	                   GTK_SIGNAL_FUNC(hscrolled), this);
//++++  gtk_widget_hide(hscroll);   // Only shown in 'frames' mode.
	// At the bottom, status bar & frame:
	GtkWidget *hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	gtk_widget_show(hbox1);
	// At left, a status bar.
	sbar = gtk_statusbar_new();
	sbar_sel = gtk_statusbar_get_context_id(GTK_STATUSBAR(sbar),
	                                        "selection");
	gtk_box_pack_start(GTK_BOX(hbox1), sbar, TRUE, TRUE, 0);
	gtk_widget_show(sbar);
	GtkWidget *label = gtk_label_new("Frame:");
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 4);
	gtk_widget_show(label);
	// A spin button for frame#.
	frame_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0,
	                           16, 1,
	                           0, 0.0));
	fspin = gtk_spin_button_new(GTK_ADJUSTMENT(frame_adj),
	                            1, 0);
	gtk_signal_connect(GTK_OBJECT(frame_adj), "value_changed",
	                   GTK_SIGNAL_FUNC(frame_changed), this);
	gtk_box_pack_start(GTK_BOX(hbox1), fspin, FALSE, FALSE, 0);
	gtk_widget_show(fspin);
	// A toggle for 'All Frames'.
	GtkWidget *allframes = gtk_toggle_button_new_with_label("Frames");
	gtk_box_pack_start(GTK_BOX(hbox1), allframes, FALSE, FALSE, 4);
	gtk_widget_show(allframes);
	gtk_signal_connect(GTK_OBJECT(allframes), "toggled",
	                   GTK_SIGNAL_FUNC(all_frames_toggled), this);
	// Add search controls to bottom.
	gtk_box_pack_start(GTK_BOX(vbox),
	                   create_controls(find_controls | locate_controls | locate_quality | locate_frame),
	                   FALSE, FALSE, 0);
}

/*
 *  Delete.
 */

Shape_chooser::~Shape_chooser(
) {
	gtk_widget_destroy(get_widget());
}

/*
 *  Unselect.
 */

void Shape_chooser::unselect(
    bool need_render            // 1 to render and show.
) {
	if (selected >= 0) {
		selected = -1;
		// Update spin button for frame #.
		gtk_adjustment_set_value(frame_adj, 0);
		gtk_widget_set_sensitive(fspin, false);
		if (need_render) {
			render();
			show();
		}
		if (sel_changed)    // Tell client.
			(*sel_changed)();
	}
	update_statusbar();
}

/*
 *  Show selection or range in window.
 */

void Shape_chooser::update_statusbar(
) {
	char buf[150];
	if (status_id >= 0)     // Remove prev. selection msg.
		gtk_statusbar_remove(GTK_STATUSBAR(sbar), sbar_sel, status_id);
	if (selected >= 0) {
		int shapenum = info[selected].shapenum;
		int nframes = ifile->get_num_frames(shapenum);
		g_snprintf(buf, sizeof(buf), "Shape %d (0x%03x, %d frames)",
		           shapenum, shapenum, nframes);
		ExultStudio *studio = ExultStudio::get_instance();
		if (shapes_file) {
			const char *nm;
			if ((nm = studio->get_shape_name(shapenum))) {
				int len = strlen(buf);
				g_snprintf(buf + len, sizeof(buf) - len,
				           ":  '%s'", nm);
			}
			shapes_file->read_info(studio->get_game_type(), true);
			int frnum = info[selected].framenum;
			const Shape_info &inf = shapes_file->get_info(shapenum);
			const Frame_name_info *nminf;
			if (inf.has_frame_name_info() &&
			        (nminf = inf.get_frame_name(frnum, -1)) != nullptr) {
				int type = nminf->get_type();
				int msgid = nminf->get_msgid();
				if (type >= 0 && msgid < get_num_misc_names()) {
					const char *msgstr = get_misc_name(msgid);
					int len = strlen(buf);
					// For safety.
					if (!nm) nm = "";
					if (!msgstr) msgstr = "";
					if (type > 0) {
						const char *otmsgstr;
						if (type > 2)
							otmsgstr = "<NPC Name>";
						else {
							int otmsg = nminf->get_othermsg();
							otmsgstr = otmsg == -255 ? nm :
							           (otmsg == -1 || otmsg >= get_num_misc_names() ? "" :
							            get_misc_name(otmsg));
							if (!otmsgstr) otmsgstr = "";
						}
						const char *prefix = nullptr;
						const char *suffix = nullptr;
						if (type & 1) {
							prefix = otmsgstr;
							suffix = msgstr;
						} else {
							prefix = msgstr;
							suffix = otmsgstr;
						}
						g_snprintf(buf + len, sizeof(buf) - len,
						           "  -  '%s%s'", prefix, suffix);
					} else
						g_snprintf(buf + len, sizeof(buf) - len,
						           "  -  '%s'", msgstr);
				}
			}
		}
		status_id = gtk_statusbar_push(GTK_STATUSBAR(sbar),
		                               sbar_sel, buf);
	} else if (!info.empty() && !group) {
		int first_shape = info[rows[row0].index0].shapenum;
		g_snprintf(buf, sizeof(buf), "Shapes %d to %d (0x%03x to 0x%03x)",
		           first_shape, last_shape, first_shape, last_shape);
		status_id = gtk_statusbar_push(GTK_STATUSBAR(sbar),
		                               sbar_sel, buf);
	} else
		status_id = -1;
}


