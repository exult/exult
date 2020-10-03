/*
 *  filesystem.h - Filesystem operations
 *
 *  Copyright (C) 2020 Aleksander Miera
 *  Copyright (C) 2000-2020  The Exult Team
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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <system_error>
#include <string>

namespace fs {
using perms = unsigned;
bool exists(const std::string& file, std::error_code& err) noexcept;
bool create_directory(const std::string& file, std::error_code& err) noexcept;
bool remove(const std::string& file, std::error_code& err) noexcept;
void permissions(const std::string& file, perms prms, std::error_code& err) noexcept;
} /* namespace fs */

#endif /* FILESYSTEM_H */


