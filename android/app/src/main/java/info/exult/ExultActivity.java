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
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.view.WindowCompat;
import org.libsdl.app.SDLActivity;

/**
 * This class implements the Activity that will launch the Exult game engine on
 * Android.
 */
public class ExultActivity extends SDLActivity {
	// Making this static for easy access from UI fragments, but it feels a bit
	// hacky.
	private static String m_consoleLog;

	private static ExultActivity m_instance;
	public static String         consoleLog;
	// D-pad docked margins (dp): further from edges, closer to center and
	// higher up
	private static final int DPAD_MARGIN_H_DP
			= 75;    // horizontal inset from edge
	private static final int DPAD_MARGIN_V_DP
			= 90;    // vertical inset from bottom
	// Glyphs for pause/play button
	private static final String GLYPH_PAUSE = "\u23F8\uFE0F";    // ⏸
	private static final String GLYPH_PLAY  = "\u25B6\uFE0F";    // ⏵

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
		runOnUiThread(() -> {
			RelativeLayout.LayoutParams dpadLayoutParams
					= (RelativeLayout.LayoutParams)
							  m_dpadImageView.getLayoutParams();
			dpadLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
			dpadLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_TOP);

			switch (dpadLocation) {
			case "left":
				mDpadSide = "left";
				dpadLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
				dpadLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
				break;
			case "right":
				mDpadSide = "right";
				dpadLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
				dpadLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
				break;
			default:
				m_dpadImageView.setVisibility(View.GONE);
				return;
			}
			final int marginH = dpToPx(DPAD_MARGIN_H_DP);
			final int marginV = dpToPx(DPAD_MARGIN_V_DP);
			dpadLayoutParams.leftMargin
					= (mDpadSide.equals("left") ? marginH : 0);
			dpadLayoutParams.rightMargin
					= (mDpadSide.equals("right") ? marginH : 0);
			dpadLayoutParams.bottomMargin = marginV;
			dpadLayoutParams.topMargin    = 0;
			m_dpadImageView.setLayoutParams(dpadLayoutParams);
			m_dpadImageView.setVisibility(View.VISIBLE);
		});
	}

	public void showPauseControls() {
		runOnUiThread(() -> {
			RelativeLayout.LayoutParams pauseLp
					= (RelativeLayout.LayoutParams)
							  m_pauseTextView.getLayoutParams();
			final int gap = dpToPx(12);

			// Anchor next to ESC based on its current side
			RelativeLayout.LayoutParams escLp
					= (RelativeLayout.LayoutParams)
							  m_escTextView.getLayoutParams();

			// Clear conflicting rules so RIGHT_OF/LEFT_OF take effect
			pauseLp.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
			pauseLp.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
			pauseLp.removeRule(RelativeLayout.RIGHT_OF);
			pauseLp.removeRule(RelativeLayout.LEFT_OF);

			if (escLp.getRule(RelativeLayout.ALIGN_PARENT_LEFT) != 0) {
				// ESC on left -> Pause to its right
				pauseLp.addRule(RelativeLayout.RIGHT_OF, m_escTextView.getId());
				pauseLp.leftMargin  = gap;
				pauseLp.rightMargin = 0;
			} else {
				// ESC on right -> Pause to its left
				pauseLp.addRule(RelativeLayout.LEFT_OF, m_escTextView.getId());
				pauseLp.rightMargin = gap;
				pauseLp.leftMargin  = 0;
			}
			m_pauseTextView.setLayoutParams(pauseLp);

			m_pauseTextView.setText(GLYPH_PAUSE);
			m_pauseTextView.setVisibility(View.VISIBLE);
		});
	}

	public void hidePauseControls() {
		runOnUiThread(() -> m_pauseTextView.setVisibility(View.GONE));
	}

	public void hideGameControls() {
		runOnUiThread(() -> { m_dpadImageView.setVisibility(View.GONE); });
	}

	public void showButtonControls(String dpadLocation) {
		runOnUiThread(() -> {
			RelativeLayout.LayoutParams escLayoutParams
					= (RelativeLayout.LayoutParams)
							  m_escTextView.getLayoutParams();
			RelativeLayout.LayoutParams pauseLayoutParams
					= (RelativeLayout.LayoutParams)
							  m_pauseTextView.getLayoutParams();

			escLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
			escLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_TOP);
			pauseLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
			pauseLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_TOP);

			// Clear relative rules
			pauseLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
			pauseLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
			pauseLayoutParams.removeRule(RelativeLayout.RIGHT_OF);
			pauseLayoutParams.removeRule(RelativeLayout.LEFT_OF);
			// Reset margins
			final int gap = dpToPx(12);

			switch (dpadLocation) {
			case "left":
				// D-pad left -> ESC on right, Pause to its left
				escLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
				escLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);

				pauseLayoutParams.addRule(
						RelativeLayout.LEFT_OF, m_escTextView.getId());
				pauseLayoutParams.rightMargin = gap;
				pauseLayoutParams.leftMargin  = 0;
				break;
			case "right":
				// D-pad right -> ESC on left, Pause to its right
				escLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
				escLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);

				pauseLayoutParams.addRule(
						RelativeLayout.RIGHT_OF, m_escTextView.getId());
				pauseLayoutParams.leftMargin  = gap;
				pauseLayoutParams.rightMargin = 0;
				break;
			default:
				// D-pad disabled -> ESC on left, Pause to its right
				escLayoutParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
				escLayoutParams.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);

				pauseLayoutParams.addRule(
						RelativeLayout.RIGHT_OF, m_escTextView.getId());
				pauseLayoutParams.leftMargin  = gap;
				pauseLayoutParams.rightMargin = 0;
				break;
			}
			m_escTextView.setLayoutParams(escLayoutParams);
			m_pauseTextView.setLayoutParams(pauseLayoutParams);
			m_escTextView.setVisibility(View.VISIBLE);
		});
	}

	public void hideButtonControls() {
		runOnUiThread(() -> {
			m_escTextView.setVisibility(View.GONE);
			m_pauseTextView.setVisibility(View.GONE);
		});
	}

	private TextView m_escTextView;
	private TextView m_pauseTextView;

	private ImageView m_dpadImageView;
	private int       mParentOnScreenX = 0;
	private int       mParentOnScreenY = 0;
	// Virtual joystick state
	private boolean mVjoyActive   = false;
	private float   mVjoyCenterX  = 0f;
	private float   mVjoyCenterY  = 0f;
	private int     mVjoyRadiusPx = 0;
	private String  mDpadSide     = "left";

	public native void setVirtualJoystick(float x, float y);

	public native void sendEscapeKeypress();

	public native void setName(String name);

	public native void sendPauseKeypress();

	public void promptForName(String name) {
		runOnUiThread(() -> {
			Context             context     = getContext();
			AlertDialog.Builder nameBuilder = new AlertDialog.Builder(context);
			nameBuilder.setTitle("Name");
			EditText nameEditText = new EditText(context);
			nameEditText.setInputType(InputType.TYPE_CLASS_TEXT);
			nameBuilder.setView(nameEditText);
			nameBuilder.setPositiveButton(
					"OK", new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int which) {
							setName(nameEditText.getText().toString());
						}
					});
			nameBuilder.setNegativeButton(
					"Cancel", new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int which) {
							dialog.cancel();
						}
					});

			nameBuilder.show();
		});
	}

	private int dpToPx(int dp) {
		final float density = getResources().getDisplayMetrics().density;
		return (int)(dp * density + 0.5f);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// stretch Exult to all edges and don't make room for the camera cutout
		WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
		getWindow().getAttributes().layoutInDisplayCutoutMode
				= WindowManager.LayoutParams
						  .LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;

		m_instance = this;

		m_escTextView = new TextView(this);
		m_escTextView.setId(View.generateViewId());
		m_escTextView.setBackground(
				AppCompatResources.getDrawable(this, R.drawable.btn));
		m_escTextView.setPadding(20, 20, 20, 20);
		m_escTextView.setAutoSizeTextTypeWithDefaults(
				TextView.AUTO_SIZE_TEXT_TYPE_UNIFORM);
		m_escTextView.setGravity(Gravity.CENTER);
		m_escTextView.setText("ESC");

		// Set fixed size 60x40 dp
		final int                   btnW = dpToPx(60);
		final int                   btnH = dpToPx(40);
		RelativeLayout.LayoutParams escParams
				= new RelativeLayout.LayoutParams(btnW, btnH);
		escParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
		escParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
		m_escTextView.setLayoutParams(escParams);

		m_escTextView.setOnTouchListener(new View.OnTouchListener() {
			@Override
			public boolean onTouch(View v, MotionEvent event) {
				boolean inside
						= !(event.getX() < 0 || event.getY() < 0
							|| event.getX() > v.getMeasuredWidth()
							|| event.getY() > v.getMeasuredHeight());
				switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN:
					m_escTextView.setBackground(AppCompatResources.getDrawable(
							v.getContext(), R.drawable.btnpressed));
					return true;
				case MotionEvent.ACTION_MOVE:
					m_escTextView.setBackground(AppCompatResources.getDrawable(
							v.getContext(),
							inside ? R.drawable.btnpressed : R.drawable.btn));
					return true;
				case MotionEvent.ACTION_UP:
					m_escTextView.setBackground(AppCompatResources.getDrawable(
							v.getContext(), R.drawable.btn));
					if (inside) {
						sendEscapeKeypress();
					}
					return true;
				default:
					// Appease CodeFactor with a default case
					break;
				}
				return false;
			}
		});

		// Pause button
		m_pauseTextView = new TextView(this);
		m_pauseTextView.setId(View.generateViewId());
		m_pauseTextView.setBackground(
				AppCompatResources.getDrawable(this, R.drawable.btn));
		m_pauseTextView.setPadding(20, 20, 20, 20);
		m_pauseTextView.setAutoSizeTextTypeWithDefaults(
				TextView.AUTO_SIZE_TEXT_TYPE_UNIFORM);
		m_pauseTextView.setGravity(Gravity.CENTER);
		m_pauseTextView.setText(GLYPH_PAUSE);

		RelativeLayout.LayoutParams pauseParams
				= new RelativeLayout.LayoutParams(btnW, btnH);
		pauseParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
		pauseParams.addRule(RelativeLayout.RIGHT_OF, m_escTextView.getId());
		pauseParams.leftMargin = dpToPx(12);
		m_pauseTextView.setLayoutParams(pauseParams);

		m_pauseTextView.setOnTouchListener(new View.OnTouchListener() {
			@Override
			public boolean onTouch(View v, MotionEvent event) {
				boolean inside
						= !(event.getX() < 0 || event.getY() < 0
							|| event.getX() > v.getMeasuredWidth()
							|| event.getY() > v.getMeasuredHeight());
				switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN:
					m_pauseTextView.setBackground(
							AppCompatResources.getDrawable(
									v.getContext(), R.drawable.btnpressed));
					return true;
				case MotionEvent.ACTION_MOVE:
					m_pauseTextView.setBackground(
							AppCompatResources.getDrawable(
									v.getContext(),
									inside ? R.drawable.btnpressed
										   : R.drawable.btn));
					return true;
				case MotionEvent.ACTION_UP:
					m_pauseTextView.setBackground(
							AppCompatResources.getDrawable(
									v.getContext(), R.drawable.btn));
					if (inside) {
						// Toggle glyph ⏸ <-> ⏵
						String cur = m_pauseTextView.getText().toString();
						m_pauseTextView.setText(
								GLYPH_PAUSE.equals(cur) ? GLYPH_PLAY
														: GLYPH_PAUSE);
						// Send SPACE keypress
						sendPauseKeypress();
					}
					return true;
				default:
					break;
				}
				return false;
			}
		});

		m_dpadImageView = new ImageView(this);
		m_dpadImageView.setImageResource(R.drawable.joythumb_glass);
		m_dpadImageView.setPadding(0, 0, 0, 0);
		final int joyDiameter = dpToPx(160);
		mVjoyRadiusPx         = dpToPx(80);
		RelativeLayout.LayoutParams dpadParams
				= new RelativeLayout.LayoutParams(joyDiameter, joyDiameter);
		dpadParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
		dpadParams.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
		final int initH         = dpToPx(DPAD_MARGIN_H_DP);
		final int initV         = dpToPx(DPAD_MARGIN_V_DP);
		dpadParams.leftMargin   = initH;
		dpadParams.bottomMargin = initV;
		m_dpadImageView.setLayoutParams(dpadParams);
		m_dpadImageView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

		m_dpadImageView.setOnTouchListener(new View.OnTouchListener() {
			@Override
			public boolean onTouch(View v, MotionEvent event) {
				switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN: {
					int[] parentOnScreen = new int[2];
					mLayout.getLocationOnScreen(parentOnScreen);
					mParentOnScreenX = parentOnScreen[0];
					mParentOnScreenY = parentOnScreen[1];

					mVjoyActive  = true;
					mVjoyCenterX = event.getRawX();
					mVjoyCenterY = event.getRawY();

					int left = (int)(mVjoyCenterX - mParentOnScreenX
									 - joyDiameter / 2f);
					int top  = (int)(mVjoyCenterY - mParentOnScreenY
                                    - joyDiameter / 2f);
					RelativeLayout.LayoutParams lp
							= (RelativeLayout.LayoutParams)
									  m_dpadImageView.getLayoutParams();
					lp.leftMargin = left;
					lp.topMargin  = top;
					lp.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
					lp.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
					lp.removeRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
					lp.removeRule(RelativeLayout.ALIGN_PARENT_TOP);
					m_dpadImageView.setLayoutParams(lp);

					setVirtualJoystick(0f, 0f);
					return true;
				}
				case MotionEvent.ACTION_MOVE: {
					if (!mVjoyActive)
						return false;
					final float curX = event.getRawX();
					final float curY = event.getRawY();
					float       dx   = curX - mVjoyCenterX;
					float       dy   = curY - mVjoyCenterY;
					double      len  = Math.hypot(dx, dy);
					if (len > mVjoyRadiusPx) {
						final float scale    = (float)(mVjoyRadiusPx / len);
						final float clampedX = curX - dx * scale;
						final float clampedY = curY - dy * scale;
						mVjoyCenterX         = clampedX;
						mVjoyCenterY         = clampedY;

						int left = (int)(mVjoyCenterX - mParentOnScreenX
										 - joyDiameter / 2f);
						int top  = (int)(mVjoyCenterY - mParentOnScreenY
                                        - joyDiameter / 2f);
						RelativeLayout.LayoutParams lp
								= (RelativeLayout.LayoutParams)
										  m_dpadImageView.getLayoutParams();
						lp.leftMargin = left;
						lp.topMargin  = top;
						m_dpadImageView.setLayoutParams(lp);

						dx  = curX - mVjoyCenterX;
						dy  = curY - mVjoyCenterY;
						len = Math.hypot(dx, dy);
					}
					setVirtualJoystick(
							(mVjoyRadiusPx > 0) ? (dx / mVjoyRadiusPx) : 0f,
							(mVjoyRadiusPx > 0) ? (dy / mVjoyRadiusPx) : 0f);
					return true;
				}
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_CANCEL: {
					mVjoyActive = false;
					setVirtualJoystick(0f, 0f);
					resetDpadDock();
					return true;
				}
				default:
					// Appease CodeFactor with a default case
					break;
				}
				return false;
			}
		});
		hideGameControls();
		hideButtonControls();
		mLayout.addView(m_escTextView);
		mLayout.addView(m_pauseTextView);
		mLayout.addView(m_dpadImageView);
		m_pauseTextView.setVisibility(View.GONE);
	}

	private void resetDpadDock() {
		RelativeLayout.LayoutParams lp
				= (RelativeLayout.LayoutParams)
						  m_dpadImageView.getLayoutParams();
		lp.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
		lp.removeRule(RelativeLayout.ALIGN_PARENT_TOP);
		final int marginH = dpToPx(DPAD_MARGIN_H_DP);
		final int marginV = dpToPx(DPAD_MARGIN_V_DP);
		if ("right".equals(mDpadSide)) {
			lp.addRule(RelativeLayout.ALIGN_PARENT_RIGHT);
			lp.removeRule(RelativeLayout.ALIGN_PARENT_LEFT);
			lp.rightMargin = marginH;
			lp.leftMargin  = 0;
		} else {
			lp.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
			lp.removeRule(RelativeLayout.ALIGN_PARENT_RIGHT);
			lp.leftMargin  = marginH;
			lp.rightMargin = 0;
		}
		lp.bottomMargin = marginV;
		lp.topMargin    = 0;
		m_dpadImageView.setLayoutParams(lp);
		m_dpadImageView.setVisibility(View.VISIBLE);
	}

	/**
	 * This method retrieves the current console buffer.
	 */
	public static String getConsole() {
		return m_consoleLog;
	}

	/**
	 * This method returns the name of the application entry point It can be
	 * overridden by derived classes.
	 */
	protected String getMainFunction() {
		return "ExultAndroid_main";
	}

	/**
	 * This method is called by SDL before loading the native shared libraries.
	 * It can be overridden to provide names of shared libraries to be loaded.
	 * The default implementation returns the defaults. It never returns null.
	 * An array returned by a new implementation must at least contain "SDL3".
	 * Also keep in mind that the order the libraries are loaded may matter.
	 *
	 * @return names of shared libraries to be loaded (e.g. "SDL3", "main").
	 */
	protected String[] getLibraries() {
		return new String[] {"SDL3", "exult-android-wrapper"};
	}
}
