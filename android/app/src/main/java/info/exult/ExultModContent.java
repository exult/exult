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
import android.util.Log;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.w3c.dom.Document;

class ExultModContent extends ExultContent {
  private String m_modName;
  private Path m_installPath;
  private Path m_modRootInArchive;

  public ExultModContent(String gameToMod, String modName, Context context) {
    super("mod", gameToMod + "." + modName, context);
    m_modName = modName.toLowerCase();
    m_installPath = context.getFilesDir().toPath().resolve(gameToMod).resolve("mods");
  }

  @Override
  protected boolean identify(Path location, ArchiveEntry archiveEntry, InputStream inputStream)
      throws IOException {
    // Nothing to do for the terminal call - we've failed to find a valid mod if we get that far.
    if (null == archiveEntry) {
      Log.d("ExultModContent", "identify: null archiveEntry");
      return false;
    }
    Log.d("ExultModContent", "identify " + archiveEntry.getName());

    // Skip over directories.
    if (archiveEntry.isDirectory()) {
      Log.d("ExultModContent", "isdirectory");
      return false;
    }

    // The Exult engine looks for "<modname>.cfg" to indicate a valid mod.
    Path fullPath = getFullArchivePath(location, archiveEntry);
    if (!fullPath.getFileName().toString().toLowerCase().endsWith(m_modName + ".cfg")) {
      Log.d("ExultModContent", "not " + m_modName + ".cfg");
      return false;
    }

    // A valid mod configuration should contain XML with a root tag of "mod_info"
    DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
    try {
      DocumentBuilder documentBuilder = documentBuilderFactory.newDocumentBuilder();
      Document document = documentBuilder.parse(inputStream);
      if (document.getElementsByTagName("mod_info").getLength() == 0) {
        Log.d("ExultModContent", "missing mod_info");
        return false;
      }
    } catch (Exception e) {
      Log.d("ExultModContent", "exception: " + e.toString());
      return false;
    }

    // If we got here, we have the mod we're looking for.  Note the directory containing it for use
    // by
    // installDestination() and then terminate the search.
    m_modRootInArchive = fullPath.getParent();
    if (null == m_modRootInArchive) {
      m_modRootInArchive = Paths.get("");
    }
    Log.d(
        "ExultModContent",
        "done - rootInArchive=" + m_modRootInArchive + ", installPath=" + m_installPath);
    return true;
  }

  @Override
  protected Path getContentRootInArchive() {
    return m_modRootInArchive;
  }

  @Override
  protected Path getContentInstallRoot() {
    return m_installPath;
  }
}
