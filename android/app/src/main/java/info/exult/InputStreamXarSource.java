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
import com.sprylab.xar.XarException;
import com.sprylab.xar.XarSource;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import okio.BufferedSource;
import okio.Okio;
import okio.Source;

/**
 * XarSource which reads from an arbitrary InputStream. Note that XarSource is supposed to support
 * random access, but this implementation assumes getRange will always move forward. The implication
 * is that a XarEntry must be read in order while traversing the archive, and that a given it can
 * only be read once. Any attempt to read a prior XarEntry or reread the current XarEntry will
 * result in an exception due to InputStreams inability to move backwards. Also note that getSize()
 * will return the initial value of InputStream.available(), but this may be inaccurate for non-file
 * streams.
 */
class InputStreamXarSource extends XarSource {
  private final InputStream m_inputStream;
  private final long m_size;
  private long m_offset;

  InputStreamXarSource(InputStream inputStream) {
    m_inputStream = inputStream;
    long size = -1;
    try {
      size = inputStream.available();
    } catch (Exception e) {
      Log.d("InputStreamXarSource", "exception getting size: " + e.toString());
    }
    m_size = size;
    m_offset = 0;
  }

  @Override
  public BufferedSource getRange(long offset, long length) throws IOException {
    byte[] rangeBuffer = new byte[(int) length];
    long toSkip = offset - m_offset;
    if (toSkip < 0) {
      throw new IOException("Backward skip not possible.");
    }
    while (toSkip > 0) {
      long skipped = m_inputStream.skip(toSkip);
      if (0 == skipped) {
        throw new IOException("Unable to complete skip.");
      }
      if (skipped > toSkip) {
        throw new IOException("Skipped too far.");
      }
      toSkip -= skipped;
    }
    m_offset = offset + length;
    // TODO: check return value
    m_inputStream.read(rangeBuffer, 0, (int) length);
    ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(rangeBuffer);
    Source source = Okio.source(byteArrayInputStream);
    return Okio.buffer(source);
  }

  @Override
  public long getSize() throws XarException {
    if (-1 == m_size) {
      throw new XarException("Size unknown.");
    }
    return m_size;
  }
}
