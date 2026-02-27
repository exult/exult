/*
 * Copyright (C) 2015  Chaoji Li
 * Copyright (C) 2015-2022  The Exult Team
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

#include "touchui.h"

#include "gamewin.h"

#include <cstring>
#include <limits>

uint32 TouchUI::eventType = std::numeric_limits<uint32>::max();

void TouchUI::onTextInput(const char* text) {
	if (text == nullptr) {
		return;
	}

	SDL_Event event;
	SDL_zero(event);
	event.type       = TouchUI::eventType;
	event.user.code  = TouchUI::EVENT_CODE_TEXT_INPUT;
	event.user.data1 = strdup(text);
	event.user.data2 = nullptr;
	SDL_PushEvent(&event);
}

void TouchUI::startTextInput(SDL_Window* window) {
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
	SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_NONE);
	SDL_StartTextInputWithProperties(window, props);
	SDL_DestroyProperties(props);
}

void TouchUI::setTextInputArea(SDL_Window* window, int gx1, int gy1, int gx2, int gy2) {
	Game_window* gwin = Game_window::get_instance();
	int          sx1, sy1, sx2, sy2;
	gwin->get_win()->game_to_screen(gx1, gy1, false, sx1, sy1);
	gwin->get_win()->game_to_screen(gx2, gy2, false, sx2, sy2);
	SDL_Renderer* renderer = SDL_GetRenderer(window);
	float         wx1, wy1, wx2, wy2;
	SDL_RenderCoordinatesToWindow(renderer, sx1, sy1, &wx1, &wy1);
	SDL_RenderCoordinatesToWindow(renderer, sx2, sy2, &wx2, &wy2);
	SDL_Rect windowRect = {static_cast<int>(wx1), static_cast<int>(wy1), static_cast<int>(wx2 - wx1), static_cast<int>(wy2 - wy1)};
	SDL_SetTextInputArea(window, &windowRect, 0);
}

TouchUI::TouchUI() {
	TouchUI::eventType = SDL_RegisterEvents(1);
}
