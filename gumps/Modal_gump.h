/*
Copyright (C) 2000-2024 The Exult Team

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

#ifndef MODAL_GUMP_H
#define MODAL_GUMP_H

#include "Gump.h"
#include "ignore_unused_variable_warning.h"
#include <chrono>
/*
 *  A modal gump object represents a 'dialog' that grabs the mouse until
 *  the user clicks okay.
 */
class Modal_gump : public Gump {
protected:
	bool         done;      // true when user clicks checkmark.
	Gump_button* pushed;    // Button currently being pushed.

public:
	Modal_gump(
			Container_game_object* cont, int initx, int inity, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA);

	// Create centered.
	Modal_gump(
			Container_game_object* cont, int shnum,
			ShapeFile shfile = SF_GUMPS_VGA);

	bool is_done() {
		return done;
	}

	bool is_modal() const override {
		return true;
	}

	virtual bool run();

	bool is_draggable() const override {
		return !no_dragging;
	}

	bool mouse_down(int mx, int my, MouseButton button) override;

	bool mouse_up(int mx, int my, MouseButton button) override;

	bool mouse_drag(int mx, int my) override;

public:
	enum class DragType {
		Unknown,
		Never,
		Always,
		Offscreen,
	};

	// Get the user set drag type from exult.cfg
	static DragType GetDragType();
	// Set new dragtype and update exult.cfg
	static void SetDragType(DragType newtype);

private:
	// the previous position of the mouse during a drag, INT_MIN if not dragging
	int drag_mx, drag_my;

	// the last known good position of the gump that it will be returned to if
	// the user drags it completely off screen only valid while dragging
	int lastgood_x, lastgood_y;

protected:
	// If set this gump cannot be dragged
	bool no_dragging;


	TileRect procedural_background;

	ShapeID checkmark_background;

	// Colours used to draw the background.
	struct ProceduralColours {
		uint8 border     = 0;      // black
		uint8 Background = 142;    // brown
		uint8 Highlight;           // 2 brighter than backgriund
		uint8 Highlight2;          // 4 brighter than background
		uint8 Shadow;              // 2 darker tab background

		ProceduralColours() {
			CalculateHighlightAndShadow();
		}

		void CalculateHighlightAndShadow() {
			Highlight  = Background - 2;
			Shadow     = Background + 2;
			Highlight2 = Background - 4;
		}

		// Remap the current colours to a new ramp (default ramp is 9)
		void RemapColours(int newramp);
	};

	// Change as desired
	ProceduralColours procedural_colours;

	//! Set ths gump to have a procedurally drawn background
	//! \param backrect The inner rectangle for the gumps background
	//! \param Checkbg_paletteramp Colour ramp to use for the checkmark
	//!  button background. if -1 use default green that is the same as 11
	//! \param centre_gump_on_screen automatically centre the gump on screen when done
	//! \remarks This will reset procedural_colours to default 
	//! and create the checkmark button if it doesn't aready exist
	//! This will not supress rendeing of a background shape. 
	//! To use this:
	//! set shnum to -1 in the constructor Argument
	//! and then call this funcion in the constructor. Do not call
	//! Gump::set_object_area() to create the checkmark button
	void SetProceduralBackground(
			TileRect backrect,
			int    Checkbg_paletteramp
			= -1,
		bool centre_gump_on_screen = true);

public:
	void paint() override;

	TileRect get_rect() const override;

private:
	std::string                           popup_message;
	std::chrono::steady_clock::time_point popup_message_timeout;

public:
	//! Set a message to display above the gump
	void SetPopupMessage(const std::string& message, int mstimeout = 5000);
};

#endif
