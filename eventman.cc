/*
 *  Copyright (C) 2001-2013  The Exult Team
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "eventman.h"

#include "actors.h"
#include "exult.h"
#include "game.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "menulist.h"
#include "mouse.h"
#include "touchui.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wunused-macros"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif    // __GNUC__
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_events.h>
#ifdef USE_EXULTSTUDIO /* Only needed for communication with exult studio */
#	define Font _XFont_
#	include <SDL_syswm.h>
#	undef Font
#endif
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
#	include "windrag.h"
void Move_dragged_shape(
		int shape, int frame, int x, int y, int prevx, int prevy, bool show);
void Move_dragged_combo(
		int xtiles, int ytiles, int tiles_right, int tiles_below, int x, int y,
		int prevx, int prevy, bool show);
void Drop_dragged_shape(int shape, int frame, int x, int y);
void Drop_dragged_chunk(int chunknum, int x, int y);
void Drop_dragged_npc(int npcnum, int x, int y);
void Drop_dragged_combo(int cnt, U7_combo_data* combo, int x, int y);
#endif

namespace {
	template <typename T, detail::require<std::is_floating_point_v<T>> = true>
	[[maybe_unused]] inline bool isAlmostZero(T val) noexcept {
		return std::abs(val) < std::numeric_limits<T>::epsilon();
	}

	template <
			typename T1, typename T2,
			detail::require<
					std::is_floating_point_v<T1>
					&& std::is_floating_point_v<T2>>
			= true>
	[[maybe_unused]] inline bool floatCompare(T1 f1, T2 f2) noexcept {
		using T = std::common_type_t<T1, T2>;
		T v1    = f1;
		T v2    = f2;
		return isAlmostZero(v1 - v2);
	}

	namespace sanity_checks {
		template <typename T, detail::require<std::is_enum_v<T>> = true>
		constexpr bool operator==(T lhs, std::underlying_type_t<T> rhs) {
			return static_cast<std::underlying_type_t<T>>(lhs) == rhs;
		}

		static_assert(
				KeyCodes::Key_Unknown == SDLK_UNKNOWN
				&& KeyCodes::Key_Return == SDLK_RETURN
				&& KeyCodes::Key_Escape == SDLK_ESCAPE
				&& KeyCodes::Key_Backspace == SDLK_BACKSPACE
				&& KeyCodes::Key_Tab == SDLK_TAB
				&& KeyCodes::Key_Space == SDLK_SPACE
				&& KeyCodes::Key_Exclaim == SDLK_EXCLAIM
				&& KeyCodes::Key_DblQuote == SDLK_QUOTEDBL
				&& KeyCodes::Key_Hash == SDLK_HASH
				&& KeyCodes::Key_Percent == SDLK_PERCENT
				&& KeyCodes::Key_Dollar == SDLK_DOLLAR
				&& KeyCodes::Key_Ampersand == SDLK_AMPERSAND
				&& KeyCodes::Key_Quote == SDLK_QUOTE
				&& KeyCodes::Key_LeftParen == SDLK_LEFTPAREN
				&& KeyCodes::Key_RightParen == SDLK_RIGHTPAREN
				&& KeyCodes::Key_Asterisk == SDLK_ASTERISK
				&& KeyCodes::Key_Plus == SDLK_PLUS
				&& KeyCodes::Key_Comma == SDLK_COMMA
				&& KeyCodes::Key_Minus == SDLK_MINUS
				&& KeyCodes::Key_Period == SDLK_PERIOD
				&& KeyCodes::Key_Slash == SDLK_SLASH
				&& KeyCodes::Key_0 == SDLK_0 && KeyCodes::Key_1 == SDLK_1
				&& KeyCodes::Key_2 == SDLK_2 && KeyCodes::Key_3 == SDLK_3
				&& KeyCodes::Key_4 == SDLK_4 && KeyCodes::Key_5 == SDLK_5
				&& KeyCodes::Key_6 == SDLK_6 && KeyCodes::Key_7 == SDLK_7
				&& KeyCodes::Key_8 == SDLK_8 && KeyCodes::Key_9 == SDLK_9
				&& KeyCodes::Key_Colon == SDLK_COLON
				&& KeyCodes::Key_Semicolon == SDLK_SEMICOLON
				&& KeyCodes::Key_Less == SDLK_LESS
				&& KeyCodes::Key_Equals == SDLK_EQUALS
				&& KeyCodes::Key_Greater == SDLK_GREATER
				&& KeyCodes::Key_Question == SDLK_QUESTION
				&& KeyCodes::Key_At == SDLK_AT
				&& KeyCodes::Key_LeftBracket == SDLK_LEFTBRACKET
				&& KeyCodes::Key_Backslash == SDLK_BACKSLASH
				&& KeyCodes::Key_RightBracket == SDLK_RIGHTBRACKET
				&& KeyCodes::Key_Caret == SDLK_CARET
				&& KeyCodes::Key_Underscore == SDLK_UNDERSCORE
				&& KeyCodes::Key_Backquote == SDLK_BACKQUOTE
				&& KeyCodes::Key_a == SDLK_a && KeyCodes::Key_b == SDLK_b
				&& KeyCodes::Key_c == SDLK_c && KeyCodes::Key_d == SDLK_d
				&& KeyCodes::Key_e == SDLK_e && KeyCodes::Key_f == SDLK_f
				&& KeyCodes::Key_g == SDLK_g && KeyCodes::Key_h == SDLK_h
				&& KeyCodes::Key_i == SDLK_i && KeyCodes::Key_j == SDLK_j
				&& KeyCodes::Key_k == SDLK_k && KeyCodes::Key_l == SDLK_l
				&& KeyCodes::Key_m == SDLK_m && KeyCodes::Key_n == SDLK_n
				&& KeyCodes::Key_o == SDLK_o && KeyCodes::Key_p == SDLK_p
				&& KeyCodes::Key_q == SDLK_q && KeyCodes::Key_r == SDLK_r
				&& KeyCodes::Key_s == SDLK_s && KeyCodes::Key_t == SDLK_t
				&& KeyCodes::Key_u == SDLK_u && KeyCodes::Key_v == SDLK_v
				&& KeyCodes::Key_w == SDLK_w && KeyCodes::Key_x == SDLK_x
				&& KeyCodes::Key_y == SDLK_y && KeyCodes::Key_z == SDLK_z
				&& KeyCodes::Key_CapsLock == SDLK_CAPSLOCK
				&& KeyCodes::Key_F1 == SDLK_F1 && KeyCodes::Key_F2 == SDLK_F2
				&& KeyCodes::Key_F3 == SDLK_F3 && KeyCodes::Key_F4 == SDLK_F4
				&& KeyCodes::Key_F5 == SDLK_F5 && KeyCodes::Key_F6 == SDLK_F6
				&& KeyCodes::Key_F7 == SDLK_F7 && KeyCodes::Key_F8 == SDLK_F8
				&& KeyCodes::Key_F9 == SDLK_F9 && KeyCodes::Key_F10 == SDLK_F10
				&& KeyCodes::Key_F11 == SDLK_F11
				&& KeyCodes::Key_F12 == SDLK_F12
				&& KeyCodes::Key_PrintScreen == SDLK_PRINTSCREEN
				&& KeyCodes::Key_ScrollLock == SDLK_SCROLLLOCK
				&& KeyCodes::Key_Pause == SDLK_PAUSE
				&& KeyCodes::Key_Insert == SDLK_INSERT
				&& KeyCodes::Key_Home == SDLK_HOME
				&& KeyCodes::Key_PageUp == SDLK_PAGEUP
				&& KeyCodes::Key_Delete == SDLK_DELETE
				&& KeyCodes::Key_End == SDLK_END
				&& KeyCodes::Key_PageDown == SDLK_PAGEDOWN
				&& KeyCodes::Key_Right == SDLK_RIGHT
				&& KeyCodes::Key_Left == SDLK_LEFT
				&& KeyCodes::Key_Down == SDLK_DOWN
				&& KeyCodes::Key_Up == SDLK_UP
				&& KeyCodes::Key_NumLockClear == SDLK_NUMLOCKCLEAR
				&& KeyCodes::Key_KP_Divide == SDLK_KP_DIVIDE
				&& KeyCodes::Key_KP_Multiply == SDLK_KP_MULTIPLY
				&& KeyCodes::Key_KP_Minus == SDLK_KP_MINUS
				&& KeyCodes::Key_KP_Plus == SDLK_KP_PLUS
				&& KeyCodes::Key_KP_Enter == SDLK_KP_ENTER
				&& KeyCodes::Key_KP_1 == SDLK_KP_1
				&& KeyCodes::Key_KP_2 == SDLK_KP_2
				&& KeyCodes::Key_KP_3 == SDLK_KP_3
				&& KeyCodes::Key_KP_4 == SDLK_KP_4
				&& KeyCodes::Key_KP_5 == SDLK_KP_5
				&& KeyCodes::Key_KP_6 == SDLK_KP_6
				&& KeyCodes::Key_KP_7 == SDLK_KP_7
				&& KeyCodes::Key_KP_8 == SDLK_KP_8
				&& KeyCodes::Key_KP_9 == SDLK_KP_9
				&& KeyCodes::Key_KP_0 == SDLK_KP_0
				&& KeyCodes::Key_KP_Period == SDLK_KP_PERIOD
				&& KeyCodes::Key_Application == SDLK_APPLICATION
				&& KeyCodes::Key_Power == SDLK_POWER
				&& KeyCodes::Key_KP_Equals == SDLK_KP_EQUALS
				&& KeyCodes::Key_F13 == SDLK_F13
				&& KeyCodes::Key_F14 == SDLK_F14
				&& KeyCodes::Key_F15 == SDLK_F15
				&& KeyCodes::Key_F16 == SDLK_F16
				&& KeyCodes::Key_F17 == SDLK_F17
				&& KeyCodes::Key_F18 == SDLK_F18
				&& KeyCodes::Key_F19 == SDLK_F19
				&& KeyCodes::Key_F20 == SDLK_F20
				&& KeyCodes::Key_F21 == SDLK_F21
				&& KeyCodes::Key_F22 == SDLK_F22
				&& KeyCodes::Key_F23 == SDLK_F23
				&& KeyCodes::Key_F24 == SDLK_F24
				&& KeyCodes::Key_Execute == SDLK_EXECUTE
				&& KeyCodes::Key_Help == SDLK_HELP
				&& KeyCodes::Key_Menu == SDLK_MENU
				&& KeyCodes::Key_Select == SDLK_SELECT
				&& KeyCodes::Key_Stop == SDLK_STOP
				&& KeyCodes::Key_Again == SDLK_AGAIN
				&& KeyCodes::Key_Undo == SDLK_UNDO
				&& KeyCodes::Key_Cut == SDLK_CUT
				&& KeyCodes::Key_Copy == SDLK_COPY
				&& KeyCodes::Key_Paste == SDLK_PASTE
				&& KeyCodes::Key_Find == SDLK_FIND
				&& KeyCodes::Key_Mute == SDLK_MUTE
				&& KeyCodes::Key_VolumeUp == SDLK_VOLUMEUP
				&& KeyCodes::Key_VolumeDown == SDLK_VOLUMEDOWN
				&& KeyCodes::Key_KP_Comma == SDLK_KP_COMMA
				&& KeyCodes::Key_KP_EqualsAS400 == SDLK_KP_EQUALSAS400
				&& KeyCodes::Key_AltErase == SDLK_ALTERASE
				&& KeyCodes::Key_SysReq == SDLK_SYSREQ
				&& KeyCodes::Key_Cancel == SDLK_CANCEL
				&& KeyCodes::Key_Clear == SDLK_CLEAR
				&& KeyCodes::Key_Prior == SDLK_PRIOR
				&& KeyCodes::Key_Return2 == SDLK_RETURN2
				&& KeyCodes::Key_Separator == SDLK_SEPARATOR
				&& KeyCodes::Key_Out == SDLK_OUT
				&& KeyCodes::Key_Oper == SDLK_OPER
				&& KeyCodes::Key_ClearAgain == SDLK_CLEARAGAIN
				&& KeyCodes::Key_CrSel == SDLK_CRSEL
				&& KeyCodes::Key_ExSel == SDLK_EXSEL
				&& KeyCodes::Key_KP_00 == SDLK_KP_00
				&& KeyCodes::Key_KP_000 == SDLK_KP_000
				&& KeyCodes::Key_ThousandsSeparator == SDLK_THOUSANDSSEPARATOR
				&& KeyCodes::Key_DecimalSeparator == SDLK_DECIMALSEPARATOR
				&& KeyCodes::Key_CurrencyUnit == SDLK_CURRENCYUNIT
				&& KeyCodes::Key_CurrencySubunit == SDLK_CURRENCYSUBUNIT
				&& KeyCodes::Key_KP_LeftParen == SDLK_KP_LEFTPAREN
				&& KeyCodes::Key_KP_RightParen == SDLK_KP_RIGHTPAREN
				&& KeyCodes::Key_KP_LeftBrace == SDLK_KP_LEFTBRACE
				&& KeyCodes::Key_KP_RightBrace == SDLK_KP_RIGHTBRACE
				&& KeyCodes::Key_KP_Tab == SDLK_KP_TAB
				&& KeyCodes::Key_KP_Backspace == SDLK_KP_BACKSPACE
				&& KeyCodes::Key_KP_A == SDLK_KP_A
				&& KeyCodes::Key_KP_B == SDLK_KP_B
				&& KeyCodes::Key_KP_C == SDLK_KP_C
				&& KeyCodes::Key_KP_D == SDLK_KP_D
				&& KeyCodes::Key_KP_E == SDLK_KP_E
				&& KeyCodes::Key_KP_F == SDLK_KP_F
				&& KeyCodes::Key_KP_Xor == SDLK_KP_XOR
				&& KeyCodes::Key_KP_Power == SDLK_KP_POWER
				&& KeyCodes::Key_KP_Percent == SDLK_KP_PERCENT
				&& KeyCodes::Key_KP_Less == SDLK_KP_LESS
				&& KeyCodes::Key_KP_Greater == SDLK_KP_GREATER
				&& KeyCodes::Key_KP_Ampersand == SDLK_KP_AMPERSAND
				&& KeyCodes::Key_KP_DblAmpersand == SDLK_KP_DBLAMPERSAND
				&& KeyCodes::Key_KP_VerticalBar == SDLK_KP_VERTICALBAR
				&& KeyCodes::Key_KP_DblVerticalBar == SDLK_KP_DBLVERTICALBAR
				&& KeyCodes::Key_KP_Colon == SDLK_KP_COLON
				&& KeyCodes::Key_KP_Hash == SDLK_KP_HASH
				&& KeyCodes::Key_KP_Space == SDLK_KP_SPACE
				&& KeyCodes::Key_KP_At == SDLK_KP_AT
				&& KeyCodes::Key_KP_Exclamation == SDLK_KP_EXCLAM
				&& KeyCodes::Key_KP_MemStore == SDLK_KP_MEMSTORE
				&& KeyCodes::Key_KP_MemRecall == SDLK_KP_MEMRECALL
				&& KeyCodes::Key_KP_MemClear == SDLK_KP_MEMCLEAR
				&& KeyCodes::Key_KP_MemAdd == SDLK_KP_MEMADD
				&& KeyCodes::Key_KP_MemSubtract == SDLK_KP_MEMSUBTRACT
				&& KeyCodes::Key_KP_MemMultiply == SDLK_KP_MEMMULTIPLY
				&& KeyCodes::Key_KP_MemDivide == SDLK_KP_MEMDIVIDE
				&& KeyCodes::Key_KP_PlusMinus == SDLK_KP_PLUSMINUS
				&& KeyCodes::Key_KP_Clear == SDLK_KP_CLEAR
				&& KeyCodes::Key_KP_ClearEntry == SDLK_KP_CLEARENTRY
				&& KeyCodes::Key_KP_Binary == SDLK_KP_BINARY
				&& KeyCodes::Key_KP_Octal == SDLK_KP_OCTAL
				&& KeyCodes::Key_KP_Decimal == SDLK_KP_DECIMAL
				&& KeyCodes::Key_KP_Hexadecimal == SDLK_KP_HEXADECIMAL
				&& KeyCodes::Key_LeftCtrl == SDLK_LCTRL
				&& KeyCodes::Key_LeftShift == SDLK_LSHIFT
				&& KeyCodes::Key_LeftAlt == SDLK_LALT
				&& KeyCodes::Key_LeftGUI == SDLK_LGUI
				&& KeyCodes::Key_RightCtrl == SDLK_RCTRL
				&& KeyCodes::Key_RightShift == SDLK_RSHIFT
				&& KeyCodes::Key_RightAlt == SDLK_RALT
				&& KeyCodes::Key_RightGUI == SDLK_RGUI
				&& KeyCodes::Key_Mode == SDLK_MODE
				&& KeyCodes::Key_AudioNext == SDLK_AUDIONEXT
				&& KeyCodes::Key_AudioPrev == SDLK_AUDIOPREV
				&& KeyCodes::Key_AudioStop == SDLK_AUDIOSTOP
				&& KeyCodes::Key_AudioPlay == SDLK_AUDIOPLAY
				&& KeyCodes::Key_AudioMute == SDLK_AUDIOMUTE
				&& KeyCodes::Key_MediaSelect == SDLK_MEDIASELECT
				&& KeyCodes::Key_WWW == SDLK_WWW
				&& KeyCodes::Key_Mail == SDLK_MAIL
				&& KeyCodes::Key_Calculator == SDLK_CALCULATOR
				&& KeyCodes::Key_Computer == SDLK_COMPUTER
				&& KeyCodes::Key_AC_Search == SDLK_AC_SEARCH
				&& KeyCodes::Key_AC_Home == SDLK_AC_HOME
				&& KeyCodes::Key_AC_Back == SDLK_AC_BACK
				&& KeyCodes::Key_AC_Forward == SDLK_AC_FORWARD
				&& KeyCodes::Key_AC_Stop == SDLK_AC_STOP
				&& KeyCodes::Key_AC_Refresh == SDLK_AC_REFRESH
				&& KeyCodes::Key_AC_Bookmarks == SDLK_AC_BOOKMARKS
				&& KeyCodes::Key_BrightnessDown == SDLK_BRIGHTNESSDOWN
				&& KeyCodes::Key_BrightnessUp == SDLK_BRIGHTNESSUP
				&& KeyCodes::Key_DisplaySwitch == SDLK_DISPLAYSWITCH
				&& KeyCodes::Key_KbdIllumToggle == SDLK_KBDILLUMTOGGLE
				&& KeyCodes::Key_KbdIllumDown == SDLK_KBDILLUMDOWN
				&& KeyCodes::Key_KbdIllumUp == SDLK_KBDILLUMUP
				&& KeyCodes::Key_Eject == SDLK_EJECT
				&& KeyCodes::Key_Sleep == SDLK_SLEEP
				&& KeyCodes::Key_App1 == SDLK_APP1
				&& KeyCodes::Key_App2 == SDLK_APP2
				&& KeyCodes::Key_AudioRewind == SDLK_AUDIOREWIND
				&& KeyCodes::Key_AudioFastforward == SDLK_AUDIOFASTFORWARD
#if SDL_VERSION_ATLEAST(2, 28, 5)
				&& KeyCodes::Key_SoftLeft == SDLK_SOFTLEFT
				&& KeyCodes::Key_SoftRight == SDLK_SOFTRIGHT
				&& KeyCodes::Key_Call == SDLK_CALL
				&& KeyCodes::Key_EndCall == SDLK_ENDCALL
#endif
		);
		static_assert(
				KeyMod::NoMods == KMOD_NONE && KeyMod::LeftShift == KMOD_LSHIFT
				&& KeyMod::RightShift == KMOD_RSHIFT
				&& KeyMod::LeftCtrl == KMOD_LCTRL
				&& KeyMod::RightCtrl == KMOD_RCTRL
				&& KeyMod::LeftAlt == KMOD_LALT && KeyMod::RightAlt == KMOD_RALT
				&& KeyMod::LeftGUI == KMOD_LGUI && KeyMod::RightGUI == KMOD_RGUI
				&& KeyMod::Num == KMOD_NUM && KeyMod::Caps == KMOD_CAPS
				&& KeyMod::Mode == KMOD_MODE && KeyMod::Scroll == KMOD_SCROLL
				&& KeyMod::Ctrl == KMOD_CTRL && KeyMod::Shift == KMOD_SHIFT
				&& KeyMod::Alt == KMOD_ALT && KeyMod::GUI == KMOD_GUI
				&& KeyMod::Reserved == KMOD_RESERVED);
		static_assert(
				GamepadButton::Invalid == SDL_CONTROLLER_BUTTON_INVALID
				&& GamepadButton::A == SDL_CONTROLLER_BUTTON_A
				&& GamepadButton::B == SDL_CONTROLLER_BUTTON_B
				&& GamepadButton::X == SDL_CONTROLLER_BUTTON_X
				&& GamepadButton::Y == SDL_CONTROLLER_BUTTON_Y
				&& GamepadButton::Back == SDL_CONTROLLER_BUTTON_BACK
				&& GamepadButton::Guide == SDL_CONTROLLER_BUTTON_GUIDE
				&& GamepadButton::Start == SDL_CONTROLLER_BUTTON_START
				&& GamepadButton::LeftStick == SDL_CONTROLLER_BUTTON_LEFTSTICK
				&& GamepadButton::RightStick == SDL_CONTROLLER_BUTTON_RIGHTSTICK
				&& GamepadButton::LeftShoulder
						   == SDL_CONTROLLER_BUTTON_LEFTSHOULDER
				&& GamepadButton::RightShoulder
						   == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
				&& GamepadButton::DPad_Up == SDL_CONTROLLER_BUTTON_DPAD_UP
				&& GamepadButton::DPad_Down == SDL_CONTROLLER_BUTTON_DPAD_DOWN
				&& GamepadButton::DPad_Left == SDL_CONTROLLER_BUTTON_DPAD_LEFT
				&& GamepadButton::DPad_Right == SDL_CONTROLLER_BUTTON_DPAD_RIGHT
				&& GamepadButton::Misc1 == SDL_CONTROLLER_BUTTON_MISC1
				&& GamepadButton::Paddle1 == SDL_CONTROLLER_BUTTON_PADDLE1
				&& GamepadButton::Paddle2 == SDL_CONTROLLER_BUTTON_PADDLE2
				&& GamepadButton::Paddle3 == SDL_CONTROLLER_BUTTON_PADDLE3
				&& GamepadButton::Paddle4 == SDL_CONTROLLER_BUTTON_PADDLE4
				&& GamepadButton::Touchpad == SDL_CONTROLLER_BUTTON_TOUCHPAD);
		static_assert(
				MouseButton::Left == SDL_BUTTON_LEFT
				&& MouseButton::Middle == SDL_BUTTON_MIDDLE
				&& MouseButton::Right == SDL_BUTTON_RIGHT
				&& MouseButton::X1 == SDL_BUTTON_X1
				&& MouseButton::X2 == SDL_BUTTON_X2);
		static_assert(
				MouseButtonMask::Left == SDL_BUTTON_LMASK
				&& MouseButtonMask::Middle == SDL_BUTTON_MMASK
				&& MouseButtonMask::Right == SDL_BUTTON_RMASK
				&& MouseButtonMask::X1 == SDL_BUTTON_X1MASK
				&& MouseButtonMask::X2 == SDL_BUTTON_X2MASK);
	}    // namespace sanity_checks

	constexpr KeyCodes translateKeyCode(uint32 code) noexcept {
		return static_cast<KeyCodes>(code);
	}

	constexpr KeyMod translateKeyMods(uint32 mods) noexcept {
		return static_cast<KeyMod>(mods);
	}

	constexpr GamepadButton translateGamepadButton(uint8 button) noexcept {
		return static_cast<GamepadButton>(button);
	}

	constexpr MouseButton translateMouseButton(uint8 button) noexcept {
		return static_cast<MouseButton>(button);
	}

	constexpr MouseButtonMask translateMouseMasks(uint32 button) noexcept {
		return static_cast<MouseButtonMask>(button);
	}
}    // namespace

bool AxisVector::isNonzero() const noexcept {
	return !isAlmostZero(x) || !isAlmostZero(y);
}

bool AxisTrigger::isNonzero() const noexcept {
	return !isAlmostZero(left) || !isAlmostZero(right);
}

bool FingerMotion::isNonzero() const noexcept {
	return !isAlmostZero(x) || !isAlmostZero(y);
}

MousePosition::MousePosition(get_from_sdl_tag) {
	int x_;
	int y_;
	SDL_GetMouseState(&x_, &y_);
	set(x_, y_);
}

MousePosition::MousePosition(int x_, int y_) {
	set(x_, y_);
}

void MousePosition::set(int x_, int y_) {
	const Game_window* gwin = Game_window::get_instance();
	gwin->get_win()->screen_to_game(x_, y_, gwin->get_fastmouse(), x, y);
}

MouseMotion::MouseMotion(int x_, int y_) {
	// Note: maybe not needed?
	const Game_window* gwin = Game_window::get_instance();
	gwin->get_win()->screen_to_game(x_, y_, gwin->get_fastmouse(), x, y);
}

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
struct OleDeleter {
	void operator()(Windnd* drag) const {
		if (drag != nullptr) {
			drag->Release();
		}
	}
};

using WindndPtr = std::unique_ptr<Windnd, OleDeleter>;
#endif

struct MouseUpData;

class SDLTimer {
	SDL_TimerID timer_id{0};

public:
	SDLTimer()                           = default;
	SDLTimer(const SDLTimer&)            = delete;
	SDLTimer& operator=(const SDLTimer&) = delete;

	SDLTimer(SDLTimer&& other) noexcept
			: timer_id(std::exchange(other.timer_id, 0)) {}

	SDLTimer& operator=(SDLTimer&& rhs) noexcept {
		timer_id = std::exchange(rhs.timer_id, 0);
		return *this;
	}

	~SDLTimer() noexcept {
		stop();
	}

	void start(std::unique_ptr<MouseUpData> data) noexcept;

	inline void stop() noexcept {
		if (timer_id != 0) {
			SDL_RemoveTimer(timer_id);
			abandon();
		}
	}

	inline void abandon() noexcept {
		timer_id = 0;
	}
};

class EventManagerImpl final : public EventManager {
public:
	EventManagerImpl();
	void handle_events() noexcept override;
	void enable_dropfile() noexcept override;
	void disable_dropfile() noexcept override;

	void do_mouse_up(MouseButton buttonID);

private:
	inline bool break_event_loop() const;
	inline void handle_event(const SDL_Event& event);
	inline void handle_event(const SDL_ControllerDeviceEvent& event) noexcept;
	inline void handle_event(
			const SDL_ControllerButtonEvent& event) const noexcept;
	inline void handle_event(const SDL_KeyboardEvent& event) const noexcept;
	inline void handle_event(const SDL_TextInputEvent& event) const noexcept;
	inline void handle_event(const SDL_MouseMotionEvent& event) const noexcept;
	inline void handle_event(const SDL_MouseButtonEvent& event) noexcept;
	inline void handle_event(const SDL_MouseWheelEvent& event) const noexcept;
	inline void handle_event(const SDL_TouchFingerEvent& event) const noexcept;
	inline void handle_event(const SDL_DropEvent& event) const noexcept;
	inline void handle_event(const SDL_WindowEvent& event) const noexcept;

	inline void handle_background_event() const noexcept;
	inline void handle_quit_event() const;
	inline void handle_custom_touch_input_event(
			const SDL_UserEvent& event) const noexcept;
	inline void handle_custom_mouse_up_event(
			const SDL_UserEvent& event) noexcept;

	inline void handle_gamepad_axis_input() noexcept;

	inline SDL_GameController* find_gamepad() const noexcept;
	inline SDL_GameController* open_gamepad(int joystick_index) const noexcept;

	SDL_GameController* active_gamepad{nullptr};
	uint32   double_click_event_type{std::numeric_limits<uint32>::max()};
	SDLTimer double_click_timer;

	enum {
		INVALID_EVENT    = 0,
		DELAYED_MOUSE_UP = 0x45585545
	};

#if defined(USE_EXULTSTUDIO) && defined(_WIN32)
	WindndPtr windnd;
	HWND      hgwin{};
#endif

	template <typename Callback_t, typename... Ts>
	std::invoke_result_t<Callback_t, Ts...> invoke_callback(
			Ts&&... args) const noexcept {
		if (auto& stack = get_callback_stack<Callback_t>(); !stack.empty()) {
			const auto& callback = stack.top();
			return callback(std::forward<Ts>(args)...);
		}
		using Result = std::invoke_result_t<Callback_t, Ts...>;
		if constexpr (!std::is_same_v<void, std::decay_t<Result>>) {
			return Result{};
		}
	}
};

struct MouseUpData {
	EventManagerImpl* eventMan;
	MouseButton       buttonID;

	MouseUpData(EventManagerImpl* eman, MouseButton button)
			: eventMan(eman), buttonID(button) {}
};

extern "C" uint32 DoMouseUp(uint32 interval, void* param) {
	ignore_unused_variable_warning(interval);
	std::unique_ptr<MouseUpData> data(static_cast<MouseUpData*>(param));
	data->eventMan->do_mouse_up(data->buttonID);
	return 0;
}

inline void SDLTimer::start(std::unique_ptr<MouseUpData> data) noexcept {
	stop();
	constexpr static const uint32 delay_ms = 500;
	timer_id = SDL_AddTimer(delay_ms, DoMouseUp, data.release());
}

void EventManagerImpl::do_mouse_up(MouseButton buttonID) {
	auto  button = static_cast<uintptr>(buttonID);
	void* data;
	std::memcpy(&data, &button, sizeof(uintptr));
	SDL_Event event;
	event.user
			= {double_click_event_type, 0, 0, DELAYED_MOUSE_UP, data, nullptr};
	SDL_PushEvent(&event);
	double_click_timer.abandon();
}

SDL_GameController* EventManagerImpl::open_gamepad(
		int joystick_index) const noexcept {
	SDL_GameController* input_device = SDL_GameControllerOpen(joystick_index);
	if (input_device != nullptr) {
		SDL_GameControllerGetJoystick(input_device);
		std::cout << "Gamepad attached and open: \""
				  << SDL_GameControllerName(input_device) << "\"\n";
	} else {
		std::cout << "Gamepad attached, but it failed to open. Error: \""
				  << SDL_GetError() << "\"\n";
	}
	return input_device;
}

SDL_GameController* EventManagerImpl::find_gamepad() const noexcept {
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i) != 0u) {
			return open_gamepad(i);
		}
	}
	// No gamepads found.
	return nullptr;
}

EventManagerImpl::EventManagerImpl() {
	active_gamepad          = find_gamepad();
	double_click_event_type = SDL_RegisterEvents(1);
}

bool EventManagerImpl::break_event_loop() const {
	return invoke_callback<BreakLoopCallback>();
}

void EventManagerImpl::handle_gamepad_axis_input() noexcept {
	if (active_gamepad == nullptr) {
		// Exit if no active gamepad
		return;
	}
	constexpr const float axis_dead_zone = 0.1f;
	constexpr const float mouse_scale    = 1.f;

	auto get_normalized_axis = [&](SDL_GameControllerAxis axis) {
		// Dead-zone is applied to each axis, X and Y, on the game's 2d
		// plane. All axis readings below this are ignored
		float value = SDL_GameControllerGetAxis(active_gamepad, axis);
		value /= static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
		// Many analog gamepads report non-zero axis values, even when the
		// gamepads isn't moving.  These non-zero values can change over time,
		// as the thumb-stick is moved by the player.
		//
		// In order to prevent idle gamepads sticks from leading to unwanted
		// movements, axis-values that are small will be ignored.  This is
		// sometimes referred to as a "dead zone".
		if (axis_dead_zone >= std::fabs(value) || isAlmostZero(value)) {
			value = 0.0f;
		}
		return value;
	};

	const AxisVector joy_aim{
			get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTX),
			get_normalized_axis(SDL_CONTROLLER_AXIS_LEFTY)};
	const AxisVector joy_mouse{
			get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTX),
			get_normalized_axis(SDL_CONTROLLER_AXIS_RIGHTY)};
	const AxisTrigger joy_rise{
			get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
			get_normalized_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)};

	if (joy_mouse.isNonzero()) {
		// This joypad axis is the equivalent of the mouse.
		const Game_window* gwin  = Game_window::get_instance();
		const int          scale = gwin->get_win()->get_scale_factor();

		const bool aspect = [&]() {
			Image_window::FillMode fillmode = gwin->get_win()->get_fill_mode();
			if (fillmode >= Image_window::Centre && fillmode < (1 << 16)) {
				return (fillmode & 1) != 0;
			}
			if (fillmode == Image_window::AspectCorrectFit) {
				return true;
			}
			return false;
		}();

		int x;
		int y;
		SDL_GetMouseState(&x, &y);
		auto delta_x = static_cast<int>(
				round(mouse_scale * joy_mouse.x * static_cast<float>(scale)));
		auto delta_y = static_cast<int>(
				round(mouse_scale * joy_mouse.y * static_cast<float>(scale)
					  * (aspect ? 1.2f : 1.0f)));
		x += delta_x;
		y += delta_y;
		TileRect rc = gwin->get_full_rect();
		rc.w *= scale;
		rc.h *= scale;
		if (aspect) {
			rc.h *= 6;
			rc.h /= 5;
		}
		x = std::clamp(x, 0, rc.w - 1);
		y = std::clamp(y, 0, rc.h - 1);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_TRUE);
		SDL_WarpMouseInWindow(nullptr, x, y);
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
	}

	invoke_callback<GamepadAxisCallback>(joy_aim, joy_mouse, joy_rise);
}

void EventManagerImpl::handle_event(
		const SDL_ControllerDeviceEvent& event) noexcept {
	switch (event.type) {
	case SDL_CONTROLLERDEVICEADDED: {
		// If we are already using a gamepad, skip.
		if (active_gamepad == nullptr) {
			const SDL_JoystickID joystick_id
					= SDL_JoystickGetDeviceInstanceID(event.which);
			if (SDL_GameControllerFromInstanceID(joystick_id) == nullptr) {
				active_gamepad = open_gamepad(event.which);
			}
		}
		break;
	}
	case SDL_CONTROLLERDEVICEREMOVED: {
		// If the gamepad we are using was removed, we need a new one.
		if (active_gamepad != nullptr
			&& event.which
					   == SDL_JoystickInstanceID(
							   SDL_GameControllerGetJoystick(active_gamepad))) {
			std::cout << "Gamepad \"" << SDL_GameControllerName(active_gamepad)
					  << "\" detached and closed.\n";
			SDL_GameControllerClose(active_gamepad);
			active_gamepad = find_gamepad();
		}
		break;
	}
	default:
		break;
	}
}

void EventManagerImpl::handle_event(
		const SDL_ControllerButtonEvent& event) const noexcept {
	// TODO: Maybe convert to mouse buttons here?
	const GamepadButtonEvent kind     = event.type == SDL_CONTROLLERBUTTONDOWN
												? GamepadButtonEvent::Pressed
												: GamepadButtonEvent::Released;
	const GamepadButton      buttonID = translateGamepadButton(event.button);
	if (buttonID != GamepadButton::Invalid) {
		invoke_callback<GamepadButtonCallback>(kind, buttonID);
	}
}

void EventManagerImpl::handle_event(
		const SDL_KeyboardEvent& event) const noexcept {
	const KeyboardEvent kind = event.type == SDL_KEYDOWN
									   ? KeyboardEvent::Pressed
									   : KeyboardEvent::Released;
	const auto          sym  = translateKeyCode(event.keysym.sym);
	const auto          mod  = translateKeyMods(event.keysym.mod);
	invoke_callback<KeyboardCallback>(kind, sym, mod);
}

void EventManagerImpl::handle_event(
		const SDL_TextInputEvent& event) const noexcept {
	// TODO: This would be a good place to convert input text into the game's
	// codepage. Currently, let's just silently convert non-ASCII characters
	// into '?'.
	char chr = event.text[0];
	if ((chr & 0x80) != 0) {
		chr = '?';
	}
	invoke_callback<TextInputCallback>(chr);
}

void EventManagerImpl::handle_event(
		const SDL_MouseButtonEvent& event) noexcept {
	const MouseButton buttonID = translateMouseButton(event.button);
	if (buttonID != MouseButton::Invalid) {
		const MouseEvent kind   = event.type == SDL_MOUSEBUTTONDOWN
										  ? MouseEvent::Pressed
										  : MouseEvent::Released;
		const uint32     clicks = event.clicks;
		if (kind == MouseEvent::Released && clicks == 1
			&& buttonID != MouseButton::Middle) {
			double_click_timer.start(
					std::make_unique<MouseUpData>(this, buttonID));
			return;
		}
		const SDL_bool state
				= kind == MouseEvent::Pressed ? SDL_TRUE : SDL_FALSE;
		const Game_window* gwin = Game_window::get_instance();
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), state);
		double_click_timer.stop();
		invoke_callback<MouseButtonCallback>(
				kind, buttonID, clicks, MousePosition(event.x, event.y));
	}
}

void EventManagerImpl::handle_event(
		const SDL_MouseMotionEvent& event) const noexcept {
	if (Mouse::use_touch_input && event.which != EXSDL_TOUCH_MOUSEID) {
		Mouse::use_touch_input = false;
	}
	MousePosition mouse(event.x, event.y);
	Mouse::mouse->move(mouse.x, mouse.y);
	Mouse::mouse_update      = true;
	MouseButtonMask buttonID = translateMouseMasks(event.state);
	invoke_callback<MouseMotionCallback>(buttonID, mouse);
}

void EventManagerImpl::handle_event(
		const SDL_MouseWheelEvent& event) const noexcept {
	MouseMotion delta(event.x, event.y);
	invoke_callback<MouseWheelCallback>(delta);
}

void EventManagerImpl::handle_event(
		const SDL_TouchFingerEvent& event) const noexcept {
	Game_window* gwin = Game_window::get_instance();
	switch (event.type) {
	case SDL_FINGERDOWN:
		if ((!Mouse::use_touch_input) && (event.fingerId != 0)) {
			Mouse::use_touch_input = true;
			gwin->set_painted();
		}
		break;

	case SDL_FINGERUP:
		// Not handled for now. These end up being synthesized into
		// mouse events by SDL anyway.
		break;

	case SDL_FINGERMOTION: {
		if (const SDL_Finger* finger0 = SDL_GetTouchFinger(event.touchId, 0);
			finger0 == nullptr) {
			break;
		}
		if (int numFingers = SDL_GetNumTouchFingers(event.touchId);
			numFingers >= 1) {
			invoke_callback<FingerMotionCallback>(
					numFingers, FingerMotion{event.dx, event.dy});
		}
		break;
	}
	default:
		break;
	}
}

struct SDLDeleter {
	void operator()(uint8* data) const {
		SDL_free(data);
	}
};

using SDLPtr = std::unique_ptr<uint8[], SDLDeleter>;

void EventManagerImpl::handle_event(const SDL_DropEvent& event) const noexcept {
	SDLPtr data(reinterpret_cast<uint8*>(event.file));
	invoke_callback<DropFileCallback>(
			event.type, data.get(), MousePosition(get_from_sdl_tag{}));
}

void EventManagerImpl::handle_background_event() const noexcept {
	invoke_callback<AppEventCallback>(AppEvents::OnEnterBackground);
}

void EventManagerImpl::handle_event(
		const SDL_WindowEvent& event) const noexcept {
	auto eventID = [&]() {
		switch (event.event) {
		case SDL_WINDOWEVENT_ENTER:
			return WindowEvents::Enter;
		case SDL_WINDOWEVENT_LEAVE:
			return WindowEvents::Leave;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			return WindowEvents::Focus_Gained;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			return WindowEvents::Focus_Lost;
		default:
			return WindowEvents::Unhandled;
		}
	}();
	invoke_callback<WindowEventCallback>(
			eventID, MousePosition(get_from_sdl_tag{}));
}

void EventManagerImpl::handle_quit_event() const {
	invoke_callback<QuitEventCallback>();
}

void EventManagerImpl::handle_custom_touch_input_event(
		const SDL_UserEvent& event) const noexcept {
	if (event.code == TouchUI::EVENT_CODE_TEXT_INPUT) {
		std::unique_ptr<const char[]> data(
				static_cast<const char*>(event.data1));
		const auto* text = data.get();
		invoke_callback<TouchInputCallback>(text);
	}
}

void EventManagerImpl::handle_custom_mouse_up_event(
		const SDL_UserEvent& event) noexcept {
	if (event.code == DELAYED_MOUSE_UP) {
		uintptr data;
		std::memcpy(&data, &event.data1, sizeof(uintptr));
		const MouseEvent   kind     = MouseEvent::Released;
		const uint32       clicks   = 1;
		const auto         buttonID = static_cast<MouseButton>(data);
		const Game_window* gwin     = Game_window::get_instance();
		SDL_SetWindowGrab(gwin->get_win()->get_screen_window(), SDL_FALSE);
		double_click_timer.stop();
		invoke_callback<MouseButtonCallback>(
				kind, buttonID, clicks, MousePosition(get_from_sdl_tag{}));
	}
}

void EventManagerImpl::handle_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_CONTROLLERDEVICEADDED:
	case SDL_CONTROLLERDEVICEREMOVED:
		handle_event(event.cdevice);
		break;

	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		handle_event(event.cbutton);
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		handle_event(event.key);
		break;

	case SDL_TEXTINPUT:
		handle_event(event.text);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		handle_event(event.button);
		break;

	case SDL_MOUSEMOTION:
		handle_event(event.motion);
		break;

	case SDL_MOUSEWHEEL:
		handle_event(event.wheel);
		break;

	case SDL_FINGERDOWN:
	case SDL_FINGERUP:
	case SDL_FINGERMOTION:
		handle_event(event.tfinger);
		break;

	case SDL_DROPFILE:
		handle_event(event.drop);
		break;

	case SDL_APP_WILLENTERBACKGROUND:
		handle_background_event();
		break;

	case SDL_WINDOWEVENT:
		handle_event(event.window);
		break;

	case SDL_QUIT:
		handle_quit_event();
		break;

	default:
		if (event.type == TouchUI::eventType) {
			handle_custom_touch_input_event(event.user);
		} else if (event.type == double_click_event_type) {
			handle_custom_mouse_up_event(event.user);
		}
		break;
	}
}

EventManager* EventManager::getInstance() {
	static auto instance(std::make_unique<EventManagerImpl>());
	return instance.get();
}

[[nodiscard]] bool EventManager::any_events_pending() const noexcept {
	SDL_PumpEvents();
	int num_events = SDL_PeepEvents(
			nullptr, 0, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	if (num_events < 0) {
		std::cerr << "SDL_PeepEvents failed with error " << SDL_GetError()
				  << '\n';
	}
	return num_events > 0;
}

void EventManager::start_text_input() const noexcept {
	if (SDL_IsTextInputActive() != SDL_TRUE) {
		SDL_StartTextInput();
	}
}

void EventManager::stop_text_input() const noexcept {
	if (SDL_IsTextInputActive() != SDL_FALSE) {
		SDL_StopTextInput();
	}
}

void EventManager::toggle_text_input() const noexcept {
	if (SDL_IsTextInputActive() != SDL_FALSE) {
		SDL_StopTextInput();
	} else {
		SDL_StartTextInput();
	}
}

void EventManagerImpl::handle_events() noexcept {
	SDL_Event event;
	while (!break_event_loop() && SDL_PollEvent(&event) != 0) {
		handle_event(event);
	}

	handle_gamepad_axis_input();
}

void EventManagerImpl::enable_dropfile() noexcept {
#ifdef USE_EXULTSTUDIO
	const Game_window* gwin = Game_window::get_instance();
	SDL_SysWMinfo      info;    // Get system info.
	SDL_GetWindowWMInfo(gwin->get_win()->get_screen_window(), &info);
#	ifndef _WIN32
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
#	else
	hgwin = info.info.win.window;
	OleInitialize(nullptr);
	windnd.reset(new (std::nothrow) Windnd(
			hgwin, Move_dragged_shape, Move_dragged_combo, Drop_dragged_shape,
			Drop_dragged_chunk, Drop_dragged_npc, Drop_dragged_combo));
	if (windnd == nullptr) {
		std::cerr << "Allocation failed for Windows Drag & Drop...\n";
		return;
	}
	HRESULT res = RegisterDragDrop(hgwin, windnd.get());
	if (FAILED(res)) {
		DWORD code = [](HRESULT hr) {
			if (HRESULT(hr & 0xFFFF0000L)
				== MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0)) {
				return HRESULT_CODE(hr);
			}

			if (hr == S_OK) {
				return ERROR_SUCCESS;
			}

			// Not a Win32 HRESULT so return a generic error code.
			return ERROR_CAN_NOT_COMPLETE;
		}(res);
		LPTSTR lpMsgBuf;
		FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
						| FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, code,
				MAKELANGID(
						LANG_NEUTRAL, SUBLANG_DEFAULT),    // Default language
				reinterpret_cast<LPTSTR>(&lpMsgBuf), 0, nullptr);
#		ifdef UNICODE
		nLen2     = _tcslen(lpMsgBuf) + 1;
		char* str = static_cast<char*>(_alloca(nLen));
		WideCharToMultiByte(
				CP_ACP, 0, lpMsgBuf, -1, str, nLen2, nullptr, nullptr);
#		else
		const char* str = lpMsgBuf;
#		endif
		std::cout << "RegisterDragDrop failed with error code " << code << ": "
				  << str << '\n';
		LocalFree(lpMsgBuf);
		windnd.reset();
	}
#	endif
#endif
}

void EventManagerImpl::disable_dropfile() noexcept {
#ifdef USE_EXULTSTUDIO
#	ifndef _WIN32
	SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
#	else
	RevokeDragDrop(hgwin);
	windnd.reset();
#	endif
#endif
}

#ifdef DEBUG
namespace {
#	define DECLARE_TEST_HELPER(x, y) x##y
#	define DECLARE_TEST(x, y) \
		[[maybe_unused]] void DECLARE_TEST_HELPER(x, y)(EventManager * events)

	namespace compile_tests {
		[[maybe_unused]] void cb1(
				const AxisVector&, const AxisVector&, const AxisTrigger&) {
			// Not implemented
		}

		[[maybe_unused]] void cb2(
				EventManager* self, const AxisVector&, const AxisVector&,
				const AxisTrigger&) {
			ignore_unused_variable_warning(self);
		}

		struct cb3 {
			[[maybe_unused]] void cb(
					const AxisVector&, const AxisVector&, const AxisTrigger&) {
				ignore_unused_variable_warning(this);
			}

			[[maybe_unused]] bool cb2() {
				ignore_unused_variable_warning(this);
				return false;
			}
		};

		struct cb4 {
			[[maybe_unused]] void operator()(
					const AxisVector&, const AxisVector&, const AxisTrigger&) {
				ignore_unused_variable_warning(this);
			}
		};

		struct cb5 {
			[[maybe_unused]] void operator()(
					const AxisVector&, const AxisVector&,
					const AxisTrigger&) noexcept {
				ignore_unused_variable_warning(this);
			}

			[[maybe_unused]] void operator()(
					GamepadButtonEvent, const GamepadButton) noexcept {
				ignore_unused_variable_warning(this);
			}

			[[maybe_unused]] void operator()(
					KeyboardEvent, const KeyCodes, const KeyMod) noexcept {
				ignore_unused_variable_warning(this);
			}
		};

		DECLARE_TEST(test_free_function, __LINE__) {
			auto guard = events->register_one_callback(cb1);
		}

		DECLARE_TEST(test_free_function_with_arg, __LINE__) {
			auto guard = events->register_one_callback(events, cb2);
		}

		DECLARE_TEST(test_one_member_function, __LINE__) {
			cb3  vcb3;
			auto guard = events->register_one_callback(&vcb3, &cb3::cb);
		}

		DECLARE_TEST(test_two_member_functions, __LINE__) {
			cb3  vcb3;
			auto guard = events->register_callbacks(vcb3, &cb3::cb, &cb3::cb2);
		}

		DECLARE_TEST(test_call_operator_ptr, __LINE__) {
			cb4  vcb4;
			auto guard = events->register_one_callback(&vcb4, &cb4::operator());
		}

		DECLARE_TEST(test_call_operator_ref, __LINE__) {
			cb4  vcb4;
			auto guard = events->register_one_callback(vcb4, &cb4::operator());
		}

		DECLARE_TEST(test_all_call_operator, __LINE__) {
			cb5  vcb5;
			auto guard = events->register_callbacks(vcb5);
		}

		DECLARE_TEST(test_stateless_lambda, __LINE__) {
			auto guard = events->register_one_callback(
					+[](const AxisVector&, const AxisVector&,
						const AxisTrigger&) noexcept { /* Not needed*/ });
		}

		DECLARE_TEST(test_stateful_lambda, __LINE__) {
			auto vcb6 = [events](
								const AxisVector&, const AxisVector&,
								const AxisTrigger&) noexcept {
				ignore_unused_variable_warning(events);
			};
			auto guard = events->register_one_callback(vcb6);
		}

		DECLARE_TEST(test_callbacks, __LINE__) {
			auto guard = events->register_one_callback(
					[events](
							const AxisVector&, const AxisVector&,
							const AxisTrigger&) noexcept {
						ignore_unused_variable_warning(events);
					});
		}

		DECLARE_TEST(test_callbacks, __LINE__) {
			auto guard = events->register_callbacks(
					cb1, +[]() {
						return true;
					});
		}
	}    // namespace compile_tests

#	undef DECLARE_TEST_HELPER
#	undef DECLARE_TEST
}    // namespace
#endif
