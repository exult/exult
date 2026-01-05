/**
 ** Combo.cc - A combination of multiple objects.
 **
 ** Written: 4/26/02 - JSF
 **/

/*
Copyright (C) 2002-2022 The Exult Team

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

#include "combo.h"

#include "exceptions.h"
#include "exult_constants.h"
#include "ibuf8.h"
#include "objserial.h"
#include "servemsg.h"
#include "shapefile.h"
#include "shapegroup.h"
#include "shapevga.h"
#include "u7drag.h"

#include <cstring>

using std::cout;
using std::endl;
using std::make_unique;
using std::unique_ptr;

class Game_object;

const int border = 2;    // Border at bottom, sides of each
//   combo in browser.
const int maxtiles = 96;    // Max. width/height in tiles.

/*
 *  Open combo window (if not already open).
 */

C_EXPORT void on_new_combo1_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->open_combo_window();
}

void ExultStudio::open_combo_window() {
	if (!vgafile) {
		EStudio::Alert("'shapes.vga' file isn't present");
		return;
	}
	// Prompt if existing combo has unsaved changes.
	if (combowin && !combowin->prompt_for_discard()) {
		return;    // User chose not to discard.
	}
	auto* svga = static_cast<Shapes_vga_file*>(vgafile->get_ifile());
	delete combowin;    // Delete old (svga may have changed).
	combowin = new Combo_editor(svga, palbuf.get());
	combowin->show(true);
	// Set edit-mode to pick.
	GtkWidget* mitem = get_widget("pick_for_combo1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem), true);
}

/*
 *  Close the combo window.
 */

void ExultStudio::close_combo_window() {
	if (combowin) {
		combowin->show(false);
	}
}

/*
 *  Save combos.
 */

void ExultStudio::save_combos() {
	// Get file info.
	Shape_file_info* combos = files->create("combos.flx");
	try {
		if (combos) {
			combos->flush();
		}
	} catch (const exult_exception& e) {
		EStudio::Alert("%s", e.what());
	}
}

/*
 *  Callbacks for "Combo" editor window.
 */
gboolean Combo_editor::on_combo_draw_expose_event(
		GtkWidget* widget,    // The view window.
		cairo_t* cairo, gpointer data) {
	ignore_unused_variable_warning(data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	combo->set_graphic_context(cairo);
	GdkRectangle area = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	combo->render_area(&area);
	combo->set_graphic_context(nullptr);
	return true;
}

C_EXPORT void on_combo_remove_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->remove();
}

C_EXPORT void on_combo_apply_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->save();
}

C_EXPORT void on_combo_ok_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	GtkWidget* win   = gtk_widget_get_toplevel(GTK_WIDGET(button));
	auto*      combo = static_cast<Combo_editor*>(
            g_object_get_data(G_OBJECT(win), "user_data"));
	combo->save();
	gtk_widget_set_visible(win, false);
}

/*
 *  Combo window's Cancel button.
 */
C_EXPORT void on_combo_cancel_clicked(GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	GtkWidget* win   = gtk_widget_get_toplevel(GTK_WIDGET(button));
	auto*      combo = static_cast<Combo_editor*>(
            g_object_get_data(G_OBJECT(win), "user_data"));
	if (combo && !combo->prompt_for_discard()) {
		return;    // User chose not to discard
	}
	ExultStudio::get_instance()->close_combo_window();
}

/*
 *  Combo name entry changed.
 */
C_EXPORT void on_combo_name_changed(GtkEntry* entry, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(entry))), "user_data"));
	if (combo) {
		combo->set_dirty(true);
	}
}

/*
 *  Combo window's close button (X).
 */
C_EXPORT gboolean on_combo_win_delete_event(
		GtkWidget* widget, GdkEvent* event, gpointer user_data) {
	ignore_unused_variable_warning(widget, event, user_data);
	auto* combo = static_cast<Combo_editor*>(
			g_object_get_data(G_OBJECT(widget), "user_data"));
	if (combo && !combo->prompt_for_discard()) {
		return true;    // Block window close
	}
	ExultStudio::get_instance()->close_combo_window();
	return true;
}

C_EXPORT void on_combo_locx_changed(GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->set_position();
}

C_EXPORT void on_combo_locy_changed(GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->set_position();
}

C_EXPORT void on_combo_locz_changed(GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->set_position();
}

C_EXPORT void on_combo_order_changed(
		GtkSpinButton* button, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(button))),
			"user_data"));
	combo->set_order();
}

/*
 *  Mouse events in draw area.
 */
C_EXPORT gboolean on_combo_draw_button_press_event(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Combo_chooser.
) {
	ignore_unused_variable_warning(data);
	auto* combo = static_cast<Combo_editor*>(g_object_get_data(
			G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			"user_data"));
	return combo->mouse_press(event);
}

/*
 *  Which member makes the better 'hot-spot' in a combo, where 'better'
 *  means lower, then southmost, then eastmost.
 *
 *  Output: 0 if c0, 1 if c1
 */

int hot_spot_compare(Combo_member& c0, Combo_member& c1) {
	if (c0.tz < c1.tz) {
		return 0;
	} else if (c1.tz < c0.tz) {
		return 1;
	} else if (c0.ty > c1.ty) {
		return 0;
	} else if (c1.ty > c0.ty) {
		return 1;
	} else {
		return c0.tx >= c1.tx ? 0 : 1;
	}
}

/*
 *  Get footprint of given member.
 */

TileRect Combo::get_member_footprint(int i    // i'th member.
) {
	Combo_member* m = members[i];
	// Get tile dims.
	const Shape_info& info   = shapes_file->get_info(m->shapenum);
	const int         xtiles = info.get_3d_xtiles(m->framenum);
	const int         ytiles = info.get_3d_ytiles(m->framenum);
	// Get tile footprint.
	TileRect box(m->tx - xtiles + 1, m->ty - ytiles + 1, xtiles, ytiles);
	return box;
}

/*
 *  Create empty combo.
 */

Combo::Combo(Shapes_vga_file* svga)
		: shapes_file(svga), hot_index(-1), starttx(c_num_tiles),
		  startty(c_num_tiles), tilefoot(0, 0, 0, 0) {
	// Read info. the first time.
	ExultStudio* es = ExultStudio::get_instance();
	if (shapes_file->read_info(es->get_game_type(), true)) {
		es->set_shapeinfo_modified();
	}
}

/*
 *  Copy another.
 */

Combo::Combo(const Combo& c2)
		: shapes_file(c2.shapes_file), hot_index(c2.hot_index),
		  starttx(c2.starttx), startty(c2.startty), name(c2.name),
		  tilefoot(c2.tilefoot) {
	for (auto* m : c2.members) {
		members.push_back(new Combo_member(
				m->tx, m->ty, m->tz, m->shapenum, m->framenum));
	}
}

/*
 *  Clean up.
 */

Combo::~Combo() {
	for (auto* member : members) {
		delete member;
	}
}

/*
 *  Add a new object.
 */

void Combo::add(
		int tx, int ty, int tz,    // Location rel. to top-left.
		int shnum, int frnum,      // Shape.
		bool toggle) {
	// Look for identical shape, pos.
	for (auto it = members.begin(); it != members.end(); ++it) {
		Combo_member* m = *it;
		if (tx == m->tx && ty == m->ty && tz == m->tz && shnum == m->shapenum
			&& frnum == m->framenum) {
			if (toggle) {
				remove(it - members.begin());
			}
			return;    // Don't add same one twice.
		}
	}
	// Get tile dims.
	const Shape_info& info   = shapes_file->get_info(shnum);
	const int         xtiles = info.get_3d_xtiles(frnum);
	const int         ytiles = info.get_3d_ytiles(frnum);
	const int         ztiles = info.get_3d_height();
	// Get tile footprint.
	const TileRect box(tx - xtiles + 1, ty - ytiles + 1, xtiles, ytiles);
	if (members.empty()) {    // First one?
		tilefoot = box;       // Init. total footprint.
	} else {
		// Too far away?
		if (tilefoot.x + tilefoot.w - box.x > maxtiles
			|| box.x + box.w - tilefoot.x > maxtiles
			|| tilefoot.y + tilefoot.h - box.y > maxtiles
			|| box.y + box.h - tilefoot.y > maxtiles) {
			// Send message to Exult game window
			const char*  msg    = "Selected object is too far from others";
			ExultStudio* studio = ExultStudio::get_instance();
			studio->send_to_server(
					Exult_server::say,
					reinterpret_cast<unsigned char*>(const_cast<char*>(msg)),
					strlen(msg) + 1);
			// Deselect the object in Exult
			studio->send_to_server(Exult_server::clear_selection, nullptr, 0);
			return;
		}
		// Add to footprint.
		tilefoot = tilefoot.add(box);
	}
	auto* memb = new Combo_member(tx, ty, tz, shnum, frnum);
	members.push_back(memb);
	// Figure visible top-left tile, with
	//   1 to spare.
	const int vtx = tx - xtiles - 2 - (tz + ztiles + 1) / 2;
	const int vty = ty - ytiles - 2 - (tz + ztiles + 1) / 2;
	if (vtx < starttx) {    // Adjust our starting point.
		starttx = vtx;
	}
	if (vty < startty) {
		startty = vty;
	}
	if (hot_index == -1 ||    // First one, or better than prev?
		hot_spot_compare(*memb, *members[hot_index]) == 0) {
		hot_index = members.size() - 1;
	}
}

/*
 *  Remove i'th object.
 */

void Combo::remove(int i) {
	if (i < 0 || unsigned(i) >= members.size()) {
		return;
	}
	// Get and remove i'th entry.
	auto          it = members.begin() + i;
	Combo_member* m  = *it;
	members.erase(it);
	delete m;
	hot_index = -1;    // Figure new hot-spot, footprint.
	tilefoot  = TileRect(0, 0, 0, 0);
	for (auto it = members.begin(); it != members.end(); ++it) {
		Combo_member*  m     = *it;
		const int      index = it - members.begin();
		const TileRect box   = get_member_footprint(index);
		if (hot_index == -1) {    // First?
			hot_index = 0;
			tilefoot  = box;
		} else {
			if (hot_spot_compare(*m, *members[hot_index]) == 0) {
				hot_index = index;
			}
			tilefoot = tilefoot.add(box);
		}
	}
}

/*
 *  Paint in a drawing area.
 */

void Combo::draw(
		Shape_draw* draw,
		int         selected,    // Index of 'selected' item, or -1.
		int xoff, int yoff       // Offset within draw.
) {
	int  selx     = -1000;
	int  sely     = -1000;
	bool selfound = false;
	for (auto it = members.begin(); it != members.end(); ++it) {
		Combo_member* m = *it;
		// Figure pixels up-left for lift.
		const int lft = m->tz * (c_tilesize / 2);
		// Figure relative tile.
		const int mtx = m->tx - starttx;
		const int mty = m->ty - startty;
		// Hot spot:
		int          x     = mtx * c_tilesize - lft;
		int          y     = mty * c_tilesize - lft;
		Shape_frame* shape = shapes_file->get_shape(m->shapenum, m->framenum);
		if (!shape) {
			continue;
		}
		// But draw_shape uses top-left.
		x -= shape->get_xleft();
		y -= shape->get_yabove();
		x += xoff;
		y += yoff;    // Add offset within area.
		draw->draw_shape(shape, x, y);
		if (it - members.begin() == selected) {
			selx     = x;    // Save coords for selected.
			sely     = y;
			selfound = true;
		}
	}
	if (selfound) {    // Now put border around selected.
		Combo_member* m = members[selected];
		draw->draw_shape_outline(
				m->shapenum, m->framenum, selx, sely,
				Shape_draw::outline_color);
	}
}

/*
 *  Find last member in list that contains a mouse point.
 *
 *  Output: Index of member found, or -1 if none.
 */

int Combo::find(
		int mx, int my    // Mouse position in draw area.
) {
	const int cnt = members.size();
	for (int i = cnt - 1; i >= 0; i--) {
		Combo_member* m = members[i];
		// Figure pixels up-left for lift.
		const int lft = m->tz * (c_tilesize / 2);
		// Figure relative tile.
		const int    mtx   = m->tx - starttx;
		const int    mty   = m->ty - startty;
		const int    x     = mtx * c_tilesize - lft;
		const int    y     = mty * c_tilesize - lft;
		Shape_frame* frame = shapes_file->get_shape(m->shapenum, m->framenum);
		if (frame && frame->has_point(mx - x, my - y)) {
			return i;
		}
	}
	return -1;
}

/*
 *  Write out.
 *
 *  Output: Allocated buffer containing result.
 */

unique_ptr<unsigned char[]> Combo::write(
		int& datalen    // Actual length of data in buf. is
						//   returned here.
) {
	const int namelen = name.length();    // Name length.
	// Room for our data + members.
	auto buf = make_unique<unsigned char[]>(
			namelen + 1 + 7 * 4 + members.size() * (5 * 4));
	unsigned char* ptr = buf.get();
	Serial_out     out(ptr);
	out << name;
	out << hot_index << starttx << startty;
	out << static_cast<short>(members.size());    // # members to follow.
	for (auto* m : members) {
		out << m->tx << m->ty << m->tz << m->shapenum << m->framenum;
	}
	datalen = ptr - buf.get();    // Return actual length.
	return buf;
}

/*
 *  Read in.
 *
 *  Output: ->past actual data read.
 */

const unsigned char* Combo::read(const unsigned char* buf, int bufsize) {
	ignore_unused_variable_warning(bufsize);
	const unsigned char* ptr = buf;
	Serial_in            in(ptr);
	in << name;
	in << hot_index << starttx << startty;
	short cnt;
	in << cnt;    // # members to follow.
	for (int i = 0; i < cnt; i++) {
		short tx;
		short ty;
		short tz;
		short shapenum;
		short framenum;
		in << tx << ty << tz << shapenum << framenum;
		auto* memb = new Combo_member(tx, ty, tz, shapenum, framenum);
		members.push_back(memb);
		const TileRect box = get_member_footprint(i);
		if (i == 0) {    // Figure footprint.
			tilefoot = box;
		} else {
			tilefoot = tilefoot.add(box);
		}
	}
	return buf;
}

/*
 *  Set to edit an existing combo.
 */

void Combo_editor::set_combo(
		Combo* newcombo,    // We'll own this.
		int    findex       // File index.
) {
	delete combo;
	combo      = newcombo;
	file_index = findex;
	dirty      = false;    // Loading existing combo, not dirty.
	reset_selected();
	ExultStudio::get_instance()->set_entry(
			"combo_name", combo->name.c_str(), true);
	set_controls();    // No selection now.
	render();
}

/*
 *  Create combo editor.
 */

Combo_editor::Combo_editor(
		Shapes_vga_file* svga,     // File containing shapes.
		unsigned char*   palbuf    // Palette for drawing shapes.
		)
		: Shape_draw(
				  svga, palbuf,
				  ExultStudio::get_instance()->get_widget("combo_draw")),
		  selected(-1), setting_controls(false), dirty(false), file_index(-1) {
	static bool first = true;
	combo             = new Combo(svga);
	win               = ExultStudio::get_instance()->get_widget("combo_win");
	g_object_set_data(G_OBJECT(win), "user_data", this);
	g_signal_connect(
			G_OBJECT(draw), "draw", G_CALLBACK(on_combo_draw_expose_event),
			this);
	if (first) {    // Indicate the events we want.
		gtk_widget_set_events(
				draw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
							  | GDK_BUTTON_RELEASE_MASK
							  | GDK_BUTTON1_MOTION_MASK);
		g_signal_connect(
				G_OBJECT(win), "delete-event",
				G_CALLBACK(on_combo_win_delete_event), nullptr);
		first = false;
	}
	set_controls();
}

/*
 *  Clean up.
 */

Combo_editor::~Combo_editor() {
	delete combo;
}

/*
 *  Show/hide.
 */

void Combo_editor::show(bool tf) {
	if (tf) {
		gtk_widget_set_visible(win, true);
	} else {
		gtk_widget_set_visible(win, false);
	}
}

/*
 *  Display.
 */

void Combo_editor::render_area(
		GdkRectangle*
				area    // 0 for whole draw area, as Combo_editor::render().
) {
	if (!draw) {
		return;
	}
	if (!area || !drawgc) {
		gtk_widget_queue_draw(draw);
		return;
	}
	Shape_draw::configure();    // Setup the first time.
	// Get dims.
	cairo_rectangle(drawgc, area->x, area->y, area->width, area->height);
	cairo_clip(drawgc);
	iwin->fill8(255);               // Fill with background color.
	combo->draw(this, selected);    // Draw shapes.
	Shape_draw::show(area->x, area->y, area->width, area->height);
}

/*
 *  Set controls according to what's selected.
 */

void Combo_editor::set_controls() {
	setting_controls     = true;    // Avoid updating.
	ExultStudio*  studio = ExultStudio::get_instance();
	Combo_member* m      = combo->get(selected);
	if (!m) {    // None selected?
		studio->set_spin("combo_locx", 0, false);
		studio->set_spin("combo_locy", 0, false);
		studio->set_spin("combo_locz", 0, false);
		studio->set_spin("combo_order", 0, false);
		studio->set_sensitive("combo_remove", false);
	} else {
		GtkAllocation alloc = {0, 0, 0, 0};
		gtk_widget_get_allocation(draw, &alloc);
		const int draww = alloc.width;
		const int drawh = alloc.height;
		studio->set_sensitive("combo_locx", true);
		studio->set_spin(
				"combo_locx", m->tx - combo->starttx, 0, draww / c_tilesize);
		studio->set_sensitive("combo_locy", true);
		studio->set_spin(
				"combo_locy", m->ty - combo->startty, 0, drawh / c_tilesize);
		studio->set_sensitive("combo_locz", true);
		studio->set_spin("combo_locz", m->tz, 0, 15);
		studio->set_sensitive("combo_order", true);
		studio->set_spin("combo_order", selected, 0, combo->members.size() - 1);
		studio->set_sensitive("combo_remove", true);
	}
	setting_controls = false;
}

/*
 *  Handle a mouse-press event.
 */

gint Combo_editor::mouse_press(GdkEventButton* event) {
	if (event->button != 1) {
		return false;    // Handling left-click.
	}
	// Get mouse position, draw dims.
	const int mx = ZoomDown(static_cast<int>(event->x));
	const int my = ZoomDown(static_cast<int>(event->y));
	selected     = combo->find(mx, my);    // Find it (or -1 if not found).
	set_controls();
	render();
	return true;
}

/*
 *  Move the selected item within the order.
 */

void Combo_editor::set_order() {
	if (setting_controls || selected < 0) {
		return;
	}
	ExultStudio* studio = ExultStudio::get_instance();
	const int    newi   = studio->get_spin("combo_order");
	if (selected == newi) {
		return;    // Already correct.
	}
	const int dir = newi > selected ? 1 : -1;
	while (newi != selected) {
		Combo_member* tmp              = combo->members[selected + dir];
		combo->members[selected + dir] = combo->members[selected];
		combo->members[selected]       = tmp;
		selected += dir;
	}
	dirty = true;
	render();
}

/*
 *  Move the selected item to the desired position in the spin buttons.
 */

void Combo_editor::set_position() {
	Combo_member* m = combo->get(selected);
	if (!m || setting_controls) {
		return;
	}
	ExultStudio* studio = ExultStudio::get_instance();
	m->tx               = combo->starttx + studio->get_spin("combo_locx");
	m->ty               = combo->startty + studio->get_spin("combo_locy");
	m->tz               = studio->get_spin("combo_locz");
	dirty               = true;
	render();
}

/*
 *  Get # shapes we can display.
 */

int Combo_chooser::get_count() {
	return group ? group->size() : combos.size();
}

/*
 *  Add an object/shape picked from Exult.
 */

void Combo_editor::add(
		unsigned char* data,    // Serialized object.
		int datalen, bool toggle) {
	Game_object* addr;
	int          tx;
	int          ty;
	int          tz;
	int          shape;
	int          frame;
	int          quality;
	std::string  name;
	if (!Object_in(
				data, datalen, addr, tx, ty, tz, shape, frame, quality, name)) {
		cout << "Error decoding object" << endl;
		return;
	}
	combo->add(tx, ty, tz, shape, frame, toggle);
	dirty = true;
	render();
}

/*
 *  Remove selected.
 */

void Combo_editor::remove() {
	if (selected >= 0) {
		combo->remove(selected);
		dirty = true;
		reset_selected();
		set_controls();
		render();
	}
}

/*
 *  Save combo.
 */

void Combo_editor::save() {
	ExultStudio* studio = ExultStudio::get_instance();
	// Get name from field.
	combo->name     = studio->get_text_entry("combo_name");
	auto* flex_info = dynamic_cast<Flex_file_info*>(
			studio->get_files()->create("combos.flx"));
	if (!flex_info) {
		EStudio::Alert("Can't open 'combos.flx'");
		return;
	}
	flex_info->set_modified();
	int  len;    // Serialize.
	auto newbuf = combo->write(len);
	// Update or append file data.
	flex_info->set(
			file_index == -1 ? flex_info->size() : file_index,
			std::move(newbuf), len);
	auto* chooser = dynamic_cast<Combo_chooser*>(studio->get_browser());
	if (chooser) {    // Browser open?
		file_index = chooser->add(new Combo(*combo), file_index);
	}
	dirty = false;    // Saved, no longer dirty.
}

/*
 *  Prompt user if combo has unsaved changes.
 *  Returns true if okay to proceed (discarded or not dirty).
 */

bool Combo_editor::prompt_for_discard() {
	return ExultStudio::get_instance()->prompt_for_discard(
			dirty, "Combo", GTK_WINDOW(win));
}

/*
 *  Blit onto screen.
 */

void Combo_chooser::show(
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
 *  Select an entry.  This should be called after rendering
 *  the combo.
 */

void Combo_chooser::select(int new_sel) {
	if (new_sel < 0 || new_sel >= info_cnt) {
		return;    // Bad value.
	}
	selected = new_sel;
	enable_controls();
	const int num   = info[selected].num;
	Combo*    combo = combos[num];
	// Remove prev. selection msg.
	// gtk_statusbar_pop(GTK_STATUSBAR(sbar), sbar_sel);
	char buf[150];    // Show new selection.
	g_snprintf(buf, sizeof(buf), "Combo %d", num);
	if (combo && !combo->name.empty()) {
		const int len = strlen(buf);
		g_snprintf(
				buf + len, sizeof(buf) - len, ":  '%s'", combo->name.c_str());
	}
	gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
}

/*
 *  Unselect.
 */

void Combo_chooser::unselect(bool need_render    // 1 to render and show.
) {
	if (selected >= 0) {
		reset_selected();
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
				buf, sizeof(buf), "Combos %d to %d", info[0].num,
				info[info_cnt - 1].num);
		gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, buf);
	} else {
		gtk_statusbar_push(GTK_STATUSBAR(sbar), sbar_sel, "No combos");
	}
}

/*
 *  Load/reload from file.
 */

void Combo_chooser::load_internal() {
	const unsigned cnt = combos.size();
	for (unsigned i = 0; i < cnt; i++) {    // Delete all the combos.
		delete combos[i];
	}
	unsigned num_combos = flex_info->size();
	// We need 'shapes.vga'.
	Shape_file_info* svga_info = ExultStudio::get_instance()->get_vgafile();
	Shapes_vga_file* svga
			= svga_info ? static_cast<Shapes_vga_file*>(svga_info->get_ifile())
						: nullptr;
	combos.resize(num_combos);    // Set size of list.
	if (!svga) {
		num_combos = 0;
	}
	// Read them all in.
	for (unsigned i = 0; i < num_combos; i++) {
		size_t         len;
		unsigned char* buf   = flex_info->get(i, len);
		auto*          combo = new Combo(svga);
		combo->read(buf, len);
		combos[i] = combo;    // Store in list.
	}
}

/*
 *  Render as many combos as fit in the combo chooser window.
 */

void Combo_chooser::render() {
	// Look for selected frame.
	int selcombo     = -1;
	int new_selected = -1;
	if (selected >= 0) {    // Save selection info.
		selcombo = info[selected].num;
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
	info     = new Combo_info[256];
	info_cnt = 0;    // Count them.
	// Clear window first.
	iwin->fill8(255);    // Background color.
	int index = index0;
	// We'll always show 128x128.
	const int combow    = 128;
	const int comboh    = 128;
	const int total_cnt = get_count();
	int       y         = border - voffset;
	while (index < total_cnt && y < winh) {
		int       x     = border;
		const int cliph = y + comboh <= winh ? comboh : (winh - y);
		while (index < total_cnt
			   && (x + combow + border <= winw || x == border)) {
			const int clipw = x + combow <= winw ? combow : (winw - x);
			iwin->set_clip(x, y, clipw, cliph);
			const int combonum = group ? (*group)[index] : index;
			combos[combonum]->draw(this, -1, x, y);
			iwin->clear_clip();
			// Store info. about where drawn.
			info[info_cnt].set(combonum, x, y, combow, comboh);
			if (combonum == selcombo) {
				// Found the selected combo.
				new_selected = info_cnt;
			}
			info_cnt++;
			index++;    // Next combo.
			x += combow + border;
		}
		y += comboh + border;
	}
	if (new_selected == -1) {
		unselect(false);
	} else {
		select(new_selected);
	}
	gtk_widget_queue_draw(draw);
}

/*
 *  Scroll to a new combo.
 */

void Combo_chooser::scroll(int newpixel    // Abs. index of leftmost to show.
) {
	const int total     = combos.size();
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

void Combo_chooser::scroll(bool upwards) {
	GtkAdjustment* adj   = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gdouble        delta = 128 + border;
	if (upwards) {
		delta = -delta;
	}
	gtk_adjustment_set_value(adj, delta + gtk_adjustment_get_value(adj));
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
	scroll(static_cast<gint>(gtk_adjustment_get_value(adj)));
}

/*
 *  Someone wants the dragged combo.
 */

void Combo_chooser::drag_data_get(
		GtkWidget*        widget,    // The view window.
		GdkDragContext*   context,
		GtkSelectionData* seldata,    // Fill this in.
		guint info, guint time,
		gpointer data    // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, context, time);
	cout << "In DRAG_DATA_GET of Combo for " << info << " and '"
		 << gdk_atom_name(gtk_selection_data_get_target(seldata)) << "'"
		 << endl;
	auto* chooser = static_cast<Combo_chooser*>(data);
	if (chooser->selected < 0
		|| (info != U7_TARGET_COMBOID && info != U7_TARGET_COMBOID + 100
			&& info != U7_TARGET_COMBOID + 200)) {
		return;    // Not sure about this.
	}
	// Get combo #.
	const int num   = chooser->info[chooser->selected].num;
	Combo*    combo = chooser->combos[num];
	// Get enough memory.
	const int cnt    = combo->members.size();
	const int buflen = U7DND_DATA_LENGTH(5 + cnt * 5);
	cout << "Buflen = " << buflen << endl;
	auto* buf  = new unsigned char[buflen];
	auto* ents = new U7_combo_data[cnt];
	// Get 'hot-spot' member.
	Combo_member* hot = combo->members[combo->hot_index];
	for (int i = 0; i < cnt; i++) {
		Combo_member* m = combo->members[i];
		ents[i].tx      = m->tx - hot->tx;
		ents[i].ty      = m->ty - hot->ty;
		ents[i].tz      = m->tz - hot->tz;
		ents[i].shape   = m->shapenum;
		ents[i].frame   = m->framenum;
	}
	const TileRect foot = combo->tilefoot;
	const int      len  = Store_u7_comboid(
            buf, foot.w, foot.h, foot.x + foot.w - 1 - hot->tx,
            foot.y + foot.h - 1 - hot->ty, cnt, ents);
	assert(len <= buflen);
	cout << "Setting selection data (" << num << ')' << " (" << len << ") '"
		 << buf << "'" << endl;
	// Set data.
	gtk_selection_data_set(
			seldata, gtk_selection_data_get_target(seldata), 8, buf, len);
	delete[] buf;
	delete[] ents;
}

/*
 *  Beginning of a drag.
 */

gint Combo_chooser::drag_begin(
		GtkWidget*      widget,    // The view window.
		GdkDragContext* context,
		gpointer        data    // ->Combo_chooser.
) {
	ignore_unused_variable_warning(widget);
	cout << "In DRAG_BEGIN of Combo" << endl;
	auto* chooser = static_cast<Combo_chooser*>(data);
	if (chooser->selected < 0) {
		return false;    // ++++Display a halt bitmap.
	}
	// Get ->combo.
	const int num   = chooser->info[chooser->selected].num;
	Combo*    combo = chooser->combos[num];
	if (combo->members.size() == 0) {
		return false;    // ++++Display a halt bitmap.
	}
	// Show 'hot' member as icon.
	Combo_member* hot = combo->members[combo->hot_index];
	Shape_frame*  shape
			= combo->shapes_file->get_shape(hot->shapenum, hot->framenum);
	const int      cnt  = combo->members.size();
	const TileRect foot = combo->tilefoot;
	unsigned char  buf[Exult_server::maxlength];
	unsigned char* ptr = &buf[0];
	little_endian::Write2(ptr, cnt);
	little_endian::Write2(ptr, foot.w);
	little_endian::Write2(ptr, foot.h);
	little_endian::Write2(ptr, foot.x + foot.w - 1 - hot->tx);
	little_endian::Write2(ptr, foot.y + foot.h - 1 - hot->ty);
	ExultStudio* studio = ExultStudio::get_instance();
	studio->send_to_server(Exult_server::drag_combo, buf, ptr - buf);
	if (shape) {
		chooser->set_drag_icon(context, shape);
	}
	return true;
}

/*
 *  Handle a scrollbar event.
 */

void Combo_chooser::scrolled(
		GtkAdjustment* adj,    // The adjustment.
		gpointer       data    // ->Combo_chooser.
) {
	auto* chooser = static_cast<Combo_chooser*>(data);
#ifdef DEBUG
	cout << "Combos : VScrolled to " << gtk_adjustment_get_value(adj)
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
 *  Callbacks for controls:
 */
/*
 *  Keystroke in draw-area.
 */
static gboolean on_combo_key_press(
		GtkEntry* entry, GdkEventKey* event, gpointer user_data) {
	ignore_unused_variable_warning(entry);
	auto* chooser = static_cast<Combo_chooser*>(user_data);
	switch (event->keyval) {
	case GDK_KEY_Delete:
		chooser->remove();
		return true;
	}
	return false;    // Let parent handle it.
}

/*
 *  Enable/disable controls after selection changed.
 */

void Combo_chooser::enable_controls() {
	if (selected == -1) {    // No selection.
		if (!group) {
			gtk_widget_set_sensitive(move_down, false);
			gtk_widget_set_sensitive(move_up, false);
		}
		return;
	}
	if (!group) {
		gtk_widget_set_sensitive(
				move_down, info[selected].num < int(combos.size()) - 1);
		gtk_widget_set_sensitive(move_up, info[selected].num > 0);
	}
}

static gint Mouse_release(
		GtkWidget* widget, GdkEventButton* event, gpointer data);

/*
 *  Create the list.
 */

Combo_chooser::Combo_chooser(
		Vga_file*       i,         // Where they're kept.
		Flex_file_info* flinfo,    // Flex-file info.
		unsigned char*  palbuf,    // Palette, 3*256 bytes (rgb triples).
		int w, int h,              // Dimensions.
		Shape_group* g             // Filter, or null.
		)
		: Object_browser(g, flinfo),
		  Shape_draw(i, palbuf, gtk_drawing_area_new()), flex_info(flinfo),
		  index0(0), info(nullptr), info_cnt(0), sel_changed(nullptr),
		  voffset(0), per_row(1) {
	load_internal();    // Init. from file data.

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
						  | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON1_MOTION_MASK
						  | GDK_KEY_PRESS_MASK);
	// Set "configure" handler.
	g_signal_connect(
			G_OBJECT(draw), "configure-event", G_CALLBACK(configure), this);
	// Set "expose-event" - "draw" handler.
	g_signal_connect(G_OBJECT(draw), "draw", G_CALLBACK(expose), this);
	// Keystroke.
	g_signal_connect(
			G_OBJECT(draw), "key-press-event", G_CALLBACK(on_combo_key_press),
			this);
	gtk_widget_set_can_focus(GTK_WIDGET(draw), true);
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
	// Want a scrollbar for the combos.
	GtkAdjustment* combo_adj = GTK_ADJUSTMENT(gtk_adjustment_new(
			0, 0, (128 + border) * combos.size(), 1, 4, 1.0));
	vscroll                  = gtk_scrollbar_new(
            GTK_ORIENTATION_VERTICAL, GTK_ADJUSTMENT(combo_adj));
	// Update window when it stops.
	// (Deprecated) gtk_range_set_update_policy(GTK_RANGE(vscroll),
	// (Deprecated)                             GTK_UPDATE_DELAYED);
	gtk_box_pack_start(GTK_BOX(hbox), vscroll, false, true, 0);
	// Set scrollbar handler.
	g_signal_connect(
			G_OBJECT(combo_adj), "value-changed", G_CALLBACK(scrolled), this);
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
	// Add controls to bottom.
	gtk_box_pack_start(
			GTK_BOX(vbox), create_controls(find_controls | move_controls),
			false, false, 0);
}

/*
 *  Delete.
 */

Combo_chooser::~Combo_chooser() {
	gtk_widget_destroy(get_widget());
	delete[] info;
	int       i;
	const int cnt = combos.size();
	for (i = 0; i < cnt; i++) {    // Delete all the combos.
		delete combos[i];
	}
}

/*
 *  Add a new or updated combo.
 *
 *  Output: Index of entry.
 */

int Combo_chooser::add(
		Combo* newcombo,    // We'll own this.
		int    index        // Index to replace, or -1 to add new.
) {
	if (index == -1) {
		// New.
		combos.push_back(newcombo);
		index = combos.size() - 1;    // Index of new entry.
	} else {
		assert(index >= 0 && unsigned(index) < combos.size());
		delete combos[index];
		combos[index] = newcombo;
	}
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gtk_adjustment_set_upper(
			adj, ((128 + border) * ((combos.size() + per_row - 1) / per_row)));
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
	render();
	return index;    // Return index.
}

/*
 *  Remove selected entry.
 */

void Combo_chooser::remove() {
	if (selected < 0) {
		return;
	}
	const int     tnum     = info[selected].num;
	Combo_editor* combowin = ExultStudio::get_instance()->get_combowin();
	if (combowin && combowin->is_visible() && combowin->file_index == tnum) {
		EStudio::Alert("Can't remove the combo you're editing");
		return;
	}
	if (EStudio::Prompt("Okay to remove selected combo?", "Yes", "no") != 0) {
		return;
	}
	reset_selected();
	Combo* todel = combos[tnum];
	delete todel;    // Delete from our list.
	combos.erase(combos.begin() + tnum);
	flex_info->set_modified();
	flex_info->remove(tnum);    // Update flex-file list.
	GtkAdjustment* adj = gtk_range_get_adjustment(GTK_RANGE(vscroll));
	gtk_adjustment_set_upper(
			adj, ((128 + border) * ((combos.size() + per_row - 1) / per_row)));
	g_signal_emit_by_name(G_OBJECT(adj), "changed");
	render();
}

/*
 *  Bring up editor for selected combo.
 */

void Combo_chooser::edit() {
	if (selected < 0) {
		return;
	}
	ExultStudio*  studio   = ExultStudio::get_instance();
	Combo_editor* combowin = studio->get_combowin();
	// Prompt if existing combo has unsaved changes.
	if (combowin && combowin->is_visible() && !combowin->prompt_for_discard()) {
		return;    // User chose not to discard.
	}
	const int tnum = info[selected].num;
	// Open window if not already visible.
	if (!combowin || !combowin->is_visible()) {
		studio->open_combo_window();
		combowin = studio->get_combowin();
		if (!combowin || !combowin->is_visible()) {
			return;    // Failed.
		}
	}
	combowin->set_combo(new Combo(*(combos[tnum])), tnum);
}

/*
 *  Configure the viewing window.
 */

gint Combo_chooser::configure(
		GtkWidget*         widget,    // The draw area.
		GdkEventConfigure* event,
		gpointer           data    // ->Combo_chooser
) {
	ignore_unused_variable_warning(widget, event);
	auto* chooser = static_cast<Combo_chooser*>(data);
	chooser->Shape_draw::configure();
	chooser->render();
	chooser->setup_info(true);
	return true;
}

void Combo_chooser::setup_info(bool savepos    // Try to keep current position.
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
			adj, ((128 + border) * ((combos.size() + per_row - 1) / per_row)));
	gtk_adjustment_set_step_increment(adj, ZoomDown(16));
	gtk_adjustment_set_page_increment(adj, h - border);
	gtk_adjustment_set_page_size(adj, h - border);
	if (savepos && selected >= 0) {
		gtk_adjustment_set_value(
				adj, ((128 + border) * (info[selected].num / per_row)));
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

gint Combo_chooser::expose(
		GtkWidget* widget,    // The view window.
		cairo_t*   cairo,
		gpointer   data    // ->Combo_chooser.
) {
	ignore_unused_variable_warning(widget);
	auto* chooser = static_cast<Combo_chooser*>(data);
	chooser->set_graphic_context(cairo);
	GdkRectangle area = {0, 0, 0, 0};
	gdk_cairo_get_clip_rectangle(cairo, &area);
	chooser->show(
			ZoomDown(area.x), ZoomDown(area.y), ZoomDown(area.width),
			ZoomDown(area.height));
	chooser->set_graphic_context(nullptr);
	return true;
}

gint Combo_chooser::drag_motion(
		GtkWidget*      widget,    // The view window.
		GdkEventMotion* event,
		gpointer        data    // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget);
	auto* chooser = static_cast<Combo_chooser*>(data);
	if (!chooser->dragging && chooser->selected >= 0) {
		chooser->start_drag(
				U7_TARGET_COMBOID_NAME, U7_TARGET_COMBOID,
				reinterpret_cast<GdkEvent*>(event));
	}
	return true;
}

/*
 *  Handle a mouse button press event.
 */

gint Combo_chooser::mouse_press(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Combo_chooser.
) {
	gtk_widget_grab_focus(widget);    // Enables keystrokes.
	auto* chooser = static_cast<Combo_chooser*>(data);

#ifdef DEBUG
	cout << "Combos : Clicked to " << (event->x) << " * " << (event->y)
		 << " by " << (event->button) << endl;
#endif
	if (event->button == 4) {
		chooser->scroll(true);
		return true;
	} else if (event->button == 5) {
		chooser->scroll(false);
		return true;
	}

	const int old_selected = chooser->selected;
	int       i;    // Search through entries.
	for (i = 0; i < chooser->info_cnt; i++) {
		if (chooser->info[i].box.has_point(
					ZoomDown(static_cast<int>(event->x)),
					ZoomDown(static_cast<int>(event->y)))) {
			// Found the box?
			// Indicate we can drag.
			chooser->selected = i;
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
	} else if (chooser->selected == old_selected && old_selected >= 0) {
		// Same square.  Check for dbl-click.
		if (reinterpret_cast<GdkEvent*>(event)->type == GDK_2BUTTON_PRESS) {
			chooser->edit();
		}
	}
	if (event->button == 3) {
		gtk_menu_popup_at_pointer(
				GTK_MENU(chooser->create_popup()),
				reinterpret_cast<GdkEvent*>(event));
	}
	return true;
}

/*
 *  Handle a mouse button-release event in the combo chooser.
 */
static gint Mouse_release(
		GtkWidget*      widget,    // The view window.
		GdkEventButton* event,
		gpointer        data    // ->Shape_chooser.
) {
	ignore_unused_variable_warning(widget, event);
	auto* chooser = static_cast<Combo_chooser*>(data);
	chooser->mouse_up();
	return true;
}

/*
 *  Move currently-selected combo up or down.
 */

void Combo_chooser::move(bool upwards) {
	if (selected < 0) {
		return;    // Shouldn't happen.
	}
	int tnum = info[selected].num;
	if ((tnum == 0 && upwards)
		|| (tnum == int(combos.size()) - 1 && !upwards)) {
		return;
	}
	if (upwards) {    // Going to swap tnum & tnum+1.
		tnum--;
	}
	Combo* tmp       = combos[tnum];
	combos[tnum]     = combos[tnum + 1];
	combos[tnum + 1] = tmp;
	selected += upwards ? -1 : 1;
	// Update editor if open.
	Combo_editor* combowin = ExultStudio::get_instance()->get_combowin();
	if (combowin && combowin->is_visible()) {
		if (combowin->file_index == tnum) {
			combowin->file_index = tnum + 1;
		} else if (combowin->file_index == tnum + 1) {
			combowin->file_index = tnum;
		}
	}
	flex_info->set_modified();
	flex_info->swap(tnum);    // Update flex-file list.
	render();
}

/*
 *  Search for an entry.
 */

void Combo_chooser::search(
		const char* srch,    // What to search for.
		int         dir      // 1 or -1.
) {
	const int total = get_count();
	if (!total) {
		return;    // Empty.
	}
	// Start with selection, or top.
	int start = selected >= 0 ? info[selected].num : 0;
	int i;
	start += dir;
	const int stop = dir == -1 ? -1 : total;
	for (i = start; i != stop; i += dir) {
		// int num = group ? (*group)[i] : i;
		const char* nm = combos[i]->name.c_str();
		if (nm && search_name(nm, srch)) {
			break;    // Found it.
		}
	}
	if (i == stop) {
		return;    // Not found.
	}
	while (i < index0) {    // Above current view?
		scroll(true);
	}
	while (i >= index0 + info_cnt) {    // Below?
		scroll(false);
	}
	const int newsel = i - index0;    // New selection.
	if (newsel >= 0 && newsel < info_cnt) {
		select(newsel);
	}
	render();
}
