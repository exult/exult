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

import android.app.AlertDialog;
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

    WebView webView = (WebView) view.findViewById(R.id.contentWebView);
    if (webView != null) {
      webView.loadData(getString(m_text), "text/html", "utf-8");
      webView.setBackgroundColor(Color.TRANSPARENT);
      webView.setLayerType(WebView.LAYER_TYPE_SOFTWARE, null);
    }

    ViewGroup contentLayout = (ViewGroup) view.findViewById(R.id.contentLayout);
    ExultLauncherActivity activity = (ExultLauncherActivity) getActivity();
    for (int i = 0; i < contentLayout.getChildCount(); ++i) {
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

              Log.d("foobar", "unchecking checkbox for requestCode " + requestCode);
              CheckBox checkBox = m_requestCodeToCheckbox.get(requestCode);
              checkBox.setChecked(false);
            });
  }

  private void onCheckboxClicked(View view, int requestCode) {
    CheckBox checkBox = (CheckBox) view;
    boolean checked = checkBox.isChecked();

    if (checked) {
      Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
      intent.addCategory(Intent.CATEGORY_OPENABLE);
      intent.setType("*/*");
      startActivityForResult(intent, requestCode);
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
      return;
    }

    ExultContent content = m_requestCodeToContent.get(requestCode);
    if (null == content) {
      return;
    }

    // The result data contains a URI for the document or directory that
    // the user selected.
    Uri uri = resultData.getData();

    // Perform operations on the document using its URI on a separate thread
    new Thread(
            () -> {
              try {
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
}
