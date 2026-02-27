/*
 *  Copyright (C) 2026  The Exult Team
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
import android.content.DialogInterface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.Document;
import org.w3c.dom.NodeList;

public class SavegamesFragment extends Fragment implements View.OnClickListener {
	private static final String TAG = "SavegamesFragment";
	private TextView m_statusText;
	private ActivityResultLauncher<String[]> m_zipPickerLauncher;
	private Uri m_selectedZipUri;

	// Game folder name to display name mapping (shared with PatchInstaller)
	private static final Map<String, String> GAME_DISPLAY_NAMES = new HashMap<>();
	static {
		GAME_DISPLAY_NAMES.put("blackgate", "Ultima VII: The Black Gate");
		GAME_DISPLAY_NAMES.put("serpentisle", "Ultima VII Part Two: Serpent Isle");
		GAME_DISPLAY_NAMES.put("forgeofvirtue", "Ultima VII: The Black Gate + The Forge of Virtue");
		GAME_DISPLAY_NAMES.put("silverseed", "Ultima VII Part Two: Serpent Isle + The Silver Seed");
		GAME_DISPLAY_NAMES.put("serpentbeta", "Ultima VII Part Two: Serpent Isle Beta");
	}

	@Override
	public void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Set up the ZIP file picker
		m_zipPickerLauncher = registerForActivityResult(
				new ActivityResultContracts.OpenDocument(),
				uri -> {
					if (uri != null) {
						m_selectedZipUri = uri;
						showGameOrModDialog();
					}
				});
	}

	@Override
	public View onCreateView(
			LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		View view = inflater.inflate(R.layout.savegames_card, container, false);

		// Get status text view
		m_statusText = view.findViewById(R.id.statusText);

		// Set up export button
		Button exportButton = view.findViewById(R.id.exportButton);
		exportButton.setOnClickListener(this);

		// Set up import button
		Button importButton = view.findViewById(R.id.importButton);
		importButton.setOnClickListener(this);
		importButton.setEnabled(true);

		return view;
	}

	@Override
	public void onClick(View view) {
		int id = view.getId();
		if (id == R.id.exportButton) {
			exportSavegames();
		} else if (id == R.id.importButton) {
			importSavegames();
		}
	}

	private void importSavegames() {
		// Launch file picker for ZIP files
		m_zipPickerLauncher.launch(new String[]{"application/zip"});
	}

	private void showGameOrModDialog() {
		String[] options = {"Game Savegames", "Mod Savegames"};

		new AlertDialog.Builder(getContext())
				.setTitle("Import Savegames For")
				.setItems(options, (dialog, which) -> {
					if (which == 0) {
						showGameSelectionDialog();
					} else {
						showModSelectionDialog();
					}
				})
				.setNegativeButton("Cancel", null)
				.show();
	}

	private void showGameSelectionDialog() {
		List<String> installedGames = getInstalledGames();

		if (installedGames.isEmpty()) {
			Toast.makeText(getContext(), "No games installed",
					Toast.LENGTH_SHORT).show();
			return;
		}

		String[] gameNames = new String[installedGames.size()];
		String[] displayNames = new String[installedGames.size()];

		for (int i = 0; i < installedGames.size(); i++) {
			String gameName = installedGames.get(i);
			gameNames[i] = gameName;
			displayNames[i] = getGameDisplayName(gameName);
		}

		new AlertDialog.Builder(getContext())
				.setTitle("Select Game")
				.setItems(displayNames, (dialog, which) -> {
					String selectedGame = gameNames[which];
					extractSavegamesToGame(selectedGame);
				})
				.setNegativeButton("Cancel", null)
				.show();
	}

	private void showModSelectionDialog() {
		List<String[]> installedMods = getInstalledMods();

		if (installedMods.isEmpty()) {
			Toast.makeText(getContext(), "No mods installed",
					Toast.LENGTH_SHORT).show();
			return;
		}

		String[] displayNames = new String[installedMods.size()];
		for (int i = 0; i < installedMods.size(); i++) {
			String[] modInfo = installedMods.get(i);
			// modInfo[0] = game folder, modInfo[1] = mod folder, modInfo[2] = display name
			displayNames[i] = modInfo[2] + " (" + getGameDisplayName(modInfo[0]) + ")";
		}

		new AlertDialog.Builder(getContext())
				.setTitle("Select Mod")
				.setItems(displayNames, (dialog, which) -> {
					String[] modInfo = installedMods.get(which);
					extractSavegamesToMod(modInfo[0], modInfo[1]);
				})
				.setNegativeButton("Cancel", null)
				.show();
	}

	private List<String> getInstalledGames() {
		List<String> installed = new ArrayList<>();
		File filesDir = getContext().getFilesDir();

		// Scan all subdirectories in filesDir for installed games
		File[] folders = filesDir.listFiles();
		if (folders != null) {
			for (File folder : folders) {
				if (!folder.isDirectory()) {
					continue;
				}
				// Skip known non-game folders
				String folderName = folder.getName();
				if (folderName.equals("data") || folderName.equals("cache")) {
					continue;
				}
				// Check if this folder has static/mainshp.flx (indicates a game)
				File staticFolder = new File(folder, "static");
				File mainshp = new File(staticFolder, "mainshp.flx");
				if (mainshp.exists() && mainshp.isFile()) {
					installed.add(folderName);
				}
			}
		}

		return installed;
	}

	private List<String[]> getInstalledMods() {
		List<String[]> installed = new ArrayList<>();
		File filesDir = getContext().getFilesDir();

		// First get all installed games, then scan their mods folders
		List<String> installedGames = getInstalledGames();
		Log.d(TAG, "getInstalledMods: checking " + installedGames.size() + " games");

		for (String gameName : installedGames) {
			File modsFolder = new File(new File(filesDir, gameName), "mods");
			Log.d(TAG, "Checking mods folder: " + modsFolder.getAbsolutePath() 
					+ " exists=" + modsFolder.exists());

			if (modsFolder.exists() && modsFolder.isDirectory()) {
				File[] modItems = modsFolder.listFiles();
				Log.d(TAG, "Found " + (modItems != null ? modItems.length : 0) 
						+ " items in mods folder");
				if (modItems != null) {
					for (File item : modItems) {
						// The cfg file is at mods/<modname>.cfg and the mod folder is at mods/<modname>/
						if (item.isFile() && item.getName().toLowerCase().endsWith(".cfg")) {
							// Found a cfg file, check if matching folder exists
							String cfgName = item.getName();
							String modName = cfgName.substring(0, cfgName.length() - 4);
							File modFolder = new File(modsFolder, modName);
							Log.d(TAG, "Found cfg: " + cfgName + ", checking for folder: " 
									+ modName + " exists=" + modFolder.exists());
							if (modFolder.exists() && modFolder.isDirectory()) {
								// Parse cfg file to get display name
								String displayName = getModDisplayName(item, modName);
								Log.d(TAG, "Adding mod: " + gameName + "/" + modName 
										+ " (" + displayName + ")");
								installed.add(new String[]{gameName, modName, displayName});
							}
						}
					}
				}
			}
		}

		Log.d(TAG, "getInstalledMods: found " + installed.size() + " mods total");
		return installed;
	}

	private String getModDisplayName(File cfgFile, String fallbackName) {
		try {
			DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
			DocumentBuilder builder = factory.newDocumentBuilder();
			Document doc = builder.parse(cfgFile);
			
			NodeList displayStrings = doc.getElementsByTagName("display_string");
			if (displayStrings.getLength() > 0) {
				String displayName = displayStrings.item(0).getTextContent().trim();
				if (!displayName.isEmpty()) {
					return displayName;
				}
			}
		} catch (Exception e) {
			Log.e(TAG, "Error parsing mod cfg file: " + cfgFile.getName(), e);
		}
		// Fallback to folder name with first letter capitalized
		return fallbackName.substring(0, 1).toUpperCase() + fallbackName.substring(1);
	}

	private String getGameDisplayName(String gameName) {
		String displayName = GAME_DISPLAY_NAMES.get(gameName.toLowerCase(Locale.US));
		if (displayName != null) {
			return displayName;
		}
		// Fallback: Title Case the folder name
		String cleaned = gameName.replace('_', ' ').replace('-', ' ');
		String[] parts = cleaned.split("\\s+");
		StringBuilder sb = new StringBuilder();
		for (String p : parts) {
			if (p.isEmpty()) continue;
			if (sb.length() > 0) sb.append(' ');
			sb.append(p.substring(0, 1).toUpperCase(Locale.US));
			if (p.length() > 1) sb.append(p.substring(1).toLowerCase(Locale.US));
		}
		return sb.toString();
	}

	private void extractSavegamesToGame(String gameName) {
		File filesDir = getContext().getFilesDir();
		File targetFolder = new File(filesDir, gameName);
		extractSavegamesToFolder(targetFolder, gameName);
	}

	private void extractSavegamesToMod(String gameName, String modName) {
		File filesDir = getContext().getFilesDir();
		File targetFolder = new File(
				new File(new File(filesDir, gameName), "mods"), modName);
		extractSavegamesToFolder(targetFolder, modName);
	}

	private void extractSavegamesToFolder(File targetFolder, String targetName) {
		if (!targetFolder.exists()) {
			Toast.makeText(getContext(), "Target folder does not exist",
					Toast.LENGTH_SHORT).show();
			return;
		}

		new Thread(() -> {
			try {
				StringBuilder statusBuilder = new StringBuilder();

				// First, backup any existing savegames to prevent data loss
				int backedUp = backupExistingSavegames(targetFolder, targetName);
				if (backedUp > 0) {
					final int backupCount = backedUp;
					requireActivity().runOnUiThread(() -> {
						Toast.makeText(getContext(),
								"Backed up " + backupCount + " existing savegame(s)",
								Toast.LENGTH_SHORT).show();
					});
					statusBuilder.append("Backed up ").append(backedUp)
							.append(" existing savegame(s)\n");
				}

				// Now extract the new savegames
				int extracted = 0;
				int renamed = 0;
				InputStream inputStream = getContext().getContentResolver()
						.openInputStream(m_selectedZipUri);

				try (ZipInputStream zis = new ZipInputStream(inputStream)) {
					ZipEntry entry;
					byte[] buffer = new byte[8192];

					while ((entry = zis.getNextEntry()) != null) {
						String fileName = entry.getName();

						// Only extract .sav files
						if (!entry.isDirectory()
								&& fileName.toLowerCase().endsWith(".sav")) {

							// Get just the filename without path
							int lastSlash = fileName.lastIndexOf('/');
							if (lastSlash >= 0) {
								fileName = fileName.substring(lastSlash + 1);
							}

							// Check if file exists and get a non-conflicting name
							File outputFile = new File(targetFolder, fileName);
							if (outputFile.exists()) {
								String newFileName = findAvailableSaveName(targetFolder, fileName);
								if (newFileName != null) {
									outputFile = new File(targetFolder, newFileName);
									Log.d(TAG, "Renamed " + fileName + " to " + newFileName);
									renamed++;
								}
							}

							try (FileOutputStream fos =
									new FileOutputStream(outputFile)) {
								int length;
								while ((length = zis.read(buffer)) > 0) {
									fos.write(buffer, 0, length);
								}
							}

							extracted++;
							Log.d(TAG, "Extracted: " + outputFile.getName());
						}

						zis.closeEntry();
					}
				}

				final int count = extracted;
				final int renamedCount = renamed;
				final String name = targetName;
				final String statusPrefix = statusBuilder.toString();
				requireActivity().runOnUiThread(() -> {
					if (count > 0) {
						String msg = statusPrefix + "Imported " + count
								+ " savegame(s) to " + name;
						if (renamedCount > 0) {
							msg += " (" + renamedCount + " renamed to avoid conflicts)";
						}
						updateStatus(msg);
						Toast.makeText(getContext(),
								"Imported " + count + " savegame(s)",
								Toast.LENGTH_LONG).show();
					} else {
						updateStatus(statusPrefix + "No .sav files found in the ZIP");
						Toast.makeText(getContext(),
								"No savegame files found in ZIP",
								Toast.LENGTH_SHORT).show();
					}
				});

			} catch (Exception e) {
				Log.e(TAG, "Error importing savegames", e);
				requireActivity().runOnUiThread(() -> {
					updateStatus("Error: " + e.getMessage());
					Toast.makeText(getContext(),
							"Import failed: " + e.getMessage(),
							Toast.LENGTH_LONG).show();
				});
			}
		}).start();
	}

	/**
	 * Find an available filename for a savegame that doesn't conflict with existing files.
	 * Savegame format: exultNUMBERgametype.sav (e.g., exult01bg.sav, exult99si.sav)
	 * 
	 * @param folder The target folder
	 * @param fileName The original filename
	 * @return A new filename that doesn't exist, or null if unable to rename
	 */
	private String findAvailableSaveName(File folder, String fileName) {
		// Parse the filename: exultNUMBERgametype.sav
		String lowerName = fileName.toLowerCase();
		if (!lowerName.startsWith("exult") || !lowerName.endsWith(".sav")) {
			return null;
		}

		// Extract the game type (bg or si) from the end before .sav
		String gameType = null;
		int gameTypeStart = -1;
		if (lowerName.endsWith("bg.sav")) {
			gameType = fileName.substring(fileName.length() - 6, fileName.length() - 4);
			gameTypeStart = fileName.length() - 6;
		} else if (lowerName.endsWith("si.sav")) {
			gameType = fileName.substring(fileName.length() - 6, fileName.length() - 4);
			gameTypeStart = fileName.length() - 6;
		} else {
			return null;
		}

		// Find the highest existing number for this game type
		int highestNum = 0;
		final String gt = gameType;
		File[] existingFiles = folder.listFiles((dir, name) -> {
			String lower = name.toLowerCase();
			return lower.startsWith("exult") && lower.endsWith(gt.toLowerCase() + ".sav");
		});
		
		if (existingFiles != null) {
			for (File f : existingFiles) {
				String name = f.getName();
				int gtStart = name.toLowerCase().lastIndexOf(gt.toLowerCase() + ".sav");
				if (gtStart > 5) {
					try {
						int num = Integer.parseInt(name.substring(5, gtStart));
						if (num > highestNum) {
							highestNum = num;
						}
					} catch (NumberFormatException e) {
						// Not a number, skip
					}
				}
			}
		}

		// Generate new filename with next available number
		int newNum = highestNum + 1;
		String format = "%0" + Math.max(2, String.valueOf(newNum).length()) + "d";
		return "exult" + String.format(format, newNum) + gameType + ".sav";
	}

	private void exportSavegames() {
		try {
			File filesDir = getContext().getFilesDir();
			File downloadsDir = Environment.getExternalStoragePublicDirectory(
					Environment.DIRECTORY_DOWNLOADS);

			if (!downloadsDir.exists()) {
				downloadsDir.mkdirs();
			}

			int totalExported = 0;
			StringBuilder status = new StringBuilder();

			// Get timestamp for unique filenames
			String timestamp = new SimpleDateFormat("yyyyMMdd_HHmmss",
					Locale.US).format(new Date());

			// Iterate through all folders in filesDir
			File[] gameFolders = filesDir.listFiles();
			if (gameFolders == null) {
				updateStatus("No game folders found.");
				return;
			}

			for (File gameFolder : gameFolders) {
				if (!gameFolder.isDirectory()) {
					continue;
				}

				String gameName = gameFolder.getName();

				// Skip non-game folders (like "data")
				if (gameName.equals("data")) {
					continue;
				}

				// Export main game savegames
				int gameExported = exportSavegamesFromFolder(
						gameFolder, gameName, timestamp, downloadsDir);
				if (gameExported > 0) {
					status.append("Exported ").append(gameExported)
							.append(" savegame(s) from ").append(gameName)
							.append("\n");
					totalExported += gameExported;
				}

				// Check for mods subfolder
				File modsFolder = new File(gameFolder, "mods");
				if (modsFolder.exists() && modsFolder.isDirectory()) {
					File[] modFolders = modsFolder.listFiles();
					if (modFolders != null) {
						for (File modFolder : modFolders) {
							if (!modFolder.isDirectory()) {
								continue;
							}

							String modName = gameName + "_mod_"
									+ modFolder.getName();
							int modExported = exportSavegamesFromFolder(
									modFolder, modName, timestamp, downloadsDir);
							if (modExported > 0) {
								status.append("Exported ").append(modExported)
										.append(" savegame(s) from mod ")
										.append(modFolder.getName())
										.append("\n");
								totalExported += modExported;
							}
						}
					}
				}
			}

			if (totalExported > 0) {
				status.append("\nTotal: ").append(totalExported)
						.append(" savegame(s) exported to Downloads folder.");
				Toast.makeText(getContext(),
						"Exported " + totalExported + " savegame(s)",
						Toast.LENGTH_LONG).show();
			} else {
				status.append("No savegames found to export.");
				Toast.makeText(getContext(), "No savegames found",
						Toast.LENGTH_SHORT).show();
			}

			updateStatus(status.toString());

		} catch (Exception e) {
			Log.e(TAG, "Error exporting savegames", e);
			updateStatus("Error: " + e.getMessage());
			Toast.makeText(getContext(), "Export failed: " + e.getMessage(),
					Toast.LENGTH_LONG).show();
		}
	}

	private int exportSavegamesFromFolder(
			File folder, String zipName, String timestamp, File downloadsDir)
			throws IOException {
		// Find all exult*.sav files in this folder
		File[] saveFiles = folder.listFiles((dir, name) ->
				name.toLowerCase().startsWith("exult")
						&& name.toLowerCase().endsWith(".sav"));

		if (saveFiles == null || saveFiles.length == 0) {
			return 0;
		}

		// Create zip file
		String zipFileName = "exult_saves_" + zipName + "_" + timestamp + ".zip";
		File zipFile = new File(downloadsDir, zipFileName);

		try (FileOutputStream fos = new FileOutputStream(zipFile);
			 ZipOutputStream zos = new ZipOutputStream(fos)) {

			byte[] buffer = new byte[8192];

			for (File saveFile : saveFiles) {
				ZipEntry zipEntry = new ZipEntry(saveFile.getName());
				zos.putNextEntry(zipEntry);

				try (FileInputStream fis = new FileInputStream(saveFile)) {
					int length;
					while ((length = fis.read(buffer)) > 0) {
						zos.write(buffer, 0, length);
					}
				}

				zos.closeEntry();
			}
		}

		Log.d(TAG, "Created " + zipFileName + " with " + saveFiles.length
				+ " savegame(s)");
		return saveFiles.length;
	}

	private int backupExistingSavegames(File folder, String targetName) {
		// Check if there are any existing savegames to backup
		File[] existingSaves = folder.listFiles((dir, name) ->
				name.toLowerCase().startsWith("exult")
						&& name.toLowerCase().endsWith(".sav"));

		if (existingSaves == null || existingSaves.length == 0) {
			return 0;  // No existing saves, nothing to backup
		}

		try {
			File downloadsDir = Environment.getExternalStoragePublicDirectory(
					Environment.DIRECTORY_DOWNLOADS);

			if (!downloadsDir.exists()) {
				downloadsDir.mkdirs();
			}

			// Create timestamp for the backup filename
			SimpleDateFormat sdf =
					new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US);
			String timestamp = sdf.format(new Date());

			// Create backup zip with "backup_" prefix
			String backupZipName = "backup_" + targetName.replace(' ', '_');
			String zipFileName = "exult_saves_" + backupZipName + "_"
					+ timestamp + ".zip";
			File zipFile = new File(downloadsDir, zipFileName);

			try (FileOutputStream fos = new FileOutputStream(zipFile);
				 ZipOutputStream zos = new ZipOutputStream(fos)) {

				byte[] buffer = new byte[8192];

				for (File saveFile : existingSaves) {
					ZipEntry zipEntry = new ZipEntry(saveFile.getName());
					zos.putNextEntry(zipEntry);

					try (FileInputStream fis = new FileInputStream(saveFile)) {
						int length;
						while ((length = fis.read(buffer)) > 0) {
							zos.write(buffer, 0, length);
						}
					}

					zos.closeEntry();
				}
			}

			Log.d(TAG, "Created backup " + zipFileName + " with "
					+ existingSaves.length + " savegame(s)");
			return existingSaves.length;

		} catch (Exception e) {
			Log.e(TAG, "Error creating backup", e);
			return 0;
		}
	}

	private void updateStatus(String status) {
		if (m_statusText != null) {
			m_statusText.setText(status);
		}
	}
}
