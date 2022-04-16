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

import android.util.Log;
import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;

public class ViewPagerAdapter extends FragmentStateAdapter {
  private static class TabData {
    public TabData(String name, Class fragment) {
      this.name = name;
      this.fragment = fragment;
    }

    public String name;
    public Class fragment;
  }

  private static final TabData[] TABS = {
    new TabData("Launcher", LauncherFragment.class),
    new TabData("Games", GamesFragment.class),
    new TabData("Console Log", ConsoleLogFragment.class)
  };

  public ViewPagerAdapter(@NonNull FragmentActivity fragmentActivity) {
    super(fragmentActivity);
  }

  @NonNull
  @Override
  public Fragment createFragment(int position) {
    try {
      return (Fragment) TABS[position].fragment.getDeclaredConstructor().newInstance();
    } catch (Exception e) {
      Log.d(
          "ViewPagerAdapter",
          "exception instantiating tab position " + position + ": " + e.toString());
      return null;
    }
  }

  @Override
  public int getItemCount() {
    return TABS.length;
  }

  public static String getItemText(int position) {
    return TABS[position].name;
  }
}
