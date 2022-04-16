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

package info.exult;

import com.sprylab.xar.XarEntry;
import java.util.Date;
import org.apache.commons.compress.archivers.ArchiveEntry;

class XarArchiveEntry implements ArchiveEntry {
  private final XarEntry m_xarEntry;

  XarArchiveEntry(XarEntry xarEntry) {
    m_xarEntry = xarEntry;
  }

  @Override
  public String getName() {
    return m_xarEntry.getName();
  }

  @Override
  public long getSize() {
    return m_xarEntry.getSize();
  }

  @Override
  public boolean isDirectory() {
    return m_xarEntry.isDirectory();
  }

  @Override
  public Date getLastModifiedDate() {
    return m_xarEntry.getTime();
  }
}
