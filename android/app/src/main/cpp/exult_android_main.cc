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

#include "android_log_streambuf.h"

#include <SDL_main.h>
#include <android/log.h>
#include <jni.h>

#include <iostream>

extern "C" JNIEXPORT int JNICALL
ExultAndroid_main(int argc, char* argv[]) {
	AndroidLog_streambuf exult_cout(ANDROID_LOG_DEBUG, "exult-android-wrapper");
	AndroidLog_streambuf exult_cerr(ANDROID_LOG_ERROR, "exult-android-wrapper");
	auto                 ndk_cout = std::cout.rdbuf(&exult_cout);
	auto                 ndk_cerr = std::cerr.rdbuf(&exult_cerr);

	auto result = SDL_main(argc, argv);

	std::cout.rdbuf(ndk_cout);
	std::cerr.rdbuf(ndk_cerr);

	// note: `exit(0)` rather than `return 0` to ensure proper cleanup of resources
	//	 between runs.
	exit(0);
}
