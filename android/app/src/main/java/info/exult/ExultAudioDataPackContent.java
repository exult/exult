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

import android.content.Context;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import org.apache.commons.compress.archivers.ArchiveEntry;

class ExultAudioDataPackContent extends ExultContent {
  private HashMap<String, Boolean> m_filenameToFoundFlag = new HashMap<String, Boolean>();
  private int m_filenamesFound = 0;
  private Path m_installPath;
  private Path m_dataRootInArchive;

  public ExultAudioDataPackContent(String name, Context context) {
    super("audioDataPack", name, context);

    // List of filenames to expect in the all-in-one audio pack.
    m_filenameToFoundFlag.put("jmsfx.flx", false);
    m_filenameToFoundFlag.put("jmsisfx.flx", false);
    m_filenameToFoundFlag.put("sqsfxbg.flx", false);
    m_filenameToFoundFlag.put("sqsfxsi.flx", false);
    m_filenameToFoundFlag.put("00bg.ogg", false); // implies all bg music files are present
    m_filenameToFoundFlag.put("si01.ogg", false); // implies all si music files are present

    m_installPath = context.getFilesDir().toPath().resolve("data");
  }

  @Override
  protected boolean identify(Path location, ArchiveEntry archiveEntry, InputStream inputStream)
      throws IOException {
    // Nothing to do for the terminal call - we've failed to find a valid audio data pack if we get
    // that far.
    if (null == archiveEntry) {
      return false;
    }

    // Skip over directories.
    if (archiveEntry.isDirectory()) {
      return false;
    }

    // Check to see if it matches one of the filenames we are matching.
    Path fullPath = getFullArchivePath(location, archiveEntry);
    String lowerCaseFileName = fullPath.getFileName().toString().toLowerCase();
    Boolean foundFlag = m_filenameToFoundFlag.get(lowerCaseFileName);

    // If it's not a file we're looking for, or if it has already been find, move along.
    if (null == foundFlag || true == foundFlag) {
      return false;
    }

    // If we haven't identified a data root yet, infer it from the file path.
    if (null == m_dataRootInArchive) {
      if (lowerCaseFileName.endsWith(".flx")) {
        // ".flx" files live at the data root.
        m_dataRootInArchive = fullPath.getParent();
        if (null == m_dataRootInArchive) {
          m_dataRootInArchive = Paths.get("");
        }
      } else if (lowerCaseFileName.endsWith(".ogg")) {
        // ".ogg" files live in a "music" subdir off the data root.
        Path oggParent = fullPath.getParent();
        if (oggParent.getFileName().toString().toLowerCase().equals("music")) {
          m_dataRootInArchive = oggParent.getParent();
          if (null == m_dataRootInArchive) {
            m_dataRootInArchive = Paths.get("");
          }
        }
      }

      // If we couldn't identify a data root, then this file isn't what we're looking for.
      if (null == m_dataRootInArchive) {
        return false;
      }
    } else {
      // If we already know the data root, make sure this file is in it.
      if (lowerCaseFileName.endsWith(".flx")) {
        // ".flx" files live at the data root.
        Path parent = fullPath.getParent();
        boolean nullParent = null == parent;
        boolean emptyRoot = m_dataRootInArchive.toString().isEmpty();
        if (nullParent && !emptyRoot || !nullParent && !m_dataRootInArchive.equals(parent)) {
          return false;
        }
      } else if (lowerCaseFileName.endsWith(".ogg")) {
        // ".ogg" files live in a "music" subdir off the data root.
        if (!fullPath.startsWith(m_dataRootInArchive.resolve("music"))) {
          return false;
        }
      } else {
        // unknown file type
        return false;
      }
    }

    // If we got this far, we've got a valid match to a previously unmatched file - update our
    // records.
    foundFlag = true;
    ++m_filenamesFound;

    // Keep searching if we haven't found all our filenames yet.
    return m_filenameToFoundFlag.size() == m_filenamesFound;
  }

  @Override
  protected Path getInstallDestination(Path location, ArchiveEntry archiveEntry) {
    // Only want to install flx/ogg files; skip over everything else
    String lowerCaseFileName = archiveEntry.getName().toLowerCase();
    if (!lowerCaseFileName.endsWith(".flx") && !lowerCaseFileName.endsWith(".ogg")) {
      return null;
    }

    return super.getInstallDestination(location, archiveEntry);
  }

  @Override
  protected Path getContentRootInArchive() {
    return m_dataRootInArchive;
  }

  @Override
  protected Path getContentInstallRoot() {
    return m_installPath;
  }
}
