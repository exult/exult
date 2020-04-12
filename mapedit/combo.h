/**
 ** Combo.h - A combination of multiple objects.
 **
 ** Written: 4/26/02 - JSF
 **/

#ifndef INCL_COMBO_H
#define INCL_COMBO_H    1

/*
Copyright (C) 2002-2013 The Exult Team

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

#include <memory>
#include <string>
#include <vector>
#include "objbrowse.h"
#include "shapedraw.h"
#include "rect.h"

class Shapes_vga_file;
class Flex_file_info;

/*
 *  A single object:
 */
class Combo_member {
	short tx, ty, tz;       // Location (tile) rel. to top-left.
	short shapenum, framenum;   // Object in shapes.vga.
public:
	friend class Combo;
	friend class Combo_editor;
	friend class Combo_chooser;
	Combo_member(short x, short y, short z, short sh, short fr)
		: tx(x), ty(y), tz(z), shapenum(sh), framenum(fr)
	{  }
	// Return which makes better hot-spot.
	friend int hot_spot_compare(Combo_member &c0, Combo_member &c1);
};

/*
 *  A combination of objects.
 */
class Combo {
	Shapes_vga_file *shapes_file;   // Where shapes come from.
	std::vector<Combo_member *> members;    // Members of this combination.
	short hot_index;        // Index of obj. whose 'hot spot' we'll
	//   use.
	short starttx, startty;     // Offset represented by top-left.
	std::string name;       // Name given by user.
	Rectangle tilefoot;     // Footprint in tiles.
	// Get footprint of given member.
	Rectangle get_member_footprint(int i);
public:
	friend class Combo_editor;
	friend class Combo_chooser;
	Combo(Shapes_vga_file *svga);
	Combo(const Combo &c2);     // Copy.
	~Combo();
	Combo_member *get(int i) {
		return i >= 0 && static_cast<unsigned>(i) < members.size() ? members[i] : nullptr;
	}
	// Add a new object.
	void add(int tx, int ty, int tz, int shnum, int frnum, bool toggle);
	void remove(int i);     // Remove object #i.
	// Paint shapes in drawing area.
	void draw(Shape_draw *draw, int selected = -1,
	          int xoff = 0, int yoff = 0);
	int find(int mx, int my);   // Find at mouse position.
	// Serialize:
	std::unique_ptr<unsigned char[]> write(int &datalen);
	const unsigned char *read(const unsigned char *buf, int bufsize);
};

/*
 *  The 'combo editor' window:
 */
class Combo_editor : public Shape_draw {
	GtkWidget *win;         // Main window.
	Combo *combo;           // Combo being edited.
	int selected;           // Index of selected item in combo.
	bool setting_controls;      // To avoid callbacks when setting.
	int file_index;         // Entry # in 'combos.flx', or -1 if
	//   new.
	// Set to edit existing combo.
	void set_combo(Combo *newcombo, int findex);
public:
	friend class Combo_chooser;
	Combo_editor(Shapes_vga_file *svga, unsigned char *palbuf);
	~Combo_editor() override;
	void show(bool tf);     // Show/hide.
	void render_area(GdkRectangle *area);
	void render() override {
		render_area(nullptr);
	}
	void set_controls();        // Set controls to selected entry.
	// Handle mouse.
	gint mouse_press(GdkEventButton *event);
	void set_order();       // Set selected to desired order.
	void set_position();        // Set selected to desired position.
	// Add object/shape picked from Exult.
	void add(unsigned char *data, int datalen, bool toggle);
	void remove();          // Remove selected.
	void save();            // Save it.
	bool is_visible() {
		return GTK_WIDGET_VISIBLE(win);
	}
};

/**********************************************************************
 *  Below are classes for the combo-browser.
 **********************************************************************/

/*
 *  Store information about an individual combo shown in the list.
 */
class Combo_info {
	friend class Combo_chooser;
	int num;
	Rectangle box;          // Box where drawn.
	void set(int n, int rx, int ry, int rw, int rh) {
		num = n;
		box = Rectangle(rx, ry, rw, rh);
	}
};

/*
 *  This class manages the list of combos.
 */
class Combo_chooser: public Object_browser, public Shape_draw {
	Flex_file_info *flex_info;  // Where file data is stored.
	std::vector<Combo *> combos;    // List of all combination-objects.
	GtkWidget *sbar;        // Status bar.
	guint sbar_sel;         // Status bar context for selection.
	int index0;         // Index (combo) # of leftmost in
	//   displayed list.
	Combo_info *info;       // An entry for each combo drawn.
	int info_cnt;           // # entries in info.
	void (*sel_changed)();      // Called when selection changes.
	// Blit onto screen.
	void show(int x, int y, int w, int h) override;
	void show() override {
		Combo_chooser::show(0, 0,
		                    draw->allocation.width, draw->allocation.height);
	}
	void select(int new_sel);   // Show new selection.
	void load() override;        // Load from file data.
	void render() override;      // Draw list.
	void set_background_color(guint32 c) override {
		Shape_draw::set_background_color(c);
	}
	int get_selected_id() override {
		return selected < 0 ? -1 : info[selected].num;
	}
	void scroll(int newindex);  // Scroll.
	void scroll(bool upwards);
	void enable_controls();     // Enable/disable controls after sel.
	//   has changed.
public:
	Combo_chooser(Vga_file *i, Flex_file_info *flinfo,
	              unsigned char *palbuf, int w, int h,
	              Shape_group *g = nullptr);
	~Combo_chooser() override;
	// Turn off selection.
	void unselect(bool need_render = true);
	bool is_selected() {     // Is a combo selected?
		return selected >= 0;
	}
	void set_selected_callback(void (*fun)()) {
		sel_changed = fun;
	}
	int get_count();        // Get # to show.
	int add(Combo *newcombo, int index);    // Add new combo.
	void remove();          // Remove selected.
	void edit();            // Edit selected.
	// Configure when created/resized.
	static gint configure(GtkWidget *widget, GdkEventConfigure *event,
	                      gpointer data);
	// Blit to screen.
	static gint expose(GtkWidget *widget, GdkEventExpose *event,
	                   gpointer data);
	// Handle mouse press.
	static gint mouse_press(GtkWidget *widget, GdkEventButton *event,
	                        gpointer data);
	// Give dragged combo.
	static void drag_data_get(GtkWidget *widget, GdkDragContext *context,
	                          GtkSelectionData *seldata, guint info, guint time, gpointer data);
	// Someone else selected.
	static gint selection_clear(GtkWidget *widget,
	                            GdkEventSelection *event, gpointer data);
	static gint drag_begin(GtkWidget *widget, GdkDragContext *context,
	                       gpointer data);
	// Handle scrollbar.
	static void scrolled(GtkAdjustment *adj, gpointer data);
	void move(bool upwards) override;// Move current selected combo.
	void search(const char *srch, int dir) override;
#ifdef _WIN32
	static gint win32_drag_motion(GtkWidget *widget, GdkEventMotion *event,
	                              gpointer data);
#else
	static gint drag_motion(GtkWidget *widget, GdkEventMotion *event,
	                        gpointer data);
#endif
};


#endif  /* INCL_COMBO_H */
