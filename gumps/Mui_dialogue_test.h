/*
Copyright (C) 2026 The Exult Team

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

#ifndef MUI_DIALOGUE_TEST_H
#define MUI_DIALOGUE_TEST_H

#include <string>

/*
 *  Conversation commands for the menu test driver (Mui_selftest): start a
 *  conversation (talk, call-usecode), inspect it (expect-conv-*, conv-log,
 *  conv-dump) and answer it (conv-choose-key). Only reached while
 *  EXULT_MUI_TEST runs a script.
 */
namespace Mui_dialogue_test {
	// Run cmd if it is a conversation command; false if it is not one.
	bool run(const std::string& cmd, const std::string& rest);
}    // namespace Mui_dialogue_test

#endif
