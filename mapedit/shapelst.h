/**
 ** A GTK widget showing a list of shapes from an image file.
 **
 ** Written: 7/25/99 - JSF
 **/

#ifndef INCL_SHAPELST
#define INCL_SHAPELST 1

/*
Copyright (C) 1999-2022 Jeffrey S. Freedman

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

#include <ctime>
#include <memory>
#include <string>
#include <vector>

class Vga_file;
class Image_buffer8;
class Shapes_vga_file;

/*
 *  Information about a file being edited externally.
 */
class Editing_file {
	std::string vga_basename;    // Name of image file this comes from.
	std::string pathname;        // Full path to file.
	time_t      mtime;           // Last modification time.
	int         shapenum;        // Shape number.
	int         framenum;        // Frame number.
	int         tiles;           // If > 0, #8x8 tiles per row or col.
	bool        bycolumns;       // If true tile by column first.
	bool is_shp;    // If true, file is SHP format (whole shape), else PNG
					// (single frame).

	// Private constructor for tiled
	Editing_file(
			const char* vganm, const char* pnm, time_t m, int sh, int ts,
			bool bycol, int /*unused*/)
			: vga_basename(vganm), pathname(pnm), mtime(m), shapenum(sh),
			  framenum(0), tiles(ts), bycolumns(bycol), is_shp(false) {}

public:
	friend class Shape_chooser;

	// Create for single frame (PNG) or whole shape (SHP):
	Editing_file(
			const char* vganm, const char* pnm, time_t m, int sh, int fr,
			bool shp)
			: vga_basename(vganm), pathname(pnm), mtime(m), shapenum(sh),
			  framenum(fr), tiles(0), bycolumns(false), is_shp(shp) {}

	// Factory method for tiled (PNG only):
	static std::unique_ptr<Editing_file> create_tiled(
			const char* vganm, const char* pnm, time_t m, int sh, int ts,
			bool bycol) {
		return std::unique_ptr<Editing_file>(
				new Editing_file(vganm, pnm, m, sh, ts, bycol, 0));
	}
};

/*
 *  Store information about an individual shape shown in the list.
 */
class Shape_entry {
	friend class Shape_chooser;
	short    shapenum, framenum;    // The given shape/frame.
	TileRect box;                   // Box where drawn.
public:
	void set(int shnum, int frnum, int rx, int ry, int rw, int rh) {
		shapenum = static_cast<short>(shnum);
		framenum = static_cast<short>(frnum);
		box      = TileRect(rx, ry, rw, rh);
	}
};

/*
 *  One row.
 */
class Shape_row {
	friend class Shape_chooser;
	short    height = 0;    // In pixels.
	long     y      = 0;    // Absolute y-coord. in pixels.
	unsigned index0 = 0;    // Index of 1st Shape_entry in row.
};

/*
 *  This class manages a list of shapes from an image file.
 */
class Shape_chooser : public Object_browser, public Shape_draw {
	Shapes_vga_file*         shapes_file;    // Non-null if 'shapes.vga'.
	GtkWidget*               sbar;           // Status bar.
	guint                    sbar_sel;     // Status bar context for selection.
	GtkWidget*               fspin;        // Spin button for frame #.
	GtkAdjustment*           frame_adj;    // Adjustment for frame spin btn.
	int                      framenum0;    // Default frame # to display.
	std::vector<Shape_entry> info;         // Pos. of each shape/frame.
	std::vector<Shape_row>   rows;
	unsigned                 row0;    // Row # at top of window.
	int  row0_voffset;                // Vert. pos. (in pixels) of top row.
	long total_height;                // In pixels, for all rows.
	int  last_shape;                  // Last shape visible in window.
	bool frames_mode;                 // Show all frames horizontally.
	int  hoffset;                     // Horizontal offset in pixels (when in
									  //   frames_mode).
	int voffset;                      // Vertical offset in pixels.
	int status_id;                    // Statusbar msg. ID.
	void (*sel_changed)();            // Called when selection changes.
	// List of files being edited by an
	//   external program (Gimp, etc.)
	static std::vector<std::unique_ptr<Editing_file>> editing_files;
	static int check_editing_timer;    // For monitoring files being edited.
	// Blit onto screen.
	void show(int x, int y, int w, int h) override;
	void tell_server_shape();    // Tell Exult what shape is selected.
	void render() override;      // Draw list.

	void set_background_color(guint32 c) override {
		Shape_draw::set_background_color(c);
	}

	void setup_info(bool savepos = true) override;
	void setup_shapes_info();
	void setup_frames_info();
	void scroll_to_frame();             // Scroll so sel. frame is visible.
	int  find_shape(int shnum);         // Find index for given shape.
	void goto_index(unsigned index);    // Get desired index in view.

public:
	void select(int new_sel);    // Show new selection.

	int get_selected_id() override {
		return selected < 0 ? -1 : info[selected].shapenum;
	}

	void       scroll_row_vertical(unsigned newrow);
	void       scroll_vertical(int newoffset);    // Scroll.
	void       setup_vscrollbar();                // Set new scroll amounts.
	void       setup_hscrollbar(int newmax);
	GtkWidget* create_popup() override;    // Popup menu.
public:
	Shape_chooser(
			Vga_file* i, unsigned char* palbuf, int w, int h,
			Shape_group* g = nullptr, Shape_file_info* fi = nullptr);
	~Shape_chooser() override;

	void set_shapes_file(Shapes_vga_file* sh) {
		shapes_file = sh;
	}

	void set_framenum0(int f) {
		framenum0 = f;
	}

	void shape_dropped_here(int file, int shapenum, int framenum);
	int  get_count();    // Get # shapes we can display.
	void search(const char* search, int dir) override;
	void locate(bool upwards) override;    // Locate shape on game map.
	// Turn off selection.
	void unselect(bool need_render = true);
	void update_statusbar();

	bool is_selected() {    // Is a shape selected?
		return selected >= 0;
	}

	void set_selected_callback(void (*fun)()) {
		sel_changed = fun;
	}

	unsigned get_num_cols(unsigned rownum) {
		return ((rownum < rows.size() - 1) ? rows[rownum + 1].index0
										   : info.size())
			   - rows[rownum].index0;
	}

	// Configure when created/resized.
	gint configure(GdkEventConfigure* event);
	// Blit to screen.
	static gint     expose(GtkWidget* widget, cairo_t* cairo, gpointer data);
	static gboolean on_new_shape_font_color_draw_expose_event(
			GtkWidget* widget, cairo_t* cairo, gpointer data);
	// Handle mouse press.
	gint mouse_press(GtkWidget* widget, GdkEventButton* event);
	// Export current frame as a PNG.
	time_t export_png(const char* fname);
	// Export given image as a PNG.
	time_t export_png(
			const char* fname, Image_buffer8& img, int xoff, int yoff);
	// Export frames tiled.
	time_t export_tiled_png(const char* fname, int tiles, bool bycols);
	void   edit_shape_info();    // Edit selected shape's info.
	// Edit selected shape-frame.
	void edit_shape(int tiles = 0, bool bycols = false);
	// Deal with list of files being edited
	//   by an external prog. (Gimp).
	static gint check_editing_files_cb(gpointer data);
	static gint check_editing_files();
	static void read_back_edited(Editing_file* ed);
	static void clear_editing_files();
	// Import/export from file selector.
	static void export_frame(const char* fname, gpointer user_data);
	static void import_frame(const char* fname, gpointer user_data);
	static void export_all_frames(const char* fname, gpointer user_data);
	static void export_shape(const char* fname, gpointer user_data);
	void        export_all_pngs(const char* fname, int shnum);
	static void import_all_frames(const char* fname, gpointer user_data);
	static void import_shape(const char* fname, gpointer user_data);
	void        import_all_pngs(const char* fname, int shnum);
	void        new_frame();    // Add/del.
	void        from_font_toggled(bool on);
	void        new_shape();
	void        create_new_shape();
	void        del_frame();
	// Give dragged shape.
	static void drag_data_get(
			GtkWidget* widget, GdkDragContext* context,
			GtkSelectionData* seldata, guint info, guint time, gpointer data);
	static gint drag_begin(
			GtkWidget* widget, GdkDragContext* context, gpointer data);
	// Handle scrollbar.
	static void vscrolled(GtkAdjustment* adj, gpointer data);
	static void hscrolled(GtkAdjustment* adj, gpointer data);
	// Handle spin-button for frames.
	static void frame_changed(GtkAdjustment* adj, gpointer data);
	static void all_frames_toggled(GtkToggleButton* btn, gpointer user_data);
	static gint drag_motion(
			GtkWidget* widget, GdkEventMotion* event, gpointer data);
	// Menu items:
	static void on_shapes_popup_info_activate(
			GtkMenuItem* item, gpointer udata);
	static void on_shapes_popup_edit_activate(
			GtkMenuItem* item, gpointer udata);
	static void on_shapes_popup_edtiles_activate(
			GtkMenuItem* item, gpointer udata);
};

#endif
