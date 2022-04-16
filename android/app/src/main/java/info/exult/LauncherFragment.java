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

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import androidx.fragment.app.Fragment;

public class LauncherFragment extends Fragment implements View.OnClickListener {
  // Probably not ideal to be assuming/referencing the parent activity from a fragment,
  // but I haven't come up with a better way to handle auto-launch yet.
  private ExultLauncherActivity getExultLauncherActivity() {
    return (ExultLauncherActivity) getActivity();
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {

    View view = inflater.inflate(R.layout.launcher_card, container, false);

    // Route auto-launch toggles to this class
    CheckBox autoLaunchCheckBox = (CheckBox) view.findViewById(R.id.autoLaunchCheckBox);
    autoLaunchCheckBox.setOnClickListener(this);

    // Set the initial auto-launch state
    autoLaunchCheckBox.setChecked(getExultLauncherActivity().getAutoLaunch());

    // Route launch clicks to this class
    Button launchExultButton = (Button) view.findViewById(R.id.launchExultButton);
    launchExultButton.setOnClickListener(this);

    return view;
  }

  @Override
  public void onClick(View view) {
    switch (view.getId()) {
      case R.id.autoLaunchCheckBox:
        CheckBox checkBox = (CheckBox) view;
        boolean checked = checkBox.isChecked();
        getExultLauncherActivity().setAutoLaunch(checked);
        break;
      case R.id.launchExultButton:
        getExultLauncherActivity().launchExult();
        break;
      default:
        break;
    }
  }
}
