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

#include <utils.h>

#include <limits>

uint32 TouchUI::eventType = std::numeric_limits<uint32>::max();

void TouchUI::onTextInput(const char* text) {
	if (text == nullptr) {
		return;
	}

	SDL_Event event;
	event.user = {TouchUI::eventType, 0,      0, TouchUI::EVENT_CODE_TEXT_INPUT,
				  newstrdup(text),    nullptr};
	SDL_PushEvent(&event);
}

TouchUI::TouchUI() {
	TouchUI::eventType = SDL_RegisterEvents(1);
}
