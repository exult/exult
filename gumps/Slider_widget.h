#pragma once
#include "Gump_widget.h"

class Slider_widget;

//! Slider_Widget implements most of the functionality to create a slider like
//! used by Slider_Gump exccpt for the text display and background. Text or a
//! background needs to be drawn by the owner if desired.
class Slider_widget : public Gump_widget {
public:
	// Interface to recieve Messages from a Slider Widget
	class ICallback {
	public:
		virtual void OnSliderValueChanged(Slider_widget* sender, int newvalue)
				= 0;

		virtual ~ICallback(){}
	};

	ICallback* callback;

public:
	//! Construct a Slider Widget
	//! /param par Parent of this Widget, If it implemets ICallback, it will be
	//! used as the default callback /param width. How wide the sliding region
	//! should be. determines how far apart the left and right buttons should be
	Slider_widget(
			Gump_Base* par, int px, int py, ShapeID sidLeft, ShapeID sidRight,
			ShapeID sidDiamond, int mival, int mxval, int step, int defval,
			int width = 64);

	// By default the callback is set to par by the construcor if par implements
	// ISlider_widget_callback but if not the callback can be set here
	// call with nullptr to clear the callback
	void setCallback(ICallback* newcalllback) {
		callback = newcalllback;
	}

private:
	int   diamondx;    // Rel. pos. where diamond is shown.
	short diamondy;
	int   min_val, max_val;    // Max., min. values to choose from.
	int   step_val;            // Amount to step by.
	int   val;                 // Current value.
	int   prev_dragx;          // Prev. x-coord. of mouse.
	void set_val(int newval, bool recalcdiamond = true);    // Set to new value.
	// Coords:
	short leftbtnx, rightbtnx, btny;
	short xmin, xmax;

	ShapeID      diamond;    // Diamond
	Gump_button *left, *right;

	Gump_button* pushed;

public:
	int get_val() {    // Get last value set.
		return val;
	}

	// An arrow was clicked on.
	void clicked_left_arrow();
	void clicked_right_arrow();
	void move_diamond(int dir);

	TileRect get_rect() override;

	// Paint it and its contents.
	void paint() override;

	Gump_button* on_button(int mx, int my) override;

	// Handle events:
	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;
	bool key_down(int chr) override;    // Character typed.

	bool mousewheel_up(int mx, int my) override;
	bool mousewheel_down(int mx, int my) override;
};
