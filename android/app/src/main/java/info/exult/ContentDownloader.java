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

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.net.Uri;
import android.text.InputType;
import android.util.Log;
import android.widget.CheckBox;
import android.widget.EditText;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Map;

/**
 * Handles downloading content files for Exult
 */
public class ContentDownloader {
	private final Context          context;
	private final Activity         activity;
	private final AlertDialog      progressDialog;
	private final DownloadCallback callback;
	private final Map<Integer, CheckBox> requestCodeToCheckbox;

	public interface DownloadCallback {
		void onDownloadComplete(Uri fileUri, int requestCode, File tempFile);
		void onDownloadFailed(String errorMessage, int requestCode);
	}

	public ContentDownloader(
			Context context, Activity activity, AlertDialog progressDialog,
			Map<Integer, CheckBox> requestCodeToCheckbox,
			DownloadCallback       callback) {
		this.context               = context;
		this.activity              = activity;
		this.progressDialog        = progressDialog;
		this.requestCodeToCheckbox = requestCodeToCheckbox;
		this.callback              = callback;
	}

	/**
	 * Show dialog to input a URL
	 */
	public void showUrlInputDialog(int requestCode) {
		AlertDialog.Builder builder = new AlertDialog.Builder(context);
		builder.setTitle("Download Content");
		builder.setMessage("Enter URL to download content");

		// Set up the input
		final EditText input = new EditText(context);
		input.setInputType(
				InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
		builder.setView(input);

		// Set up the buttons
		builder.setPositiveButton("Download", (dialog, which) -> {
			String   url      = input.getText().toString();
			CheckBox checkBox = requestCodeToCheckbox.get(requestCode);
			if (!url.isEmpty()) {
				downloadFileFromUrl(url, requestCode);
			} else if (checkBox != null) {
				checkBox.setChecked(false);
			}
		});

		builder.setNegativeButton("Cancel", (dialog, which) -> {
			CheckBox checkBox = requestCodeToCheckbox.get(requestCode);
			if (checkBox != null) {
				checkBox.setChecked(false);
			}
		});

		builder.show();
	}

	/**
	 * Download a file from a URL
	 */
	public void downloadFileFromUrl(String url, int requestCode) {
		// Create a temporary file
		File outputDir = context.getCacheDir();
		File outputFile;

		try {
			outputFile
					= File.createTempFile("exult_download_", ".tmp", outputDir);
		} catch (IOException e) {
			callback.onDownloadFailed(
					"Could not create temporary file: " + e.getMessage(),
					requestCode);
			return;
		}

		// Show progress dialog
		activity.runOnUiThread(() -> {
			progressDialog.setTitle("Downloading");
			progressDialog.setMessage("Starting download...");
			progressDialog.show();
		});

		// Download the file in a background thread
		new Thread(() -> {
			try {
				URL               downloadUrl = new URL(url);
				HttpURLConnection connection
						= (HttpURLConnection)downloadUrl.openConnection();
				connection.connect();

				if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
					callback.onDownloadFailed(
							"Server returned HTTP "
									+ connection.getResponseCode(),
							requestCode);
					return;
				}

				// Get file size
				int fileLength = connection.getContentLength();

				// Download the file
				InputStream input
						= new BufferedInputStream(connection.getInputStream());
				OutputStream output = new FileOutputStream(outputFile);

				byte[] data  = new byte[4096];
				long   total = 0;
				int    count;
				while ((count = input.read(data)) != -1) {
					total += count;

					// Update progress
					final int progress
							= fileLength > 0 ? (int)(total * 100 / fileLength)
											 : -1;
					final String progressText
							= progress >= 0 ? progress + "%" : "Downloading...";
					activity.runOnUiThread(
							() -> { progressDialog.setMessage(progressText); });

					output.write(data, 0, count);
				}

				output.flush();
				output.close();
				input.close();

				// Process the downloaded file
				Uri fileUri = Uri.fromFile(outputFile);
				activity.runOnUiThread(() -> {
					if (progressDialog != null && progressDialog.isShowing()) {
						progressDialog.dismiss();
					}
				});
				callback.onDownloadComplete(fileUri, requestCode, outputFile);

			} catch (Exception e) {
				Log.e("ContentDownloader", "Download exception", e);
				callback.onDownloadFailed(
						"Download failed: " + e.getMessage(), requestCode);
			}
		}).start();
	}
}