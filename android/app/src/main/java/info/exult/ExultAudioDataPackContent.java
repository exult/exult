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

import android.content.Context;
import android.util.Log;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import org.apache.commons.compress.archivers.ArchiveEntry;

class ExultAudioDataPackContent extends ExultContent {
	private HashMap<String, Boolean> m_filenameToFoundFlag
			= new HashMap<String, Boolean>();
	private HashMap<String, Boolean> m_MT32filenameToFoundFlag
			= new HashMap<String, Boolean>();
	private HashMap<String, Boolean> m_SF2filenameToFoundFlag
			= new HashMap<String, Boolean>();
	private String  m_audioName;
	private int     m_filenamesFound = 0;
	private Path    m_installPath;
	private Path    m_dataRootInArchive;
	private Boolean foundFlag = false;

	public ExultAudioDataPackContent(String name, Context context) {
		super("audioDataPack", name, context);

		m_audioName = name.toLowerCase();
		// List of filenames to expect in the all-in-one audio pack.
		m_filenameToFoundFlag.put("jmsfx.flx", false);
		m_filenameToFoundFlag.put("jmsisfx.flx", false);
		m_filenameToFoundFlag.put("sqsfxbg.flx", false);
		m_filenameToFoundFlag.put("sqsfxsi.flx", false);
		m_filenameToFoundFlag.put(
				"00bg.ogg", false);    // implies all bg music files are present
		m_filenameToFoundFlag.put(
				"si01.ogg", false);    // implies all si music files are present
		m_MT32filenameToFoundFlag.put("MT32_CONTROL.ROM", false);
		m_MT32filenameToFoundFlag.put("MT32_PCM.ROM", false);
		m_SF2filenameToFoundFlag.put("sf2_placeholder", false);

		m_installPath = context.getFilesDir().toPath().resolve("data");
	}

	@Override
	protected boolean identify(
			Path location, ArchiveEntry archiveEntry, InputStream inputStream)
			throws IOException {
		// Nothing to do for the terminal call - we've failed to find a valid
		// audio data pack if we get that far.
		if (null == archiveEntry) {
			return false;
		}

		Log.d("ExultAudioPackContent", "identify " + archiveEntry.getName());
		// Skip over directories.
		if (archiveEntry.isDirectory()) {
			return false;
		}

		// Check to see if it matches one of the filenames we are matching.
		Path   fullPath = getFullArchivePath(location, archiveEntry);
		String lowerCaseFileName
				= fullPath.getFileName().toString().toLowerCase();
		// MT32 ROM and Soundfont need to be the exact case given
		String keepCaseFileName = fullPath.getFileName().toString();
		if (m_audioName.equals("allinone")) {
			foundFlag = m_filenameToFoundFlag.get(lowerCaseFileName);
		} else if (m_audioName.equals("mt32roms")) {
			foundFlag = m_MT32filenameToFoundFlag.get(keepCaseFileName);
		} else if (m_audioName.equals("soundfont")) {
			// Check for SF2 or DLS files by extension instead of specific
			// filename
			if (lowerCaseFileName.endsWith(".sf2")
				|| lowerCaseFileName.endsWith(".dls")) {
				// We found an SF2 or DLS file, consider that a match to our
				// placeholder
				foundFlag = m_SF2filenameToFoundFlag.containsKey(
									"sf2_placeholder")
									? false
									: null;
				// If we haven't already found an SF2 or DLS file, we'll process
				// this one
				if (!m_SF2filenameToFoundFlag.get("sf2_placeholder")) {
					m_filenamesFound = 1;
				}
			} else {
				foundFlag = null;
			}
		}
		// If it's not a file we're looking for, or if it has already been
		// found, move along.
		if (null == foundFlag || true == foundFlag) {
			return false;
		}

		// If we haven't identified a data root yet, infer it from the file
		// path.
		if (null == m_dataRootInArchive) {
			if (lowerCaseFileName.endsWith(".flx")
				|| lowerCaseFileName.endsWith(".rom")
				|| lowerCaseFileName.endsWith(".sf2")
				|| lowerCaseFileName.endsWith(".dls")) {
				// ".flx", ".rom", ".sf2", ".dls" files live at the data root.
				m_dataRootInArchive = fullPath.getParent();
				if (null == m_dataRootInArchive) {
					m_dataRootInArchive = Paths.get("");
				}
			} else if (lowerCaseFileName.endsWith(".ogg")) {
				// ".ogg" files live in a "music" subdir off the data root.
				Path oggParent = fullPath.getParent();
				if (oggParent.getFileName().toString().toLowerCase().equals(
							"music")) {
					m_dataRootInArchive = oggParent.getParent();
					if (null == m_dataRootInArchive) {
						m_dataRootInArchive = Paths.get("");
					}
				}
			}

			// If we couldn't identify a data root, then this file isn't what
			// we're looking for.
			if (null == m_dataRootInArchive) {
				return false;
			}
		} else {
			// If we already know the data root, make sure this file is in it.
			if (lowerCaseFileName.endsWith(".flx")
				|| lowerCaseFileName.endsWith(".rom")
				|| lowerCaseFileName.endsWith(".sf2")
				|| lowerCaseFileName.endsWith(".dls")) {
				// ".flx", ".rom", ".sf2", ".dls" files live at the data root.
				Path    parent     = fullPath.getParent();
				boolean nullParent = null == parent;
				boolean emptyRoot  = m_dataRootInArchive.toString().isEmpty();
				if (nullParent && !emptyRoot
					|| !nullParent && !m_dataRootInArchive.equals(parent)) {
					return false;
				}
			} else if (lowerCaseFileName.endsWith(".ogg")) {
				// ".ogg" files live in a "music" subdir off the data root.
				if (!fullPath.startsWith(
							m_dataRootInArchive.resolve("music"))) {
					return false;
				}
			} else {
				// unknown file type
				return false;
			}
		}

		// If we got this far, we've got a valid match to a previously unmatched
		// file - update our records.
		foundFlag = true;
		++m_filenamesFound;

		// Keep searching if we haven't found all our filenames yet.
		if (m_audioName.equals("allinone")) {
			return m_filenameToFoundFlag.size() == m_filenamesFound;
		} else if (m_audioName.equals("mt32roms")) {
			return m_MT32filenameToFoundFlag.size() == m_filenamesFound;
		} else if (m_audioName.equals("soundfont")) {
			return m_filenamesFound >= 1;
		}
		return false;
	}

	@Override
	protected Path getInstallDestination(
			Path location, ArchiveEntry archiveEntry) {
		// Only want to install flx/ogg/rom/sf2/dls files; skip over everything
		// else
		String lowerCaseFileName = archiveEntry.getName().toLowerCase();
		if (!lowerCaseFileName.endsWith(".flx")
			&& !lowerCaseFileName.endsWith(".ogg")
			&& !lowerCaseFileName.endsWith(".rom")
			&& !lowerCaseFileName.endsWith(".sf2")
			&& !lowerCaseFileName.endsWith(".dls")) {
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
