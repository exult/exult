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
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;

/**
 * This class implements the Activity that will launch the Exult game engine on Android.
 */
public class ExultActivity extends SDLActivity {
  // Making this static for easy access from UI fragments, but it feels a bit hacky.
  private static String m_consoleLog;

  private static ExultActivity m_instance;
  public static String consoleLog;

  /**
   * This method clears the console buffer.
   */
  public static void clearConsole() {
      m_consoleLog = "";
  }

  /**
   * This method appends a new line to the console buffer.
   *
   * @param message The message to append to the console buffer.
   */
  public void writeToConsole(String message) {
    if (message != null) {
      m_consoleLog += message + "\n";
    }
  }

  public static ExultActivity instance() {
    return m_instance;
  }

  public void showGameControls(String dpadLocation) {
    runOnUiThread(
        () -> {
          RelativeLayout.LayoutParams layoutParams =
              (RelativeLayout.LayoutParams) m_dpadImageView.getLayoutParams();
          layoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
          layoutParams.removeRule(RelativeLayout.ALIGN_PARENT_TOP);
          switch (dpadLocation) {
            case "left":
              layoutParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
              layoutParams.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
              break;
            case "right":
              layoutParams.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
              layoutParams.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
              break;
            default:
              m_dpadImageView.setVisibility(View.GONE);
              return;
          }
          Log.d("ExultActivity", "showGameControls: " + dpadLocation);
          m_dpadImageView.setLayoutParams(layoutParams);
          m_dpadImageView.setVisibility(View.VISIBLE);
        });
  }

  public void hideGameControls() {
    runOnUiThread(
        () -> {
          Log.d("ExultActivity", "hideGameControls");
          m_dpadImageView.setVisibility(View.GONE);
        });
  }

  private ImageView m_dpadImageView;
  private float m_dpadImageViewActionDownX;
  private float m_dpadImageViewActionDownY;

  public native void setVirtualJoystick(float x, float y);

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    m_instance = this;

    m_dpadImageView = new ImageView(this);
    m_dpadImageView.setImageResource(R.drawable.dpad_center);
    m_dpadImageView.setOnTouchListener(
        new View.OnTouchListener() {
          @Override
          public boolean onTouch(View v, MotionEvent event) {
            switch (event.getActionMasked()) {
              case MotionEvent.ACTION_DOWN:
                m_dpadImageViewActionDownX = event.getRawX();
                m_dpadImageViewActionDownY = event.getRawY();
                return true;
              case MotionEvent.ACTION_MOVE:
                float deltaX = event.getRawX() - m_dpadImageViewActionDownX;
                float deltaY = event.getRawY() - m_dpadImageViewActionDownY;

                setVirtualJoystick(
                    deltaX / (m_dpadImageView.getWidth() / 2),
                    deltaY / (m_dpadImageView.getHeight() / 2));

                double radius = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
                if (radius < m_dpadImageView.getWidth() / 8) {
                  m_dpadImageView.setImageResource(R.drawable.dpad_center);
                  return true;
                }
                int[] directionDrawables = {
                  R.drawable.dpad_east,
                  R.drawable.dpad_southeast,
                  R.drawable.dpad_south,
                  R.drawable.dpad_southwest,
                  R.drawable.dpad_west,
                  R.drawable.dpad_northwest,
                  R.drawable.dpad_north,
                  R.drawable.dpad_northeast
                };
                double angle = Math.toDegrees(Math.atan2(deltaY, deltaX)) + 360;
                int directionCount = directionDrawables.length;
                double directionWidth = 360.0 / directionCount;
                int direction = (int) Math.round(angle / directionWidth) % directionCount;
                m_dpadImageView.setImageResource(directionDrawables[direction]);
                return true;
              case MotionEvent.ACTION_UP:
                setVirtualJoystick(0, 0);
                m_dpadImageView.setImageResource(R.drawable.dpad_center);
                return true;
              default:
                // Appease CodeFactor with a default case
                break;
            }
            return false;
          }
        });
    hideGameControls();
    mLayout.addView(m_dpadImageView);
  }

  /**
   * This method retrieves the current console buffer.
   */
  public static String getConsole() {
      return m_consoleLog;
  }

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
