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
import android.view.View;

public class ModsFragment extends ContentInstallerFragment {

  ModsFragment() {
    super(R.layout.mods_card, R.string.mods_card_text);
  }

  @Override
  protected ExultContent buildContentFromView(String name, View view) {
    String game = (String) view.getTag(R.id.game);
    if (null == game) {
      return null;
    }
    Log.d("ModsFragment", "Found " + game + "." + name);
    return new ExultModContent(game, name, view.getContext());
  }
}
