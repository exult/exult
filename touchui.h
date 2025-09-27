/*
 * Copyright (C) 2015  Chaoji Li
 * Copyright (C) 2015-2025  The Exult Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#ifndef TOUCHUI_H
#define TOUCHUI_H

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include "common_types.h"

class TouchUI {
public:
	static uint32 eventType;

	enum {
		EVENT_CODE_INVALID    = 0,
		EVENT_CODE_TEXT_INPUT = 1
	};

	static void onTextInput(const char* text);

	TouchUI();
	virtual ~TouchUI()                           = default;
	virtual void promptForName(const char* name) = 0;
	virtual void showGameControls()              = 0;
	virtual void hideGameControls()              = 0;
	virtual void showButtonControls()            = 0;
	virtual void hideButtonControls()            = 0;
	virtual void showPauseControls()             = 0;
	virtual void hidePauseControls()             = 0;
	virtual void onDpadLocationChanged()         = 0;
};

#endif
