/*
Copyright (C) 2000-2022 The Exult Team

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

#ifndef OBJBROWSE_H
#define OBJBROWSE_H

#include "studio.h"

#include <string>
#include <vector>

class Shape_group;
class Shape_file_info;

class Object_browser {
private:
	GtkWidget* widget;

protected:
	int selected = -1;    // Index of user-selected entry,
	//   counting from the top-left entry
	//   currently rendered.
	int index0 = 0;    // Index of top-leftmost in
	//   displayed list.
	GtkWidget*          vscroll = nullptr;    // Vertical scrollbar.
	GtkWidget*          hscroll = nullptr;    // Horizontal scrollbar.
	GtkEventController* vscroll_ctlr
			= nullptr;                       // Vertical scroll in browser.
	Shape_group*     group;                  // Non-null to use filter.
	GtkWidget*       popup = nullptr;        // Popup menu in draw area.
	Shape_file_info* file_info;              // Our creator (or null).
	GtkWidget*       find_text = nullptr;    // For searching.
	GtkWidget *loc_down = nullptr, *loc_up = nullptr;    // 'Locate' buttons.
	GtkWidget* loc_q = nullptr;    // 'Locate' quality/quantity.
	GtkWidget* loc_f = nullptr;    // 'Locate' frame
	// 'Move' buttons:
	GtkWidget *move_down = nullptr, *move_up = nullptr;
	int config_width = 0, config_height = 0;    // For storing prev. dims.

	void        set_widget(GtkWidget* w);
	static bool search_name(const char* nm, const char* srch);

public:
	Object_browser(Shape_group* grp = nullptr, Shape_file_info* fi = nullptr);
	virtual ~Object_browser();

	GtkWidget* get_widget();

	Shape_group* get_group() {
		return group;
	}

	int get_selected() {    // Return index of sel'd item, or -1.
		return selected < 0 ? -1 : index0 + selected;
	}

	void reset_selected() {
		selected = -1;
	}

	GtkWidget* get_find_text() {    // Get 'find' text widget.
		return find_text;
	}

	GtkWidget* get_loc_q() {
		return loc_q;
	}

	GtkWidget* get_loc_f() {
		return loc_f;
	}

	virtual void load() {    // Load from file data.
	}

	virtual void setup_info(bool savepos = true) {
		ignore_unused_variable_warning(savepos);
	}

	virtual void render() = 0;
	// Blit onto screen.
	virtual void show(int x, int y, int w, int h) = 0;

	virtual int get_selected_id() {
		return -1;
	}

	virtual bool server_response(int id, unsigned char* data, int datalen);
	virtual void end_terrain_editing();
	virtual void set_background_color(guint32 c);
	// Menu items:
	static void on_browser_group_add(GtkMenuItem* item, gpointer udata);
	// Add 'Add to group...' submenu.
	void        add_group_submenu(GtkWidget* popup);
	static void on_browser_file_save(GtkMenuItem* item, gpointer udata);
	static void on_browser_file_revert(GtkMenuItem* item, gpointer udata);

	virtual GtkWidget* create_popup() {
		return create_popup_internal(true);
	}

	// Handle scroll events.
	void        enable_draw_vscroll(GtkWidget* draw);
	static void draw_vscrolled(
			GtkEventControllerScroll* self, gdouble dx, gdouble dy,
			gpointer data);

protected:
	GtkWidget* create_popup_internal(bool files);    // Popup menu.

public:
	enum {    // Create controls at bottom.
		// OR together what you want.
		find_controls   = 1,
		locate_controls = 2,
		locate_quality  = 4,
		move_controls   = 8,
		locate_frame    = 16
	};

	GtkWidget* create_controls(int controls);

	// Virtuals for controls.
	virtual void search(const char* srch, int dir) {
		ignore_unused_variable_warning(srch, dir);
	}

	virtual void locate(bool upwards) {    // Locate terrain on game map.
		ignore_unused_variable_warning(upwards);
	}

	virtual void move(bool upwards) {    // Move current selected chunk.
		ignore_unused_variable_warning(upwards);
	}
};

// File-selector utility:
// Callback for file-selector 'ok':
using File_sel_okay_fun = void (*)(const char*, gpointer);
void Create_file_selection(
		const char* title, const char* path, const char* filtername,
		const std::vector<std::string>& filters, GtkFileChooserAction action,
		File_sel_okay_fun ok_handler, gpointer user_data);

#endif
