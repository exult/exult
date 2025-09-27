/*
 * Copyright (C) 2015 Chaoji Li
 * Copyright (C) 2015-2025 The Exult Team
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

#ifndef IOS_UTILS_H
#define IOS_UTILS_H

#include "touchui.h"

class TouchUI_iOS : public TouchUI {
public:
	TouchUI_iOS();
	void promptForName(const char* name) final;
	void showGameControls() final;
	void hideGameControls() final;
	void showButtonControls() final;
	void hideButtonControls() final;
	void showPauseControls() final;
	void hidePauseControls() final;
	void onDpadLocationChanged() final;
};

const char* IOSGetDocumentsDir();

extern "C" {
void        iOS_SetupQuickActions();
const char* iOS_GetLaunchGameFlag();
void        iOS_ClearLaunchGameFlag();
}

#endif
