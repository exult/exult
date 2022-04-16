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

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.viewpager2.widget.ViewPager2;
import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

public class ExultLauncherActivity extends AppCompatActivity {
  private SharedPreferences m_sharedPreferences;

  // This feels pretty hacky, but haven't figured out a better way to detect when the Activity
  // is returning from a launch versus being opened and eligible to auto-launch.
  private boolean m_launched = false;

  @Override
  public void onResume() {
    super.onResume();
    if (getAutoLaunch()) {
      if (!m_launched) {
        launchExult();
      } else {
        m_launched = false;
      }
    }
  }

  private static final String AUTO_LAUNCH_KEY = "autoLaunch";

  public void setAutoLaunch(boolean autoLaunch) {
    m_sharedPreferences.edit().putBoolean(AUTO_LAUNCH_KEY, autoLaunch).commit();
  }

  public boolean getAutoLaunch() {
    return m_sharedPreferences.getBoolean(AUTO_LAUNCH_KEY, false);
  }

  public void launchExult() {
    m_launched = true;
    Intent launchExultIntent = new Intent(this, ExultActivity.class);
    ExultActivity.clearConsole();
    startActivity(launchExultIntent);
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    m_sharedPreferences = getSharedPreferences(getClass().getSimpleName(), Context.MODE_PRIVATE);

    ViewPager2 viewPager = findViewById(R.id.view_pager);
    TabLayout tabLayout = findViewById(R.id.tabs);

    viewPager.setAdapter(new ViewPagerAdapter(this));
    new TabLayoutMediator(
            tabLayout,
            viewPager,
            new TabLayoutMediator.TabConfigurationStrategy() {
              @Override
              public void onConfigureTab(@NonNull TabLayout.Tab tab, int position) {
                tab.setText(ViewPagerAdapter.getItemText(position));
              }
            })
        .attach();
  }
}
