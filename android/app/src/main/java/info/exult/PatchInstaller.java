/*
 *  Copyright (C) 2025  The Exult Team
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

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.widget.Toast;
import androidx.fragment.app.Fragment;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveStreamFactory;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;

/**
 * Handles installing a patch ZIP into a game's "patch" folder.
 */
public class PatchInstaller {

	private final Context  context;
    private final Fragment parentFragment;
    private final ActivityResultLauncher<String[]> filePickerLauncher;

	// Map short folder names to friendly display names
	private static final Map<String, String> DISPLAY_NAMES = new HashMap<>();
	static {
		// Common Exult game folders (extend as needed)
		DISPLAY_NAMES.put("blackgate", "Ultima VII: The Black Gate");
		DISPLAY_NAMES.put("serpentisle", "Ultima VII Part Two: Serpent Isle");
		DISPLAY_NAMES.put(
				"forgeofvirtue",
				"Ultima VII: The Black Gate + The Forge of Virtue");
		DISPLAY_NAMES.put(
				"silverseed",
				"Ultima VII Part Two: Serpent Isle + The Silver Seed");
		DISPLAY_NAMES.put(
				"serpentbeta", "Ultima VII Part Two: Serpent Isle Beta");
	}

	public PatchInstaller(Context context, Fragment parentFragment) {
        this.context = context;
        this.parentFragment = parentFragment;
        this.filePickerLauncher = parentFragment.registerForActivityResult(
                new ActivityResultContracts.OpenDocument(),
                uri -> {
                    if (uri != null) {
                        handleFilePickerResult(uri);
                    }
                });
    }

    public void launchFilePicker() {
        filePickerLauncher.launch(new String[] {"*/*"});
    }

	public void handleFilePickerResult(Uri fileUri) {
		// Query installed games, then prompt user to choose target
		List<GameEntry> games = getInstalledGames();
		if (games.isEmpty()) {
			Toast.makeText(
						 context, "No installed games found", Toast.LENGTH_LONG)
					.show();
			return;
		}

		CharSequence[] items = new CharSequence[games.size()];
		for (int i = 0; i < games.size(); i++) {
			items[i] = games.get(i).displayName;
		}

		new AlertDialog.Builder(context)
				.setTitle("Install patch for which game?")
				.setItems(
						items,
						(dialog, which) -> {
							GameEntry game = games.get(which);
							installPatchToGameAsync(fileUri, game.shortName);
						})
				.setNegativeButton("Cancel", null)
				.show();
	}

	private static final class GameEntry {
		final String shortName;
		final String displayName;

		GameEntry(String shortName, String displayName) {
			this.shortName   = shortName;
			this.displayName = displayName;
		}
	}

	private List<GameEntry> getInstalledGames() {
		// Consider a game “installed” if filesDir/<game>/static/mainshp.flx
		// exists
		List<GameEntry> games    = new ArrayList<>();
		File            filesDir = context.getFilesDir();
		File[]          children = filesDir.listFiles();
		if (children == null)
			return games;

		for (File f : children) {
			if (!f.isDirectory())
				continue;
			File init = new File(new File(f, "static"), "mainshp.flx");
			if (init.exists()) {
				final String shortName = f.getName();
				games.add(new GameEntry(shortName, toDisplayName(shortName)));
			}
		}
		return games;
	}

	private static String toDisplayName(String shortName) {
		String mapped = DISPLAY_NAMES.get(shortName.toLowerCase(Locale.US));
		if (mapped != null)
			return mapped;
		// Fallback: Title Case the folder name (replace separators with spaces)
		String        cleaned = shortName.replace('_', ' ').replace('-', ' ');
		String[]      parts   = cleaned.split("\\s+");
		StringBuilder sb      = new StringBuilder();
		for (String p : parts) {
			if (p.isEmpty())
				continue;
			if (sb.length() > 0)
				sb.append(' ');
			sb.append(p.substring(0, 1).toUpperCase(Locale.US));
			if (p.length() > 1)
				sb.append(p.substring(1).toLowerCase(Locale.US));
		}
		return sb.toString();
	}

	private void installPatchToGameAsync(Uri zipUri, String game) {
		new Thread(() -> {
			boolean ok  = false;
			String  err = null;
			try {
				ok = extractArchiveToPatch(zipUri, game);
			} catch (Exception e) {
				err = e.getMessage();
			}
			final boolean success = ok;
			final String  error   = err;
			parentFragment.getActivity().runOnUiThread(() -> {
				if (success) {
					Toast.makeText(
								 context,
								 "Patch installed to " + game + "/patch",
								 Toast.LENGTH_LONG)
							.show();
				} else {
					new AlertDialog.Builder(context)
							.setTitle("Patch Install Failed")
							.setMessage(error != null ? error : "Unknown error")
							.setPositiveButton("OK", null)
							.show();
				}
			});
		}).start();
	}

	private boolean extractArchiveToPatch(Uri archiveUri, String game)
			throws Exception {
		File gameDir  = new File(context.getFilesDir(), game);
		File patchDir = new File(gameDir, "patch");
		if (!patchDir.exists() && !patchDir.mkdirs()) {
			throw new Exception(
					"Unable to create patch folder: "
					+ patchDir.getAbsolutePath());
		}
		final Path patchBase = patchDir.toPath().toAbsolutePath().normalize();

		ContentResolver resolver = context.getContentResolver();
		try (InputStream         in  = resolver.openInputStream(archiveUri);
			 BufferedInputStream bin = new BufferedInputStream(in);
			 ArchiveInputStream  ais
			 = new ArchiveStreamFactory().createArchiveInputStream(bin)) {
			ArchiveEntry entry;
			byte[]       buf = new byte[8192];
			while ((entry = ais.getNextEntry()) != null) {
				String name = entry.getName();
				// Normalize and prevent Zip Slip
				Path outPath = patchBase.resolve(name).normalize();
				if (!outPath.startsWith(patchBase)) {
					// skip suspicious path
					continue;
				}
				File outFile = outPath.toFile();

				if (entry.isDirectory()) {
					if (!outFile.exists() && !outFile.mkdirs()) {
						throw new Exception(
								"Unable to create folder: "
								+ outFile.getAbsolutePath());
					}
				} else {
					File parent = outFile.getParentFile();
					if (parent != null && !parent.exists()
						&& !parent.mkdirs()) {
						throw new Exception(
								"Unable to create folder: "
								+ parent.getAbsolutePath());
					}
					try (FileOutputStream fos = new FileOutputStream(outFile)) {
						int r;
						while ((r = ais.read(buf)) != -1) {
							fos.write(buf, 0, r);
						}
					}
				}
			}
		}
		return true;
	}
}