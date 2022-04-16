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

import android.util.Log;
import com.sprylab.xar.XarEntry;
import com.sprylab.xar.XarSource;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.archivers.ArchiveInputStream;

class XarArchiveInputStream extends ArchiveInputStream {
  private static final byte[] XAR_MAGIC = "xar!".getBytes();
  private Iterator<XarEntry> m_nextXarEntry;
  private InputStream m_xarEntryInputStream;

  XarArchiveInputStream(InputStream inputStream) {
    XarSource xarSource = new InputStreamXarSource(inputStream);
    List<XarEntry> xarEntries = null;
    try {
      xarEntries = xarSource.getEntries();
    } catch (Exception e) {
      Log.d("XarArchiveInputStream", "exception getting XAR entries: " + e.toString());
      // TODO: throw something?
    }

    // Awful hack to get access to entry offsets so we can sort the and ensure in-order access from
    // the InputStream.
    // This is needed because the sprylab xar decoder returns the entries in alphabetical order
    // rather than sorting
    // them by offset, and it doesn't expose the offset field publicly.
    Field xarEntryOffsetField_ = null;
    try {
      xarEntryOffsetField_ = XarEntry.class.getDeclaredField("offset");
      xarEntryOffsetField_.setAccessible(true);
    } catch (Exception e) {
      Log.d("ExultLauncherActivity", "exception accessing XarEntry offset field: " + e.toString());
    }
    final Field xarEntryOffsetField = xarEntryOffsetField_;
    xarEntries.sort(
        new Comparator<XarEntry>() {
          @Override
          public int compare(XarEntry xarEntry1, XarEntry xarEntry2) {
            long offset1 = 0;
            long offset2 = 0;
            try {
              offset1 = (long) xarEntryOffsetField.get(xarEntry1);
              offset2 = (long) xarEntryOffsetField.get(xarEntry2);
            } catch (Exception e) {
              Log.d("ExultLauncherActivity", "exception reading offset: " + e.toString());
            }
            if (offset1 < offset2) {
              return -1;
            }
            if (offset1 > offset2) {
              return 1;
            }
            return 0;
          }
        });

    m_nextXarEntry = xarEntries.iterator();
  }

  @Override
  public ArchiveEntry getNextEntry() throws IOException {
    if (!m_nextXarEntry.hasNext()) {
      return null;
    }
    XarEntry xarEntry = m_nextXarEntry.next();
    if (xarEntry.isDirectory()) {
      m_xarEntryInputStream = null;
    } else {
      m_xarEntryInputStream = xarEntry.getInputStream();
    }
    return new XarArchiveEntry(xarEntry);
  }

  @Override
  public int read() throws IOException {
    return m_xarEntryInputStream.read();
  }

  @Override
  public int read(byte[] b, int off, int len) throws IOException {
    return m_xarEntryInputStream.read(b, off, len);
  }

  public static boolean matches(byte[] signature, int length) {
    if (length < XAR_MAGIC.length) {
      return false;
    }
    for (int i = 0; i < XAR_MAGIC.length; ++i) {
      if (signature[i] != XAR_MAGIC[i]) {
        return false;
      }
    }
    return true;
  }
}
