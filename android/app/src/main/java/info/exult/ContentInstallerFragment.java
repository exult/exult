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
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
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

	ContentInstallerFragment(int layout, int text) {
		m_layout = layout;
		m_text   = text;
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

			webView.loadDataWithBaseURL(
					null, htmlContent, "text/html", "utf-8", null);
			webView.setBackgroundColor(Color.TRANSPARENT);
			webView.setLayerType(WebView.LAYER_TYPE_SOFTWARE, null);

			// External URL handling
			webView.setWebViewClient(new WebViewClient() {
				@Override
				public boolean shouldOverrideUrlLoading(
						WebView view, String url) {
					if (url.startsWith("http://")
						|| url.startsWith("https://")) {
						Intent browserIntent = new Intent(
								Intent.ACTION_VIEW, Uri.parse(url));
						startActivity(browserIntent);
						return true;
					}
					return super.shouldOverrideUrlLoading(view, url);
				}
			});
		}

		ViewGroup contentLayout
				= (ViewGroup)view.findViewById(R.id.contentLayout);
		if (contentLayout != null) {
			// Add custom mod button
			Button customModButton = new Button(
					getContext(), null, android.R.attr.buttonStyle);
			customModButton.setText("Install Custom Mod");
			customModButton.setOnClickListener(
					v -> launchCustomModFilePicker());

			// Apply proper theming to match launchExultButton
			int[] attrs = new int[] {android.R.attr.colorPrimary};
			android.content.res.TypedArray a
					= getContext().obtainStyledAttributes(attrs);
			int color = a.getColor(0, Color.parseColor("#3F51B5"));
			a.recycle();

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

			// Set the button height and width using standard Android dimensions
			LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
					LinearLayout.LayoutParams.WRAP_CONTENT,
					(int)(getResources().getDisplayMetrics().density * 36));
			params.gravity      = Gravity.CENTER_HORIZONTAL;
			params.topMargin    = 40;
			params.bottomMargin = 20;

			contentLayout.addView(customModButton, params);

			// Use minimum width to match standard buttons
			customModButton.setMinWidth(
					(int)(getResources().getDisplayMetrics().density * 88));

			// Reuse the existing activity variable defined above
			for (int i = 0; i < contentLayout.getChildCount() - 1; ++i) {
				// Note: -1 to skip the button we just added
				View   viewInContentLayout = contentLayout.getChildAt(i);
				String name = (String)viewInContentLayout.getTag(R.id.name);
				if (null == name) {
					continue;
				}
				ExultContent content
						= buildContentFromView(name, viewInContentLayout);
				if (null == content) {
					continue;
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
				return;
			}
			AlertDialog.Builder builder
					= new AlertDialog.Builder(getView().getContext());
			builder.setTitle("Content Installer");
			builder.setMessage("Error: " + details);
			builder.setPositiveButton("Dismiss", (dialog, which) -> {});
			AlertDialog alert = builder.create();
			alert.show();

			// Only try to uncheck the checkbox if one exists for this request
			// code
			Log.d("foobar",
				  "unchecking checkbox for requestCode " + requestCode);
			CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
			if (checkBox != null) {
				checkBox.setChecked(false);
			}
		});
	}

	public void onCheckboxClicked(View view, int requestCode) {
		CheckBox checkBox = (CheckBox)view;
		boolean  checked  = checkBox.isChecked();

		if (checked) {
			// Get hardcoded URL if it exists
			String hardcodedUrl = (String)view.getTag(R.id.downloadUrl);

			// Show options dialog
			AlertDialog.Builder builder
					= new AlertDialog.Builder(view.getContext());
			builder.setTitle("Install " + checkBox.getText());

			// Determine options based on whether a hardcoded URL exists
			String[] options;
			if (hardcodedUrl != null && !hardcodedUrl.isEmpty()) {
				options = new String[] {
						"Select file on your device",
						"Download from Exult website"};
			} else {
				options = new String[] {
						"Select file on your device", "Download from URL"};
			}

			builder.setItems(options, (dialog, which) -> {
				if (which == 0) {
					// Select file from device (existing functionality)
					Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
					intent.addCategory(Intent.CATEGORY_OPENABLE);
					intent.setType("*/*");
					startActivityForResult(intent, requestCode);
				} else if (which == 1) {
					// Download option selected
					if (hardcodedUrl != null && !hardcodedUrl.isEmpty()) {
						// Use hardcoded URL directly
						downloadFileFromUrl(hardcodedUrl, requestCode);
					} else {
						// Show URL input dialog
						showUrlInputDialog(requestCode);
					}
				}
			});
			builder.setOnCancelListener(dialog -> checkBox.setChecked(false));
			builder.show();
		} else {
			AlertDialog.Builder builder
					= new AlertDialog.Builder(view.getContext());
			builder.setTitle("Uninstall " + checkBox.getText() + "?");
			builder.setMessage(
					"This will uninstall " + checkBox.getText()
					+ ".  Are you sure?");
			builder.setPositiveButton("Yes", (dialog, which) -> {
				new Thread(() -> {
					ExultContent content
							= m_requestCodeToContent.get(requestCode);
					if (null == content) {
						return;
					}
					if (content.isInstalled()) {
						try {
							content.uninstall(m_progressReporter);
						} catch (Exception e) {
							Log.d("ExultLauncherActivity",
								  "exception deleting " + content.getName()
										  + ": " + e.toString());
						}
					}
				}).start();
			});
			builder.setNegativeButton(
					"No", (dialog, which) -> { checkBox.setChecked(true); });
			AlertDialog alert = builder.create();
			alert.show();
		}
	}

	@Override
	public void onActivityResult(
			int requestCode, int resultCode, Intent resultData) {
		super.onActivityResult(requestCode, resultCode, resultData);
		if (null == resultData) {
			// User cancelled file selection
			CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
			if (checkBox != null) {
				checkBox.setChecked(false);
			}
			return;
		}

		// For custom mod request code
		if (requestCode == CustomModInstaller.getRequestCode()) {
			Uri uri = resultData.getData();
			customModInstaller.handleFilePickerResult(uri, requestCode, null);
			return;
		}

		Uri uri = resultData.getData();

		// Check if this is the custom mod
		CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);

		// Perform operations on the document using its URI on a separate thread
		new Thread(() -> {
			try {
				ExultContent content = m_requestCodeToContent.get(requestCode);
				if (null == content) {
					return;
				}
				if (content.isInstalled()) {
					content.uninstall(m_progressReporter);
				}
				content.install(
						uri, m_progressReporter, (successful, details) -> {
							handleContentDone(requestCode, successful, details);
						});
			} catch (Exception e) {
				Log.d("ExultLauncherActivity",
					  "exception opening content: " + e.toString());
			}
		}).start();
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
	 * Process the downloaded file similar to onActivityResult
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
}
