/*
 * Copyright (C) 2015-2020 Litchie
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

class TouchUI_iOS: public TouchUI {
public:
	TouchUI_iOS();
	void promptForName(const char *name) final;
	void showGameControls() final;
	void hideGameControls() final;
	void showButtonControls() final;
	void hideButtonControls() final;
	void onDpadLocationChanged() final;
};

const char* ios_get_documents_dir();
void ios_open_url(const char *);

#endif
