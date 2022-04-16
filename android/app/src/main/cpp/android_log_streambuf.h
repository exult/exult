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

#include <jni.h>

#include <streambuf>
#include <string>

/**
 * Implementation of @c std::streambuf which routes all content to the android logger.  This is
 * useful for redirecting @c std::cout and @c std::cerr to simplify debugging.
 */
class AndroidLog_streambuf : public std::streambuf {
public:
	/**
	 * Constructs a new @c AndroidLog_streambuf
	 *
	 * @param priority The android logger priority to use for messages.
	 * @param tag The android logger tag to use for messages.
	 */
	AndroidLog_streambuf(int priority, const char* tag);

protected:
	int_type overflow(int_type ch) override;

private:
	/// The android logger priority to use for messages.
	const int m_priority;

	/// The android logger tag to use for messages.
	const std::string m_tag;

	/// Buffer to accumulate characters in until we reach the end of a line.
	std::string m_lineBuf;

	/// JNI Environment for routing logs to UI
	JNIEnv*			m_jniEnv;

	/// The Activity to use for routing logs to UI
	jobject			m_exultActivityObject;

	/// The Method to use for routing logs to UI
	jmethodID		m_writeToConsoleMethod;
};
