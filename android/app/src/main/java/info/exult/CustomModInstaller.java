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
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.fragment.app.Fragment;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.InputStream;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveStreamFactory;
import java.util.function.Consumer;

/**
 * Handles the installation of custom mods in Exult.
 */
public class CustomModInstaller {
    private static final int CUSTOM_MOD_REQUEST_CODE = 9999;
    
    private final Context context;
    private final Fragment parentFragment;
    private final ExultContent.ProgressReporter progressReporter;
    private final ModInstallCallback callback;

    public interface ModInstallCallback {
        void onInstallStarted(ExultContent content, int requestCode);
        void onInstallComplete(boolean successful, String details, int requestCode);
        void onCancelled(int requestCode);
    }

    public CustomModInstaller(Context context, Fragment parentFragment, 
                             ExultContent.ProgressReporter progressReporter,
                             ModInstallCallback callback) {
        this.context = context;
        this.parentFragment = parentFragment;
        this.progressReporter = progressReporter;
        this.callback = callback;
    }

    /**
     * Launches the file picker to select a mod file
     */
    public void launchFilePicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");

        // Create a placeholder content
        ExultContent placeholderContent = new ExultModContent("placeholder", "customMod", context);

        // Notify callback of installation start
        callback.onInstallStarted(placeholderContent, CUSTOM_MOD_REQUEST_CODE);

        // Launch the file picker
        parentFragment.startActivityForResult(intent, CUSTOM_MOD_REQUEST_CODE);
    }

    /**
     * Handles the result from the file picker
     */
    public void handleFilePickerResult(Uri fileUri, int requestCode, File tempFile) {
        // Scan the archive for CFG files to auto-detect mod name
        scanArchiveForCfg(fileUri, (cfgName) -> {
            // Show dialog with the found name
            showCustomModDialog(fileUri, requestCode, tempFile, cfgName);
        });
    }

    /**
     * Scans an archive file for .cfg files to use as the mod name
     */
    private void scanArchiveForCfg(Uri fileUri, Consumer<String> callback) {
        new Thread(() -> {
            String foundCfgName = null;

            try {
                ContentResolver resolver = context.getContentResolver();
                InputStream inputStream = resolver.openInputStream(fileUri);
                BufferedInputStream bufferedInputStream = new BufferedInputStream(inputStream);

                try (ArchiveInputStream archiveStream = new ArchiveStreamFactory()
                        .createArchiveInputStream(bufferedInputStream)) {
                    
                    ArchiveEntry entry;
                    while ((entry = archiveStream.getNextEntry()) != null) {
                        String name = entry.getName();
                        if (name.toLowerCase().endsWith(".cfg")) {
                            // Extract base name without extension and path
                            String baseName = new File(name).getName();
                            foundCfgName = baseName.substring(0, baseName.length() - 4); // Remove .cfg
                            break;
                        }
                    }
                }
            } catch (Exception e) {
                Log.e("CustomModInstaller", "Error scanning archive", e);
            }

            // Return to the UI thread with result
            final String cfgName = foundCfgName;
            parentFragment.getActivity().runOnUiThread(() -> callback.accept(cfgName));
        }).start();
    }

    /**
     * Shows the custom mod dialog with the detected name
     */
    private void showCustomModDialog(Uri fileUri, int requestCode, File tempFile, String suggestedName) {
        // Create a safe final mod name
        final String modName = (suggestedName != null && !suggestedName.isEmpty()) 
            ? suggestedName 
            : "customMod_" + System.currentTimeMillis();

        // Create dialog builder
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle("Custom Mod Information");

        // Create layout programmatically
        LinearLayout layout = new LinearLayout(context);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(50, 30, 50, 30);

        // Display mod name (read-only)
        TextView nameLabel = new TextView(context);
        nameLabel.setText("Mod Name:");
        layout.addView(nameLabel);

        TextView nameDisplay = new TextView(context);
        nameDisplay.setPadding(20, 10, 20, 30);
        nameDisplay.setTextSize(16);
        nameDisplay.setText(modName);
        layout.addView(nameDisplay);

        // Add a note about the name
        TextView nameNote = new TextView(context);
        nameNote.setText("This name was detected from the mod archive");
        nameNote.setTextSize(12);
        nameNote.setPadding(20, 0, 20, 20);
        layout.addView(nameNote);

        // Add game type checkbox
        final CheckBox serpentIsleCheckbox = new CheckBox(context);
        serpentIsleCheckbox.setText("This is a Serpent Isle mod (unchecked = Black Gate mod)");
        layout.addView(serpentIsleCheckbox);

        builder.setView(layout);

        // Set up install button
        builder.setPositiveButton("Install", (dialog, which) -> {
            // Determine game type
            String gameType = serpentIsleCheckbox.isChecked() ? "silverseed" : "forgeofvirtue";

            // Create content object
            ExultContent customContent = new ExultModContent(gameType, modName, context);

            // Perform installation
            try {
                customContent.install(
                    fileUri,
                    progressReporter,
                    (successful, details) -> {
                        if (tempFile != null) {
                            tempFile.delete();
                        }
                        callback.onInstallComplete(successful, details, requestCode);
                    });
            } catch (Exception e) {
                if (tempFile != null) {
                    tempFile.delete();
                }
                callback.onInstallComplete(false, e.getMessage(), requestCode);
            }
        });

        // Set up cancel button
        builder.setNegativeButton("Cancel", (dialog, which) -> {
            if (tempFile != null) {
                tempFile.delete();
            }
            callback.onCancelled(requestCode);
        });

        builder.show();
    }

    /**
     * Get the custom mod request code
     */
    public static int getRequestCode() {
        return CUSTOM_MOD_REQUEST_CODE;
    }
}