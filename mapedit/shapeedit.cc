/**
 ** Shapeedit.cc - Shape-editing methods.
 **
 ** Written: 6/8/01 - JSF
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
#  include <config.h>
#endif

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "ammoinf.h"
#include "aniinf.h"
#include "armorinf.h"
#include "array_size.h"
#include "bodyinf.h"
#include "continf.h"
#include "data_utils.h"
#include "effhpinf.h"
#include "expinf.h"
#include "exult_constants.h"
#include "frflags.h"
#include "frnameinf.h"
#include "frusefun.h"
#include "items.h"
#include "lightinf.h"
#include "monstinf.h"
#include "npcdollinf.h"
#include "objdollinf.h"
#include "ready.h"
#include "servemsg.h"
#include "sfxinf.h"
#include "shapedraw.h"
#include "shapefile.h"
#include "shapevga.h"
#include "shapeinf.h"
#include "shapelst.h"
#include "studio.h"
#include "u7drag.h"
#include "utils.h"
#include "warminf.h"
#include "weaponinf.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

using std::string;

// HP Info columns
enum {
	HP_FRAME_COLUMN = 0,
	HP_QUALITY_COLUMN,
	HP_HIT_POINTS,
	HP_FROM_PATCH,
	HP_MODIFIED,
	HP_SHAPE_IMAGE,
	HP_COLUMN_COUNT
};

// Brightness Info columns
enum {
	BRIGHTNESS_FRAME_COLUMN = 0,
	BRIGHTNESS_VALUE_COLUMN,
	BRIGHTNESS_FROM_PATCH,
	BRIGHTNESS_MODIFIED,
	BRIGHTNESS_SHAPE_IMAGE,
	BRIGHTNESS_COLUMN_COUNT
};

// Warmth Info columns
enum {
	WARM_FRAME_COLUMN = 0,
	WARM_VALUE_COLUMN,
	WARM_FROM_PATCH,
	WARM_MODIFIED,
	WARM_SHAPE_IMAGE,
	WARM_COLUMN_COUNT
};

// Obj paperdoll Info columns
enum {
	DOLL_WORLD_FRAME = 0,
	DOLL_SPOT,
	DOLL_TRANSLUCENT,
	DOLL_GENDER_BASED,
	DOLL_SPOT_TYPE,
	DOLL_SHAPE,
	DOLL_FRAME_0,
	DOLL_FRAME_1,
	DOLL_FRAME_2,
	DOLL_FRAME_3,
	DOLL_FROM_PATCH,
	DOLL_MODIFIED,
	DOLL_SHAPE_IMAGE,
	DOLL_COLUMN_COUNT
};

// Content rules columns
enum {
	CNT_SHAPE_COLUMN = 0,
	CNT_ACCEPT_COLUMN,
	CNT_FROM_PATCH,
	CNT_MODIFIED,
	CNT_SHAPE_IMAGE,
	CNT_COLUMN_COUNT
};

// Frame flags columns
enum {
	FRFLAG_FRAME_COLUMN = 0,
	FRFLAG_QUAL_COLUMN,
	FRFLAG_FLAGS_COLUMN,
	FRFLAG_FROM_PATCH,
	FRFLAG_MODIFIED,
	FRFLAG_SHAPE_IMAGE,
	FRFLAG_COLUMN_COUNT
};

// Frame usecode columns
enum {
	FRUC_FRAME_COLUMN = 0,
	FRUC_QUAL_COLUMN,
	FRUC_USEFUN_COLUMN,
	FRUC_FROM_PATCH,
	FRUC_MODIFIED,
	FRUC_SHAPE_IMAGE,
	FRUC_COLUMN_COUNT
};

// Frame name columns
enum {
	FNAME_FRAME = 0,
	FNAME_QUALITY,
	FNAME_MSGTYPE,
	FNAME_MSGSTR,
	FNAME_OTHERTYPE,
	FNAME_OTHERMSG,
	FNAME_FROM_PATCH,
	FNAME_MODIFIED,
	FNAME_SHAPE_IMAGE,
	FNAME_COLUMN_COUNT
};

/*
 *  Widgets in one row of the 'equipment' dialog:
 */
struct Equip_row_widgets {
	Shape_single *single;       // Where the shape is drawn.
	GtkWidget *shape, *name, *chance, *count;
};
// Holds widgets from equip. dialog.
static Equip_row_widgets equip_rows[10] = {{nullptr, nullptr, nullptr, nullptr, nullptr}};

/*
 *  Equip window's Okay, Apply buttons.
 */
C_EXPORT void on_equip_okay_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_equip_window();
	ExultStudio::get_instance()->close_equip_window();
}
C_EXPORT void on_equip_apply_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_equip_window();
}

/*
 *  Equip window's Cancel button.
 */
C_EXPORT void on_equip_cancel_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_equip_window();
}

/*
 *  Equip window's close button.
 */
C_EXPORT gboolean on_equip_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_equip_window();
	return TRUE;
}

/*
 *  Record # changed, so update what's displayed.
 */
C_EXPORT gboolean on_equip_recnum_changed(
    GtkWidget *widget,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const int recnum = gtk_spin_button_get_value_as_int(
	                 GTK_SPIN_BUTTON(widget));
	ExultStudio::get_instance()->init_equip_window(recnum);
	return TRUE;
}

/*
 *  Add new record.
 */
C_EXPORT void on_equip_new_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->new_equip_record();
}

/*
 *  Set up 'equipment' dialog's table, which has 10 identical rows.
 */

static void Setup_equip(
    GtkContainer *viewport,     // Viewport to receive Table to fill.
    Vga_file *vgafile,          // Shapes.
    unsigned char *palbuf,      // Palette for drawing shapes.
    Equip_row_widgets rows[10]  // Filled in.
) {
	GtkGrid  *table =  GTK_GRID(gtk_grid_new());
	gtk_container_add(viewport, GTK_WIDGET(table));
	gtk_widget_show(GTK_WIDGET(table));
	// Labels at top:
	GtkWidget *label = gtk_label_new("Shape");
	gtk_widget_show(label);
	gtk_grid_attach(table, label, 0, 0, 3, 1);
	label = gtk_label_new("Chance (%)");
	gtk_widget_show(label);
	gtk_grid_attach(table, label, 4, 0, 1, 1);
	label = gtk_label_new("Count");
	gtk_widget_show(label);
	gtk_grid_attach(table, label, 6, 0, 1, 1);
	// Separators:
	GtkWidget *vsep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(vsep);
	gtk_grid_attach(table, vsep, 3, 0, 1, 12);
	gtk_widget_set_margin_start(vsep, 2);
	gtk_widget_set_margin_end(vsep, 2);
	gtk_widget_set_halign(vsep, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(vsep, TRUE);
	vsep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(vsep);
	gtk_grid_attach(table, vsep, 5, 0, 1, 12);
	gtk_widget_set_margin_start(vsep, 2);
	gtk_widget_set_margin_end(vsep, 2);
	gtk_widget_set_halign(vsep, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(vsep, TRUE);
	GtkWidget *hsep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show(hsep);
	gtk_grid_attach(table, hsep, 0, 1, 7, 1);
	gtk_widget_set_halign(hsep, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(hsep, TRUE);
	gtk_widget_set_valign(hsep, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand(hsep, TRUE);

	// Create the rows.
	for (int row = 0; row < 10; row++) {
		// Create frame, shape drawing area.
		GtkWidget *frame = gtk_frame_new(nullptr);
		gtk_widget_show(frame);
		gtk_grid_attach(table, frame, 0, row + 2, 1, 1);
		gtk_widget_set_margin_start(frame, 3);
		gtk_widget_set_margin_end(frame, 3);
		gtk_widget_set_margin_top(frame, 2);
		gtk_widget_set_margin_bottom(frame, 2);
		gtk_widget_set_halign(frame, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(frame, TRUE);
		gtk_widget_set_valign(frame, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(frame, TRUE);
		gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

		GtkWidget *drawingarea = gtk_drawing_area_new();
		gtk_widget_show(drawingarea);
		gtk_container_add(GTK_CONTAINER(frame), drawingarea);
		gtk_widget_set_size_request(drawingarea, 20, 40);
		// Shape #:
		GtkWidget *spin = gtk_spin_button_new(GTK_ADJUSTMENT(
		        gtk_adjustment_new(1, 0, c_max_shapes - 1, 1, 50, 50)), 1, 0);
		rows[row].shape = spin;
		gtk_widget_show(spin);
		gtk_grid_attach(table, spin, 1, row + 2, 1, 1);
		gtk_widget_set_margin_top(spin, 2);
		gtk_widget_set_margin_bottom(spin, 2);
		gtk_widget_set_halign(spin, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(spin, TRUE);
		// Name:
		label = gtk_label_new("label1");
		rows[row].name = label;
		gtk_widget_show(label);
		gtk_grid_attach(table, label, 2, row + 2, 1, 1);
		gtk_widget_set_halign(label, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(label, TRUE);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_label_set_xalign(GTK_LABEL(label), 0.2);
		gtk_label_set_yalign(GTK_LABEL(label), 0.5);
		if (vgafile && palbuf) {
			rows[row].single = new Shape_single(
			    spin, label,
			    [](int shnum)->bool{ return (shnum >= 1) && (shnum < c_max_shapes); },
			    nullptr, U7_SHAPE_SHAPES,
			    vgafile, palbuf, drawingarea);
		}
		// Chance:
		spin = gtk_spin_button_new(GTK_ADJUSTMENT(
		                               gtk_adjustment_new(1, 0, 100, 1, 10, 10)), 1, 0);
		rows[row].chance = spin;
		gtk_widget_show(spin);
		gtk_grid_attach(table, spin, 4, row + 2, 1, 1);
		gtk_widget_set_margin_top(spin, 2);
		gtk_widget_set_margin_bottom(spin, 2);
		gtk_widget_set_halign(spin, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(spin, TRUE);
		// Count:
		spin = gtk_spin_button_new(GTK_ADJUSTMENT(
		                               gtk_adjustment_new(1, 0, 100, 1, 10, 10)), 1, 0);
		rows[row].count = spin;
		gtk_widget_show(spin);
		gtk_grid_attach(table, spin, 6, row + 2, 1, 1);
		gtk_widget_set_margin_end(spin, 2);
		gtk_widget_set_margin_top(spin, 2);
		gtk_widget_set_margin_bottom(spin, 2);
		gtk_widget_set_halign(spin, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand(spin, TRUE);
	}
}

/*
 *  Set the fields to a given record.
 */

void ExultStudio::init_equip_window(
    int recnum          // Record # to start with (1-based).
) {
	// Fill in the record.
	Equip_record &rec = Monster_info::get_equip(recnum - 1);
	// Go through rows.
	for (int row = 0; row < 10; row++) {
		const Equip_element &elem = rec.get(row);
		const Equip_row_widgets &widgets = equip_rows[row];
		const int shnum = elem.get_shapenum();
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets.shape),
		                          shnum);
		const char *nm = get_shape_name(shnum);
		gtk_label_set_text(GTK_LABEL(widgets.name), nm);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets.chance),
		                          elem.get_probability());
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widgets.count),
		                          elem.get_quantity());
	}
}

/*
 *  Store the fields to a given record.
 */

void ExultStudio::save_equip_window(
) {
	const int recnum = get_spin("equip_recnum");
	// Get the record.
	Equip_record &rec = Monster_info::get_equip(recnum - 1);
	// Go through rows.
	for (int row = 0; row < 10; row++) {
		Equip_element &elem = rec.get(row);
		const Equip_row_widgets &widgets = equip_rows[row];
		elem.set(gtk_spin_button_get_value_as_int(
		             GTK_SPIN_BUTTON(widgets.shape)),
		         gtk_spin_button_get_value_as_int(
		             GTK_SPIN_BUTTON(widgets.chance)),
		         gtk_spin_button_get_value_as_int(
		             GTK_SPIN_BUTTON(widgets.count)));
	}
	shape_info_modified = true;
}

/*
 *  Open the equip-editing window.
 */

void ExultStudio::open_equip_window(
    int recnum          // Record # to start with (1-based).
) {
	const int ecnt = Monster_info::get_equip_cnt();
	if (recnum <= 0 || recnum > ecnt)
		return;
	if (!equipwin) {        // First time?
		equipwin = get_widget("equip_window");
		GtkContainer *viewport = GTK_CONTAINER(get_widget("equip_viewport"));
		Setup_equip(viewport, vgafile->get_ifile(),
		            palbuf.get(), equip_rows);
	}
	// This will cause the data to be set:
	set_spin("equip_recnum", recnum, 1, ecnt);
	set_sensitive("equip_new", ecnt < 255);
	gtk_widget_show(equipwin);
}

/*
 *  Close the equip window.
 */

void ExultStudio::close_equip_window(
) {
	if (equipwin)
		gtk_widget_hide(equipwin);
}

/*
 *  Add a new equipment record.
 */

void ExultStudio::new_equip_record(
) {
	Equip_record rec;
	Monster_info::add_equip(rec);
	const int ecnt = Monster_info::get_equip_cnt();
	const int recnum = ecnt;
	// Show new entry.
	set_spin("equip_recnum", recnum, 1, ecnt);
	set_sensitive("equip_new", ecnt < 255);
	shape_info_modified = true;
}

/*
 *  Shape window's Okay, Apply buttons.
 */
C_EXPORT void on_shinfo_okay_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_shape_window();
	ExultStudio::get_instance()->close_shape_window();
}
C_EXPORT void on_shinfo_apply_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->save_shape_window();
}

/*
 *  Shape window's Cancel button.
 */
C_EXPORT void on_shinfo_cancel_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio::get_instance()->close_shape_window();
}

/*
 *  Shape window's close button.
 */
C_EXPORT gboolean on_shape_window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer user_data
) {
	ignore_unused_variable_warning(widget, event, user_data);
	ExultStudio::get_instance()->close_shape_window();
	return TRUE;
}

/*
 *  Weapon ammo type changed.
 */
C_EXPORT gboolean on_shinfo_weapon_ammo_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	const bool on = sel == 0;
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_weapon_ammo_shape", on);
	return TRUE;
}

/*
 *  Weapon projectile type changed.
 */
C_EXPORT gboolean on_shinfo_weapon_sprite_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	const bool on = sel == 0;
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_weapon_proj", on);
	return TRUE;
}

/*
 *  Ammo projectile type changed.
 */
C_EXPORT gboolean on_shinfo_ammo_sprite_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	const bool on = sel == 0;
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_ammo_proj", on);
	return TRUE;
}

/*
 *  Animation type changed.
 */
C_EXPORT gboolean on_shinfo_animation_type_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	bool on = sel != static_cast<int>(Animation_info::FA_HOURLY);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_animation_ticks", on);
	studio->set_sensitive("shinfo_animation_sfxsynch", on);
	const bool sfxon = !studio->get_toggle("shinfo_animation_sfxsynch");
	studio->set_sensitive("shinfo_animation_sfxdelay", on && sfxon);
	on = (sel == static_cast<int>(Animation_info::FA_LOOPING));
	const bool freezeon = studio->get_optmenu("shinfo_animation_freezefirst") == 2;
	studio->set_sensitive("shinfo_animation_freezefirst", on);
	studio->set_sensitive("shinfo_animation_freezechance", on && freezeon);
	studio->set_sensitive("shinfo_animation_rectype", on);
	const bool recon = studio->get_toggle("shinfo_animation_rectype");
	studio->set_sensitive("shinfo_animation_recycle", on && !recon);
	return TRUE;
}
/*
 *  Animation frame count menu changed.
 */
C_EXPORT gboolean on_shinfo_animation_frtype_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_animation_frcount", on);
	return TRUE;
}


/*
 *  Animation recycle type menu changed.
 */
C_EXPORT gboolean on_shinfo_animation_rectype_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_animation_recycle", on);
	return TRUE;
}

/*
 *  Animation SFX synchronization type changed.
 */
C_EXPORT gboolean on_shinfo_animation_sfxsynch_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_animation_sfxdelay", on);
	return TRUE;
}

/*
 *  Animation first frame freeze type changed.
 */
C_EXPORT gboolean on_shinfo_animation_freezefirst_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_animation_freezechance", sel == 2);
	return TRUE;
}

/*
 *  Effective HPs frame type changed.
 */
C_EXPORT gboolean on_shinfo_effhps_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_effhps_frame_num", on);
	return TRUE;
}

/*
 *  Effective HPs quality type changed.
 */
C_EXPORT gboolean on_shinfo_effhps_qual_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_effhps_qual_num", on);
	return TRUE;
}

/*
 *  Effective HPs HP type changed.
 */
C_EXPORT gboolean on_shinfo_effhps_hp_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_effhps_hp_val", on);
	return TRUE;
}

/*
 *  Frame usecode frame type changed.
 */
C_EXPORT gboolean on_shinfo_frameusecode_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_frameusecode_frame_num", on);
	return TRUE;
}

/*
 *  Frame usecode quality type changed.
 */
C_EXPORT gboolean on_shinfo_frameusecode_qual_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_frameusecode_qual_num", on);
	return TRUE;
}

/*
 *  Frame names frame type changed.
 */
C_EXPORT gboolean on_shinfo_framenames_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_framenames_frame_num", on);
	return TRUE;
}

/*
 *  Frame names quality type changed.
 */
C_EXPORT gboolean on_shinfo_framenames_qual_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_framenames_qual_num", on);
	return TRUE;
}

/*
 *  Frame name main name type changed.
 */
C_EXPORT gboolean on_shinfo_framenames_name_type_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int type = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	if (type == 0)
		studio->set_entry("shinfo_framenames_name_text",
		                  studio->get_text_entry("shinfo_name"), false);
	else if (type == 1)
		studio->set_entry("shinfo_framenames_name_text", "", false);
	else
		studio->set_sensitive("shinfo_framenames_name_text", true);
	if (type != 3) {
		studio->set_optmenu("shinfo_framenames_comp_pos", 0, false);
		studio->set_optmenu("shinfo_framenames_comp_type", 0, false);
		studio->set_optmenu("shinfo_framenames_comp_msg_type", 1, false);
		studio->set_entry("shinfo_framenames_comp_msg_text", "", false);
	} else {
		studio->set_sensitive("shinfo_framenames_comp_pos", true);
		studio->set_sensitive("shinfo_framenames_comp_type", true);
		int ot = studio->get_optmenu("shinfo_framenames_comp_type");
		if (!ot)
			studio->set_sensitive("shinfo_framenames_comp_msg_type", true);
		else
			studio->set_optmenu("shinfo_framenames_comp_msg_type", 0, false);
		ot = studio->get_optmenu("shinfo_framenames_comp_msg_type");
		studio->set_sensitive("shinfo_framenames_comp_msg_text", ot == 2);
	}
	return TRUE;
}

/*
 *  Frame name other kind changed.
 */
C_EXPORT gboolean on_shinfo_framenames_comp_msg_type_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_framenames_comp_msg_text", val == 2);
	return TRUE;
}

/*
 *  Frame name other type changed.
 */
C_EXPORT gboolean on_shinfo_framenames_comp_type_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	if (!val)
		studio->set_sensitive("shinfo_framenames_comp_msg_type", true);
	else
		studio->set_optmenu("shinfo_framenames_comp_msg_type", 0, false);
	val = studio->get_optmenu("shinfo_framenames_comp_msg_type");
	studio->set_sensitive("shinfo_framenames_comp_msg_text", val == 2);
	return TRUE;
}

/*
 *  Frame flags frame type changed.
 */
C_EXPORT gboolean on_shinfo_frameflags_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_frameflags_frame_num", on);
	return TRUE;
}

/*
 *  Frame flags quality type changed.
 */
C_EXPORT gboolean on_shinfo_frameflags_qual_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_frameflags_qual_num", on);
	return TRUE;
}

/*
 *  Brightness frame type changed.
 */
C_EXPORT gboolean on_shinfo_brightness_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_brightness_frame_num", on);
	return TRUE;
}

/*
 *  Warmth frame type changed.
 */
C_EXPORT gboolean on_shinfo_warmth_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_warmth_frame_num", on);
	return TRUE;
}

/*
 *  Content rules shape type changed.
 */
C_EXPORT gboolean on_shinfo_cntrules_shape_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_cntrules_shape_num", on);
	return TRUE;
}

/*
 *  Explosion SFX type changed.
 */
C_EXPORT gboolean on_shinfo_explosion_sfx_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_explosion_sfx_number", on);
	return TRUE;
}

/*
 *  Shape window's Equip-Edit button.
 */
C_EXPORT void on_open_equip_button_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *s = ExultStudio::get_instance();
	s->open_equip_window(s->get_spin("shinfo_monster_equip"));
}

/*
 *  Frame # in shape-info window:
 */
C_EXPORT void
on_shinfo_frame_changed(GtkSpinButton *button,
                        gpointer     user_data) {
	ignore_unused_variable_warning(user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_shape_notebook_frame(
	    gtk_spin_button_get_value_as_int(button));
	// Force repaint of shape.
	gtk_widget_queue_draw(studio->get_widget("shinfo_draw"));
}

const auto TreeIterDeleter = [](GtkTreeIter *iter) {
	if (iter != nullptr) {
		gtk_tree_iter_free(iter);
	}
};
using UniqueTreeIter = std::unique_ptr<GtkTreeIter, decltype(TreeIterDeleter)>;

/*
 *  Helper tree functions.
 */
template<typename T>
static inline int Find_unary_iter(
    GtkTreeModel *model,
    UniqueTreeIter &out_iter,
    T newval
) {
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(model, &iter)) {
		out_iter.reset(nullptr);
		return 1;
	}
	do {
		out_iter.reset(gtk_tree_iter_copy(&iter));
		T val;
		gtk_tree_model_get(model, &iter, 0, &val, -1);
		if (val == newval) {
			return 0;
		} else if (newval < val) {
			return -1;
		}
	} while (gtk_tree_model_iter_next(model, &iter));
	return 1;
}

template<typename T1, typename T2>
static inline int Find_binary_iter(
    GtkTreeModel *model,
    UniqueTreeIter &out_iter,
    T1 newval1,
    T2 newval2
) {
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(model, &iter)) {
		out_iter.reset(nullptr);
		return 1;
	}
	do {
		out_iter.reset(gtk_tree_iter_copy(&iter));
		T1 val1;
		T2 val2;
		gtk_tree_model_get(model, &iter, 0, &val1, 1, &val2, -1);
		if (val1 == newval1 && val2 == newval2) {
			return 0;
		} else if ((newval1 == val1 && newval2 < val2) || (newval1 < val1)) {
			return -1;
		}

	} while (gtk_tree_model_iter_next(model, &iter));
	return 1;
}

/*
 *  Helper functions for effective HP data.
 */
static inline void Get_hp_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    unsigned int &qual,
    unsigned int *hps = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_effhps_frame_type");
	const bool anyq = studio->get_toggle("shinfo_effhps_qual_type");
	const bool nohps = studio->get_toggle("shinfo_effhps_hp_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_effhps_frame_num");
	qual = anyq ? ~0u :
	       studio->get_spin("shinfo_effhps_qual_num");
	if (hps)
		*hps = nohps ? 0 : studio->get_spin("shinfo_effhps_hp_val");
}

static inline bool Have_quality() {
	ExultStudio *studio = ExultStudio::get_instance();
	const int c = studio->get_optmenu("shinfo_shape_class");
	return c == 1 || c == 5 || c == 6 || c == 9 || c == 10 || c == 11;
}

static inline void Set_hp_fields(
    int frnum = 0,
    int qual = 0,
    int hps = 0
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_effhps_frame_type", frnum == -1);
	const bool havequal = Have_quality();
	if (!havequal)
		qual = -1;
	studio->set_toggle("shinfo_effhps_qual_type", qual == -1, havequal);
	studio->set_toggle("shinfo_effhps_hp_type", hps == 0);
	studio->set_spin("shinfo_effhps_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_spin("shinfo_effhps_qual_num", qual == -1 ? 0 : qual, qual != -1);
	studio->set_spin("shinfo_effhps_hp_val", hps, hps != 0);
}

/*
 *  Adding HP information.
 */
C_EXPORT void on_shinfo_effhps_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *hptree = GTK_TREE_VIEW(
	                          studio->get_widget("shinfo_effhps_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(hptree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	unsigned int newqual;
	unsigned int newhps;
	Get_hp_fields(studio, newfrnum, newqual, &newhps);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_binary_iter(model, iter, newfrnum, newqual);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter, HP_FRAME_COLUMN, static_cast<int>(newfrnum),
		                   HP_QUALITY_COLUMN, static_cast<int>(newqual),
		                   HP_HIT_POINTS, static_cast<int>(newhps),
		                   HP_FROM_PATCH, 1, HP_MODIFIED, 1,
		                   HP_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		unsigned int hps;
		gtk_tree_model_get(model, iter.get(), HP_HIT_POINTS, &hps, -1);
		if (hps != newhps)
			gtk_tree_store_set(store, iter.get(), HP_HIT_POINTS, newhps,
			                   HP_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting HP information.
 */
C_EXPORT void on_shinfo_effhps_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *hptree = GTK_TREE_VIEW(
	                          studio->get_widget("shinfo_effhps_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(hptree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	unsigned int newqual;
	Get_hp_fields(studio, newfrnum, newqual);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_binary_iter(model, iter, newfrnum, newqual))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed HP selection.
 */
C_EXPORT void on_shinfo_effhps_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int frnum;
	int qual;
	int hps;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, HP_FRAME_COLUMN, &frnum,
	                   HP_QUALITY_COLUMN, &qual, HP_HIT_POINTS, &hps, -1);
	Set_hp_fields(frnum, qual, hps);
}

/*
 *  Changed shape class.
 */
C_EXPORT gboolean on_shinfo_shape_class_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(widget, data);
	ExultStudio *studio = ExultStudio::get_instance();
	if (!Have_quality()) {
		studio->set_toggle("shinfo_effhps_qual_type", true, false);
		studio->set_spin("shinfo_effhps_qual_num", 0, false);
		studio->set_toggle("shinfo_framenames_qual_type", true, false);
		studio->set_spin("shinfo_framenames_qual_num", 0, false);
		studio->set_toggle("shinfo_frameflags_qual_type", true, false);
		studio->set_spin("shinfo_frameflags_qual_num", 0, false);
		studio->set_toggle("shinfo_frameusecode_qual_type", true, false);
		studio->set_spin("shinfo_frameusecode_qual_num", 0, false);
	} else {
		studio->set_sensitive("shinfo_effhps_qual_type", true);
		studio->set_sensitive("shinfo_effhps_qual_num", true);
		studio->set_sensitive("shinfo_framenames_qual_type", true);
		studio->set_sensitive("shinfo_framenames_qual_num", true);
		studio->set_sensitive("shinfo_frameflags_qual_type", true);
		studio->set_sensitive("shinfo_frameflags_qual_num", true);
		studio->set_sensitive("shinfo_frameusecode_qual_type", true);
		studio->set_sensitive("shinfo_frameusecode_qual_num", true);
	}

	if (studio->get_optmenu("shinfo_shape_class") == 12)
		studio->set_sensitive("shinfo_mountaintop_type", true);
	else
		studio->set_optmenu("shinfo_mountaintop_type", 0, false);

	return TRUE;
}

/*
 *  Helper functions for brightness data.
 */
static inline void Get_brightness_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    unsigned int *brightness = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_brightness_frame_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_brightness_frame_num");
	if (brightness)
		*brightness = studio->get_spin("shinfo_brightness_val");
}

static void Set_brightness_fields(
    int frnum = 0,
    int brightness = 0
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_brightness_frame_type", frnum == -1);
	studio->set_spin("shinfo_brightness_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_spin("shinfo_brightness_val", brightness);
}

/*
 *  Adding brightness information.
 */
C_EXPORT void on_shinfo_brightness_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	unsigned int newbrightness;
	Get_brightness_fields(studio, newfrnum, &newbrightness);

	GtkTreeView *brightnesstree = GTK_TREE_VIEW(
	                                  studio->get_widget("shinfo_brightness_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(brightnesstree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_unary_iter(model, iter, newfrnum);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter, BRIGHTNESS_FRAME_COLUMN, static_cast<int>(newfrnum),
		                   BRIGHTNESS_VALUE_COLUMN, static_cast<int>(newbrightness), BRIGHTNESS_FROM_PATCH, 1,
		                   BRIGHTNESS_MODIFIED, 1,
		                   BRIGHTNESS_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		unsigned int brightness;
		gtk_tree_model_get(model, iter.get(), BRIGHTNESS_VALUE_COLUMN, &brightness, -1);
		if (brightness != newbrightness)
			gtk_tree_store_set(store, iter.get(), BRIGHTNESS_VALUE_COLUMN, newbrightness,
			                   BRIGHTNESS_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting brightness information.
 */
C_EXPORT void on_shinfo_brightness_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *brightnesstree = GTK_TREE_VIEW(
	                                  studio->get_widget("shinfo_brightness_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(brightnesstree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	Get_brightness_fields(studio, newfrnum);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_unary_iter(model, iter, newfrnum))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed brightness selection.
 */
C_EXPORT void on_shinfo_brightness_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int frnum;
	int brightness;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, BRIGHTNESS_FRAME_COLUMN, &frnum,
	                   BRIGHTNESS_VALUE_COLUMN, &brightness, -1);
	Set_brightness_fields(frnum, brightness);
}

/*
 *  Helper functions for warmth data.
 */
static inline void Get_warmth_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    int *warm = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_warmth_frame_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_warmth_frame_num");
	if (warm)
		*warm = studio->get_spin("shinfo_warmth_val");
}

static void Set_warmth_fields(
    int frnum = 0,
    int warmth = 0
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_warmth_frame_type", frnum == -1);
	studio->set_spin("shinfo_warmth_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_spin("shinfo_warmth_val", warmth);
}

/*
 *  Adding warmth information.
 */
C_EXPORT void on_shinfo_warmth_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	int newwarm;
	Get_warmth_fields(studio, newfrnum, &newwarm);

	GtkTreeView *warmtree = GTK_TREE_VIEW(
	                            studio->get_widget("shinfo_warmth_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(warmtree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_unary_iter(model, iter, newfrnum);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter, WARM_FRAME_COLUMN, static_cast<int>(newfrnum),
		                   WARM_VALUE_COLUMN, newwarm, WARM_FROM_PATCH, 1,
		                   WARM_MODIFIED, 1,
		                   WARM_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		int warm;
		gtk_tree_model_get(model, iter.get(), WARM_VALUE_COLUMN, &warm, -1);
		if (warm != newwarm)
			gtk_tree_store_set(store, iter.get(), WARM_VALUE_COLUMN, newwarm,
			                   WARM_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting warmth information.
 */
C_EXPORT void on_shinfo_warmth_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *warmtree = GTK_TREE_VIEW(
	                            studio->get_widget("shinfo_warmth_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(warmtree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	Get_warmth_fields(studio, newfrnum);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_unary_iter(model, iter, newfrnum))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed warmth selection.
 */
C_EXPORT void on_shinfo_warmth_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int frnum;
	int warmth;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, WARM_FRAME_COLUMN, &frnum,
	                   WARM_VALUE_COLUMN, &warmth, -1);
	Set_warmth_fields(frnum, warmth);
}

/*
 *  Helper functions for content rules.
 */
static inline void Get_cntrules_fields(
    ExultStudio *studio,
    unsigned int &shnum,
    bool *accept = nullptr
) {
	const bool anysh = studio->get_toggle("shinfo_cntrules_shape_type");
	shnum = anysh ? ~0u :
	        studio->get_spin("shinfo_cntrules_shape_num");
	if (accept)
		*accept = studio->get_toggle("shinfo_cntrules_accept");
}

static void Set_cntrules_fields(
    int shnum = 0,
    bool accept = false
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_cntrules_shape_type", shnum == -1);
	studio->set_spin("shinfo_cntrules_shape_num", shnum == -1 ? 0 : shnum, shnum != -1);
	studio->set_toggle("shinfo_cntrules_accept", accept);
}

/*
 *  Adding content rules.
 */
C_EXPORT void on_shinfo_cntrules_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newshnum;
	bool newaccept;
	Get_cntrules_fields(studio, newshnum, &newaccept);

	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_cntrules_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_unary_iter(model, iter, newshnum);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    static_cast<int>(newshnum), 0, true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter, CNT_SHAPE_COLUMN, static_cast<int>(newshnum),
		                   CNT_ACCEPT_COLUMN, static_cast<int>(newaccept), CNT_FROM_PATCH, 1,
		                   CNT_MODIFIED, 1,
		                   CNT_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		unsigned int accept;
		gtk_tree_model_get(model, iter.get(), CNT_ACCEPT_COLUMN, &accept, -1);
		if (accept != newaccept)
			gtk_tree_store_set(store, iter.get(), CNT_ACCEPT_COLUMN, newaccept,
			                   CNT_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting content rules.
 */
C_EXPORT void on_shinfo_cntrules_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_cntrules_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newshnum;
	Get_cntrules_fields(studio, newshnum);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_unary_iter(model, iter, newshnum))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed content rules selection.
 */
C_EXPORT void on_shinfo_cntrules_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int shnum;
	int accept;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, CNT_SHAPE_COLUMN, &shnum,
	                   CNT_ACCEPT_COLUMN, &accept, -1);
	Set_cntrules_fields(shnum, accept);
}

/*
 *  Helper functions for frame flags.
 */
static inline void Get_frameflags_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    unsigned int &qual,
    unsigned int *flags = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_frameflags_frame_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_frameflags_frame_num");
	const bool anyq = studio->get_toggle("shinfo_frameflags_qual_type");
	qual = anyq ? ~0u :
	       studio->get_spin("shinfo_frameflags_qual_num");
	if (flags) {
		static const char *frflags[] = {
			"shinfo_frameflags_flag0",
			"shinfo_frameflags_flag1",
			"shinfo_frameflags_flag2",
			"shinfo_frameflags_flag3",
			"shinfo_frameflags_flag4",
			"shinfo_frameflags_flag5",
			"shinfo_frameflags_flag6",
			"shinfo_frameflags_flag7",
			"shinfo_frameflags_flag8",
			"shinfo_frameflags_flag9",
			"shinfo_frameflags_flag10",
			"shinfo_frameflags_flag11",
			"shinfo_frameflags_flag12"
		};
		*flags = studio->get_bit_toggles(&frflags[0], array_size(frflags));
	}
}

static void Set_frameflags_fields(
    int frnum = 0,
    int qual = 0,
    unsigned int flags = 0
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_frameflags_frame_type", frnum == -1);
	studio->set_spin("shinfo_frameflags_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_toggle("shinfo_frameflags_qual_type", qual == -1);
	studio->set_spin("shinfo_frameflags_qual_num", qual == -1 ? 0 : qual, qual != -1);
	static const char *frflags[] = {
		"shinfo_frameflags_flag0",
		"shinfo_frameflags_flag1",
		"shinfo_frameflags_flag2",
		"shinfo_frameflags_flag3",
		"shinfo_frameflags_flag4",
		"shinfo_frameflags_flag5",
		"shinfo_frameflags_flag6",
		"shinfo_frameflags_flag7",
		"shinfo_frameflags_flag8",
		"shinfo_frameflags_flag9",
		"shinfo_frameflags_flag10",
		"shinfo_frameflags_flag11",
		"shinfo_frameflags_flag12"
	};
	studio->set_bit_toggles(&frflags[0], array_size(frflags), flags);
}

/*
 *  Adding frame flags.
 */
C_EXPORT void on_shinfo_frameflags_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	unsigned int newqual;
	unsigned int newflags;
	Get_frameflags_fields(studio, newfrnum, newqual, &newflags);

	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_frameflags_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_binary_iter(model, iter, newfrnum, newqual);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter,
		                   FRFLAG_FRAME_COLUMN, static_cast<int>(newfrnum),
		                   FRFLAG_QUAL_COLUMN, static_cast<int>(newqual),
		                   FRFLAG_FLAGS_COLUMN, static_cast<int>(newflags), FRFLAG_FROM_PATCH, 1,
		                   FRFLAG_MODIFIED, 1,
		                   FRFLAG_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		unsigned int flags;
		gtk_tree_model_get(model, iter.get(), FRFLAG_FLAGS_COLUMN, &flags, -1);
		if (flags != newflags)
			gtk_tree_store_set(store, iter.get(), FRFLAG_FLAGS_COLUMN, newflags,
			                   FRFLAG_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting frame flags.
 */
C_EXPORT void on_shinfo_frameflags_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_frameflags_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	unsigned int newqual;
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	Get_frameflags_fields(studio, newfrnum, newqual);
	if (!Find_binary_iter(model, iter, newfrnum, newqual))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed frame flags selection.
 */
C_EXPORT void on_shinfo_frameflags_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);

	int frnum;
	int qual;
	int flags;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, FRFLAG_FRAME_COLUMN, &frnum,
	                   FRFLAG_QUAL_COLUMN, &qual, FRFLAG_FLAGS_COLUMN, &flags, -1);
	Set_frameflags_fields(frnum, qual, flags);
}

/*
 *  Helper functions for frame usecode.
 */
static inline void Get_frameusecode_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    unsigned int &qual,
    const char **ucfun = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_frameusecode_frame_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_frameusecode_frame_num");
	const bool anyq = studio->get_toggle("shinfo_frameusecode_qual_type");
	qual = anyq ? ~0u :
	       studio->get_spin("shinfo_frameusecode_qual_num");
	if (ucfun)
		*ucfun = studio->get_text_entry("shinfo_frameusecode_ucfun");
}

static void Set_frameusecode_fields(
    int frnum = 0,
    int qual = 0,
    const char *ucfun = nullptr
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_frameusecode_frame_type", frnum == -1);
	studio->set_spin("shinfo_frameusecode_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_toggle("shinfo_frameusecode_qual_type", qual == -1);
	studio->set_spin("shinfo_frameusecode_qual_num", qual == -1 ? 0 : qual, qual != -1);
	studio->set_entry("shinfo_frameusecode_ucfun", ucfun ? ucfun : "");
}

/*
 *  Adding frame usecode.
 */
C_EXPORT void on_shinfo_frameusecode_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	unsigned int newqual;
	const char *newucfun;
	Get_frameusecode_fields(studio, newfrnum, newqual, &newucfun);

	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_frameusecode_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_binary_iter(model, iter, newfrnum, newqual);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter,
		                   FRUC_FRAME_COLUMN, static_cast<int>(newfrnum),
		                   FRUC_QUAL_COLUMN, static_cast<int>(newqual),
		                   FRUC_USEFUN_COLUMN, newucfun, FRUC_FROM_PATCH, 1,
		                   FRUC_MODIFIED, 1,
		                   FRUC_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		const char *ucfun;
		gtk_tree_model_get(model, iter.get(), FRUC_USEFUN_COLUMN, &ucfun, -1);
		if (!std::strcmp(ucfun, newucfun))
			gtk_tree_store_set(store, iter.get(), FRUC_USEFUN_COLUMN, newucfun,
			                   FRUC_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting frame usecode.
 */
C_EXPORT void on_shinfo_frameusecode_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           studio->get_widget("shinfo_frameusecode_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newqual;
	unsigned int newfrnum;
	Get_frameusecode_fields(studio, newfrnum, newqual);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_binary_iter(model, iter, newfrnum, newqual))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Browsing for frame usecode.
 */
C_EXPORT void on_shinfo_frameusecode_browse_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	const char *uc = studio->browse_usecode(true);
	if (*uc)
		studio->set_entry("shinfo_frameusecode_ucfun", uc, true);
}

/*
 *  Changed frame usecode selection.
 */
C_EXPORT void on_shinfo_frameusecode_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int frnum;
	int qual;
	char *ucfun;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, FRUC_FRAME_COLUMN, &frnum,
	                   FRUC_QUAL_COLUMN, &qual, FRUC_USEFUN_COLUMN, &ucfun, -1);
	Set_frameusecode_fields(frnum, qual, ucfun);
}


/*
 *  Helper functions for frame names.
 */
static inline void Get_framenames_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    unsigned int &qual,
    int *type = nullptr,
    const char **str = nullptr,
    int *othertype = nullptr,
    const char **othermsg = nullptr
) {
	const bool anyfr = studio->get_toggle("shinfo_framenames_frame_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_framenames_frame_num");
	const bool anyq = studio->get_toggle("shinfo_framenames_qual_type");
	qual = anyq ? ~0u :
	       studio->get_spin("shinfo_framenames_qual_num");
	if (type) {
		switch (studio->get_optmenu("shinfo_framenames_name_type")) {
		case 0:
			*type = -255;
			break;
		case 1:
			*type = -1;
			break;
		case 2:
			*type = 0;
			break;
		default: {
			const int b0 = studio->get_optmenu("shinfo_framenames_comp_pos") + 1;
			const int b1 = studio->get_optmenu("shinfo_framenames_comp_type") * 2;
			*type = b0 + b1;
			break;
		}
		}
	}
	if (str)
		*str = studio->get_text_entry("shinfo_framenames_name_text");
	if (othertype) {
		switch (studio->get_optmenu("shinfo_framenames_comp_msg_type")) {
		case 0:
			*othertype = -255;
			break;
		case 1:
			*othertype = -1;
			break;
		default:
			*othertype = 1;
			break;
		}
	}
	if (othermsg)
		*othermsg = studio->get_text_entry("shinfo_framenames_comp_msg_text");
}

static void Set_framenames_fields(
    int frnum = 0,
    int qual = 0,
    int type = 0,
    const char *str = nullptr,
    int othertype = 0,
    const char *othermsg = nullptr
) {
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_toggle("shinfo_framenames_frame_type", frnum == -1);
	studio->set_spin("shinfo_framenames_frame_num", frnum == -1 ? 0 : frnum, frnum != -1);
	const bool havequal = Have_quality();
	if (!havequal)
		qual = -1;
	studio->set_toggle("shinfo_framenames_qual_type", qual == -1, havequal);
	studio->set_spin("shinfo_framenames_qual_num", qual == -1 ? 0 : qual, qual != -1);
	studio->set_entry("shinfo_framenames_name_text", str ? str : "", type >= 0);
	if (type <= 0) {
		studio->set_optmenu("shinfo_framenames_name_type",
		                    type == -255 ? 0 : (type == -1 ? 1 : 2));
		studio->set_optmenu("shinfo_framenames_comp_pos", 0, false);
		studio->set_optmenu("shinfo_framenames_comp_type", 0, false);
		studio->set_optmenu("shinfo_framenames_comp_msg_type", 1, false);
		studio->set_entry("shinfo_framenames_comp_msg_text", "", false);
	} else {
		--type;
		const int flag = (type & 2) / 2;
		studio->set_optmenu("shinfo_framenames_name_type", 3);
		studio->set_optmenu("shinfo_framenames_comp_pos", (type & 1));
		studio->set_optmenu("shinfo_framenames_comp_type", flag);
		const int compindex = !flag ? (othertype == -255 ? 0 :
		                         (othertype == -1 ? 1 : 2)) : 0;
		studio->set_optmenu("shinfo_framenames_comp_msg_type", compindex, !flag);
		studio->set_entry("shinfo_framenames_comp_msg_text",
		                  othermsg ? othermsg : nullptr, othertype >= 0);
	}
}

/*
 *  Adding frame names.
 */
C_EXPORT void on_shinfo_framenames_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	unsigned int newqual;
	int newtype;
	int newothertype;
	const char *newstr;
	const char *newothermsg;
	Get_framenames_fields(studio, newfrnum, newqual, &newtype, &newstr,
	                      &newothertype, &newothermsg);

	GtkTreeView *nametree = GTK_TREE_VIEW(
	                            studio->get_widget("shinfo_framenames_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(nametree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_binary_iter(model, iter, newfrnum, newqual);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter, FNAME_FRAME, static_cast<int>(newfrnum),
		                   FNAME_QUALITY, static_cast<int>(newqual), FNAME_MSGTYPE, newtype,
		                   FNAME_MSGSTR, newstr, FNAME_OTHERTYPE, newothertype,
		                   FNAME_OTHERMSG, newothermsg,
		                   FNAME_FROM_PATCH, 1, FNAME_MODIFIED, 1,
		                   FNAME_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		int type;
		int othertype;
		const char *str;
		const char *othermsg;
		gtk_tree_model_get(model, iter.get(), FNAME_MSGTYPE, &type,
		                   FNAME_MSGSTR, &str, FNAME_OTHERTYPE, &othertype,
		                   FNAME_OTHERMSG, &othermsg, -1);
		if (type != newtype || othertype != newothertype
		        || !strcmp(str, newstr) || !strcmp(othermsg, newothermsg))
			gtk_tree_store_set(store, iter.get(), FNAME_FRAME, static_cast<int>(newfrnum),
			                   FNAME_QUALITY, static_cast<int>(newqual), FNAME_MSGTYPE, newtype,
			                   FNAME_MSGSTR, newstr, FNAME_OTHERTYPE, newothertype,
			                   FNAME_OTHERMSG, newothermsg,
			                   FNAME_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting frame name.
 */
C_EXPORT void on_shinfo_framenames_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *nametree = GTK_TREE_VIEW(
	                            studio->get_widget("shinfo_framenames_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(nametree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	unsigned int newqual;
	Get_framenames_fields(studio, newfrnum, newqual);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_binary_iter(model, iter, newfrnum, newqual))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Changed frame name selection.
 */
C_EXPORT void on_shinfo_framenames_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	int frnum;
	int qual;
	int type;
	int othertype;
	const char *str;
	const char *othermsg;
	gtk_tree_model_get(model, &iter, FNAME_FRAME, &frnum,
	                   FNAME_QUALITY, &qual, FNAME_MSGTYPE, &type,
	                   FNAME_MSGSTR, &str, FNAME_OTHERTYPE, &othertype,
	                   FNAME_OTHERMSG, &othermsg, -1);
	Set_framenames_fields(frnum, qual, type, str, othertype, othermsg);
}

/*
 *  Helper functions for object paperdoll data.
 */
static inline void Get_objpaperdoll_fields(
    ExultStudio *studio,
    unsigned int &frnum,
    int &spot,
    int *shape = nullptr,
    int *frame0 = nullptr,
    int *frame1 = nullptr,
    int *frame2 = nullptr,
    int *frame3 = nullptr,
    int *type = nullptr,
    bool *trans = nullptr,
    bool *gender = nullptr
) {
	const int ready = studio->get_optmenu("shinfo_ready_spot");
	bool fron = ready == 6 || ready == 7 || ready == 8 || ready == 9;
	const int altspot = studio->get_optmenu("shinfo_altready1_spot");
	const bool armor = ready == 12 && altspot != 3;
	if (armor) fron = true;
	spot = studio->get_optmenu("shinfo_objpaperdoll_spot");
	int max;
	if (spot == 0 && ready == 11) // helmet
		max = 1;
	else if (spot == 3 && ready == 2)
		max = 2;
	else
		max = 0;
	spot = spot == 18 ? 102 : spot;

	const bool anyfr = studio->get_toggle("shinfo_objpaperdoll_wframe_type");
	frnum = anyfr ? ~0u :
	        studio->get_spin("shinfo_objpaperdoll_wframe");
	if (shape)
		*shape = studio->get_spin("shinfo_objpaperdoll_shape");
	if (frame0)
		*frame0 = studio->get_spin("shinfo_objpaperdoll_frame0");
	if (frame1)
		*frame1 = fron ? studio->get_spin("shinfo_objpaperdoll_frame1") : -1;
	if (frame2)
		*frame2 = fron ? studio->get_spin("shinfo_objpaperdoll_frame2") : -1;
	if (frame3)
		*frame3 = armor ? studio->get_spin("shinfo_objpaperdoll_frame3") : -1;
	if (type)
		*type = max > 0 ? studio->get_spin("shinfo_objpaperdoll_spotframe") : 0;
	if (trans)
		*trans = studio->get_toggle("shinfo_objpaperdoll_trans");
	if (gender)
		*gender = studio->get_toggle("shinfo_objpaperdoll_gender");
}

static void Set_objpaperdoll_fields(
    int frnum = 0, int spot = 0,
    bool trans = false, bool gender = false, short type = 0,
    int shape = 0,
    int frame0 = 0, int frame1 = 0, int frame2 = 0, int frame3 = 0
) {
	ExultStudio *studio = ExultStudio::get_instance();
	const int ready = studio->get_optmenu("shinfo_ready_spot");
	bool fron = ready == 6 || ready == 7 || ready == 8 || ready == 9;
	const int altspot = studio->get_optmenu("shinfo_altready1_spot");
	const bool armor = ready == 12 && altspot != 3;
	if (armor) fron = true;
	int max;
	if (spot == 0 && ready == 11) // helmet
		max = 1;
	else if (spot == 3 && ready == 2)
		max = 2;
	else
		max = 0;
	studio->set_sensitive("shinfo_objpaperdoll_spotframe", max > 0);
	if (type > max)
		type = 0;
	studio->set_toggle("shinfo_objpaperdoll_wframe_type", frnum == -1);
	studio->set_spin("shinfo_objpaperdoll_wframe",
	                 frnum == -1 ? 0 : frnum, frnum != -1);
	studio->set_optmenu("shinfo_objpaperdoll_spot", spot == 102 ? 18 : spot);
	studio->set_toggle("shinfo_objpaperdoll_trans", trans);
	studio->set_toggle("shinfo_objpaperdoll_gender", gender);
	studio->set_spin("shinfo_objpaperdoll_spotframe", type, 0, max);
	studio->set_spin("shinfo_objpaperdoll_shape", shape);
	studio->set_spin("shinfo_objpaperdoll_frame0", frame0);
	studio->set_spin("shinfo_objpaperdoll_frame1",
	                 fron ? frame1 : 0, fron);
	studio->set_spin("shinfo_objpaperdoll_frame2",
	                 fron ? frame2 : 0, fron);
	studio->set_spin("shinfo_objpaperdoll_frame3",
	                 armor ? frame3 : 0, armor);
}

static inline void Set_objpaperdoll_sensitivity(
    ExultStudio *studio,
    int spot,
    int ready
) {
	int max;
	if (spot == 0 && ready == 11) // helmet
		max = 1;
	else if (spot == 3 && ready == 2)
		max = 2;
	else
		max = 0;
	studio->set_spin("shinfo_objpaperdoll_spotframe", 0, max);
	studio->set_sensitive("shinfo_objpaperdoll_spotframe", max > 0);
	bool fron = ready == 6 || ready == 7 || ready == 8 || ready == 9;
	const int altspot = studio->get_optmenu("shinfo_altready1_spot");
	const bool armor = ready == 12 && altspot != 3;
	if (armor) fron = true;
	studio->set_sensitive("shinfo_objpaperdoll_frame1", fron);
	studio->set_sensitive("shinfo_objpaperdoll_frame2", fron);
	studio->set_sensitive("shinfo_objpaperdoll_frame3", armor);
}

/*
 *  Paperdoll selection changed.
 */
C_EXPORT void on_shinfo_objpaperdoll_list_cursor_changed(
    GtkTreeView *treeview,
    gpointer     user_data
) {
	ignore_unused_variable_warning(user_data);
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *col;
	gtk_tree_view_get_cursor(treeview, &path, &col);
	if ((!col) || (!path))
		return;

	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int frnum;
	int spot;
	int trans;
	int gender;
	int type;
	int shape;
	int frame0;
	int frame1;
	int frame2;
	int frame3;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(model, &iter, DOLL_WORLD_FRAME, &frnum,
	                   DOLL_SPOT, &spot,
	                   DOLL_TRANSLUCENT, &trans,
	                   DOLL_GENDER_BASED, &gender,
	                   DOLL_SPOT_TYPE, &type,
	                   DOLL_SHAPE, &shape,
	                   DOLL_FRAME_0, &frame0,
	                   DOLL_FRAME_1, &frame1,
	                   DOLL_FRAME_2, &frame2,
	                   DOLL_FRAME_3, &frame3, -1);
	Set_objpaperdoll_fields(frnum, spot, trans, gender, type, shape,
	                        frame0, frame1, frame2, frame3);
}

/*
 *  Paperdoll world frame type changed.
 */
C_EXPORT gboolean on_shinfo_objpaperdoll_frame_type_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio *studio = ExultStudio::get_instance();
	studio->set_sensitive("shinfo_objpaperdoll_wframe", on);
	return TRUE;
}

/*
 *  Adding paperdoll information.
 */
C_EXPORT void on_shinfo_objpaperdoll_update_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	unsigned int newfrnum;
	int newspot;
	int newshape;
	int newframe0;
	int newframe1;
	int newframe2;
	int newframe3;
	int newtype;
	bool newtrans;
	bool newgender;
	Get_objpaperdoll_fields(studio, newfrnum, newspot, &newshape, &newframe0,
	                        &newframe1, &newframe2, &newframe3, &newtype, &newtrans, &newgender);

	GtkTreeView *dolltree = GTK_TREE_VIEW(
	                            studio->get_widget("shinfo_objpaperdoll_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(dolltree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	const int cmp = Find_binary_iter(model, iter, newfrnum, newspot);
	if (cmp) {
		GtkTreeIter newiter;
		GdkPixbuf *nshape = studio->shape_image(
		    static_cast<Shape_file_info *>(
		        g_object_get_data(G_OBJECT(studio->get_widget("shape_window")), "file_info"))->get_ifile(),
		    studio->get_num_entry("shinfo_shape"), static_cast<int>(newfrnum), true);
		if (cmp > 0)
			gtk_tree_store_insert_after(store, &newiter, nullptr, iter.get());
		else
			gtk_tree_store_insert_before(store, &newiter, nullptr, iter.get());
		gtk_tree_store_set(store, &newiter,
		                   DOLL_WORLD_FRAME, static_cast<int>(newfrnum), DOLL_SPOT, newspot,
		                   DOLL_TRANSLUCENT, newtrans, DOLL_GENDER_BASED, newgender,
		                   DOLL_SPOT_TYPE, newtype, DOLL_SHAPE, newshape,
		                   DOLL_FRAME_0, newframe0, DOLL_FRAME_1, newframe1,
		                   DOLL_FRAME_2, newframe2, DOLL_FRAME_3, newframe3,
		                   DOLL_FROM_PATCH, 1, DOLL_MODIFIED, 1,
		                   DOLL_SHAPE_IMAGE, nshape, -1);
		if (nshape) {
			g_object_unref(nshape);
		}
	} else {
		int trans;
		int gender;
		int type;
		int shape;
		int frame0;
		int frame1;
		int frame2;
		int frame3;
		int patch;
		int modded;
		gtk_tree_model_get(model, iter.get(), DOLL_TRANSLUCENT, &trans,
		                   DOLL_GENDER_BASED, &gender, DOLL_SPOT_TYPE, &type,
		                   DOLL_SHAPE, &shape, DOLL_FRAME_0, &frame0,
		                   DOLL_FRAME_1, &frame1, DOLL_FRAME_2, &frame2,
		                   DOLL_FRAME_3, &frame3, DOLL_FROM_PATCH, &patch,
		                   DOLL_MODIFIED, &modded, -1);
		if ((trans != newtrans) || (gender != newgender)
		        || (type != newtype) || (shape != newshape)
		        || (frame0 != newframe0) || (frame1 != newframe1)
		        || (frame2 != newframe2) || (frame3 != newframe3))
			gtk_tree_store_set(store, iter.get(),
			                   DOLL_TRANSLUCENT, newtrans, DOLL_GENDER_BASED, newgender,
			                   DOLL_SPOT_TYPE, newtype, DOLL_SHAPE, newshape,
			                   DOLL_FRAME_0, newframe0, DOLL_FRAME_1, newframe1,
			                   DOLL_FRAME_2, newframe2, DOLL_FRAME_3, newframe3,
			                   DOLL_MODIFIED, 1, -1);
	}
}

/*
 *  Deleting object paperdoll information.
 */
C_EXPORT void on_shinfo_objpaperdoll_remove_clicked(
    GtkButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(btn, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	GtkTreeView *hptree = GTK_TREE_VIEW(
	                          studio->get_widget("shinfo_objpaperdoll_list"));
	GtkTreeModel *model = gtk_tree_view_get_model(hptree);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	unsigned int newfrnum;
	int newspot;
	Get_objpaperdoll_fields(studio, newfrnum, newspot);
	UniqueTreeIter iter(nullptr, TreeIterDeleter);
	if (!Find_binary_iter(model, iter, newfrnum, newspot))
		gtk_tree_store_remove(store, iter.get());
}

/*
 *  Paperdoll spot changed.
 */
C_EXPORT gboolean on_shinfo_objpaperdoll_spot_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int spot = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	const int ready = studio->get_optmenu("shinfo_ready_spot");
	Set_objpaperdoll_sensitivity(studio, spot, ready);
	return TRUE;
}

/*
 *  Ready spot changed.
 */
C_EXPORT gboolean on_shinfo_ready_spot_changed(
    GtkWidget *widget,
    gpointer data
) {
	ignore_unused_variable_warning(data);
	const int ready = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	ExultStudio *studio = ExultStudio::get_instance();
	const int spot = studio->get_optmenu("shinfo_objpaperdoll_spot");
	Set_objpaperdoll_sensitivity(studio, spot, ready);
	return TRUE;
}

/*
 *  Special-shapes toggles:
 */
C_EXPORT void on_shinfo_weapon_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_weapon_box", on);
}
C_EXPORT void on_shinfo_ammo_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_ammo_box", on);
}
C_EXPORT void on_shinfo_armor_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_armor_box", on);
}
C_EXPORT void on_shinfo_monster_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_monster_box", on);
}
C_EXPORT void on_shinfo_container_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_container_box", on);
}
C_EXPORT void on_shinfo_mountaintop_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_mountaintop_box", on);
}
C_EXPORT void on_shinfo_bargetype_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_barge_box", on);
}
C_EXPORT void on_shinfo_fieldinfo_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_fieldinfo_box", on);
}
C_EXPORT void on_shinfo_barge_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_sensitive("shinfo_barge_type", on);
}
C_EXPORT void on_shinfo_field_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_sensitive("shinfo_fieldinfo_type", on);
}
C_EXPORT void on_shinfo_body_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_coffin", on);
}
C_EXPORT void on_shinfo_animation_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_animation_box", on);
}
C_EXPORT void on_shinfo_explosion_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_explosion_box", on);
}
C_EXPORT void on_shinfo_effhps_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_effhps_box", on);
}
C_EXPORT void on_shinfo_npcflags_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_npcflags_box", on);
}
C_EXPORT void on_shinfo_sound_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_sfx_box", on);
}
C_EXPORT void on_shinfo_brightness_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_brightness_box", on);
}
C_EXPORT void on_shinfo_warmth_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_warmth_box", on);
}
C_EXPORT void on_shinfo_cntrules_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_cntrules_box", on);
}
C_EXPORT void on_shinfo_framenames_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_framenames_box", on);
}
C_EXPORT void on_shinfo_frameflags_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_frameflags_box", on);
}
C_EXPORT void on_shinfo_frameusecode_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_frameusecode_box", on);
}
C_EXPORT void on_shinfo_npcpaperdoll_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_npcpaperdoll_box", on);
}
C_EXPORT void on_shinfo_objpaperdoll_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_visible("shinfo_objpaperdoll_box", on);
}
C_EXPORT void on_shinfo_ammo_special_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_sensitive("shinfo_ammo_drop", !on);
}
C_EXPORT void on_shinfo_single_sfx_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_sensitive("shinfo_sfx_type", on);
	ExultStudio::get_instance()->set_sensitive("shinfo_sfx_count", on);
}
C_EXPORT void on_shinfo_sfx_clock_check_toggled(
    GtkToggleButton *btn,
    gpointer user_data
) {
	ignore_unused_variable_warning(user_data);
	const bool on = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
	ExultStudio::get_instance()->set_sensitive("shinfo_sfx_clock_sfx", on);
}

/*
 *  Browse for usecode.
 */
C_EXPORT void on_shinfo_weapon_uc_browse_clicked(
    GtkButton *button,
    gpointer         user_data
) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio *studio = ExultStudio::get_instance();
	const char *uc = studio->browse_usecode(true);
	if (*uc)
		studio->set_entry("shinfo_weapon_uc", uc, true);
}

/*
 *  Set frame-dependent fields in the shape-editing notebook.
 */

void ExultStudio::set_shape_notebook_frame(
    int frnum           // Frame # to set.
) {
	auto *file_info = static_cast<Shape_file_info *>(
	                      g_object_get_data(G_OBJECT(shapewin), "file_info"));
	const int shnum = get_num_entry("shinfo_shape");
	Vga_file *ifile = file_info->get_ifile();
	Shape_frame *shape = ifile->get_shape(shnum, frnum);
	if (shape == nullptr) {
		set_spin("shinfo_originx", 0, 0, 0);
		set_spin("shinfo_originy", 0, 0, 0);
	} else {
		set_spin("shinfo_originx", shape->get_xright(), -shape->get_width(),
		         shape->get_width());
		set_spin("shinfo_originy", shape->get_ybelow(), -shape->get_height(),
		         shape->get_height());
	}

	const auto *info = static_cast<const Shape_info *>(
	                       g_object_get_data(G_OBJECT(shapewin), "user_data"));
	if (!info)
		return;
	set_spin("shinfo_xtiles", info->get_3d_xtiles(frnum));
	set_spin("shinfo_ytiles", info->get_3d_ytiles(frnum));
	unsigned char wx;
	unsigned char wy;       // Weapon-in-hand offset.
	info->get_weapon_offset(frnum, wx, wy);
	set_spin("shinfo_wihx", wx, 0, 255);    // Negative???
	set_spin("shinfo_wihy", wy, 0, 255);
}

inline short get_spots(short spot) {
	switch (spot) {
	case rhand:
		return 0;
	case lhand:
		return 1;
	case both_hands:
		return 2;
	case amulet:
		return 3;
	case cloak:
		return 4;
	case neck:
		return 5;
	case lfinger:
		return 6;
	case rfinger:
		return 6;
	case gloves:
		return 7;
	case lrgloves:
		return 8;
	case quiver:
		return 9;
	case earrings:
		return 10;
	case head:
		return 11;
	case torso:
		return 12;
	case backpack:
		return 13;
	case belt:
		return 14;
	case legs:
		return 15;
	case feet:
		return 16;
	case ucont:
		return 17;
	case triple_bolts:
		return 18;
	default:
		return 0;
	}
}

inline short get_alt_spots(short spot) {
	switch (spot) {
	case rhand:
		return 1;
	case lhand:
		return 2;
	case neck:
		return 3;
	case belt:
		return 4;
	case back_2h:
		return 5;
	case back_shield:
		return 6;
	case scabbard:
		return 7;
	case backpack:
		return 8;
	default:
		return 0;
	}
}

/*
 *  Fill in the shape-editing notebook.
 */

void ExultStudio::init_shape_notebook(
    const Shape_info &info,
    GtkWidget *book,        // The notebook.
    int shnum,          // Shape #.
    int frnum           // Frame #.
) {
	auto *shpfile = static_cast<Shape_file_info *>(
		g_object_get_data(G_OBJECT(shapewin), "file_info"))->get_ifile();
	static const int classes[] = {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0,
	                              9, 10, 11, 12, 0
	                             };
	const int numclasses = array_size(classes);
	const int shclass = static_cast<int>(info.get_shape_class());
	set_optmenu("shinfo_shape_class", shclass < numclasses ?
	            classes[shclass] : 0);
	set_shape_notebook_frame(frnum);
	set_spin("shinfo_ztiles", info.get_3d_height());
	int spot = get_spots(info.get_ready_type());
	set_optmenu("shinfo_ready_spot", spot);
	set_toggle("shinfo_is_spell_check", info.is_spell());
	// New Exult stuff:
	spot = get_alt_spots(info.get_alt_ready1());
	set_optmenu("shinfo_altready1_spot", spot);
	spot = get_alt_spots(info.get_alt_ready2());
	set_optmenu("shinfo_altready2_spot", spot);

	set_spin("shinfo_weight", info.get_weight(), 0, 255);
	set_spin("shinfo_volume", info.get_volume(), 0, 255);
	// Bunch of flags:
	set_toggle("shinfo_sfx_check", info.has_sfx());
	set_toggle("shinfo_strange_check", info.has_strange_movement());
	set_toggle("shinfo_animated_check", info.is_animated());
	set_toggle("shinfo_solid_check", info.is_solid());
	set_toggle("shinfo_water_check", info.is_water());
	// ++++Set poison for flats, field for non-flats.
	set_toggle("shinfo_poison_check", info.is_poisonous());
	set_toggle("shinfo_field_check", info.is_field());
	set_toggle("shinfo_door_check", info.is_door());
	set_toggle("shinfo_barge_check", info.is_barge_part());
	set_toggle("shinfo_transp_check", info.is_transparent());
	set_toggle("shinfo_light_check", info.is_light_source());
	set_toggle("shinfo_transl_check", info.has_translucency());
	set_toggle("shinfo_obstaclex_check", info.is_xobstacle());
	set_toggle("shinfo_obstacley_check", info.is_yobstacle());
	set_toggle("shinfo_occludes_check", info.occludes());
	// More flags, originally hard-coded:
	static const char *shpflags[] = {
		"shinfo_shape_flag0",
		"shinfo_shape_flag1",
		"shinfo_shape_flag2",
		"shinfo_shape_flag3",
		"shinfo_shape_flag4",
		"shinfo_shape_flag5",
		"shinfo_shape_flag6",
		"shinfo_shape_flag7",
		"shinfo_shape_flag8",
		"shinfo_shape_flag9"
	};
	set_bit_toggles(&shpflags[0],
	                array_size(shpflags), info.get_shape_flags());
	// Extras.
	const Weapon_info *winfo = info.get_weapon_info();
	set_toggle("shinfo_weapon_check", winfo != nullptr);
	set_visible("shinfo_weapon_box", winfo != nullptr);
	if (winfo) {        // Setup weapon page.
		set_spin("shinfo_weapon_damage", winfo->get_damage());
		set_spin("shinfo_weapon_range", winfo->get_range());
		set_optmenu("shinfo_weapon_type", winfo->get_damage_type());
		const int ammo = winfo->get_ammo_consumed();
		const int ammo_use = ammo >= 0 ? 0 : -ammo;
		set_optmenu("shinfo_weapon_ammo", ammo_use);
		set_spin("shinfo_weapon_ammo_shape", ammo >= 0 ? ammo : 0, ammo >= 0);
		const int proj = winfo->get_projectile();
		const int proj_type = proj >= 0 ? 0 : (proj == -1 ? 1 : 2);
		set_optmenu("shinfo_weapon_sprite", proj_type);
		set_spin("shinfo_weapon_proj", proj >= 0 ? proj : 0, proj >= 0);
		set_optmenu("shinfo_weapon_uses", winfo->get_uses());
		set_spin("shinfo_weapon_sfx", winfo->get_sfx(), 0, 255);
		set_spin("shinfo_weapon_hitsfx", winfo->get_hitsfx(), 0, 255);
		// Show usecode in hex.
		set_entry("shinfo_weapon_uc", winfo->get_usecode(), true);
		static const char *powers[] = {
			"shinfo_weapon_pow0",
			"shinfo_weapon_pow1",
			"shinfo_weapon_pow2",
			"shinfo_weapon_pow3",
			"shinfo_weapon_pow4",
			"shinfo_weapon_pow5",
			"shinfo_weapon_pow6",
			"shinfo_weapon_pow7"
		};
		set_bit_toggles(&powers[0],
		                array_size(powers), winfo->get_powers());
		set_toggle("shinfo_weapon_flag0", winfo->lucky());
		set_toggle("shinfo_weapon_flag1", winfo->explodes());
		set_toggle("shinfo_weapon_flag2", winfo->no_blocking());
		set_toggle("shinfo_weapon_flag3", winfo->delete_depleted());
		set_toggle("shinfo_weapon_always", winfo->autohits());
		set_toggle("shinfo_weapon_target", winfo->needs_target(), false);
		set_toggle("shinfo_weapon_returns", winfo->returns());
		set_optmenu("shinfo_weapon_frames", winfo->get_actor_frames(false));
		set_optmenu("shinfo_proj_frames", winfo->get_actor_frames(true));
		set_spin("shinfo_proj_rotation", winfo->get_rotation_speed());
		set_optmenu("shinfo_proj_speed", winfo->get_missile_speed() - 1);
	}
	const Ammo_info *ainfo = info.get_ammo_info();
	set_toggle("shinfo_ammo_check", ainfo != nullptr);
	set_visible("shinfo_ammo_box", ainfo != nullptr);
	if (ainfo) {        // Setup ammo page.
		set_spin("shinfo_ammo_damage", ainfo->get_damage());
		set_spin("shinfo_ammo_family", ainfo->get_family_shape());
		const int proj = ainfo->get_sprite_shape();
		const int proj_type = proj >= 0 ? 0 : (proj == -1 ? 1 : 2);
		set_optmenu("shinfo_ammo_sprite", proj_type);
		set_spin("shinfo_ammo_proj", proj >= 0 ? proj : 0, proj >= 0);
		set_optmenu("shinfo_ammo_type", ainfo->get_damage_type());
		static const char *powers[] = {
			"shinfo_ammo_pow0",
			"shinfo_ammo_pow1",
			"shinfo_ammo_pow2",
			"shinfo_ammo_pow3",
			"shinfo_ammo_pow4",
			"shinfo_ammo_pow5",
			"shinfo_ammo_pow6",
			"shinfo_ammo_pow7"
		};
		set_bit_toggles(&powers[0],
		                array_size(powers), ainfo->get_powers());
		// 'Explode'???
		set_toggle("shinfo_ammo_flag0", ainfo->lucky());
		set_toggle("shinfo_ammo_flag1", ainfo->autohits());
		set_toggle("shinfo_ammo_flag2", ainfo->returns());
		set_toggle("shinfo_ammo_flag3", ainfo->no_blocking());
		set_toggle("shinfo_ammo_flag6", ainfo->explodes());
		set_toggle("shinfo_ammo_special", ainfo->is_homing());
		set_optmenu("shinfo_ammo_drop", ainfo->get_drop_type(), !ainfo->is_homing());
	}
	const Armor_info *arinfo = info.get_armor_info();
	set_toggle("shinfo_armor_check", arinfo != nullptr);
	set_visible("shinfo_armor_box", arinfo != nullptr);
	if (arinfo) {       // Setup armor page.
		static const char *immun[] = {"shinfo_armor_immun0",
		                              "shinfo_armor_immun1",
		                              "shinfo_armor_immun2",
		                              "shinfo_armor_immun3",
		                              "shinfo_armor_immun4",
		                              "shinfo_armor_immun5",
		                              "shinfo_armor_immun6",
		                              "shinfo_armor_immun7"
		                             };
		set_spin("shinfo_armor_value", arinfo->get_prot());
		set_bit_toggles(&immun[0],
		                array_size(immun), arinfo->get_immune());
	}
	const unsigned char aflags = info.get_actor_flags();
	set_toggle("shinfo_npcflags_check", aflags != 0);
	set_visible("shinfo_npcflags_box", aflags != 0);
	if (aflags) {
		static const char *flags[] = {  "shinfo_actor_flag0",
		                                "shinfo_actor_flag1",
		                                "shinfo_actor_flag2",
		                                "shinfo_actor_flag3",
		                                "shinfo_actor_flag4",
		                                "shinfo_actor_flag5",
		                                "shinfo_actor_flag6",
		                                "shinfo_actor_flag7"
		                             };
		set_bit_toggles(&flags[0], array_size(flags), aflags);
	}
	const Monster_info *minfo = info.get_monster_info();
	set_toggle("shinfo_monster_check", minfo != nullptr);
	set_visible("shinfo_monster_box", minfo != nullptr);
	if (minfo) {        // Setup monster page.
		set_spin("shinfo_monster_str", minfo->get_strength());
		set_spin("shinfo_monster_dex", minfo->get_dexterity());
		set_spin("shinfo_monster_intel", minfo->get_intelligence());
		set_spin("shinfo_monster_cmb", minfo->get_combat());
		set_spin("shinfo_monster_armor", minfo->get_armor());
		set_spin("shinfo_monster_wpn", minfo->get_weapon());
		set_spin("shinfo_monster_reach", minfo->get_reach());
		set_spin("shinfo_monster_equip", minfo->get_equip_offset());
		set_spin("shinfo_monster_sfx", minfo->get_hitsfx());
		set_spin("shinfo_monster_food", info.get_monster_food());
		set_optmenu("shinfo_monster_align", minfo->get_alignment());
		set_optmenu("shinfo_monster_attackmode", minfo->get_attackmode());
		static const char *vuln[] = {   "shinfo_monster_vuln0",
		                                "shinfo_monster_vuln1",
		                                "shinfo_monster_vuln2",
		                                "shinfo_monster_vuln3",
		                                "shinfo_monster_vuln4",
		                                "shinfo_monster_vuln5",
		                                "shinfo_monster_vuln6",
		                                "shinfo_monster_vuln7"
		                            };
		static const char *immun[] = {"shinfo_monster_immun0",
		                              "shinfo_monster_immun1",
		                              "shinfo_monster_immun2",
		                              "shinfo_monster_immun3",
		                              "shinfo_monster_immun4",
		                              "shinfo_monster_immun5",
		                              "shinfo_monster_immun6",
		                              "shinfo_monster_immun7"
		                             };
		set_bit_toggles(&vuln[0], array_size(vuln),
		                minfo->get_vulnerable());
		set_bit_toggles(&immun[0],
		                array_size(immun), minfo->get_immune());
		set_toggle("shinfo_monster_splits", minfo->splits());
		set_toggle("shinfo_monster_cant_yell", minfo->cant_yell());
		set_toggle("shinfo_monster_cant_bleed", minfo->cant_bleed());
		set_toggle("shinfo_monster_sleep_safe", minfo->sleep_safe());
		set_toggle("shinfo_monster_charm_safe", minfo->charm_safe());
		set_toggle("shinfo_monster_curse_safe", minfo->curse_safe());
		set_toggle("shinfo_monster_paralysis_safe", minfo->paralysis_safe());
		set_toggle("shinfo_monster_poison_safe", minfo->poison_safe());
		set_toggle("shinfo_monster_death_safe", minfo->death_safe());
		set_toggle("shinfo_monster_power_safe", minfo->power_safe());
		set_toggle("shinfo_monster_cant_die", minfo->cant_die());
		static const char *flags[] = {"shinfo_monster_flag0",
		                              "shinfo_monster_flag1",
		                              "shinfo_monster_flag2",
		                              "shinfo_monster_flag3",
		                              "shinfo_monster_flag4",
		                              "shinfo_monster_flag5",
		                              "shinfo_monster_flag6",
		                              "shinfo_monster_flag7"
		                             };
		set_bit_toggles(&flags[0],
		                array_size(flags), minfo->get_flags());
	}
	const int gump_shape = info.get_gump_shape();
	const int gump_font = info.get_gump_font();
	set_toggle("shinfo_container_check", gump_shape >= 0);
	set_visible("shinfo_container_box", gump_shape >= 0);
	set_spin("shinfo_gump_num", gump_shape >= 0 ? gump_shape : 0);
	set_spin("shinfo_gump_font", gump_font);

	int mountain_top = info.get_mountain_top_type();
	if (mountain_top > 2 || mountain_top < 0)
		mountain_top = 0;
	set_toggle("shinfo_mountaintop_check", mountain_top != 0);
	set_visible("shinfo_mountaintop_box", mountain_top != 0);
	const bool top_on = get_optmenu("shinfo_shape_class") == 12;
	set_optmenu("shinfo_mountaintop_type", top_on ? mountain_top : 0,
	            mountain_top != 0 || top_on);

	int barge_type = info.get_barge_type();
	if (barge_type > 6 || barge_type < 0)
		barge_type = 0;
	set_toggle("shinfo_bargetype_check", barge_type != 0);
	set_visible("shinfo_barge_box", barge_type != 0);
	const bool barge_on = get_toggle("shinfo_barge_check");
	set_optmenu("shinfo_barge_type", barge_type, barge_type != 0 || barge_on);

	int field_type = info.get_field_type();
	if (field_type > 4 || field_type < 0)
		field_type = -1;
	const bool field_on = get_toggle("shinfo_field_check");
	set_toggle("shinfo_fieldinfo_check", field_type >= 0);
	set_visible("shinfo_fieldinfo_box", field_type >= 0 || field_on);
	set_optmenu("shinfo_fieldinfo_type", field_type + 1, field_type >= 0 || field_on);

	const bool hasbody = info.has_body_info();
	set_toggle("shinfo_body_check", hasbody);
	set_visible("shinfo_coffin", hasbody);
	if (hasbody) {
		const int shnum = info.get_body_shape();
		set_spin("shinfo_body_shape", shnum);
		set_spin("shinfo_body_frame", info.get_body_frame(),
		         0, vgafile->get_ifile()->get_num_frames(shnum) - 1);
	}

	const bool hasexp = info.has_explosion_info();
	set_toggle("shinfo_explosion_check", hasexp);
	set_visible("shinfo_explosion_box", hasexp);
	if (hasexp) {
		set_spin("shinfo_explosion_sprite", info.get_explosion_sprite());
		const int sfxnum = info.get_explosion_sfx();
		if (sfxnum < 0) {
			set_toggle("shinfo_explosion_sfx_type", true);
			set_spin("shinfo_explosion_sfx_number", 0, false);
		} else {
			set_toggle("shinfo_explosion_sfx_type", false);
			set_spin("shinfo_explosion_sfx_number", sfxnum, true);
		}
	}

	const SFX_info *sfxinf = info.get_sfx_info();
	set_toggle("shinfo_sound_check", sfxinf != nullptr);
	set_visible("shinfo_sfx_box", sfxinf != nullptr);
	if (sfxinf) {
		const int range = sfxinf->get_sfx_range();
		set_toggle("shinfo_single_sfx", range == 1);
		set_spin("shinfo_sfx_first", sfxinf->get_sfx());
		set_spin("shinfo_sfx_chance", sfxinf->get_chance());
		set_optmenu("shinfo_sfx_type", sfxinf->play_randomly(), range > 1);
		set_spin("shinfo_sfx_count", range, range > 1);
		const bool hourly = sfxinf->play_horly_ticks();
		set_toggle("shinfo_sfx_clock_check", hourly);
		set_spin("shinfo_sfx_clock_sfx", sfxinf->get_extra_sfx(), hourly);
	}

	const Paperdoll_npc *npcinf = info.get_npc_paperdoll();
	set_toggle("shinfo_npcpaperdoll_check", npcinf != nullptr);
	set_visible("shinfo_npcpaperdoll_box", npcinf != nullptr);
	if (npcinf) {
		set_toggle("shinfo_npcpaperdoll_female", npcinf->is_npc_female());
		set_toggle("shinfo_npcpaperdoll_trans", npcinf->is_translucent());
		set_spin("shinfo_npcpaperdoll_bshape", npcinf->get_body_shape());
		set_spin("shinfo_npcpaperdoll_bframe", npcinf->get_body_frame());
		set_spin("shinfo_npcpaperdoll_hshape", npcinf->get_head_shape());
		set_spin("shinfo_npcpaperdoll_hframe", npcinf->get_head_frame());
		set_spin("shinfo_npcpaperdoll_hhelm", npcinf->get_head_frame_helm());
		set_spin("shinfo_npcpaperdoll_ashape", npcinf->get_arms_shape());
		static const char * const arm_names[] = {"shinfo_npcpaperdoll_aframe",
		                                  "shinfo_npcpaperdoll_atwohanded",
		                                  "shinfo_npcpaperdoll_astaff"
		                                 };
		for (size_t i = 0; i < array_size(arm_names); i++)
			set_spin(arm_names[i], npcinf->get_arms_frame(i));
	}

	const Animation_info *aniinf = info.get_animation_info();
	set_toggle("shinfo_animation_check", aniinf != nullptr);
	set_visible("shinfo_animation_box", aniinf != nullptr);
	if (aniinf) {
		const Animation_info::AniType type = aniinf->get_type();
		bool on = (type != Animation_info::FA_NON_LOOPING);
		set_optmenu("shinfo_animation_type", static_cast<int>(type));
		Vga_file *ifile = curfile->get_ifile();
		const int nframes = ifile->get_num_frames(shnum);
		const int count = aniinf->get_frame_count();
		if (count == nframes || count < 0) {
			set_toggle("shinfo_animation_frtype", true);
			set_spin("shinfo_animation_frcount", nframes, false);
		} else {
			set_toggle("shinfo_animation_frtype", false);
			set_spin("shinfo_animation_frcount", nframes, true);
		}
		set_spin("shinfo_animation_ticks", aniinf->get_frame_delay(), on);
		int sfxdelay = aniinf->get_sfx_delay();
		set_toggle("shinfo_animation_sfxsynch", sfxdelay < 0, on);
		if (sfxdelay == 0)
			sfxdelay = 1;
		set_spin("shinfo_animation_sfxdelay", sfxdelay < 0 ? 1 : sfxdelay,
		         on && sfxdelay > 0);
		on = (type == Animation_info::FA_LOOPING);
		const int chance = aniinf->get_freeze_first_chance();
		set_optmenu("shinfo_animation_freezefirst", chance == 100 ? 0 :
		            (chance == 0 ? 1 : 2), on);
		const bool usespin = (chance > 0) && (chance < 100);
		set_spin("shinfo_animation_freezechance",
		         usespin ? chance : 1, on && usespin);
		const int rec = aniinf->get_recycle();
		set_toggle("shinfo_animation_rectype", rec == 0, on);
		set_spin("shinfo_animation_recycle", rec ? rec : nframes, on && rec);
	}

	const std::vector<Effective_hp_info> &hpinf = info.get_effective_hp_info();
	set_toggle("shinfo_effhps_check", !hpinf.empty());
	set_visible("shinfo_effhps_box", !hpinf.empty());
	if (!hpinf.empty()) {
		GtkTreeView *hptree = GTK_TREE_VIEW(
		                          get_widget("shinfo_effhps_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(hptree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Effective_hp_info *first = nullptr;
		for (const auto &hps : hpinf) {
			if (hps.is_invalid())
				continue;
			if (!first)
				first = &hps;
			GdkPixbuf *nshape = shape_image(shpfile, shnum, hps.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter, HP_FRAME_COLUMN, hps.get_frame(),
			                   HP_QUALITY_COLUMN, hps.get_quality(),
			                   HP_HIT_POINTS, hps.get_hps(),
			                   HP_FROM_PATCH, hps.from_patch(),
			                   HP_MODIFIED, hps.was_modified(),
			                   HP_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(hptree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(hptree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first)
			Set_hp_fields(first->get_frame(), first->get_quality(),
			              first->get_hps());
		else
			Set_hp_fields();
	} else
		Set_hp_fields();

	const std::vector<Light_info> &lightinf = info.get_light_info();
	set_toggle("shinfo_brightness_check", !lightinf.empty());
	set_visible("shinfo_brightness_box", !lightinf.empty());
	if (!lightinf.empty()) {
		GtkTreeView *brightnesstree = GTK_TREE_VIEW(
		                                  get_widget("shinfo_brightness_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(brightnesstree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Light_info *first = nullptr;
		for (const auto &light : lightinf) {
			if (light.is_invalid())
				continue;
			if (!first)
				first = &light;
			GdkPixbuf *nshape = shape_image(shpfile, shnum, light.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter, BRIGHTNESS_FRAME_COLUMN, light.get_frame(),
			                   BRIGHTNESS_VALUE_COLUMN, light.get_light(),
			                   BRIGHTNESS_FROM_PATCH, light.from_patch(),
			                   BRIGHTNESS_MODIFIED, light.was_modified(),
			                   BRIGHTNESS_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(brightnesstree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(brightnesstree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first)
			Set_brightness_fields(first->get_frame(), first->get_light());
		else
			Set_brightness_fields();
	} else
		Set_brightness_fields();

	const std::vector<Warmth_info> &warminf = info.get_warmth_info();
	set_toggle("shinfo_warmth_check", !warminf.empty());
	set_visible("shinfo_warmth_box", !warminf.empty());
	if (!warminf.empty()) {
		GtkTreeView *warmtree = GTK_TREE_VIEW(
		                            get_widget("shinfo_warmth_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(warmtree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Warmth_info *first = nullptr;
		for (const auto &warm : warminf) {
			if (warm.is_invalid())
				continue;
			if (!first)
				first = &warm;
			GdkPixbuf *nshape = shape_image(shpfile, shnum, warm.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter, WARM_FRAME_COLUMN, warm.get_frame(),
			                   WARM_VALUE_COLUMN, warm.get_warmth(),
			                   WARM_FROM_PATCH, warm.from_patch(),
			                   WARM_MODIFIED, warm.was_modified(),
			                   WARM_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(warmtree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(warmtree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first)
			Set_warmth_fields(first->get_frame(), first->get_warmth());
		else
			Set_warmth_fields();
	} else
		Set_warmth_fields();

	const std::vector<Content_rules> &cntinf = info.get_content_rules();
	set_toggle("shinfo_cntrules_check", !cntinf.empty());
	set_visible("shinfo_cntrules_box", !cntinf.empty());
	if (!cntinf.empty()) {
		GtkTreeView *cnttree = GTK_TREE_VIEW(
		                           get_widget("shinfo_cntrules_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(cnttree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Content_rules *first = nullptr;
		for (const auto &cntr : cntinf) {
			if (cntr.is_invalid())
				continue;
			if (!first)
				first = &cntr;
			GdkPixbuf *nshape = shape_image(shpfile, cntr.get_shape(), 0, true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter, CNT_SHAPE_COLUMN, cntr.get_shape(),
			                   CNT_ACCEPT_COLUMN, cntr.accepts_shape(),
			                   CNT_FROM_PATCH, cntr.from_patch(),
			                   CNT_MODIFIED, cntr.was_modified(),
			                   CNT_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(cnttree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(cnttree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first)
			Set_cntrules_fields(first->get_shape(), first->accepts_shape());
		else
			Set_cntrules_fields();
	} else
		Set_cntrules_fields();

	const std::vector<Frame_flags_info> &frflaginf = info.get_frame_flags();
	set_toggle("shinfo_frameflags_check", !frflaginf.empty());
	set_visible("shinfo_frameflags_box", !frflaginf.empty());
	if (!frflaginf.empty()) {
		GtkTreeView *flagtree = GTK_TREE_VIEW(
		                            get_widget("shinfo_frameflags_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(flagtree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Frame_flags_info *first = nullptr;
		for (const auto &frflag : frflaginf) {
			if (frflag.is_invalid())
				continue;
			if (!first)
				first = &frflag;
			GdkPixbuf *nshape = shape_image(shpfile, shnum, frflag.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter,
			                   FRFLAG_FRAME_COLUMN, frflag.get_frame(),
			                   FRFLAG_QUAL_COLUMN, frflag.get_quality(),
			                   FRFLAG_FLAGS_COLUMN, frflag.get_flags(),
			                   FRFLAG_FROM_PATCH, frflag.from_patch(),
			                   FRFLAG_MODIFIED, frflag.was_modified(),
			                   FRFLAG_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(flagtree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(flagtree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first)
			Set_frameflags_fields(first->get_frame(), first->get_quality(),
			                      first->get_flags());
		else
			Set_frameflags_fields();
	} else
		Set_frameflags_fields();

	const std::vector<Frame_usecode_info> &frucinf = info.get_frame_usecode_info();
	set_toggle("shinfo_frameusecode_check", !frucinf.empty());
	set_visible("shinfo_frameusecode_box", !frucinf.empty());
	if (!frucinf.empty()) {
		GtkTreeView *uctree = GTK_TREE_VIEW(
		                          get_widget("shinfo_frameusecode_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(uctree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Frame_usecode_info *first = nullptr;
		for (const auto &frucfun : frucinf) {
			if (frucfun.is_invalid())
				continue;
			if (!first)
				first = &frucfun;

			std::stringstream ucfun;
			if (frucfun.get_usecode_name().length())
				ucfun << frucfun.get_usecode_name();
			else
				ucfun << frucfun.get_usecode();
			GdkPixbuf *nshape = shape_image(shpfile, shnum, frucfun.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter,
			                   FRUC_FRAME_COLUMN, frucfun.get_frame(),
			                   FRUC_QUAL_COLUMN, frucfun.get_quality(),
			                   FRUC_USEFUN_COLUMN, ucfun.str().c_str(),
			                   FRUC_FROM_PATCH, frucfun.from_patch(),
			                   FRUC_MODIFIED, frucfun.was_modified(),
			                   FRUC_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(uctree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(uctree, path, nullptr, false);
		gtk_tree_path_free(path);
		std::stringstream ucfun;
		if (first) {
			if (first->get_usecode_name().length())
				ucfun << first->get_usecode_name();
			else
				ucfun << first->get_usecode();
			Set_frameusecode_fields(first->get_frame(), first->get_quality(),
			                        ucfun.str().c_str());
		} else
			Set_frameusecode_fields();
	} else
		Set_frameusecode_fields();

	const std::vector<Frame_name_info> &nmvec = info.get_frame_name_info();
	set_toggle("shinfo_framenames_check", !nmvec.empty());
	set_visible("shinfo_framenames_box", !nmvec.empty());
	if (!nmvec.empty()) {
		GtkTreeView *nametree = GTK_TREE_VIEW(
		                            get_widget("shinfo_framenames_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(nametree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Frame_name_info *first = nullptr;
		const string locsname(convertFromUTF8(get_text_entry("shinfo_name")));
		const char *sname = locsname.c_str();
		for (const auto &nmit : nmvec) {
			if (nmit.is_invalid())
				continue;
			if (!first)
				first = &nmit;
			const int type = nmit.get_type();
			const int msgid = nmit.get_msgid();
			const char *msgstr = type == -255 ? sname :
			                     (type == -1 || msgid >= get_num_misc_names() ? nullptr : get_misc_name(msgid));
			const int otmsg = nmit.get_othermsg();
			const int otype = type <= 0 ? -1 : (otmsg < 0 ? otmsg : 2);
			const char *otmsgstr = otype == -255 ? sname :
			                       (otype == -1 || otmsg >= get_num_misc_names() ? nullptr : get_misc_name(otmsg));
			const string utf8msg(convertToUTF8(msgstr));
			const string utf8otmsg(convertToUTF8(otmsgstr));
			GdkPixbuf *nshape = shape_image(shpfile, shnum, nmit.get_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter, FNAME_FRAME, nmit.get_frame(),
			                   FNAME_QUALITY, nmit.get_quality(), FNAME_MSGTYPE, type,
			                   FNAME_MSGSTR, utf8msg.c_str(), FNAME_OTHERTYPE, otype,
			                   FNAME_OTHERMSG, utf8otmsg.c_str(), FNAME_FROM_PATCH, nmit.from_patch(),
			                   FNAME_MODIFIED, nmit.was_modified(),
			                   FNAME_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(nametree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(nametree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first) {
			const int type = first->get_type();
			const int msgid = first->get_msgid();
			const char *msgstr = type == -255 ? sname :
			                     (type == -1 || msgid >= get_num_misc_names() ? "" : get_misc_name(msgid));
			const int otmsg = first->get_othermsg();
			const int otype = type <= 0 ? -1 : (otmsg < 0 ? otmsg : 2);
			const char *otmsgstr = otype == -255 ? sname :
			                       (otype == -1 || otmsg >= get_num_misc_names() ? "" : get_misc_name(otmsg));
			const string utf8msg(convertToUTF8(msgstr));
			const string utf8otmsg(convertToUTF8(otmsgstr));
			Set_framenames_fields(first->get_frame(), first->get_quality(),
			                      type, utf8msg.c_str(), otype, utf8otmsg.c_str());
		} else
			Set_framenames_fields();
	} else
		Set_framenames_fields();

	const std::vector<Paperdoll_item> &dollinf = info.get_paperdoll_info();
	set_toggle("shinfo_objpaperdoll_check", !dollinf.empty());
	set_visible("shinfo_objpaperdoll_box", !dollinf.empty());
	if (!dollinf.empty()) {
		GtkTreeView *dolltree = GTK_TREE_VIEW(
		                            get_widget("shinfo_objpaperdoll_list"));
		GtkTreeModel *model = gtk_tree_view_get_model(dolltree);
		GtkTreeStore *store = GTK_TREE_STORE(model);
		GtkTreeIter iter;
		const Paperdoll_item *first = nullptr;
		for (const auto &doll : dollinf) {
			if (doll.is_invalid())
				continue;
			if (!first)
				first = &doll;
			GdkPixbuf *nshape = shape_image(shpfile, shnum, doll.get_world_frame(), true);
			gtk_tree_store_append(store, &iter, nullptr);
			gtk_tree_store_set(store, &iter,
			                   DOLL_WORLD_FRAME, doll.get_world_frame(),
			                   DOLL_SPOT, doll.get_object_spot(),
			                   DOLL_TRANSLUCENT, doll.is_translucent(),
			                   DOLL_GENDER_BASED, doll.is_gender_based(),
			                   DOLL_SPOT_TYPE, doll.get_spot_frame(),
			                   DOLL_SHAPE, doll.get_paperdoll_shape(),
			                   DOLL_FRAME_0, doll.get_paperdoll_frame(0),
			                   DOLL_FRAME_1, doll.get_paperdoll_frame(1),
			                   DOLL_FRAME_2, doll.get_paperdoll_frame(2),
			                   DOLL_FRAME_3, doll.get_paperdoll_frame(3),
			                   DOLL_FROM_PATCH, doll.from_patch(),
			                   DOLL_MODIFIED, doll.was_modified(),
			                   DOLL_SHAPE_IMAGE, nshape, -1);
			if (nshape) {
				g_object_unref(nshape);
			}
		}
		GtkTreeSelection *sel = gtk_tree_view_get_selection(dolltree);
		gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_selection_select_iter(sel, &iter);
		GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_set_cursor(dolltree, path, nullptr, false);
		gtk_tree_path_free(path);
		if (first) {
			const Paperdoll_item &doll = *first;
			Set_objpaperdoll_fields(doll.get_world_frame(), doll.get_object_spot(),
			                        doll.is_translucent(), doll.is_gender_based(),
			                        doll.get_spot_frame(), doll.get_paperdoll_shape(),
			                        doll.get_paperdoll_frame(0), doll.get_paperdoll_frame(1),
			                        doll.get_paperdoll_frame(2), doll.get_paperdoll_frame(3));
		} else
			Set_objpaperdoll_fields();
	} else
		Set_objpaperdoll_fields();

	gtk_widget_show(book);
}

struct Update_hps {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int hps;
		unsigned int patch;
		unsigned int modded;
		int qual;
		gtk_tree_model_get(model, iter, HP_FRAME_COLUMN, &frnum,
		                   HP_QUALITY_COLUMN, &qual, HP_HIT_POINTS, &hps,
		                   HP_FROM_PATCH, &patch, HP_MODIFIED, &modded, -1);
		if (!info.has_quality() && qual != -1)  // Ignore these.
			return;
		Effective_hp_info hpinf(frnum, qual, hps, patch, modded);
		info.add_effective_hp_info(hpinf);
	}
};

struct Update_brightness {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int brightness;
		unsigned int patch;
		unsigned int modded;
		gtk_tree_model_get(model, iter, BRIGHTNESS_FRAME_COLUMN, &frnum,
		                   BRIGHTNESS_VALUE_COLUMN, &brightness,
		                   BRIGHTNESS_FROM_PATCH, &patch, BRIGHTNESS_MODIFIED, &modded, -1);
		Light_info lightinf(frnum, brightness, patch, modded);
		info.add_light_info(lightinf);
	}
};

struct Update_warmth {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int warm;
		unsigned int patch;
		unsigned int modded;
		gtk_tree_model_get(model, iter, WARM_FRAME_COLUMN, &frnum,
		                   WARM_VALUE_COLUMN, &warm,
		                   WARM_FROM_PATCH, &patch, WARM_MODIFIED, &modded, -1);
		Warmth_info warminf(frnum, warm, patch, modded);
		info.add_warmth_info(warminf);
	}
};

struct Update_cntrules {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int shnum;
		unsigned int accept;
		unsigned int patch;
		unsigned int modded;
		gtk_tree_model_get(model, iter, CNT_SHAPE_COLUMN, &shnum,
		                   CNT_ACCEPT_COLUMN, &accept,
		                   CNT_FROM_PATCH, &patch, CNT_MODIFIED, &modded, -1);
		Content_rules cntinf(shnum, accept, patch, modded);
		info.add_content_rule(cntinf);
	}
};

struct Update_frflags {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int qual;
		unsigned int flags;
		unsigned int patch;
		unsigned int modded;
		gtk_tree_model_get(model, iter, FRFLAG_FRAME_COLUMN, &frnum,
		                   FRFLAG_QUAL_COLUMN, &qual, FRFLAG_FLAGS_COLUMN, &flags,
		                   FRFLAG_FROM_PATCH, &patch, FRFLAG_MODIFIED, &modded, -1);
		Frame_flags_info flaginf(frnum, qual, flags, patch, modded);
		info.add_frame_flags(flaginf);
	}
};

struct Update_frusecode {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int qual;
		unsigned int patch;
		unsigned int modded;
		char *ucfun;
		gtk_tree_model_get(model, iter, FRUC_FRAME_COLUMN, &frnum,
		                   FRUC_QUAL_COLUMN, &qual, FRUC_USEFUN_COLUMN, &ucfun,
		                   FRUC_FROM_PATCH, &patch, FRUC_MODIFIED, &modded, -1);
		char *eptr;
		long int ucid = strtol(ucfun, &eptr, 0);
		if (ucfun == eptr || strlen(ucfun) == 0)
			ucid = -1;
		Frame_usecode_info ucinf(frnum, qual, ucid, ucfun, patch, modded);
		info.add_frame_usecode_info(ucinf);
	}
};

struct Update_paperdolls {
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		int spot;
		int trans;
		int gender;
		int type;
		int shape;
		int frame0;
		int frame1;
		int frame2;
		int frame3;
		int patch;
		int modded;
		gtk_tree_model_get(model, iter, DOLL_WORLD_FRAME, &frnum,
		                   DOLL_SPOT, &spot,
		                   DOLL_TRANSLUCENT, &trans,
		                   DOLL_GENDER_BASED, &gender,
		                   DOLL_SPOT_TYPE, &type,
		                   DOLL_SHAPE, &shape,
		                   DOLL_FRAME_0, &frame0,
		                   DOLL_FRAME_1, &frame1,
		                   DOLL_FRAME_2, &frame2,
		                   DOLL_FRAME_3, &frame3,
		                   DOLL_FROM_PATCH, &patch,
		                   DOLL_MODIFIED, &modded, -1);
		Paperdoll_item dollinf(frnum, spot, type, trans, gender, shape,
		                       frame0, frame1, frame2, frame3, patch, modded);
		info.add_paperdoll_info(dollinf);
	}
};

int ExultStudio::find_misc_name(const char *id) const {
	auto it = misc_name_map.find(id);
	if (it != misc_name_map.end())
		return it->second;
	return -1;
}

int ExultStudio::add_misc_name(const char *id) {
	const int num = get_num_misc_names();
	Set_misc_name(num, id);
	misc_name_map.insert(std::pair<string, int>(string(id), num));
	shape_names_modified = true;
	return num;
}

struct Update_framenames {
private:
	int Find_name_id(const char *msg) {
		ExultStudio *studio = ExultStudio::get_instance();
		const string locmsg(convertFromUTF8(msg));
		const int idnum = studio->find_misc_name(locmsg.c_str());
		if (idnum < 0)
			return studio->add_misc_name(locmsg.c_str());
		return idnum;
	}
public:
	void operator()(Shape_info &info, GtkTreeModel *model, GtkTreeIter *iter) {
		unsigned int frnum;
		unsigned int qual;
		int type;
		int othertype;
		int patch;
		int modded;
		const char *str;
		const char *othermsg;
		gtk_tree_model_get(model, iter, FNAME_FRAME, &frnum,
		                   FNAME_QUALITY, &qual, FNAME_MSGTYPE, &type,
		                   FNAME_MSGSTR, &str, FNAME_OTHERTYPE, &othertype,
		                   FNAME_OTHERMSG, &othermsg,
		                   FNAME_FROM_PATCH, &patch, FNAME_MODIFIED, &modded, -1);
		int msgid;
		int otherid;
		msgid = type < 0 ? -1 : Find_name_id(str);
		otherid = type <= 0 ? -1 :
		          (othertype < 0 ? othertype : Find_name_id(othermsg));
		Frame_name_info nminf(static_cast<int>(frnum), static_cast<int>(qual),
		                      type, msgid, otherid, patch, modded);
		info.add_frame_name_info(nminf);
	}
};

template <typename Functor>
static inline void update_shape_vector(
    const char *treename,
    Shape_info &info
) {
	Functor update;
	GtkTreeView *tree = GTK_TREE_VIEW(
	                        ExultStudio::get_instance()->get_widget(treename));
	GtkTreeModel *model = gtk_tree_view_get_model(tree);
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first(model, &iter))
		// Tree has contents.
		do {
			update(info, model, &iter);
		} while (gtk_tree_model_iter_next(model, &iter));
}

inline Weapon_info *Shape_info::set_weapon_info(bool tf) {
	return set_info(tf, weapon);
}
inline Ammo_info *Shape_info::set_ammo_info(bool tf) {
	return set_info(tf, ammo);
}
inline Armor_info *Shape_info::set_armor_info(bool tf) {
	return set_info(tf, armor);
}
inline Monster_info *Shape_info::set_monster_info(bool tf) {
	return set_info(tf, monstinf);
}
inline Paperdoll_npc *Shape_info::set_npc_paperdoll_info(bool tf) {
	return set_info(tf, npcpaperdoll);
}
inline SFX_info *Shape_info::set_sfx_info(bool tf) {
	return set_info(tf, sfxinf);
}
inline Explosion_info *Shape_info::set_explosion_info(bool tf) {
	return set_info(tf, explosion);
}
inline Animation_info *Shape_info::set_animation_info(bool tf) {
	return set_info(tf, aniinf);
}
inline Body_info *Shape_info::set_body_info(bool tf) {
	return set_info(tf, body);
}

/*
 *  Save the shape-editing notebook.
 */

void ExultStudio::save_shape_notebook(
    Shape_info &info,
    int shnum,          // Shape #.
    int frnum           // Frame #.
) {
	static const int classes[] = {0, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14};
	info.set_shape_class(static_cast<Shape_info::Shape_class>(
	                         classes[get_optmenu("shinfo_shape_class")]));
	info.set_3d(get_spin("shinfo_xtiles"), get_spin("shinfo_ytiles"),
	            get_spin("shinfo_ztiles"));
	const int spot = get_optmenu("shinfo_ready_spot");
	static const unsigned char conv_spots[] = {
		rhand, lhand, both_hands, amulet, cloak,
		neck, lfinger, gloves, lrgloves,
		quiver, earrings, head, torso, backpack, belt,
		legs, feet, ucont, triple_bolts
	};
	info.set_ready_type(conv_spots[spot]);
	info.set_is_spell(get_toggle("shinfo_is_spell_check"));
	// Some Exult stuff.
	static const unsigned char alt_spots[] = {
		invalid_spot, rhand, lhand,
		neck, belt, back_2h, back_shield, scabbard,
		backpack
	};
	info.set_alt_ready(alt_spots[get_optmenu("shinfo_altready1_spot")],
	                   alt_spots[get_optmenu("shinfo_altready2_spot")]);

	info.set_weight_volume(get_spin("shinfo_weight"),
	                       get_spin("shinfo_volume"));
	info.set_weapon_offset(frnum, get_spin("shinfo_wihx"),
	                       get_spin("shinfo_wihy"));
	// Bunch of flags:
	info.set_sfx(get_toggle("shinfo_sfx_check"));
	info.set_strange_movement(get_toggle("shinfo_strange_check"));
	info.set_animated(get_toggle("shinfo_animated_check"));
	info.set_solid(get_toggle("shinfo_solid_check"));
	info.set_water(get_toggle("shinfo_water_check"));
	// ++++Set poison for flats, field for non-flats.
	info.set_field(get_toggle("shinfo_field_check") ||
	               get_toggle("shinfo_poison_check"));
	info.set_door(get_toggle("shinfo_door_check"));
	info.set_barge_part(get_toggle("shinfo_barge_check"));
	info.set_transparent(get_toggle("shinfo_transp_check"));
	info.set_light_source(get_toggle("shinfo_light_check"));
	info.set_translucency(get_toggle("shinfo_transl_check"));
	info.set_obstacle(get_toggle("shinfo_obstaclex_check"),
	                  get_toggle("shinfo_obstacley_check"));
	info.set_occludes(get_toggle("shinfo_occludes_check"));
	// More flags, originally hard-coded:
	static const char *shpflags[] = {
		"shinfo_shape_flag0",
		"shinfo_shape_flag1",
		"shinfo_shape_flag2",
		"shinfo_shape_flag3",
		"shinfo_shape_flag4",
		"shinfo_shape_flag5",
		"shinfo_shape_flag6",
		"shinfo_shape_flag7",
		"shinfo_shape_flag8",
		"shinfo_shape_flag9"
	};
	info.set_shape_flags(get_bit_toggles(&shpflags[0],
	                                     array_size(shpflags)));
	// Extras.
	if (!get_toggle("shinfo_weapon_check"))
		info.set_weapon_info(false);    // Not a weapon.
	else {
		Weapon_info *winfo = info.set_weapon_info(true);
		winfo->set_damage(
		    get_spin("shinfo_weapon_damage"),
		    get_optmenu("shinfo_weapon_type"));
		winfo->set_range(get_spin("shinfo_weapon_range"));
		const int ammo_use = get_optmenu("shinfo_weapon_ammo");
		const int ammo = ammo_use == 0 ?
		           get_spin("shinfo_weapon_ammo_shape")
		           :  ammo_use == 2 ? -2       // Charges.
		           :  ammo_use == 3 ? -3       // Thrown.
		           :  -1;              // None.
		winfo->set_ammo(ammo);
		const int proj_type = get_optmenu("shinfo_weapon_sprite");
		const int proj = proj_type == 0 ?
		           get_spin("shinfo_weapon_proj")
		           :  ammo_use == 2 ? -3       // Weapon.
		           :  -1;              // None.
		winfo->set_projectile(proj);
		winfo->set_uses(get_optmenu("shinfo_weapon_uses"));
		winfo->set_sfxs(get_spin("shinfo_weapon_sfx"),
		                get_spin("shinfo_weapon_hitsfx"));
		// Get usecode in hex.
		winfo->set_usecode(get_num_entry("shinfo_weapon_uc"));
		static const char *powers[] = {
			"shinfo_weapon_pow0",
			"shinfo_weapon_pow1",
			"shinfo_weapon_pow2",
			"shinfo_weapon_pow3",
			"shinfo_weapon_pow4",
			"shinfo_weapon_pow5",
			"shinfo_weapon_pow6",
			"shinfo_weapon_pow7"
		};
		winfo->set_powers(get_bit_toggles(&powers[0],
		                                  array_size(powers)));
		winfo->set_lucky(get_toggle("shinfo_weapon_flag0"));
		winfo->set_explodes(get_toggle("shinfo_weapon_flag1"));
		winfo->set_no_blocking(get_toggle("shinfo_weapon_flag2"));
		winfo->set_delete_depleted(get_toggle("shinfo_weapon_flag3"));
		winfo->set_autohits(get_toggle("shinfo_weapon_always"));
		winfo->set_needs_target(get_toggle("shinfo_weapon_target"));
		winfo->set_returns(get_toggle("shinfo_weapon_returns"));
		winfo->set_actor_frames(get_optmenu("shinfo_weapon_frames") |
		                        (get_optmenu("shinfo_proj_frames") << 2));
		winfo->set_rotation_speed(get_spin("shinfo_proj_rotation"));
		winfo->set_missile_speed(get_optmenu("shinfo_proj_speed") + 1);
	}
	if (!get_toggle("shinfo_ammo_check"))
		info.set_ammo_info(false);  // Not ammo.
	else {
		Ammo_info *ainfo = info.set_ammo_info(true);
		ainfo->set_family_shape(get_spin("shinfo_ammo_family"));
		ainfo->set_damage(get_spin("shinfo_ammo_damage"),
		                  get_optmenu("shinfo_ammo_type"));
		const int proj_type = get_optmenu("shinfo_ammo_sprite");
		const int proj = proj_type == 0 ?
		           get_spin("shinfo_ammo_proj")
		           :  proj_type == 2 ? -3      // Weapon.
		           :  -1;              // None.
		ainfo->set_sprite_shape(proj);
		static const char *powers[] = {
			"shinfo_ammo_pow0",
			"shinfo_ammo_pow1",
			"shinfo_ammo_pow2",
			"shinfo_ammo_pow3",
			"shinfo_ammo_pow4",
			"shinfo_ammo_pow5",
			"shinfo_ammo_pow6",
			"shinfo_ammo_pow7"
		};
		ainfo->set_powers(get_bit_toggles(&powers[0],
		                                  array_size(powers)));
		ainfo->set_lucky(get_toggle("shinfo_ammo_flag0"));
		ainfo->set_autohits(get_toggle("shinfo_ammo_flag1"));
		ainfo->set_returns(get_toggle("shinfo_ammo_flag2"));
		ainfo->set_no_blocking(get_toggle("shinfo_ammo_flag3"));
		ainfo->set_explodes(get_toggle("shinfo_ammo_flag6"));
		ainfo->set_homing(get_toggle("shinfo_ammo_special"));

		if (get_toggle("shinfo_ammo_special")) {
			ainfo->set_homing(true);
			ainfo->set_drop_type(0);
		} else {
			ainfo->set_homing(false);
			ainfo->set_drop_type(get_optmenu("shinfo_ammo_drop"));
		}
	}
	if (!get_toggle("shinfo_armor_check"))
		info.set_armor_info(false); // Not armor.
	else {
		Armor_info *arinfo = info.set_armor_info(true);
		static const char *immun[] = {"shinfo_armor_immun0",
		                              "shinfo_armor_immun1",
		                              "shinfo_armor_immun2",
		                              "shinfo_armor_immun3",
		                              "shinfo_armor_immun4",
		                              "shinfo_armor_immun5"
		                             };
		arinfo->set_prot(get_spin("shinfo_armor_value"));
		arinfo->set_immune(get_bit_toggles(&immun[0],
		                                   array_size(immun)));
	}
	if (!get_toggle("shinfo_npcflags_check"))
		info.set_actor_flags(0);
	else {
		static const char *flags[] = {  "shinfo_actor_flag0",
		                                "shinfo_actor_flag1",
		                                "shinfo_actor_flag2",
		                                "shinfo_actor_flag3",
		                                "shinfo_actor_flag4",
		                                "shinfo_actor_flag5",
		                                "shinfo_actor_flag6",
		                                "shinfo_actor_flag7"
		                             };
		info.set_actor_flags(get_bit_toggles(&flags[0],
		                                     array_size(flags)));
	}
	if (!get_toggle("shinfo_monster_check"))
		info.set_monster_info(false);
	else {
		Monster_info *minfo = info.set_monster_info(true);
		minfo->set_stats(
		    get_spin("shinfo_monster_str"),
		    get_spin("shinfo_monster_dex"),
		    get_spin("shinfo_monster_intel"),
		    get_spin("shinfo_monster_cmb"),
		    get_spin("shinfo_monster_armor"),
		    get_spin("shinfo_monster_wpn"),
		    get_spin("shinfo_monster_reach"));
		minfo->set_equip_offset(get_spin("shinfo_monster_equip"));
		minfo->set_hitsfx(get_spin("shinfo_monster_sfx"));
		minfo->set_alignment(get_optmenu("shinfo_monster_align"));
		minfo->set_attackmode(get_optmenu("shinfo_monster_attackmode"));
		static const char *vuln[] = {   "shinfo_monster_vuln0",
		                                "shinfo_monster_vuln1",
		                                "shinfo_monster_vuln2",
		                                "shinfo_monster_vuln3",
		                                "shinfo_monster_vuln4",
		                                "shinfo_monster_vuln5",
		                                "shinfo_monster_vuln6",
		                                "shinfo_monster_vuln7"
		                            };
		static const char *immun[] = {"shinfo_monster_immun0",
		                              "shinfo_monster_immun1",
		                              "shinfo_monster_immun2",
		                              "shinfo_monster_immun3",
		                              "shinfo_monster_immun4",
		                              "shinfo_monster_immun5",
		                              "shinfo_monster_immun6",
		                              "shinfo_monster_immun7"
		                             };
		minfo->set_vulnerable(get_bit_toggles(&vuln[0],
		                                      array_size(vuln)));
		minfo->set_immune(get_bit_toggles(&immun[0],
		                                  array_size(immun)));
		minfo->set_splits(get_toggle("shinfo_monster_splits"));
		minfo->set_cant_yell(get_toggle("shinfo_monster_cant_yell"));
		minfo->set_cant_bleed(get_toggle("shinfo_monster_cant_bleed"));
		minfo->set_sleep_safe(get_toggle("shinfo_monster_sleep_safe"));
		minfo->set_charm_safe(get_toggle("shinfo_monster_charm_safe"));
		minfo->set_curse_safe(get_toggle("shinfo_monster_curse_safe"));
		minfo->set_paralysis_safe(get_toggle("shinfo_monster_paralysis_safe"));
		minfo->set_poison_safe(get_toggle("shinfo_monster_poison_safe"));
		minfo->set_death_safe(get_toggle("shinfo_monster_death_safe"));
		minfo->set_power_safe(get_toggle("shinfo_monster_power_safe"));
		minfo->set_cant_die(get_toggle("shinfo_monster_cant_die"));
		static const char *flags[] = {"shinfo_monster_flag0",
		                              "shinfo_monster_flag1",
		                              "shinfo_monster_flag2",
		                              "shinfo_monster_flag3",
		                              "shinfo_monster_flag4",
		                              "shinfo_monster_flag5",
		                              "shinfo_monster_flag6",
		                              "shinfo_monster_flag7"
		                             };
		minfo->set_flags(get_bit_toggles(&flags[0],
		                                 array_size(flags)));
		info.set_monster_food(get_spin("shinfo_monster_food"));
	}
	if (!get_toggle("shinfo_container_check"))
		info.set_gump_data(-1, -1);
	else {
		info.set_gump_data(get_spin("shinfo_gump_num"),
		                   get_spin("shinfo_gump_font"));
	}
	if (!get_toggle("shinfo_mountaintop_check"))
		info.set_mountain_top(0);
	else {
		info.set_mountain_top(get_optmenu("shinfo_mountaintop_type"));
	}
	if (!get_toggle("shinfo_bargetype_check"))
		info.set_barge_type(0);
	else {
		info.set_barge_type(get_optmenu("shinfo_barge_type"));
	}
	if (!get_toggle("shinfo_fieldinfo_check"))
		info.set_field_type(-1);
	else {
		info.set_field_type(get_optmenu("shinfo_fieldinfo_type") - 1);
	}
	if (!get_toggle("shinfo_body_check"))
		info.set_body_info(false);
	else {
		Body_info *binfo = info.set_body_info(true);
		binfo->set(get_spin("shinfo_body_shape"), get_spin("shinfo_body_frame"));
	}
	if (!get_toggle("shinfo_explosion_check"))
		info.set_explosion_info(false);
	else {
		Explosion_info *einfo = info.set_explosion_info(true);
		const int sfx = get_toggle("shinfo_explosion_sfx_type") ? -1
		          : get_spin("shinfo_explosion_sfx_number");
		einfo->set(get_spin("shinfo_explosion_sprite"), sfx);
	}
	if (!get_toggle("shinfo_sound_check"))
		info.set_sfx_info(false);
	else {
		SFX_info *sfxinf = info.set_sfx_info(true);
		sfxinf->set_sfx(get_spin("shinfo_sfx_first"));
		sfxinf->set_play_randomly(get_optmenu("shinfo_sfx_type"));
		sfxinf->set_chance(get_spin("shinfo_sfx_chance"));
		const int extra = get_toggle("shinfo_sfx_clock_check") ?
		            get_spin("shinfo_sfx_clock_sfx") : -1;
		sfxinf->set_extra_sfx(extra);
		const int range = get_toggle("shinfo_single_sfx") ? 1 :
		            get_spin("shinfo_sfx_count");
		sfxinf->set_sfx_range(range);
	}
	if (!get_toggle("shinfo_npcpaperdoll_check"))
		info.set_npc_paperdoll_info(false);
	else {
		Paperdoll_npc *npcinf = info.set_npc_paperdoll_info(true);
		npcinf->set_is_female(get_toggle("shinfo_npcpaperdoll_female"));
		npcinf->set_translucent(get_toggle("shinfo_npcpaperdoll_trans"));
		npcinf->set_body_shape(get_spin("shinfo_npcpaperdoll_bshape"));
		npcinf->set_body_frame(get_spin("shinfo_npcpaperdoll_bframe"));
		npcinf->set_head_shape(get_spin("shinfo_npcpaperdoll_hshape"));
		npcinf->set_head_frame(get_spin("shinfo_npcpaperdoll_hframe"));
		npcinf->set_head_frame_helm(get_spin("shinfo_npcpaperdoll_hhelm"));
		npcinf->set_arms_shape(get_spin("shinfo_npcpaperdoll_ashape"));
		static const char * const arm_names[] = {"shinfo_npcpaperdoll_aframe",
		                                  "shinfo_npcpaperdoll_atwohanded",
		                                  "shinfo_npcpaperdoll_astaff"
		                                 };
		for (size_t i = 0; i < array_size(arm_names); i++)
			npcinf->set_arms_frame(i, get_spin(arm_names[i]));
	}
	if (!get_toggle("shinfo_animation_check"))
		info.set_animation_info(false);
	else {
		Animation_info *aniinf = info.set_animation_info(true);
		auto type =
		    static_cast<Animation_info::AniType>(get_optmenu("shinfo_animation_type"));
		aniinf->set_type(type);
		const int count = get_spin("shinfo_animation_frcount");
		Vga_file *ifile = curfile->get_ifile();
		const int nframes = ifile->get_num_frames(shnum);
		if (get_toggle("shinfo_animation_frtype") || count == nframes)
			aniinf->set_frame_count(-1);
		else
			aniinf->set_frame_count(count);
		if (type != Animation_info::FA_NON_LOOPING) {
			aniinf->set_frame_delay(get_spin("shinfo_animation_ticks"));
			const int sfxdelay = get_toggle("shinfo_animation_sfxsynch") ? -1
			               : get_spin("shinfo_animation_sfxdelay");
			aniinf->set_sfx_delay(sfxdelay);
			if (type == Animation_info::FA_LOOPING) {
				const int menu = get_optmenu("shinfo_animation_freezefirst");
				const int chance = menu == 0 ? 100
				             : (menu == 1 ? 0 : get_spin("shinfo_animation_freezechance"));
				aniinf->set_freeze_first_chance(chance);
				int rec;
				if (get_toggle("shinfo_animation_rectype"))
					rec = nframes;
				else
					rec = get_spin("shinfo_animation_recycle");
				aniinf->set_recycle(rec == nframes ? 0 : rec);
			}
		}
	}
	if (!get_toggle("shinfo_effhps_check"))
		info.set_effective_hp_info(false);
	else {
		info.set_effective_hp_info(true);
		update_shape_vector<Update_hps>("shinfo_effhps_list", info);
		info.clean_invalid_hp_info();
	}
	if (!get_toggle("shinfo_brightness_check"))
		info.set_light_info(false);
	else {
		info.set_light_info(true);
		update_shape_vector<Update_brightness>("shinfo_brightness_list", info);
		info.clean_invalid_light_info();
	}
	if (!get_toggle("shinfo_warmth_check"))
		info.set_warmth_info(false);
	else {
		info.set_warmth_info(true);
		update_shape_vector<Update_warmth>("shinfo_warmth_list", info);
		info.clean_invalid_warmth_info();
	}
	if (!get_toggle("shinfo_objpaperdoll_check"))
		info.set_paperdoll_info(false);
	else {
		info.set_paperdoll_info(true);
		update_shape_vector<Update_paperdolls>("shinfo_objpaperdoll_list", info);
		info.clean_invalid_paperdolls();
	}
	if (!get_toggle("shinfo_cntrules_check"))
		info.set_content_rules(false);
	else {
		info.set_content_rules(true);
		update_shape_vector<Update_cntrules>("shinfo_cntrules_list", info);
		info.clean_invalid_content_rules();
	}
	if (!get_toggle("shinfo_frameflags_check"))
		info.set_frame_flags(false);
	else {
		info.set_frame_flags(true);
		update_shape_vector<Update_frflags>("shinfo_frameflags_list", info);
		info.clean_invalid_frame_flags();
	}
	if (!get_toggle("shinfo_frameusecode_check"))
		info.set_frame_usecode_info(false);
	else {
		info.set_frame_usecode_info(true);
		update_shape_vector<Update_frusecode>("shinfo_frameusecode_list", info);
		info.clean_invalid_usecode_info();
	}
	if (!get_toggle("shinfo_framenames_check"))
		info.set_frame_name_info(false);
	else {
		info.set_frame_name_info(true);
		update_shape_vector<Update_framenames>("shinfo_framenames_list", info);
		info.clean_invalid_name_info();
	}

	shape_info_modified = true;
	auto *shpchoose = dynamic_cast<Shape_chooser *>(browser);
	if (shpchoose)
		shpchoose->update_statusbar();
}

static inline void add_terminal_columns(
    GtkTreeView *tree,
    int patch_index,
    int modified_index,
    int shape_index
) {
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
	                             "Patch", renderer, "text", patch_index, nullptr);
	gtk_tree_view_column_set_visible(col, false);
	gtk_tree_view_append_column(tree, col);
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
	          "Modified", renderer, "text", modified_index, nullptr);
	gtk_tree_view_column_set_visible(col, false);
	gtk_tree_view_append_column(tree, col);
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_alignment(renderer, 0.0, 0.5);
	gtk_cell_renderer_set_padding(renderer, 2*HMARGIN, 2*VMARGIN);
	col = gtk_tree_view_column_new_with_attributes(
	          "Shape", renderer, "pixbuf", shape_index, nullptr);
	gtk_tree_view_append_column(tree, col);
}


/*
 *  Open the shape-editing window.
 */

void ExultStudio::open_shape_window(
    int shnum,          // Shape #.
    int frnum,          // Frame #.
    Shape_file_info *file_info, // For image file shape is in.
    const char *shname,         // ->shape name, or null.
    Shape_info *info        // Info. if in main object shapes.
) {
	if (!shapewin)          // First time?
		shapewin = get_widget("shape_window");
	// Ifile might have changed.
	delete shape_single;
	shape_single = nullptr;
	delete gump_single;
	gump_single = nullptr;
	delete body_single;
	body_single = nullptr;
	delete explosion_single;
	explosion_single = nullptr;
	// Note: ifile and vgafile can't possibly be null if we are here.
	Vga_file *ifile = file_info->get_ifile();
	if (palbuf) {
		shape_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_frame"),
		    -1,
		    ifile,
		    palbuf.get(),
		    get_widget("shinfo_draw"));
		body_single = new Shape_single(
		    get_widget("shinfo_body_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_body_frame"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_body_draw"));
	}
	if (gumpfile && palbuf) {
		gump_single = new Shape_single(
		    get_widget("shinfo_gump_num"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    nullptr,
		    U7_SHAPE_GUMPS,
		    gumpfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_gump_draw"));
	}
	if (spritefile && palbuf) {
		explosion_single = new Shape_single(
		    get_widget("shinfo_explosion_sprite"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    nullptr,
		    U7_SHAPE_SPRITES,
		    spritefile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_explosion_draw"));
	}
	delete weapon_family_single;
	weapon_family_single = nullptr;
	delete weapon_projectile_single;
	weapon_projectile_single = nullptr;
	delete ammo_family_single;
	ammo_family_single = nullptr;
	delete ammo_sprite_single;
	ammo_sprite_single = nullptr;
	delete cntrules_shape_single;
	cntrules_shape_single = nullptr;
	delete frameflags_frame_single;
	frameflags_frame_single = nullptr;
	delete effhps_frame_single;
	effhps_frame_single = nullptr;
	delete framenames_frame_single;
	framenames_frame_single = nullptr;
	delete frameusecode_frame_single;
	frameusecode_frame_single = nullptr;
	delete objpaperdoll_wframe_single;
	objpaperdoll_spotframe_single = nullptr;
	delete objpaperdoll_spotframe_single;
	objpaperdoll_wframe_single = nullptr;
	delete brightness_frame_single;
	brightness_frame_single = nullptr;
	delete warmth_frame_single;
	warmth_frame_single = nullptr;
	if (palbuf) {
		weapon_family_single = new Shape_single(
		    get_widget("shinfo_weapon_ammo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    nullptr,
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_weapon_family_draw"), true);
		weapon_projectile_single = new Shape_single(
		    get_widget("shinfo_weapon_proj"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    nullptr,
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_weapon_projectile_draw"), true);
		ammo_family_single = new Shape_single(
		    get_widget("shinfo_ammo_family"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    nullptr,
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_ammo_family_draw"));
		ammo_sprite_single = new Shape_single(
		    get_widget("shinfo_ammo_proj"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    nullptr,
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_ammo_sprite_draw"), true);
		cntrules_shape_single = new Shape_single(
		    get_widget("shinfo_cntrules_shape_num"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    nullptr,
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_cntrules_shape_draw"), true);
		frameflags_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_frameflags_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_frameflags_frame_draw"), true);
		effhps_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_effhps_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_effhps_frame_draw"), true);
		framenames_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_framenames_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_framenames_frame_draw"), true);
		frameusecode_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_frameusecode_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_frameusecode_frame_draw"), true);
		objpaperdoll_wframe_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_objpaperdoll_wframe"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_wframe_draw"), true);
		objpaperdoll_spotframe_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_objpaperdoll_spotframe"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_spotframe_draw"), true);
		brightness_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_brightness_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_brightness_frame_draw"), true);
		warmth_frame_single = new Shape_single(
		    get_widget("shinfo_shape"), nullptr,
		    [](int shnum)->bool{ return (shnum >= c_first_obj_shape) &&
		                                (shnum < c_max_shapes); },
		    get_widget("shinfo_warmth_frame_num"),
		    U7_SHAPE_SHAPES,
		    vgafile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_warmth_frame_draw"), true);
	}
	delete npcpaperdoll_aframe_single;
	npcpaperdoll_aframe_single = nullptr;
	delete npcpaperdoll_atwohanded_single;
	npcpaperdoll_atwohanded_single = nullptr;
	delete npcpaperdoll_astaff_single;
	npcpaperdoll_astaff_single = nullptr;
	delete npcpaperdoll_bframe_single;
	npcpaperdoll_bframe_single = nullptr;
	delete npcpaperdoll_hframe_single;
	npcpaperdoll_hframe_single = nullptr;
	delete npcpaperdoll_hhelm_single;
	npcpaperdoll_hhelm_single = nullptr;
	delete objpaperdoll_frame0_single;
	objpaperdoll_frame0_single = nullptr;
	delete objpaperdoll_frame1_single;
	objpaperdoll_frame1_single = nullptr;
	delete objpaperdoll_frame2_single;
	objpaperdoll_frame2_single = nullptr;
	delete objpaperdoll_frame3_single;
	objpaperdoll_frame3_single = nullptr;
	if (paperdolfile && palbuf) {
		npcpaperdoll_aframe_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_ashape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_aframe"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_aframe_draw"), true);
		npcpaperdoll_atwohanded_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_ashape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_atwohanded"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_atwohanded_draw"), true);
		npcpaperdoll_astaff_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_ashape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_astaff"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_astaff_draw"), true);
		npcpaperdoll_bframe_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_bshape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_bframe"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_bframe_draw"), true);
		npcpaperdoll_hframe_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_hshape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_hframe"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_hframe_draw"), true);
		npcpaperdoll_hhelm_single = new Shape_single(
		    get_widget("shinfo_npcpaperdoll_hshape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_npcpaperdoll_hhelm"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_npcpaperdoll_hhelm_draw"), true);
		objpaperdoll_frame0_single = new Shape_single(
		    get_widget("shinfo_objpaperdoll_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_objpaperdoll_frame0"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_frame0_draw"), true);
		objpaperdoll_frame1_single = new Shape_single(
		    get_widget("shinfo_objpaperdoll_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_objpaperdoll_frame1"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_frame1_draw"), true);
		objpaperdoll_frame2_single = new Shape_single(
		    get_widget("shinfo_objpaperdoll_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_objpaperdoll_frame2"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_frame2_draw"), true);
		objpaperdoll_frame3_single = new Shape_single(
		    get_widget("shinfo_objpaperdoll_shape"), nullptr,
		    [](int shnum)->bool{ return shnum >= 0; },
		    get_widget("shinfo_objpaperdoll_frame3"),
		    U7_SHAPE_PAPERDOL,
		    paperdolfile->get_ifile(),
		    palbuf.get(),
		    get_widget("shinfo_objpaperdoll_frame3_draw"), true);
	} else {
		set_visible("shinfo_npcpaperdoll_aframe_draw", false);
		set_visible("shinfo_npcpaperdoll_atwohanded_draw", false);
		set_visible("shinfo_npcpaperdoll_astaff_draw", false);
		set_visible("shinfo_npcpaperdoll_bframe_draw", false);
		set_visible("shinfo_npcpaperdoll_hframe_draw", false);
		set_visible("shinfo_npcpaperdoll_hhelm_draw", false);
		set_visible("shinfo_objpaperdoll_frame0_draw", false);
		set_visible("shinfo_objpaperdoll_frame1_draw", false);
		set_visible("shinfo_objpaperdoll_frame2_draw", false);
		set_visible("shinfo_objpaperdoll_frame3_draw", false);
	}
	// Store ->'s.
	g_object_set_data(G_OBJECT(shapewin), "user_data", info);
	g_object_set_data(G_OBJECT(shapewin), "file_info", file_info);
	// Shape/frame.
	set_entry("shinfo_shape", shnum, false, false);
	const int nframes = ifile->get_num_frames(shnum);
	set_spin("shinfo_frame", frnum, 0, nframes - 1);
	set_spin("shinfo_effhps_frame_num", 0, nframes - 1);
	set_spin("shinfo_brightness_frame_num", 0, nframes - 1);
	set_spin("shinfo_framenames_frame_num", 0, nframes - 1);
	set_spin("shinfo_frameflags_frame_num", 0, nframes - 1);
	set_spin("shinfo_frameusecode_frame_num", 0, nframes - 1);
	// Store name, #frames.
	const string utf8shname(convertToUTF8(shname ? shname : ""));
	set_entry("shinfo_name", utf8shname.c_str());
//	set_spin("shinfo_num_frames", nframes);
	// Show xright, ybelow.
	Shape_frame *shape = ifile->get_shape(shnum, frnum);
	set_spin("shinfo_originx", shape->get_xright(), -shape->get_width(),
	         shape->get_width());
	set_spin("shinfo_originy", shape->get_ybelow(), -shape->get_height(),
	         shape->get_height());
	// Get info. notebook.
	// Ensure proper bounds for shapes:
	set_spin("shinfo_weapon_ammo_shape", 0, c_max_shapes - 1);
	set_spin("shinfo_weapon_proj", 0, c_max_shapes - 1);
	set_spin("shinfo_ammo_family", 0, c_max_shapes - 1);
	set_spin("shinfo_body_shape", 0, c_max_shapes - 1);
	set_spin("shinfo_cntrules_shape_num", 0, c_max_shapes - 1);
	set_spin("shinfo_animation_frcount", 1, nframes);
	set_spin("shinfo_animation_recycle", 1, nframes);
	if (gumpfile) {
		set_spin("shinfo_gump_num", 0,
		         gumpfile->get_ifile()->get_num_shapes() - 1);
	}
	if (fontfile)
		set_spin("shinfo_gump_font", -1,
		         fontfile->get_ifile()->get_num_shapes() - 1);
	if (spritefile)
		set_spin("shinfo_explosion_sprite", 0,
		         spritefile->get_ifile()->get_num_shapes() - 1);
	GtkTreeView *hptree = GTK_TREE_VIEW(
	                          get_widget("shinfo_effhps_list"));
	g_signal_connect(G_OBJECT(hptree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_effhps_list_cursor_changed),
	                 nullptr);
	GtkTreeModel *model = gtk_tree_view_get_model(hptree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          HP_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(hptree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", HP_FRAME_COLUMN, nullptr);
		gtk_tree_view_append_column(hptree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Quality", renderer, "text", HP_QUALITY_COLUMN, nullptr);
		gtk_tree_view_append_column(hptree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Hit Points", renderer, "text", HP_HIT_POINTS, nullptr);
		gtk_tree_view_append_column(hptree, col);
		add_terminal_columns(hptree, HP_FROM_PATCH, HP_MODIFIED, HP_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(hptree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkTreeView *brightnesstree = GTK_TREE_VIEW(
	                                  get_widget("shinfo_brightness_list"));
	g_signal_connect(G_OBJECT(brightnesstree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_brightness_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(brightnesstree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          BRIGHTNESS_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(brightnesstree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", BRIGHTNESS_FRAME_COLUMN, nullptr);
		gtk_tree_view_append_column(brightnesstree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Brightness", renderer, "text", BRIGHTNESS_VALUE_COLUMN, nullptr);
		gtk_tree_view_append_column(brightnesstree, col);
		add_terminal_columns(brightnesstree, BRIGHTNESS_FROM_PATCH, BRIGHTNESS_MODIFIED, BRIGHTNESS_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(brightnesstree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}
	GtkTreeView *warmtree = GTK_TREE_VIEW(
	                            get_widget("shinfo_warmth_list"));
	g_signal_connect(G_OBJECT(warmtree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_warmth_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(warmtree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          WARM_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(warmtree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", WARM_FRAME_COLUMN, nullptr);
		gtk_tree_view_append_column(warmtree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Warmth", renderer, "text", WARM_VALUE_COLUMN, nullptr);
		gtk_tree_view_append_column(warmtree, col);
		add_terminal_columns(warmtree, WARM_FROM_PATCH, WARM_MODIFIED, WARM_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(warmtree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}
	GtkTreeView *dolltree = GTK_TREE_VIEW(
	                            get_widget("shinfo_objpaperdoll_list"));
	g_signal_connect(G_OBJECT(dolltree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_objpaperdoll_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(dolltree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          DOLL_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(dolltree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", DOLL_WORLD_FRAME, nullptr);
		gtk_tree_view_append_column(dolltree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Spot", renderer, "text", DOLL_SPOT, nullptr);
		gtk_tree_view_append_column(dolltree, col);
		const char * const columns[] = {"Trans", "Gender", "Spot frame", "DShape",
		                         "DFrame0", "DFrame1", "DFrame2", "DFrame3"
		                        };
		for (size_t i = 0; i < array_size(columns); i++) {
			renderer = gtk_cell_renderer_text_new();
			col = gtk_tree_view_column_new_with_attributes(
			          columns[i], renderer, "text", i + DOLL_TRANSLUCENT, nullptr);
			if (i + DOLL_TRANSLUCENT > DOLL_SPOT_TYPE)
				gtk_tree_view_column_set_visible(col, false);
			gtk_tree_view_append_column(dolltree, col);
		}
		add_terminal_columns(dolltree, DOLL_FROM_PATCH, DOLL_MODIFIED, DOLL_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(dolltree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkTreeView *cnttree = GTK_TREE_VIEW(
	                           get_widget("shinfo_cntrules_list"));
	g_signal_connect(G_OBJECT(cnttree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_cntrules_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(cnttree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          CNT_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(cnttree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Shape", renderer, "text", CNT_SHAPE_COLUMN, nullptr);
		gtk_tree_view_append_column(cnttree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Accept", renderer, "text", CNT_ACCEPT_COLUMN, nullptr);
		gtk_tree_view_append_column(cnttree, col);
		add_terminal_columns(cnttree, CNT_FROM_PATCH, CNT_MODIFIED, CNT_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(cnttree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkTreeView *flagtree = GTK_TREE_VIEW(
	                            get_widget("shinfo_frameflags_list"));
	g_signal_connect(G_OBJECT(flagtree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_frameflags_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(flagtree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          FRFLAG_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(flagtree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", FRFLAG_FRAME_COLUMN, nullptr);
		gtk_tree_view_append_column(flagtree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Quality", renderer, "text", FRFLAG_QUAL_COLUMN, nullptr);
		gtk_tree_view_append_column(flagtree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Flags", renderer, "text", FRFLAG_FLAGS_COLUMN, nullptr);
		gtk_tree_view_append_column(flagtree, col);
		add_terminal_columns(flagtree, FRFLAG_FROM_PATCH, FRFLAG_MODIFIED, FRFLAG_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(flagtree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkTreeView *ucfuntree = GTK_TREE_VIEW(
	                             get_widget("shinfo_frameusecode_list"));
	g_signal_connect(G_OBJECT(ucfuntree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_frameusecode_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(ucfuntree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          FRUC_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING,
		                          G_TYPE_INT, G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(ucfuntree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", FRUC_FRAME_COLUMN, nullptr);
		gtk_tree_view_append_column(ucfuntree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Quality", renderer, "text", FRUC_QUAL_COLUMN, nullptr);
		gtk_tree_view_append_column(ucfuntree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Usecode", renderer, "text", FRUC_USEFUN_COLUMN, nullptr);
		gtk_tree_view_append_column(ucfuntree, col);
		add_terminal_columns(ucfuntree, FRUC_FROM_PATCH, FRUC_MODIFIED, FRUC_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(ucfuntree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkTreeView *nametree = GTK_TREE_VIEW(
	                            get_widget("shinfo_framenames_list"));
	g_signal_connect(G_OBJECT(nametree), "cursor-changed",
	                 G_CALLBACK(on_shinfo_framenames_list_cursor_changed),
	                 nullptr);
	model = gtk_tree_view_get_model(nametree);
	if (!model) {
		GtkTreeStore *store = gtk_tree_store_new(
		                          FNAME_COLUMN_COUNT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT,
		                          G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT,
		                          G_TYPE_INT, GDK_TYPE_PIXBUF);
		gtk_tree_view_set_model(nametree, GTK_TREE_MODEL(store));
		g_object_unref(store);
		// Create each column.
		GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
		                             "Frame", renderer, "text", FNAME_FRAME, nullptr);
		gtk_tree_view_append_column(nametree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Quality", renderer, "text", FNAME_QUALITY, nullptr);
		gtk_tree_view_append_column(nametree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Type", renderer, "text", FNAME_MSGTYPE, nullptr);
		gtk_tree_view_column_set_visible(col, false);
		gtk_tree_view_append_column(nametree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Name", renderer, "text", FNAME_MSGSTR, nullptr);
		gtk_tree_view_append_column(nametree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "Other Type", renderer, "text", FNAME_OTHERTYPE, nullptr);
		gtk_tree_view_column_set_visible(col, false);
		gtk_tree_view_append_column(nametree, col);
		renderer = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(
		          "(Pre|Suf)fix/Default", renderer, "text", FNAME_OTHERMSG, nullptr);
		gtk_tree_view_append_column(nametree, col);
		add_terminal_columns(nametree, FNAME_FROM_PATCH, FNAME_MODIFIED, FNAME_SHAPE_IMAGE);
	} else {
		// Clear it.
		GtkTreeModel *model = gtk_tree_view_get_model(
		                          GTK_TREE_VIEW(nametree));
		GtkTreeStore *store = GTK_TREE_STORE(model);
		gtk_tree_store_clear(store);
	}

	GtkWidget *notebook = get_widget("shinfo_notebook");
	if (info)
		init_shape_notebook(*info, notebook, shnum, frnum);
	else
		gtk_widget_hide(notebook);
	gtk_widget_show(shapewin);
}

/*
 *  Save the shape-editing window.
 */

void ExultStudio::save_shape_window(
) {
	const int shnum = get_num_entry("shinfo_shape");
	const int frnum = get_num_entry("shinfo_frame");
	auto *info = static_cast<Shape_info *>(
	                 g_object_get_data(G_OBJECT(shapewin), "user_data"));
	auto *file_info = static_cast<Shape_file_info *>(
	                      g_object_get_data(G_OBJECT(shapewin), "file_info"));
	Vga_file *ifile = file_info->get_ifile();
	if (info) {         // If 'shapes.vga', get name.
		const string locnm(convertFromUTF8(get_text_entry("shinfo_name")));
		const gchar *nm = locnm.c_str();
		if (!nm)
			nm = "";
		const char *oldname = get_shape_name(shnum);
		if (!oldname)
			oldname = "";
		if (strcmp(nm, oldname) != 0) {
			// Name changed.
			Set_item_name(shnum, nm);
			shape_names_modified = true;
		}
	}
	// Update origin.
	Shape_frame *frame = ifile->get_shape(shnum, frnum);
	if (frame != nullptr) {
		const int xright = get_spin("shinfo_originx");
		const int ybelow = get_spin("shinfo_originy");
		if (xright != frame->get_xright() || ybelow != frame->get_ybelow()) {
			// It changed.
			file_info->set_modified();
			frame->set_offset(xright, ybelow);
			Shape *shape = ifile->extract_shape(shnum);
			shape->set_modified();
		}
	}
	if (info)
		save_shape_notebook(*info, shnum, frnum);
}

/*
 *  Close the shape-editing window.
 */

void ExultStudio::close_shape_window(
) {
	if (shapewin)
		gtk_widget_hide(shapewin);
}
