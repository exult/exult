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

import org.libsdl.app.SDLActivity;

/**
 * This class implements the Activity that will launch the Exult game engine on Android.
 */
public class ExultActivity extends SDLActivity {

  /**
   * This method returns the name of the application entry point It can be overridden by derived
   * classes.
   */
  protected String getMainFunction() {
    return "ExultAndroid_main";
  }

  /**
   * This method is called by SDL before loading the native shared libraries. It can be overridden
   * to provide names of shared libraries to be loaded. The default implementation returns the
   * defaults. It never returns null. An array returned by a new implementation must at least
   * contain "SDL2". Also keep in mind that the order the libraries are loaded may matter.
   *
   * @return names of shared libraries to be loaded (e.g. "SDL2", "main").
   */
  protected String[] getLibraries() {
    return new String[] {"SDL2", "exult-android-wrapper"};
  }
}
