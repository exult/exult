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
import android.content.ContentResolver;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.widget.CheckBox;
import androidx.fragment.app.Fragment;
import java.util.HashMap;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import android.text.InputType;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.util.function.Consumer;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveStreamFactory;
import android.webkit.WebViewClient;
import android.content.DialogInterface;
import android.widget.Button;
import android.view.Gravity;
import android.graphics.drawable.GradientDrawable;

public abstract class ContentInstallerFragment extends Fragment implements View.OnClickListener {

  private HashMap<String, Integer> m_nameToRequestCode = new HashMap<String, Integer>();
  private HashMap<Integer, ExultContent> m_requestCodeToContent =
      new HashMap<Integer, ExultContent>();
  private HashMap<Integer, CheckBox> m_requestCodeToCheckbox = new HashMap<Integer, CheckBox>();
  private final int m_layout;
  private final int m_text;
  private AlertDialog m_progressDialog;

  ContentInstallerFragment(int layout, int text) {
    m_layout = layout;
    m_text = text;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {

    View view = inflater.inflate(m_layout, container, false);

    // Set up WebView with custom URL handling
    WebView webView = (WebView) view.findViewById(R.id.contentWebView);
    if (webView != null) {
      String htmlContent = getString(m_text);
      int buttonIndex = htmlContent.indexOf("<div style=\"text-align:center");
      if (buttonIndex > 0) {
        htmlContent = htmlContent.substring(0, buttonIndex);
      }
      
      webView.loadDataWithBaseURL(null, htmlContent, "text/html", "utf-8", null);
      webView.setBackgroundColor(Color.TRANSPARENT);
      webView.setLayerType(WebView.LAYER_TYPE_SOFTWARE, null);
      
      // External URL handling
      webView.setWebViewClient(new WebViewClient() {
        @Override
        public boolean shouldOverrideUrlLoading(WebView view, String url) {
          if (url.startsWith("http://") || url.startsWith("https://")) {
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(browserIntent);
            return true;
          }
          return super.shouldOverrideUrlLoading(view, url);
        }
      });
    }
    
    ViewGroup contentLayout = (ViewGroup) view.findViewById(R.id.contentLayout);
    if (contentLayout != null) {
        // Add custom mod button
        Button customModButton = new Button(getContext(), null, android.R.attr.buttonStyle);
        customModButton.setText("Install Custom Mod");
        customModButton.setOnClickListener(v -> launchCustomModFilePicker());

        // Apply proper theming to match launchExultButton
        int[] attrs = new int[] { android.R.attr.colorPrimary };
        android.content.res.TypedArray a = getContext().obtainStyledAttributes(attrs);
        int color = a.getColor(0, Color.parseColor("#3F51B5")); // Default to Material Blue if theme attr not found
        a.recycle();

        // Create rounded corner background
        GradientDrawable shape = new GradientDrawable();
        shape.setShape(GradientDrawable.RECTANGLE);
        shape.setColor(color);
        shape.setCornerRadius(getResources().getDisplayMetrics().density * 4); // 4dp rounded corners

        // Apply the styling
        customModButton.setBackground(shape);
        customModButton.setTextColor(Color.WHITE);
        customModButton.setAllCaps(true);
        customModButton.setElevation(0); // Remove any elevation/shadow

        // Set the button height and width using standard Android dimensions
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            (int) (getResources().getDisplayMetrics().density * 36)); // 36dp is standard Android button height
        params.gravity = Gravity.CENTER_HORIZONTAL;
        params.topMargin = 40;
        params.bottomMargin = 20;

        contentLayout.addView(customModButton, params);

        // Use minimum width to match standard buttons
        customModButton.setMinWidth((int) (getResources().getDisplayMetrics().density * 88)); // Standard min width
        
        // Continue with the rest of the contentLayout processing
        // Reuse the existing activity variable defined above
        for (int i = 0; i < contentLayout.getChildCount() - 1; ++i) {
            // Note: -1 to skip the button we just added
            View viewInContentLayout = contentLayout.getChildAt(i);
            String name = (String) viewInContentLayout.getTag(R.id.name);
            if (null == name) {
              continue;
            }
            ExultContent content = buildContentFromView(name, viewInContentLayout);
            if (null == content) {
              continue;
            }
            int requestCode = m_requestCodeToContent.size() + 1;
            m_nameToRequestCode.put(name, requestCode);
            m_requestCodeToContent.put(requestCode, content);
            viewInContentLayout.setOnClickListener(this);
            CheckBox contentCheckBox = (CheckBox) viewInContentLayout;
            m_requestCodeToCheckbox.put(requestCode, contentCheckBox);
            if (!content.isInstalled()) {
              continue;
            }
            contentCheckBox.setChecked(true);
        }
    }

    m_progressDialog =
        new AlertDialog.Builder(getContext())
            .setView(inflater.inflate(R.layout.progress_dialog, container, false))
            .create();

    return view;
  }

  protected abstract ExultContent buildContentFromView(String name, View view);

  final ExultContent.ProgressReporter m_progressReporter =
      (stage, file) -> {
        getActivity()
            .runOnUiThread(
                () -> {
                  if (null == stage || null == file) {
                    m_progressDialog.hide();
                    return;
                  }
                  m_progressDialog.setTitle(stage);
                  m_progressDialog.setMessage(file);
                  m_progressDialog.show();
                });
      };

  private void handleContentDone(int requestCode, boolean successful, String details) {
    getActivity()
        .runOnUiThread(
            () -> {
              if (successful) {
                return;
              }
              AlertDialog.Builder builder = new AlertDialog.Builder(getView().getContext());
              builder.setTitle("Content Installer");
              builder.setMessage("Error: " + details);
              builder.setPositiveButton("Dismiss", (dialog, which) -> {});
              AlertDialog alert = builder.create();
              alert.show();

              // Only try to uncheck the checkbox if one exists for this request code
              Log.d("foobar", "unchecking checkbox for requestCode " + requestCode);
              CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
              if (checkBox != null) {
                  checkBox.setChecked(false);
              }
            });
  }

  public void onCheckboxClicked(View view, int requestCode) {
    CheckBox checkBox = (CheckBox) view;
    boolean checked = checkBox.isChecked();

    if (checked) {
      // Get hardcoded URL if it exists
      String hardcodedUrl = (String) view.getTag(R.id.downloadUrl);
      
      // Show options dialog
      AlertDialog.Builder builder = new AlertDialog.Builder(view.getContext());
      builder.setTitle("Install " + checkBox.getText());
      
      // Determine options based on whether a hardcoded URL exists
      String[] options;
      if (hardcodedUrl != null && !hardcodedUrl.isEmpty()) {
        options = new String[]{"Select file on your device", "Download from Exult website"};
      } else {
        options = new String[]{"Select file on your device", "Download from URL"};
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
      AlertDialog.Builder builder = new AlertDialog.Builder(view.getContext());
      builder.setTitle("Uninstall " + checkBox.getText() + "?");
      builder.setMessage("This will uninstall " + checkBox.getText() + ".  Are you sure?");
      builder.setPositiveButton(
          "Yes",
          (dialog, which) -> {
            new Thread(
                    () -> {
                      ExultContent content = m_requestCodeToContent.get(requestCode);
                      if (null == content) {
                        return;
                      }
                      if (content.isInstalled()) {
                        try {
                          content.uninstall(m_progressReporter);
                        } catch (Exception e) {
                          Log.d(
                              "ExultLauncherActivity",
                              "exception deleting " + content.getName() + ": " + e.toString());
                        }
                      }
                    })
                .start();
          });
      builder.setNegativeButton(
          "No",
          (dialog, which) -> {
            checkBox.setChecked(true);
          });
      AlertDialog alert = builder.create();
      alert.show();
    }
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent resultData) {
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
    if (requestCode == 9999) { // Match the customModRequestCode
      Uri uri = resultData.getData();
      handleCustomModInstallation(uri, requestCode, null);
      return;
    }

    Uri uri = resultData.getData();
    
    // Check if this is the custom mod
    CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
    
    // Perform operations on the document using its URI on a separate thread
    new Thread(
            () -> {
              try {
                ExultContent content = m_requestCodeToContent.get(requestCode);
                if (null == content) {
                    return;
                }
                if (content.isInstalled()) {
                  content.uninstall(m_progressReporter);
                }
                content.install(
                    uri,
                    m_progressReporter,
                    (successful, details) -> {
                      handleContentDone(requestCode, successful, details);
                    });
              } catch (Exception e) {
                Log.d("ExultLauncherActivity", "exception opening content: " + e.toString());
              }
            })
        .start();
  }

  @Override
  public void onClick(View view) {
    String name = (String) view.getTag(R.id.name);
    Integer requestCode = m_nameToRequestCode.get(name);
    Log.d("GamesFragment", "onClick(), name=" + name + ", requestCode=" + requestCode);
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
    // Create a temporary file to save the download
    File outputDir = getContext().getCacheDir();
    File outputFile;
    
    try {
      outputFile = File.createTempFile("exult_download_", ".tmp", outputDir);
    } catch (IOException e) {
      handleContentDone(requestCode, false, "Could not create temporary file: " + e.getMessage());
      return;
    }

    // Show progress dialog
    getActivity().runOnUiThread(() -> {
      m_progressDialog.setTitle("Downloading");
      m_progressDialog.setMessage("Starting download...");
      m_progressDialog.show();
    });

    // Download the file in a background thread
    new Thread(() -> {
      try {
        URL downloadUrl = new URL(url);
        HttpURLConnection connection = (HttpURLConnection) downloadUrl.openConnection();
        connection.connect();

        if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
          handleContentDone(requestCode, false, "Server returned HTTP " + connection.getResponseCode());
          return;
        }

        // Get file size
        int fileLength = connection.getContentLength();

        // Download the file
        InputStream input = new BufferedInputStream(connection.getInputStream());
        OutputStream output = new FileOutputStream(outputFile);

        byte[] data = new byte[4096];
        long total = 0;
        int count;
        while ((count = input.read(data)) != -1) {
          total += count;
          
          // Update progress
          final int progress = fileLength > 0 ? (int) (total * 100 / fileLength) : -1;
          final String progressText = progress >= 0 ? progress + "%" : "Downloading...";
          getActivity().runOnUiThread(() -> {
            m_progressDialog.setMessage(progressText);
          });
          
          output.write(data, 0, count);
        }

        output.flush();
        output.close();
        input.close();

        // Process the downloaded file
        Uri fileUri = Uri.fromFile(outputFile);
        processDownloadedFile(fileUri, requestCode, outputFile);
        
      } catch (Exception e) {
        Log.e("ExultLauncherActivity", "Download exception", e);
        handleContentDone(requestCode, false, "Download failed: " + e.getMessage());
      }
    }).start();
  }

  /**
   * Process the downloaded file similar to onActivityResult
   */
  private void processDownloadedFile(Uri uri, int requestCode, File tempFile) {
    // Check if this is the custom mod
    CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
    
    // Regular content handling
    ExultContent content = m_requestCodeToContent.get(requestCode);
    if (null == content) {
        if (tempFile != null) {
            tempFile.delete();
        }
        return;
    }
    
    // Continue with normal installation - don't modify the content object
    try {
        if (content.isInstalled()) {
            content.uninstall(m_progressReporter);
        }
        content.install(
            uri,
            m_progressReporter,
            (successful, details) -> {
                // Clean up temp file after installation
                tempFile.delete();
                handleContentDone(requestCode, successful, details);
            });
    } catch (Exception e) {
        tempFile.delete();
        Log.d("ExultLauncherActivity", "exception processing downloaded content: " + e.toString());
        handleContentDone(requestCode, false, e.getMessage());
    }

    getActivity().runOnUiThread(() -> {
        if (m_progressDialog != null && m_progressDialog.isShowing()) {
            m_progressDialog.dismiss();
        }
    });
  }

  /**
   * Handles the special case of a custom mod installation.
   * Prompts the user for mod name and game type.
   */
  private void handleCustomModInstallation(Uri fileUri, int requestCode, File tempFile) {
    // First, scan the archive to find any .cfg files
    scanArchiveForCfg(fileUri, (cfgName) -> {
        // Continue with installation using the found CFG name or user input
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
            ContentResolver resolver = getContext().getContentResolver();
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
            Log.e("ExultLauncherActivity", "Error scanning archive", e);
        }
        
        // Return to the UI thread with result
        final String cfgName = foundCfgName;
        getActivity().runOnUiThread(() -> callback.accept(cfgName));
    }).start();
  }

  /**
   * Shows the custom mod dialog, pre-filling with the CFG name if found
   */
  private void showCustomModDialog(Uri fileUri, int requestCode, File tempFile, String suggestedName) {
    // Create a safe final mod name
    final String modName = (suggestedName != null && !suggestedName.isEmpty()) 
        ? suggestedName 
        : "customMod_" + System.currentTimeMillis();
    
    // Create dialog builder
    AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
    builder.setTitle("Custom Mod Information");
    
    // Create layout programmatically (simpler and more reliable)
    LinearLayout layout = new LinearLayout(getContext());
    layout.setOrientation(LinearLayout.VERTICAL);
    layout.setPadding(50, 30, 50, 30);
    
    // Display mod name (read-only)
    TextView nameLabel = new TextView(getContext());
    nameLabel.setText("Mod Name:");
    layout.addView(nameLabel);
    
    TextView nameDisplay = new TextView(getContext());
    nameDisplay.setPadding(20, 10, 20, 30);
    nameDisplay.setTextSize(16);
    nameDisplay.setText(modName);
    layout.addView(nameDisplay);
    
    // Add a note about the name
    TextView nameNote = new TextView(getContext());
    nameNote.setText("This name was detected from the mod archive");
    nameNote.setTextSize(12);
    nameNote.setPadding(20, 0, 20, 20);
    layout.addView(nameNote);
    
    // Add game type checkbox
    final CheckBox serpentIsleCheckbox = new CheckBox(getContext());
    serpentIsleCheckbox.setText("This is a Serpent Isle mod (unchecked = Black Gate mod)");
    layout.addView(serpentIsleCheckbox);
    
    builder.setView(layout);
    
    // Set up install button
    builder.setPositiveButton("Install", (dialog, which) -> {
        // Determine game type
        String gameType = serpentIsleCheckbox.isChecked() ? "silverseed" : "forgeofvirtue";
        
        // Create content object with correct parameter order
        ExultContent customContent = new ExultModContent(gameType, modName, getContext());
        
        // Store content and install
        m_requestCodeToContent.put(requestCode, customContent);
        try {
            customContent.install(
                fileUri,
                m_progressReporter,
                new ExultContent.DoneReporter() {
                    @Override
                    public void report(boolean successful, String details) {
                        if (tempFile != null) {
                            tempFile.delete();
                        }
                        handleContentDone(requestCode, successful, details);
                    }
                });
        } catch (Exception e) {
            if (tempFile != null) {
                tempFile.delete();
            }
            handleContentDone(requestCode, false, e.getMessage());
        }
    });
    
    // Set up cancel button
    builder.setNegativeButton("Cancel", (dialog, which) -> {
        if (tempFile != null) {
            tempFile.delete();
        }
        CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
        if (checkBox != null) {
            checkBox.setChecked(false);
        }
    });
    
    builder.show();
  }

  /**
   * Shows a dialog for entering a URL to download content
   */
  private void showUrlInputDialog(int requestCode) {
    AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
    builder.setTitle("Download Content");
    builder.setMessage("Enter URL to download content");
    
    // Set up the input
    final EditText input = new EditText(getContext());
    input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
    builder.setView(input);
    
    // Set up the buttons
    builder.setPositiveButton("Download", (dialog, which) -> {
      String url = input.getText().toString();
      CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
      if (!url.isEmpty()) {
          downloadFileFromUrl(url, requestCode);
      } else if (checkBox != null) {
          checkBox.setChecked(false);
      }
    });
    
    builder.setNegativeButton("Cancel", (dialog, which) -> {
      CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
      if (checkBox != null) {
        checkBox.setChecked(false);
      }
    });
    
    builder.show();
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    if (m_progressDialog != null && m_progressDialog.isShowing()) {
        m_progressDialog.dismiss();
    }
    // If you add the executor from point 2:
    // executor.shutdown();
  }

  // New method to handle launching the file picker for custom mods
  private void launchCustomModFilePicker() {
    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
    intent.addCategory(Intent.CATEGORY_OPENABLE);
    intent.setType("*/*");
    
    // Use a special request code for custom mods
    int customModRequestCode = 9999; // Choose a unique request code that won't conflict
    
    // Store a reference to handle this request later
    ExultContent placeholderContent = new ExultModContent("placeholder", "customMod", getContext());
    m_requestCodeToContent.put(customModRequestCode, placeholderContent);
    
    startActivityForResult(intent, customModRequestCode);
  }
}
