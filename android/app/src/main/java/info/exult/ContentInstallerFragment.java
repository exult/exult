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

import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.Toast;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.Fragment;
import java.io.File;
import java.util.HashMap;

public abstract class ContentInstallerFragment
		extends Fragment implements View.OnClickListener {
	private HashMap<String, Integer> m_nameToRequestCode
			= new HashMap<String, Integer>();
	private HashMap<Integer, ExultContent> m_requestCodeToContent
			= new HashMap<Integer, ExultContent>();
	private HashMap<Integer, CheckBox> m_requestCodeToCheckbox
			= new HashMap<Integer, CheckBox>();
	private final int          m_layout;
	private final int          m_text;
	private AlertDialog        m_progressDialog;
	private CustomModInstaller customModInstaller;
	private ContentDownloader  contentDownloader;
	private PatchInstaller     patchInstaller;
	private static final int   REQUEST_INSTALL_GAME = 9100;
	private final java.util.Map<Long, String> m_crcToGameName
			= new java.util.HashMap<>();
	private ActivityResultLauncher<String[]>
			m_contentPickerLauncher;    // for list-row installs
	private ActivityResultLauncher<String[]>
				m_installGamePickerLauncher;    // for "Install Game" button
	private int m_pendingRequestCode
			= -1;    // ties list-row selection to the right content

	ContentInstallerFragment(int layout, int text) {
		m_layout = layout;
		m_text   = text;
	}

	@Override
	public void onCreate(@Nullable Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Picker for list-row "Select file on your device"
		m_contentPickerLauncher = registerForActivityResult(
				new ActivityResultContracts.OpenDocument(), Uri -> {
					final Uri uri         = Uri;
					final int requestCode = m_pendingRequestCode;
					m_pendingRequestCode  = -1;

					if (uri == null) {
						// User cancelled; uncheck if we have a checkbox
						CheckBox cb = m_requestCodeToCheckbox.get(requestCode);
						if (cb != null)
							cb.setChecked(false);
						return;
					}
					new Thread(() -> {
						try {
							ExultContent content
									= m_requestCodeToContent.get(requestCode);
							if (content == null)
								return;
							content.install(
									uri, m_progressReporter,
									(successful, details) -> {
										handleContentDone(
												requestCode, successful,
												details);
									});
						} catch (Exception e) {
							Log.d("ContentInstaller", "install failed: " + e);
							requireActivity().runOnUiThread(
									() -> showErrorDialog(e.getMessage()));
						}
					}).start();
				});

		// Picker for the "Install Game" button (CRC detect mainshp.flx, then
		// install)
		m_installGamePickerLauncher = registerForActivityResult(
				new ActivityResultContracts.OpenDocument(), Uri -> {
					final Uri uri = Uri;
					if (uri == null)
						return;

					new Thread(() -> {
						try {
							Long crc = detectMainshpCrc(
									uri);    // already scanning for mainshp.flx
							if (crc == null) {
								requireActivity().runOnUiThread(
										()
												-> showErrorDialog(
														"Could not find "
														+ "mainshp.flx in the "
														+ "selected file."));
								return;
							}
							final String shortName = m_crcToGameName.get(crc);
							if (shortName == null) {
								requireActivity().runOnUiThread(
										()
												-> showErrorDialog(
														"Unknown game CRC 0x"
														+ Long.toHexString(crc)
														+ (". Unable to "
														   + "determine "
														   + "game.")));
								return;
							}

							ExultContent content = new ExultGameContent(
									shortName, getContext(), crc);
							// Block install if the shortname folder already has
							// any variant installed
							if (content instanceof ExultGameContent
								&& ((ExultGameContent)content)
										   .isAnyVariantInstalledInFolder()) {
								requireActivity().runOnUiThread(
										()
												-> showErrorDialog(
														"A game is already "
														+ "installed in folder "
														+ "'" + shortName
														+ ("'. Please "
														   + "uninstall it "
														   + "first.")));
								return;
							}

							content.install(
									uri, m_progressReporter,
									(successful, details) -> {
										handleContentDone(
												REQUEST_INSTALL_GAME,
												successful, details);
									});
						} catch (Exception e) {
							Log.d("ContentInstaller",
								  "Install Game failed: " + e);
							requireActivity().runOnUiThread(
									() -> showErrorDialog(e.getMessage()));
						}
					}).start();
				});
	}

	@Override
	public View onCreateView(
			LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		View view = inflater.inflate(m_layout, container, false);

		// Set up WebView with custom URL handling
		WebView webView = (WebView)view.findViewById(R.id.contentWebView);
		if (webView != null) {
			String htmlContent = getString(m_text);
			int    buttonIndex
					= htmlContent.indexOf("<div style=\"text-align:center");
			if (buttonIndex > 0) {
				htmlContent = htmlContent.substring(0, buttonIndex);
			}

			// Apply theme color to content
			htmlContent = applyThemeColorToContent(htmlContent);

			webView.loadDataWithBaseURL(
					null, htmlContent, "text/html", "utf-8", null);
			webView.setBackgroundColor(Color.TRANSPARENT);
			webView.setLayerType(WebView.LAYER_TYPE_SOFTWARE, null);

			// External URL handling
			webView.setWebViewClient(new WebViewClient() {
				@Override
				public boolean shouldOverrideUrlLoading(
						WebView view, WebResourceRequest request) {
					Uri    uri    = request.getUrl();
					String scheme = uri != null ? uri.getScheme() : null;
					if ("http".equalsIgnoreCase(scheme)
						|| "https".equalsIgnoreCase(scheme)) {
						Intent i = new Intent(Intent.ACTION_VIEW, uri);
						view.getContext().startActivity(i);
						return true;
					}
					return false;
				}

				@SuppressWarnings("deprecation")
				@Override
				public boolean shouldOverrideUrlLoading(
						WebView view, String url) {
					// Fallback for pre-Lollipop devices
					if (url != null
						&& (url.startsWith("http://")
							|| url.startsWith("https://"))) {
						Intent i = new Intent(
								Intent.ACTION_VIEW, Uri.parse(url));
						view.getContext().startActivity(i);
						return true;
					}
					return false;
				}
			});
		}

		ViewGroup contentLayout
				= (ViewGroup)view.findViewById(R.id.contentLayout);
		if (contentLayout != null) {
			int[] attrs = new int[] {android.R.attr.colorPrimary};
			android.content.res.TypedArray a
					= getContext().obtainStyledAttributes(attrs);
			int color = a.getColor(0, Color.parseColor("#3F51B5"));
			a.recycle();

			// Games tab: add "Install Game" button
			if (shouldShowInstallGameButton()) {
				Button installGameButton = new Button(
						getContext(), null, android.R.attr.buttonStyle);
				installGameButton.setText("Install Game");
				installGameButton.setOnClickListener(
						v -> launchInstallGameFilePicker());

				GradientDrawable shape = new GradientDrawable();
				shape.setShape(GradientDrawable.RECTANGLE);
				shape.setColor(color);
				shape.setCornerRadius(
						getResources().getDisplayMetrics().density * 4);
				installGameButton.setBackground(shape);
				installGameButton.setTextColor(Color.WHITE);
				installGameButton.setAllCaps(true);
				installGameButton.setElevation(0);

				LinearLayout.LayoutParams params
						= new LinearLayout.LayoutParams(
								LinearLayout.LayoutParams.WRAP_CONTENT,
								(int)(getResources().getDisplayMetrics().density
									  * 36));
				params.gravity = Gravity.CENTER_HORIZONTAL;
				params.topMargin
						= (int)(getResources().getDisplayMetrics().density
								* 10);
				params.bottomMargin
						= (int)(getResources().getDisplayMetrics().density * 8);
				contentLayout.addView(installGameButton, params);
				installGameButton.setMinWidth(
						(int)(getResources().getDisplayMetrics().density * 88));

				// Build CRC->name map from the games rows (so we can resolve
				// CRC to folder)
				buildCrcToGameNameMap(contentLayout);
			}
			// Only add the custom mod button if this is the Mods fragment
			if (shouldShowCustomModButton()) {
				// Add custom mod button
				Button customModButton = new Button(
						getContext(), null, android.R.attr.buttonStyle);
				customModButton.setText("Install Custom Mod");
				customModButton.setOnClickListener(
						v -> launchCustomModFilePicker());

				// Create rounded corner background
				GradientDrawable shape = new GradientDrawable();
				shape.setShape(GradientDrawable.RECTANGLE);
				shape.setColor(color);
				shape.setCornerRadius(
						getResources().getDisplayMetrics().density * 4);

				// Apply the styling
				customModButton.setBackground(shape);
				customModButton.setTextColor(Color.WHITE);
				customModButton.setAllCaps(true);
				customModButton.setElevation(0);

				// Set the button height and width using standard Android
				// dimensions
				LinearLayout.LayoutParams params
						= new LinearLayout.LayoutParams(
								LinearLayout.LayoutParams.WRAP_CONTENT,
								(int)(getResources().getDisplayMetrics().density
									  * 36));
				params.gravity = Gravity.CENTER_HORIZONTAL;
				params.topMargin
						= (int)(getResources().getDisplayMetrics().density
								* 12);    // was 40
				params.bottomMargin
						= (int)(getResources().getDisplayMetrics().density
								* 4);    // was 10

				contentLayout.addView(customModButton, params);

				// Use minimum width to match standard buttons
				customModButton.setMinWidth(
						(int)(getResources().getDisplayMetrics().density * 88));

				Button patchButton = new Button(
						getContext(), null, android.R.attr.buttonStyle);
				patchButton.setText("Install Patch");
				patchButton.setOnClickListener(v -> launchPatchFilePicker());

				GradientDrawable shape2 = new GradientDrawable();
				shape2.setShape(GradientDrawable.RECTANGLE);
				shape2.setColor(color);
				shape2.setCornerRadius(
						getResources().getDisplayMetrics().density * 4);
				patchButton.setBackground(shape2);
				patchButton.setTextColor(Color.WHITE);
				patchButton.setAllCaps(true);
				patchButton.setElevation(0);

				LinearLayout.LayoutParams params2
						= new LinearLayout.LayoutParams(
								LinearLayout.LayoutParams.WRAP_CONTENT,
								(int)(getResources().getDisplayMetrics().density
									  * 36));
				params2.gravity = Gravity.CENTER_HORIZONTAL;
				params2.topMargin
						= (int)(getResources().getDisplayMetrics().density
								* 6);    // was 10
				params2.bottomMargin
						= (int)(getResources().getDisplayMetrics().density
								* 8);    // was 20
				contentLayout.addView(patchButton, params2);
				patchButton.setMinWidth(
						(int)(getResources().getDisplayMetrics().density * 88));
			}
			// Reuse the existing activity variable defined above
			for (int i = 0;
				 i < contentLayout.getChildCount()
							 - (shouldShowCustomModButton() ? 2 : 0)
							 - (shouldShowInstallGameButton() ? 1 : 0);
				 ++i) {
				// Note: -1 to skip the button we just added
				View   viewInContentLayout = contentLayout.getChildAt(i);
				String name = (String)viewInContentLayout.getTag(R.id.name);
				if (name == null) {
					continue;
				}
				ExultContent content
						= buildContentFromView(name, viewInContentLayout);
				if (content == null) {
					continue;
				}

				if (showOnlyInstalledEntries() && !content.isInstalled()) {
					viewInContentLayout.setVisibility(View.GONE);
					continue;    // don't wire listeners/mappings for hidden
								 // rows
				} else {
					viewInContentLayout.setVisibility(View.VISIBLE);
				}

				int requestCode = m_requestCodeToContent.size() + 1;
				m_nameToRequestCode.put(name, requestCode);
				m_requestCodeToContent.put(requestCode, content);
				viewInContentLayout.setOnClickListener(this);
				CheckBox contentCheckBox = (CheckBox)viewInContentLayout;
				m_requestCodeToCheckbox.put(requestCode, contentCheckBox);
				if (!content.isInstalled()) {
					continue;
				}
				contentCheckBox.setChecked(true);
			}
		}

		m_progressDialog
				= new AlertDialog.Builder(getContext())
						  .setView(inflater.inflate(
								  R.layout.progress_dialog, container, false))
						  .create();

		// Initialize our helper classes
		customModInstaller = new CustomModInstaller(
				getContext(), this, m_progressReporter,
				new CustomModInstaller.ModInstallCallback() {
					@Override
					public void onInstallStarted(
							ExultContent content, int requestCode) {
						m_requestCodeToContent.put(requestCode, content);
					}

					@Override
					public void onInstallComplete(
							boolean successful, String details,
							int requestCode) {
						handleContentDone(requestCode, successful, details);
					}

					@Override
					public void onCancelled(int requestCode) {
						CheckBox checkBox
								= m_requestCodeToCheckbox.get(requestCode);
						if (checkBox != null) {
							checkBox.setChecked(false);
						}
					}
				});

		patchInstaller = new PatchInstaller(getContext(), this);

		contentDownloader = new ContentDownloader(
				getContext(), getActivity(), m_progressDialog,
				m_requestCodeToCheckbox,
				new ContentDownloader.DownloadCallback() {
					@Override
					public void onDownloadComplete(
							Uri fileUri, int requestCode, File tempFile) {
						processDownloadedFile(fileUri, requestCode, tempFile);
					}

					@Override
					public void onDownloadFailed(
							String errorMessage, int requestCode) {
						handleContentDone(requestCode, false, errorMessage);
						CheckBox checkBox
								= m_requestCodeToCheckbox.get(requestCode);
						if (checkBox != null) {
							checkBox.setChecked(false);
						}
					}
				});

		return view;
	}

	// Only Games tab should show this button
	protected boolean shouldShowInstallGameButton() {
		return false;
	}

	protected abstract ExultContent
			buildContentFromView(String name, View view);

	final ExultContent.ProgressReporter m_progressReporter = (stage, file) -> {
		getActivity().runOnUiThread(() -> {
			if (null == stage || null == file) {
				m_progressDialog.hide();
				return;
			}
			m_progressDialog.setTitle(stage);
			m_progressDialog.setMessage(file);
			m_progressDialog.show();
		});
	};

	private void handleContentDone(
			int requestCode, boolean successful, String details) {
		getActivity().runOnUiThread(() -> {
			if (successful) {
				if (requestCode == REQUEST_INSTALL_GAME
					|| showOnlyInstalledEntries()) {
					refreshContentList();
				}
				return;
			}
			AlertDialog.Builder builder
					= new AlertDialog.Builder(getView().getContext());
			builder.setTitle("Content Installer");
			builder.setMessage("Error: " + details);
			builder.setPositiveButton("Dismiss", (dialog, which) -> {});
			builder.create().show();

			// Only try to uncheck the checkbox if one exists for this request
			// code
			CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
			if (checkBox != null) {
				checkBox.setChecked(false);
			}

			if (showOnlyInstalledEntries()) {
				refreshContentList();
			}
		});
	}

	public void onCheckboxClicked(View view, int requestCode) {
		CheckBox checkBox = (CheckBox)view;
		boolean  checked  = checkBox.isChecked();

		if (showOnlyInstalledEntries() && checked) {
			checkBox.setChecked(true);
			new AlertDialog.Builder(view.getContext())
					.setTitle("Install Game")
					.setMessage(
							"Use the Install Game button to install.\n"
							+ "It detects your game by the mainshp.flx CRC.")
					.setPositiveButton("OK", null)
					.show();
			return;
		}

		if (checked) {
			String hardcodedUrl = (String)view.getTag(R.id.downloadUrl);
			AlertDialog.Builder builder
					= new AlertDialog.Builder(view.getContext());
			builder.setTitle("Install " + checkBox.getText());
			String[] options
					= (hardcodedUrl != null && !hardcodedUrl.isEmpty())
							  ? new String
										[] {"Select file on your device",
											"Download from Exult website"}
							  : new String[] {
										"Select file on your device",
										"Download from URL"};
			builder.setItems(options, (dialog, which) -> {
				if (which == 0) {
					m_pendingRequestCode = requestCode;
					m_contentPickerLauncher.launch(new String[] {"*/*"});
				} else if (which == 1) {
					if (hardcodedUrl != null && !hardcodedUrl.isEmpty()) {
						downloadFileFromUrl(hardcodedUrl, requestCode);
					} else {
						showUrlInputDialog(requestCode);
					}
				}
			});
			builder.setOnCancelListener(dialog -> checkBox.setChecked(false));
			builder.show();
		} else {
			// uninstall (unchanged, but posts UI refresh)
			final CheckBox      clickedCb = (CheckBox)view;
			AlertDialog.Builder builder
					= new AlertDialog.Builder(view.getContext());
			builder.setTitle("Uninstall " + clickedCb.getText() + "?");
			builder.setMessage(
					"This will uninstall " + clickedCb.getText()
					+ ".  Are you sure?");
			builder.setPositiveButton("Yes", (dialog, which) -> {
				new Thread(() -> {
					ExultContent content
							= m_requestCodeToContent.get(requestCode);
					try {
						if (content != null && content.isInstalled()) {
							content.uninstall(m_progressReporter);    // synchronous
																	  // delete
						}
					} catch (Exception e) {
						Log.d("ExultLauncherActivity",
							  "exception deleting "
									  + (content != null ? content.getName()
														 : "?")
									  + ": " + e);
					}
					requireActivity().runOnUiThread(() -> {
						// For tabs that show all items, uncheck the exact
						// checkbox the user tapped. On Games tab
						// (installed-only), the row will be hidden, so skip
						// manual uncheck.
						if (!showOnlyInstalledEntries()) {
							clickedCb.setChecked(false);
						}
						// Now rebuild rows/mappings; Games tab hides the
						// uninstalled row here.
						refreshContentList();
					});
				}).start();
			});
			builder.setNegativeButton(
					"No", (dialog, which) -> { clickedCb.setChecked(true); });
			builder.create().show();
		}
	}

	private void buildCrcToGameNameMap(ViewGroup contentLayout) {
		m_crcToGameName.clear();
		int rows = contentLayout.getChildCount()
				   - (shouldShowCustomModButton() ? 2 : 0)
				   - (shouldShowInstallGameButton() ? 1 : 0);
		if (rows < 0)
			rows = 0;
		for (int i = 0; i < rows; i++) {
			View   row    = contentLayout.getChildAt(i);
			String name   = (String)row.getTag(R.id.name);
			String crcStr = (String)row.getTag(R.id.crc32);
			if (name == null || crcStr == null)
				continue;
			try {
				Long crc = Long.decode(crcStr);
				m_crcToGameName.put(crc, name);
			} catch (Exception ignored) {
			}
		}
	}

	private void launchInstallGameFilePicker() {
		m_installGamePickerLauncher.launch(new String[] {"*/*"});
	}

	// Recursively detect CRC of mainshp.flx inside archives/compressed streams
	private Long detectMainshpCrc(Uri sourceUri) throws Exception {
		java.io.InputStream in
				= getContext().getContentResolver().openInputStream(sourceUri);
		try (java.io.BufferedInputStream bin
			 = new java.io.BufferedInputStream(in)) {
			org.apache.commons.compress.archivers.ArchiveInputStream ais
					= info.exult.ArchiveStreamFactoryWithXar
							  .createArchiveInputStream(bin);
			if (ais == null)
				return null;
			return scanArchiveForMainshpCrc(ais);
		}
	}

	private Long scanArchiveForMainshpCrc(
			org.apache.commons.compress.archivers.ArchiveInputStream ais)
			throws Exception {
		org.apache.commons.compress.archivers.ArchiveEntry entry;
		org.apache.commons.compress.compressors
				.CompressorStreamFactory compFactory
				= new org.apache.commons.compress.compressors
						  .CompressorStreamFactory();

		byte[] buf = new byte[8192];
		while ((entry = ais.getNextEntry()) != null) {
			if (entry.isDirectory())
				continue;

			String name = entry.getName();
			if (name != null && name.toLowerCase().endsWith("mainshp.flx")) {
				java.util.zip.CheckedInputStream cis
						= new java.util.zip.CheckedInputStream(
								ais, new java.util.zip.CRC32());
				while (cis.read(buf) != -1) { /* consume */
				}
				return cis.getChecksum().getValue();
			}

			// Try as compressed stream
			java.io.InputStream nested = new java.io.BufferedInputStream(ais);
			try {
				nested = new java.io.BufferedInputStream(
						compFactory.createCompressorInputStream(nested));
			} catch (org.apache.commons.compress.compressors
							 .CompressorException ignored) {
			}

			// Then try as archive
			org.apache.commons.compress.archivers.ArchiveInputStream nestedAis
					= null;
			try {
				nestedAis = info.exult.ArchiveStreamFactoryWithXar
									.createArchiveInputStream(nested);
			} catch (org.apache.commons.compress.archivers
							 .ArchiveException ignored) {
			}
			if (nestedAis != null) {
				Long crc = scanArchiveForMainshpCrc(nestedAis);
				if (crc != null)
					return crc;
			}
		}
		return null;
	}

	private void refreshContentList() {
		View root = getView();
		if (root == null)
			return;

		ViewGroup contentLayout = root.findViewById(R.id.contentLayout);
		if (contentLayout == null)
			return;

		// Clear old mappings so requestCodes and listeners are rebuilt
		m_nameToRequestCode.clear();
		m_requestCodeToContent.clear();
		m_requestCodeToCheckbox.clear();

		// Determine how many children are the rows (exclude action buttons if
		// present)
		int rows = contentLayout.getChildCount()
				   - (shouldShowCustomModButton() ? 2 : 0)
				   - (shouldShowInstallGameButton() ? 1 : 0);
		if (rows < 0)
			rows = 0;

		for (int i = 0; i < rows; ++i) {
			View   row  = contentLayout.getChildAt(i);
			String name = (String)row.getTag(R.id.name);
			if (name == null)
				continue;

			ExultContent content = buildContentFromView(name, row);
			if (content == null)
				continue;

			// In Games tab, hide rows that are not installed
			if (showOnlyInstalledEntries() && !content.isInstalled()) {
				row.setVisibility(View.GONE);
				continue;
			} else {
				row.setVisibility(View.VISIBLE);
			}

			int requestCode = m_requestCodeToContent.size() + 1;
			m_nameToRequestCode.put(name, requestCode);
			m_requestCodeToContent.put(requestCode, content);

			// Expecting CheckBox rows
			if (row instanceof CheckBox) {
				CheckBox cb = (CheckBox)row;
				m_requestCodeToCheckbox.put(requestCode, cb);
				cb.setOnClickListener(this);
				cb.setChecked(content.isInstalled());
			} else {
				// Still wire click listener if not a CheckBox for any reason
				row.setOnClickListener(this);
			}
		}

		// Rebuild CRC -> game name map for the Install Game button
		if (shouldShowInstallGameButton()) {
			buildCrcToGameNameMap(contentLayout);
		}
	}

	@Override
	public void onClick(View view) {
		String  name        = (String)view.getTag(R.id.name);
		Integer requestCode = m_nameToRequestCode.get(name);
		Log.d("GamesFragment",
			  "onClick(), name=" + name + ", requestCode=" + requestCode);
		if (null == requestCode) {
			return;
		}
		onCheckboxClicked(view, requestCode);
	}

	@Override
	public void onResume() {
		super.onResume();
		// Re-evaluate installed games when the tab becomes visible again
		try {
			java.lang.reflect.Method m
					= ContentInstallerFragment.class.getDeclaredMethod(
							"refreshContentList");
			m.setAccessible(true);
			m.invoke(this);
		} catch (Exception ignored) {
		}
	}

	/**
	 * Downloads a file from the specified URL and prepares it for installation.
	 *
	 * @param url The URL to download from
	 * @param requestCode The requestCode associated with the content
	 */
	private void downloadFileFromUrl(String url, int requestCode) {
		contentDownloader.downloadFileFromUrl(url, requestCode);
	}

	/**
	 * Process the downloaded file
	 */
	private void processDownloadedFile(
			Uri uri, int requestCode, File tempFile) {
		// Regular content handling
		ExultContent content = m_requestCodeToContent.get(requestCode);
		if (null == content) {
			if (tempFile != null) {
				tempFile.delete();
			}
			return;
		}

		// Continue with normal installation
		try {
			if (content.isInstalled()) {
				content.uninstall(m_progressReporter);
			}

			// Use a simplified reporter that handles temp file cleanup
			content.install(uri, m_progressReporter, (successful, details) -> {
				if (tempFile != null) {
					tempFile.delete();
				}
				handleContentDone(requestCode, successful, details);
			});
		} catch (Exception e) {
			if (tempFile != null) {
				tempFile.delete();
			}
			handleContentDone(requestCode, false, e.getMessage());
		}
	}

	/**
	 * Shows a dialog for entering a URL to download content
	 */
	private void showUrlInputDialog(int requestCode) {
		contentDownloader.showUrlInputDialog(requestCode);
	}

	private void showErrorDialog(String msg) {
		AlertDialog.Builder builder
				= new AlertDialog.Builder(getView().getContext());
		builder.setTitle("Install Game");
		builder.setMessage("Error: " + msg);
		builder.setPositiveButton("Dismiss", (d, w) -> {});
		builder.create().show();
	}

	@Override
	public void onDestroy() {
		super.onDestroy();
		if (m_progressDialog != null && m_progressDialog.isShowing()) {
			m_progressDialog.dismiss();
		}
	}

	// New method to handle launching the file picker for custom mods
	private void launchCustomModFilePicker() {
		customModInstaller.launchFilePicker();
	}

	private void launchPatchFilePicker() {
		patchInstaller.launchFilePicker();
	}

	/**
	 * Determines whether this fragment should show the custom mod button.
	 * Default is false. Override in subclasses to enable.
	 *
	 * @return true if the fragment should show the custom mod button
	 */
	protected boolean shouldShowCustomModButton() {
		return false;    // Default to false - subclasses can override
	}

	// Show only entries that are already installed (Games tab)
	protected boolean showOnlyInstalledEntries() {
		return false;
	}

	/**
	 * Applies the theme's colorOnSecondary to the HTML content
	 * This works with both existing span color elements and plain HTML
	 */
	private String applyThemeColorToContent(String htmlContent) {
		// Get colorOnSecondary from the current theme
		TypedValue typedValue = new TypedValue();
		getContext().getTheme().resolveAttribute(
				com.google.android.material.R.attr.colorOnSecondary, typedValue,
				true);

		// If we couldn't resolve it, try alternative approaches
		if (typedValue.type == TypedValue.TYPE_NULL) {
			// Try to get the color directly from resources
			typedValue.data = ContextCompat.getColor(
					getContext(),
					R.color.white);    // Default fallback

			try {
				// Try to get the color resource ID
				int colorResId = getResources().getIdentifier(
						"colorOnSecondary", "color",
						getContext().getPackageName());
				if (colorResId != 0) {
					typedValue.data
							= ContextCompat.getColor(getContext(), colorResId);
				}
			} catch (Exception e) {
				Log.e("ContentInstaller", "Error getting theme color", e);
			}
		}

		// Convert the color to a hex string without alpha
		String hexColor = String.format("#%06X", (0xFFFFFF & typedValue.data));

		// Check if the content already has a span with color
		if (htmlContent.contains("<span style=\"color:")) {
			// Replace existing color with our theme color
			htmlContent = htmlContent.replaceAll(
					"<span style=\"color:[^\"]*\"",
					"<span style=\"color:" + hexColor + "\"");
		} else {
			// Wrap the entire content in a span with our theme color
			htmlContent = "<span style=\"color:" + hexColor + "\">"
						  + htmlContent + "</span>";
		}

		return htmlContent;
	}
}
