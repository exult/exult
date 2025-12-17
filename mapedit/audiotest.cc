/*
Copyright (C) 2000-2025 The Exult Team

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

#include "endianio.h"
#include "servemsg.h"
#include "studio.h"
#include "utils.h"

/*
 *  GTK callback handlers for audio testing dialog.
 */

C_EXPORT void on_play_audio_activate(
		GtkMenuItem* menuitem, gpointer user_data) {
	ignore_unused_variable_warning(menuitem, user_data);
	ExultStudio::get_instance()->play_audio_dialog();
}

C_EXPORT void on_audio_type_combo_changed(
		GtkComboBox* combo, gpointer user_data) {
	ignore_unused_variable_warning(user_data);
	const int    type         = gtk_combo_box_get_active(combo);
	ExultStudio* studio       = ExultStudio::get_instance();
	GtkWidget*   repeat_check = studio->get_widget("audio_repeat_check");

	if (repeat_check) {
		// Disable repeat checkbox for voice (type == 2)
		gtk_widget_set_sensitive(repeat_check, type != 2);
		if (type == 2) {
			gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(repeat_check), false);
		}
	}
}

C_EXPORT void on_play_audio_play_clicked(
		GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio* studio = ExultStudio::get_instance();

	GtkWidget* type_combo   = studio->get_widget("audio_type_combo");
	GtkWidget* track_spin   = studio->get_widget("audio_track_spin");
	GtkWidget* volume_spin  = studio->get_widget("audio_volume_spin");
	GtkWidget* repeat_check = studio->get_widget("audio_repeat_check");

	if (!type_combo || !track_spin || !volume_spin || !repeat_check) {
		return;
	}

	const int type = gtk_combo_box_get_active(GTK_COMBO_BOX(type_combo));
	const int track
			= gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(track_spin));
	const int volume
			= gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(volume_spin));
	const bool repeat
			= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(repeat_check));

	studio->play_audio(type, track, volume, repeat);
}

C_EXPORT void on_play_audio_stop_clicked(
		GtkButton* button, gpointer user_data) {
	ignore_unused_variable_warning(button, user_data);
	ExultStudio::get_instance()->stop_audio();
}

/*
 *  ExultStudio member function implementations.
 */

/*
 *  Open audio playback dialog.
 */

void ExultStudio::play_audio_dialog() {
	GtkWidget* dialog = get_widget("play_audio_dialog");
	if (!dialog) {
		return;
	}

	// Reset dialog to defaults
	GtkWidget* type_combo   = get_widget("audio_type_combo");
	GtkWidget* track_spin   = get_widget("audio_track_spin");
	GtkWidget* volume_spin  = get_widget("audio_volume_spin");
	GtkWidget* repeat_check = get_widget("audio_repeat_check");

	if (type_combo) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);
	}
	if (track_spin) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(track_spin), 0);
	}
	if (volume_spin) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(volume_spin), 100);
	}
	if (repeat_check) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(repeat_check), false);
		gtk_widget_set_sensitive(repeat_check, true);    // Enable by default
	}

	gtk_widget_show(dialog);
}

/*
 *  Play audio (music/sfx/voice).
 */

void ExultStudio::play_audio(int type, int track, int volume, bool repeat) {
	unsigned char  data[Exult_server::maxlength];
	unsigned char* ptr = &data[0];
	Write1(ptr, type);    // 0=music, 1=sfx, 2=voice
	little_endian::Write2(ptr, track);
	little_endian::Write2(ptr, volume);
	Write1(ptr, repeat ? 1 : 0);
	send_to_server(Exult_server::play_audio, data, ptr - data);
}

/*
 *  Stop all audio playback.
 */

void ExultStudio::stop_audio() {
	send_to_server(Exult_server::stop_audio, nullptr, 0);
}
