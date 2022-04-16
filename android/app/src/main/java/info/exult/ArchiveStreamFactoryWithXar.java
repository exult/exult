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

import java.io.IOException;
import java.io.InputStream;
import java.util.Set;
import org.apache.commons.compress.archivers.ArchiveException;
import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveStreamFactory;
import org.apache.commons.io.IOUtils;

/**
 * Adds detection of Xar archives to ArchiveStreamFactory, which is missing in Apache Commons
 * Compress 1.20 API.
 *
 * @see https://commons.apache.org/proper/commons-compress/javadocs/api-1.20/
 */
class ArchiveStreamFactoryWithXar extends ArchiveStreamFactory {

  /** Constant (value {@value}) used to identify the XAR archive format. */
  public static final String XAR = "xar";

  /** Sufficient bytes from start of a XAR file to match magic number. */
  private static final int XAR_SIGNATURE_SIZE = 4;

  @Override
  public ArchiveInputStream createArchiveInputStream(final InputStream in) throws ArchiveException {
    return createArchiveInputStream(detect(in), in);
  }

  @Override
  public ArchiveInputStream createArchiveInputStream(String archiverName, InputStream in)
      throws ArchiveException {
    if (archiverName.equals(XAR)) {
      return new XarArchiveInputStream(in);
    } else {
      return super.createArchiveInputStream(archiverName, in);
    }
  }

  /** Adds XAR detection to org.apache.commons.compress.archivers.ArchiveStreamFactory.detect(). */
  public static String detect(InputStream in) throws ArchiveException {
    if (in != null && in.markSupported()) {
      final byte[] signature = new byte[XAR_SIGNATURE_SIZE];
      in.mark(signature.length);
      try {
        IOUtils.readFully(in, signature);
        in.reset();
      } catch (IOException e) {
        throw new ArchiveException("IOException while reading signature.", e);
      }
      if (XarArchiveInputStream.matches(signature, signature.length)) {
        return XAR;
      }
    }
    return ArchiveStreamFactory.detect(in);
  }

  @Override
  public Set<String> getInputStreamArchiveNames() {
    Set<String> inputStreamArchiveNames = super.getInputStreamArchiveNames();
    inputStreamArchiveNames.add(XAR);
    return inputStreamArchiveNames;
  }
}
