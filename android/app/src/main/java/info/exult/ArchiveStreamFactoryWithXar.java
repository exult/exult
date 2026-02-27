/*
 *  Copyright (C) 2021-2025  The Exult Team
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
 * Adds detection of Xar archives to ArchiveStreamFactory, which is missing in
 * Apache Commons Compress API.
 */

public final class ArchiveStreamFactoryWithXar {
	public static final String XAR                = "xar";
	private static final int   XAR_SIGNATURE_SIZE = 4;

	private ArchiveStreamFactoryWithXar() {}

	public static ArchiveInputStream createArchiveInputStream(
			final InputStream in) throws ArchiveException {
		return createArchiveInputStream(detect(in), in);
	}

	public static ArchiveInputStream createArchiveInputStream(
			final String archiverName, final InputStream in)
			throws ArchiveException {
		try {
			if (XAR.equals(archiverName)) {
				return new XarArchiveInputStream(in);
			}
			return new ArchiveStreamFactory().createArchiveInputStream(
					archiverName, in);
		} catch (IOException e) {
			throw new ArchiveException(
					"Failed to create XAR archive input stream", e);
		}
	}

	// Adds XAR detection to ArchiveStreamFactory.detect()
	public static String detect(InputStream in) throws ArchiveException {
		if (in != null && in.markSupported()) {
			final byte[] signature = new byte[XAR_SIGNATURE_SIZE];
			in.mark(signature.length);
			try {
				IOUtils.readFully(in, signature);
				in.reset();
			} catch (IOException e) {
				throw new ArchiveException(
						"IOException while reading signature.", e);
			}
			if (XarArchiveInputStream.matches(signature, signature.length)) {
				return XAR;
			}
		}
		return ArchiveStreamFactory.detect(in);
	}
}