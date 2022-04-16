/*
 *  Copyright (C) 2021-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TOUCHUI_ANDROID_H
#define TOUCHUI_ANDROID_H

#include <jni.h>
#include <touchui.h>

class TouchUI_Android: public TouchUI {
public:
	TouchUI_Android();
	~TouchUI_Android() final;
	void promptForName(const char* name) final;
	void showGameControls() final;
	void hideGameControls() final;
	void showButtonControls() final;
	void hideButtonControls() final;
	void onDpadLocationChanged() final;

	static TouchUI_Android* getInstance();
	void                    setVirtualJoystick(Sint16 x, Sint16 y);
        void                    sendEscapeKeypress();

private:
	static TouchUI_Android* m_instance;
	JNIEnv*                 m_jniEnv;
	jobject                 m_exultActivityObject;
	jmethodID               m_showGameControlsMethod;
	jmethodID               m_hideGameControlsMethod;
	jmethodID               m_showButtonControlsMethod;
	jmethodID               m_hideButtonControlsMethod;
	SDL_GameController*     m_gameController;
	jmethodID               m_promptForNameMethod;
};

#endif
