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

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "common_types.h"
#include "ignore_unused_variable_warning.h"

#include <cctype>
#include <functional>
#include <stack>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef __clang__
// Working around this clang bug with pragma push/pop:
// https://github.com/clangd/clangd/issues/1167
static_assert(true);
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-template"
#	pragma GCC diagnostic ignored "-Wswitch-enum"
#endif    // __clang__

// Callback related stuff.

// This is called before every call to SDL_PollEvent. If the callback returns
// true, the event loop is terminated.
using BreakLoopCallback = bool();

// If a gamepad is connected, this callback is called after processing all
// events for the current tick. The game mouse is updated to be in the position
// indicated by 'pos' before the callback.
struct AxisVector {
	float x;
	float y;
	bool  isNonzero() const noexcept;
};

struct AxisTrigger {
	float left;
	float right;
	bool  isNonzero() const noexcept;
};

using GamepadAxisCallback
		= void(const AxisVector& leftAxis, const AxisVector& rightAxis,
			   const AxisTrigger& triggers);

// Gamepad button up/down callbacks.
enum class GamepadButtonEvent {
	Pressed,
	Released,
};

// Note: recreating this enum here so that:
// (1) we don'y have to include the SDL include in the header;
// (2) to better isolate against SDL changes.
enum class GamepadButton {
	Invalid = -1,
	A,
	B,
	X,
	Y,
	Back,
	Guide,
	Start,
	LeftStick,
	RightStick,
	LeftShoulder,
	RightShoulder,
	DPad_Up,
	DPad_Down,
	DPad_Left,
	DPad_Right,
	// Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro
	// capture button, Amazon Luna microphone button
	Misc1,
	Paddle1,     // Xbox Elite paddle P1 (upper left, facing the back)
	Paddle2,     // Xbox Elite paddle P3 (upper right, facing the back)
	Paddle3,     // Xbox Elite paddle P2 (lower left, facing the back)
	Paddle4,     // Xbox Elite paddle P4 (lower right, facing the back)
	Touchpad,    // PS4/PS5 touchpad button
};

using GamepadButtonCallback
		= void(GamepadButtonEvent type, const GamepadButton sym);

// Keyboard key up/down callbacks.
enum class KeyboardEvent {
	Pressed,
	Released,
};

namespace {
	constexpr inline const uint32 ScancodeMask = 1 << 30;

	constexpr uint32 ScancodeToKeycode(uint32 key) {
		return key | ScancodeMask;
	}
}    // namespace

enum class KeyCodes {
	Key_Unknown            = 0,
	Key_Return             = '\r',
	Key_Escape             = '\x1B',
	Key_Backspace          = '\b',
	Key_Tab                = '\t',
	Key_Space              = ' ',
	Key_Exclaim            = '!',
	Key_DblQuote           = '"',
	Key_Hash               = '#',
	Key_Percent            = '%',
	Key_Dollar             = '$',
	Key_Ampersand          = '&',
	Key_Quote              = '\'',
	Key_LeftParen          = '(',
	Key_RightParen         = ')',
	Key_Asterisk           = '*',
	Key_Plus               = '+',
	Key_Comma              = ',',
	Key_Minus              = '-',
	Key_Period             = '.',
	Key_Slash              = '/',
	Key_0                  = '0',
	Key_1                  = '1',
	Key_2                  = '2',
	Key_3                  = '3',
	Key_4                  = '4',
	Key_5                  = '5',
	Key_6                  = '6',
	Key_7                  = '7',
	Key_8                  = '8',
	Key_9                  = '9',
	Key_Colon              = ':',
	Key_Semicolon          = ';',
	Key_Less               = '<',
	Key_Equals             = '=',
	Key_Greater            = '>',
	Key_Question           = '?',
	Key_At                 = '@',
	Key_LeftBracket        = '[',
	Key_Backslash          = '\\',
	Key_RightBracket       = ']',
	Key_Caret              = '^',
	Key_Underscore         = '_',
	Key_Backquote          = '`',
	Key_a                  = 'a',
	Key_b                  = 'b',
	Key_c                  = 'c',
	Key_d                  = 'd',
	Key_e                  = 'e',
	Key_f                  = 'f',
	Key_g                  = 'g',
	Key_h                  = 'h',
	Key_i                  = 'i',
	Key_j                  = 'j',
	Key_k                  = 'k',
	Key_l                  = 'l',    // NOLINT(misc-confusable-identifiers)
	Key_m                  = 'm',
	Key_n                  = 'n',
	Key_o                  = 'o',
	Key_p                  = 'p',
	Key_q                  = 'q',
	Key_r                  = 'r',
	Key_s                  = 's',
	Key_t                  = 't',
	Key_u                  = 'u',
	Key_v                  = 'v',
	Key_w                  = 'w',
	Key_x                  = 'x',
	Key_y                  = 'y',
	Key_z                  = 'z',
	Key_CapsLock           = ScancodeToKeycode(57),
	Key_F1                 = ScancodeToKeycode(58),
	Key_F2                 = ScancodeToKeycode(59),
	Key_F3                 = ScancodeToKeycode(60),
	Key_F4                 = ScancodeToKeycode(61),
	Key_F5                 = ScancodeToKeycode(62),
	Key_F6                 = ScancodeToKeycode(63),
	Key_F7                 = ScancodeToKeycode(64),
	Key_F8                 = ScancodeToKeycode(65),
	Key_F9                 = ScancodeToKeycode(66),
	Key_F10                = ScancodeToKeycode(67),
	Key_F11                = ScancodeToKeycode(68),
	Key_F12                = ScancodeToKeycode(69),
	Key_PrintScreen        = ScancodeToKeycode(70),
	Key_ScrollLock         = ScancodeToKeycode(71),
	Key_Pause              = ScancodeToKeycode(72),
	Key_Insert             = ScancodeToKeycode(73),
	Key_Home               = ScancodeToKeycode(74),
	Key_PageUp             = ScancodeToKeycode(75),
	Key_Delete             = '\x7F',
	Key_End                = ScancodeToKeycode(77),
	Key_PageDown           = ScancodeToKeycode(78),
	Key_Right              = ScancodeToKeycode(79),
	Key_Left               = ScancodeToKeycode(80),
	Key_Down               = ScancodeToKeycode(81),
	Key_Up                 = ScancodeToKeycode(82),
	Key_NumLockClear       = ScancodeToKeycode(83),
	Key_KP_Divide          = ScancodeToKeycode(84),
	Key_KP_Multiply        = ScancodeToKeycode(85),
	Key_KP_Minus           = ScancodeToKeycode(86),
	Key_KP_Plus            = ScancodeToKeycode(87),
	Key_KP_Enter           = ScancodeToKeycode(88),
	Key_KP_1               = ScancodeToKeycode(89),
	Key_KP_2               = ScancodeToKeycode(90),
	Key_KP_3               = ScancodeToKeycode(91),
	Key_KP_4               = ScancodeToKeycode(92),
	Key_KP_5               = ScancodeToKeycode(93),
	Key_KP_6               = ScancodeToKeycode(94),
	Key_KP_7               = ScancodeToKeycode(95),
	Key_KP_8               = ScancodeToKeycode(96),
	Key_KP_9               = ScancodeToKeycode(97),
	Key_KP_0               = ScancodeToKeycode(98),
	Key_KP_Period          = ScancodeToKeycode(99),
	Key_Application        = ScancodeToKeycode(101),
	Key_Power              = ScancodeToKeycode(102),
	Key_KP_Equals          = ScancodeToKeycode(103),
	Key_F13                = ScancodeToKeycode(104),
	Key_F14                = ScancodeToKeycode(105),
	Key_F15                = ScancodeToKeycode(106),
	Key_F16                = ScancodeToKeycode(107),
	Key_F17                = ScancodeToKeycode(108),
	Key_F18                = ScancodeToKeycode(109),
	Key_F19                = ScancodeToKeycode(110),
	Key_F20                = ScancodeToKeycode(111),
	Key_F21                = ScancodeToKeycode(112),
	Key_F22                = ScancodeToKeycode(113),
	Key_F23                = ScancodeToKeycode(114),
	Key_F24                = ScancodeToKeycode(115),
	Key_Execute            = ScancodeToKeycode(116),
	Key_Help               = ScancodeToKeycode(117),
	Key_Menu               = ScancodeToKeycode(118),
	Key_Select             = ScancodeToKeycode(119),
	Key_Stop               = ScancodeToKeycode(120),
	Key_Again              = ScancodeToKeycode(121),
	Key_Undo               = ScancodeToKeycode(122),
	Key_Cut                = ScancodeToKeycode(123),
	Key_Copy               = ScancodeToKeycode(124),
	Key_Paste              = ScancodeToKeycode(125),
	Key_Find               = ScancodeToKeycode(126),
	Key_Mute               = ScancodeToKeycode(127),
	Key_VolumeUp           = ScancodeToKeycode(128),
	Key_VolumeDown         = ScancodeToKeycode(129),
	Key_KP_Comma           = ScancodeToKeycode(133),
	Key_KP_EqualsAS400     = ScancodeToKeycode(134),
	Key_AltErase           = ScancodeToKeycode(153),
	Key_SysReq             = ScancodeToKeycode(154),
	Key_Cancel             = ScancodeToKeycode(155),
	Key_Clear              = ScancodeToKeycode(156),
	Key_Prior              = ScancodeToKeycode(157),
	Key_Return2            = ScancodeToKeycode(158),
	Key_Separator          = ScancodeToKeycode(159),
	Key_Out                = ScancodeToKeycode(160),
	Key_Oper               = ScancodeToKeycode(161),
	Key_ClearAgain         = ScancodeToKeycode(162),
	Key_CrSel              = ScancodeToKeycode(163),
	Key_ExSel              = ScancodeToKeycode(164),
	Key_KP_00              = ScancodeToKeycode(176),
	Key_KP_000             = ScancodeToKeycode(177),
	Key_ThousandsSeparator = ScancodeToKeycode(178),
	Key_DecimalSeparator   = ScancodeToKeycode(179),
	Key_CurrencyUnit       = ScancodeToKeycode(180),
	Key_CurrencySubunit    = ScancodeToKeycode(181),
	Key_KP_LeftParen       = ScancodeToKeycode(182),
	Key_KP_RightParen      = ScancodeToKeycode(183),
	Key_KP_LeftBrace       = ScancodeToKeycode(184),
	Key_KP_RightBrace      = ScancodeToKeycode(185),
	Key_KP_Tab             = ScancodeToKeycode(186),
	Key_KP_Backspace       = ScancodeToKeycode(187),
	Key_KP_A               = ScancodeToKeycode(188),
	Key_KP_B               = ScancodeToKeycode(189),
	Key_KP_C               = ScancodeToKeycode(190),
	Key_KP_D               = ScancodeToKeycode(191),
	Key_KP_E               = ScancodeToKeycode(192),
	Key_KP_F               = ScancodeToKeycode(193),
	Key_KP_Xor             = ScancodeToKeycode(194),
	Key_KP_Power           = ScancodeToKeycode(195),
	Key_KP_Percent         = ScancodeToKeycode(196),
	Key_KP_Less            = ScancodeToKeycode(197),
	Key_KP_Greater         = ScancodeToKeycode(198),
	Key_KP_Ampersand       = ScancodeToKeycode(199),
	Key_KP_DblAmpersand    = ScancodeToKeycode(200),
	Key_KP_VerticalBar     = ScancodeToKeycode(201),
	Key_KP_DblVerticalBar  = ScancodeToKeycode(202),
	Key_KP_Colon           = ScancodeToKeycode(203),
	Key_KP_Hash            = ScancodeToKeycode(204),
	Key_KP_Space           = ScancodeToKeycode(205),
	Key_KP_At              = ScancodeToKeycode(206),
	Key_KP_Exclamation     = ScancodeToKeycode(207),
	Key_KP_MemStore        = ScancodeToKeycode(208),
	Key_KP_MemRecall       = ScancodeToKeycode(209),
	Key_KP_MemClear        = ScancodeToKeycode(210),
	Key_KP_MemAdd          = ScancodeToKeycode(211),
	Key_KP_MemSubtract     = ScancodeToKeycode(212),
	Key_KP_MemMultiply     = ScancodeToKeycode(213),
	Key_KP_MemDivide       = ScancodeToKeycode(214),
	Key_KP_PlusMinus       = ScancodeToKeycode(215),
	Key_KP_Clear           = ScancodeToKeycode(216),
	Key_KP_ClearEntry      = ScancodeToKeycode(217),
	Key_KP_Binary          = ScancodeToKeycode(218),
	Key_KP_Octal           = ScancodeToKeycode(219),
	Key_KP_Decimal         = ScancodeToKeycode(220),
	Key_KP_Hexadecimal     = ScancodeToKeycode(221),
	Key_LeftCtrl           = ScancodeToKeycode(224),
	Key_LeftShift          = ScancodeToKeycode(225),
	Key_LeftAlt            = ScancodeToKeycode(226),
	Key_LeftGUI            = ScancodeToKeycode(227),
	Key_RightCtrl          = ScancodeToKeycode(228),
	Key_RightShift         = ScancodeToKeycode(229),
	Key_RightAlt           = ScancodeToKeycode(230),
	Key_RightGUI           = ScancodeToKeycode(231),
	Key_Mode               = ScancodeToKeycode(257),
	Key_AudioNext          = ScancodeToKeycode(258),
	Key_AudioPrev          = ScancodeToKeycode(259),
	Key_AudioStop          = ScancodeToKeycode(260),
	Key_AudioPlay          = ScancodeToKeycode(261),
	Key_AudioMute          = ScancodeToKeycode(262),
	Key_MediaSelect        = ScancodeToKeycode(263),
	Key_WWW                = ScancodeToKeycode(264),
	Key_Mail               = ScancodeToKeycode(265),
	Key_Calculator         = ScancodeToKeycode(266),
	Key_Computer           = ScancodeToKeycode(267),
	Key_AC_Search          = ScancodeToKeycode(268),
	Key_AC_Home            = ScancodeToKeycode(269),
	Key_AC_Back            = ScancodeToKeycode(270),
	Key_AC_Forward         = ScancodeToKeycode(271),
	Key_AC_Stop            = ScancodeToKeycode(272),
	Key_AC_Refresh         = ScancodeToKeycode(273),
	Key_AC_Bookmarks       = ScancodeToKeycode(274),
	Key_BrightnessDown     = ScancodeToKeycode(275),
	Key_BrightnessUp       = ScancodeToKeycode(276),
	Key_DisplaySwitch      = ScancodeToKeycode(277),
	Key_KbdIllumToggle     = ScancodeToKeycode(278),
	Key_KbdIllumDown       = ScancodeToKeycode(279),
	Key_KbdIllumUp         = ScancodeToKeycode(280),
	Key_Eject              = ScancodeToKeycode(281),
	Key_Sleep              = ScancodeToKeycode(282),
	Key_App1               = ScancodeToKeycode(283),
	Key_App2               = ScancodeToKeycode(284),
	Key_AudioRewind        = ScancodeToKeycode(285),
	Key_AudioFastforward   = ScancodeToKeycode(286),
	Key_SoftLeft           = ScancodeToKeycode(287),
	Key_SoftRight          = ScancodeToKeycode(288),
	Key_Call               = ScancodeToKeycode(289),
	Key_EndCall            = ScancodeToKeycode(290)
};

constexpr int operator-(KeyCodes lhs, KeyCodes rhs) {
	using Tp = std::underlying_type_t<KeyCodes>;
	return static_cast<Tp>(lhs) - static_cast<Tp>(rhs);
}

constexpr KeyCodes operator-=(KeyCodes& lhs, int rhs) {
	using Tp = std::underlying_type_t<KeyCodes>;
	lhs      = static_cast<KeyCodes>(static_cast<Tp>(lhs) - rhs);
	return lhs;
}

constexpr KeyCodes operator-(KeyCodes lhs, int rhs) {
	lhs -= rhs;
	return lhs;
}

constexpr KeyCodes operator+=(KeyCodes& lhs, int rhs) {
	using Tp = std::underlying_type_t<KeyCodes>;
	lhs      = static_cast<KeyCodes>(static_cast<Tp>(lhs) + rhs);
	return lhs;
}

constexpr KeyCodes operator+(KeyCodes lhs, int rhs) {
	lhs += rhs;
	return lhs;
}

/**
 * \brief Enumeration of valid key mods (possibly OR'd together).
 */
enum class KeyMod {
	NoMods     = 0x0000,
	LeftShift  = 0x0001,
	RightShift = 0x0002,
	LeftCtrl   = 0x0040,
	RightCtrl  = 0x0080,
	LeftAlt    = 0x0100,
	RightAlt   = 0x0200,
	LeftGUI    = 0x0400,
	RightGUI   = 0x0800,
	Num        = 0x1000,
	Caps       = 0x2000,
	Mode       = 0x4000,
	Scroll     = 0x8000,

	Ctrl  = LeftCtrl | RightCtrl,
	Shift = LeftShift | RightShift,
	Alt   = LeftAlt | RightAlt,
	GUI   = LeftGUI | RightGUI,

	Reserved = Scroll
};

constexpr KeyMod operator|=(KeyMod& lhs, KeyMod rhs) {
	using Tp = std::underlying_type_t<KeyMod>;
	lhs      = static_cast<KeyMod>(static_cast<Tp>(lhs) | static_cast<Tp>(rhs));
	return lhs;
}

constexpr KeyMod operator|(KeyMod lhs, KeyMod rhs) {
	lhs |= rhs;
	return lhs;
}

constexpr KeyMod operator&=(KeyMod& lhs, KeyMod rhs) {
	using Tp = std::underlying_type_t<KeyMod>;
	lhs      = static_cast<KeyMod>(static_cast<Tp>(lhs) & static_cast<Tp>(rhs));
	return lhs;
}

constexpr KeyMod operator&(KeyMod lhs, KeyMod rhs) {
	lhs &= rhs;
	return lhs;
}

inline bool is_digit(KeyCodes key, KeyMod mod) {
	switch (key) {
	case KeyCodes::Key_0:
	case KeyCodes::Key_1:
	case KeyCodes::Key_2:
	case KeyCodes::Key_3:
	case KeyCodes::Key_4:
	case KeyCodes::Key_5:
	case KeyCodes::Key_6:
	case KeyCodes::Key_7:
	case KeyCodes::Key_8:
	case KeyCodes::Key_9:
		return (mod
				& (KeyMod::Shift | KeyMod::Alt | KeyMod::Ctrl | KeyMod::GUI))
			   == KeyMod::NoMods;
	case KeyCodes::Key_KP_0:
	case KeyCodes::Key_KP_1:
	case KeyCodes::Key_KP_2:
	case KeyCodes::Key_KP_3:
	case KeyCodes::Key_KP_4:
	case KeyCodes::Key_KP_5:
	case KeyCodes::Key_KP_6:
	case KeyCodes::Key_KP_7:
	case KeyCodes::Key_KP_8:
	case KeyCodes::Key_KP_9:
		return (mod & KeyMod::Num) == KeyMod::Num;
	default:
		return false;
	}
}

inline bool is_xdigit(KeyCodes key, KeyMod mod) {
	switch (key) {
	case KeyCodes::Key_a:
	case KeyCodes::Key_b:
	case KeyCodes::Key_c:
	case KeyCodes::Key_d:
	case KeyCodes::Key_e:
	case KeyCodes::Key_f:
		return (mod & (KeyMod::Alt | KeyMod::Ctrl | KeyMod::GUI))
			   == KeyMod::NoMods;
	case KeyCodes::Key_KP_A:
	case KeyCodes::Key_KP_B:
	case KeyCodes::Key_KP_C:
	case KeyCodes::Key_KP_D:
	case KeyCodes::Key_KP_E:
	case KeyCodes::Key_KP_F:
		return (mod & KeyMod::Num) == KeyMod::Num;
	default:
		return is_digit(key, mod);
	}
}

inline bool is_alpha(KeyCodes key, KeyMod mod) {
	switch (key) {
	case KeyCodes::Key_a:
	case KeyCodes::Key_b:
	case KeyCodes::Key_c:
	case KeyCodes::Key_d:
	case KeyCodes::Key_e:
	case KeyCodes::Key_f:
	case KeyCodes::Key_g:
	case KeyCodes::Key_h:
	case KeyCodes::Key_i:
	case KeyCodes::Key_j:
	case KeyCodes::Key_k:
	case KeyCodes::Key_l:
	case KeyCodes::Key_m:
	case KeyCodes::Key_n:
	case KeyCodes::Key_o:
	case KeyCodes::Key_p:
	case KeyCodes::Key_q:
	case KeyCodes::Key_r:
	case KeyCodes::Key_s:
	case KeyCodes::Key_t:
	case KeyCodes::Key_u:
	case KeyCodes::Key_v:
	case KeyCodes::Key_w:
	case KeyCodes::Key_x:
	case KeyCodes::Key_y:
	case KeyCodes::Key_z:
		return (mod & (KeyMod::Alt | KeyMod::Ctrl | KeyMod::GUI))
			   == KeyMod::NoMods;
	case KeyCodes::Key_KP_A:
	case KeyCodes::Key_KP_B:
	case KeyCodes::Key_KP_C:
	case KeyCodes::Key_KP_D:
	case KeyCodes::Key_KP_E:
	case KeyCodes::Key_KP_F:
		return (mod & KeyMod::Num) == KeyMod::Num;
	default:
		return false;
	}
}

inline bool is_alnum(KeyCodes key, KeyMod mod) {
	return is_alpha(key, mod) || is_digit(key, mod);
}

inline bool is_minus(KeyCodes key, KeyMod mod) {
	ignore_unused_variable_warning(mod);
	if (key == KeyCodes::Key_Minus) {
		return (mod & (KeyMod::Shift | KeyMod::Caps)) != KeyMod::NoMods;
	}
	return key == KeyCodes::Key_KP_Minus;
}

inline bool is_return(KeyCodes key, KeyMod mod) {
	ignore_unused_variable_warning(mod);
	return key == KeyCodes::Key_Return || key == KeyCodes::Key_KP_Enter;
}

inline int get_digit(KeyCodes key, KeyMod mod) {
	if (!is_digit(key, mod)) {
		return -1;
	}
	switch (key) {
	case KeyCodes::Key_0:
	case KeyCodes::Key_KP_0:
		return '0';
	case KeyCodes::Key_1:
	case KeyCodes::Key_KP_1:
		return '1';
	case KeyCodes::Key_2:
	case KeyCodes::Key_KP_2:
		return '2';
	case KeyCodes::Key_3:
	case KeyCodes::Key_KP_3:
		return '3';
	case KeyCodes::Key_4:
	case KeyCodes::Key_KP_4:
		return '4';
	case KeyCodes::Key_5:
	case KeyCodes::Key_KP_5:
		return '5';
	case KeyCodes::Key_6:
	case KeyCodes::Key_KP_6:
		return '6';
	case KeyCodes::Key_7:
	case KeyCodes::Key_KP_7:
		return '7';
	case KeyCodes::Key_8:
	case KeyCodes::Key_KP_8:
		return '8';
	case KeyCodes::Key_9:
	case KeyCodes::Key_KP_9:
		return '9';
	default:
		return -1;
	}
}

inline int get_xdigit(KeyCodes key, KeyMod mod) {
	if (!is_xdigit(key, mod)) {
		return -1;
	}
	switch (key) {
	case KeyCodes::Key_a:
	case KeyCodes::Key_b:
	case KeyCodes::Key_c:
	case KeyCodes::Key_d:
	case KeyCodes::Key_e:
	case KeyCodes::Key_f:
		return static_cast<std::underlying_type_t<KeyCodes>>(key);
	case KeyCodes::Key_KP_A:
		return 'a';
	case KeyCodes::Key_KP_B:
		return 'b';
	case KeyCodes::Key_KP_C:
		return 'c';
	case KeyCodes::Key_KP_D:
		return 'd';
	case KeyCodes::Key_KP_E:
		return 'e';
	case KeyCodes::Key_KP_F:
		return 'f';
	default:
		return get_digit(key, mod);
	}
}

inline int get_alpha(KeyCodes key, KeyMod mod) {
	if (is_alpha(key, mod)) {
		auto code = static_cast<std::underlying_type_t<KeyCodes>>(key);
		if ((mod & (KeyMod::Shift | KeyMod::Caps)) != KeyMod::NoMods) {
			code = std::toupper(code);
		}
		return code;
	}
	return -1;
}

inline int get_alnum(KeyCodes key, KeyMod mod) {
	if (is_digit(key, mod)) {
		return get_digit(key, mod);
	}
	return get_alpha(key, mod);
}

using KeyboardCallback
		= void(KeyboardEvent type, const KeyCodes sym, const KeyMod mod);

// This callback is called when compositing text is finished.
using TextInputCallback = void(char chr);

// This callback is called when mouse buttons are pressed or released.
struct get_from_sdl_tag {};

struct MousePosition {
	int x;
	int y;
	MousePosition() = default;
	explicit MousePosition(get_from_sdl_tag);
	MousePosition(int x_, int y_);
	void set(int x_, int y_);
};

// Note: recreating this enum here so that:
// (1) we don'y have to include the SDL include in the header;
// (2) to better isolate against SDL changes.
enum class MouseButton {
	Invalid = 0,
	Left    = 1,
	Middle  = 2,
	Right   = 3,
	X1      = 4,
	X2      = 5,
};

// Note: recreating this enum here so that:
// (1) we don'y have to include the SDL include in the header;
// (2) to better isolate against SDL changes.
enum class MouseButtonMask {
	None   = 0,
	Left   = 1,
	Middle = 2,
	Right  = 4,
	X1     = 8,
	X2     = 16,
};

constexpr MouseButtonMask operator|=(
		MouseButtonMask& lhs, MouseButtonMask rhs) {
	using Tp = std::underlying_type_t<MouseButtonMask>;
	lhs      = static_cast<MouseButtonMask>(
            static_cast<Tp>(lhs) | static_cast<Tp>(rhs));
	return lhs;
}

constexpr MouseButtonMask operator|(MouseButtonMask lhs, MouseButtonMask rhs) {
	lhs |= rhs;
	return lhs;
}

constexpr MouseButtonMask operator&=(
		MouseButtonMask& lhs, MouseButtonMask rhs) {
	using Tp = std::underlying_type_t<MouseButtonMask>;
	lhs      = static_cast<MouseButtonMask>(
            static_cast<Tp>(lhs) & static_cast<Tp>(rhs));
	return lhs;
}

constexpr MouseButtonMask operator&(MouseButtonMask lhs, MouseButtonMask rhs) {
	lhs &= rhs;
	return lhs;
}

enum class MouseEvent {
	Pressed,
	Released,
};

using MouseButtonCallback
		= void(MouseEvent type, MouseButton button, int numClicks,
			   const MousePosition& pos);

// This callback is called when a mouse moves.
using MouseMotionCallback
		= void(MouseButtonMask button, const MousePosition& pos);

// This callback is called on mouse wheel is scrolled.
struct MouseMotion {
	int x;
	int y;
	MouseMotion() = default;
	MouseMotion(int x_, int y_);
};

using MouseWheelCallback = void(MouseMotion& delta);

// This callback is called when a touch-enabled display device sends an event
// for finger(s) moving on the device.
struct FingerMotion {
	float x;
	float y;
	bool  isNonzero() const noexcept;
};

// TODO: Maybe add pressure?
using FingerMotionCallback = void(int numFingers, const FingerMotion& delta);

// This callback is called when any of the monitored events happen.
enum class AppEvents {
	Unhandled,
	OnEnterBackground,    //  The application is about to enter the background
};

using AppEventCallback = void(AppEvents event);

// This callback is called when any of the monitored events happen.
enum class WindowEvents {
	Unhandled,
	Enter,           // Window has gained mouse focus
	Leave,           // Window has lost mouse focus
	Focus_Gained,    // Window has gained keyboard focus
	Focus_Lost,      // Window has lost keyboard focus
};

using WindowEventCallback = void(WindowEvents event, const MousePosition& pos);

// This callback is called whenever a drop from ES happens, if it was enabled by
// a call to EventManager::enable_dropfile.
using DropFileCallback
		= void(uint32 type, const uint8* file, const MousePosition& pos);

// This callback is called when user requests the program to finish (e.g., by
// clicking the 'X' button on the window).
using QuitEventCallback = void();

// This callback is called when the TouchUI custom event happens.
using TouchInputCallback = void(const char* text);

// This callback is called on a delayed click is performed at the shortcut bar.
struct SDL_UserEvent;
using ShortcutBarClickCallback = void(SDL_UserEvent& event);

namespace { namespace detail {
	// Some stuff for constraining templates.
	// Cheeky alias for clarity.
	template <bool Condition>
	using require = std::enable_if_t<Condition, bool>;

	// A helper meta-function. I needed this because std::invoke_result is NOT
	// SFINAE-friendly.
	template <typename R, typename F, typename... Ts>
	[[maybe_unused]] constexpr auto apply_invoke_result() {
		if constexpr (std::is_invocable_r_v<R, F, Ts...>) {
			return std::is_same<
					decltype(std::invoke(
							std::declval<F>(), std::declval<Ts>()...)),
					R>{};
		} else {
			return std::false_type{};
		}
	}

	// Some types for checking callback compatibility.
	template <typename C, typename F, typename... Ts>
	struct is_compatible_with : std::false_type {};

	template <typename R, typename... Args, typename F, typename... Ts>
	struct is_compatible_with<R(Args...), F, Ts...>
			: decltype(apply_invoke_result<R, F, Ts..., Args...>()) {};

	template <typename C, typename F, typename... Ts>
	[[maybe_unused]] constexpr inline const bool is_compatible_with_v
			= is_compatible_with<C, F, Ts...>::value;

	template <typename C, typename F, typename... Ts>
	using compatible_with = is_compatible_with<C, F, Ts...>;

	template <typename C, typename F, typename... Ts>
	using compatible_with_t = require<is_compatible_with_v<C, F, Ts...>>;

	// A c++17 version of std::bind_front. Another thing in favor of c++20?
	// From https://stackoverflow.com/a/64714179/5103768
	template <typename T, typename... Args>
	[[maybe_unused]] auto tuple_append(T&& t, Args&&... args) {
		return std::tuple_cat(
				std::forward<T>(t), std::forward_as_tuple(args...));
	}

	template <typename F, typename... FrontArgs>
	[[maybe_unused]] decltype(auto) bind_front(
			F&& callable, FrontArgs&&... args) {
		return [functor   = std::forward<F>(callable),
				frontArgs = std::make_tuple(std::forward<FrontArgs>(args)...)](
					   auto&&... backArgs) {
			return std::apply(
					functor,
					tuple_append(
							frontArgs,
							std::forward<decltype(backArgs)>(backArgs)...));
		};
	}

	// A type list.
	template <typename... Ts>
	struct Type_list {
		constexpr static const size_t size = sizeof...(Ts);

		using tail = Type_list<Ts...>;
	};

	template <typename T, typename... Ts>
	struct Type_list<T, Ts...> {
		constexpr static const size_t size = sizeof...(Ts) + 1;

		using head = T;
		using tail = Type_list<Ts...>;
	};

	// A meta filter for type lists.
	// From https://codereview.stackexchange.com/a/115774
	template <typename... Lists>
	struct concat;

	template <>
	struct concat<> {
		using type = Type_list<>;
	};

	template <typename... Ts>
	struct concat<Type_list<Ts...>> {
		using type = Type_list<Ts...>;
	};

	template <typename... Ts, typename... Us>
	struct concat<Type_list<Ts...>, Type_list<Us...>> {
		using type = Type_list<Ts..., Us...>;
	};

	template <typename... Ts, typename... Us, typename... Rest>
	struct concat<Type_list<Ts...>, Type_list<Us...>, Rest...> {
		using type = typename concat<Type_list<Ts..., Us...>, Rest...>::type;
	};

	template <template <typename...> typename Pred, typename T>
	using filter_helper
			= std::conditional_t<Pred<T>::value, Type_list<T>, Type_list<>>;

	template <template <typename...> typename Pred, typename... Ts>
	using filter = typename concat<filter_helper<Pred, Ts>...>::type;

	// A custom version of the filter function which passes additional
	// arguments to the predicate.
	template <template <typename...> typename Pred, typename Arg1, typename T>
	using filter_helper1 = std::conditional_t<
			Pred<T, Arg1>::value, Type_list<T>, Type_list<>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename... Ts>
	using filter1 = typename concat<filter_helper1<Pred, Arg1, Ts>...>::type;

	// Underlying data structure for event manager.
	template <typename Callback_t>
	using CallbackStack = std::stack<std::function<Callback_t>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename Arg2,
			typename T>
	using filter_helper2 = std::conditional_t<
			Pred<T, Arg1, Arg2>::value, Type_list<T>, Type_list<>>;

	template <
			template <typename...> typename Pred, typename Arg1, typename Arg2,
			typename... Ts>
	using filter2 =
			typename concat<filter_helper2<Pred, Arg1, Arg2, Ts>...>::type;

	// RAII type for restoring old callback state after registering new
	// callbacks.
	template <typename Callback_t>
	class [[nodiscard]] Callback_guard {
		CallbackStack<Callback_t>* m_target = nullptr;

	public:
		template <
				typename Callable,
				require<std::is_same_v<
						Callback_t, std::remove_reference_t<
											std::remove_pointer_t<Callable>>>>
				= true>
		[[nodiscard]] Callback_guard(
				Callable&& callback, CallbackStack<Callback_t>& target)
				: m_target(&target) {
			m_target->emplace(std::forward<Callable>(callback));
		}

		template <typename Callable, compatible_with_t<Callback_t, Callable>>
		[[nodiscard]] Callback_guard(
				Callable&& callback, CallbackStack<Callback_t>& target)
				: m_target(&target) {
			m_target->emplace(std::forward<Callable>(callback));
		}

		[[nodiscard]] Callback_guard(
				std::function<Callback_t>&& callback,
				CallbackStack<Callback_t>&  target)
				: m_target(&target) {
			m_target->push(std::move(callback));
		}

		~Callback_guard() noexcept {
			if (m_target != nullptr) {
				m_target->pop();
			}
		}

		Callback_guard()                                 = default;
		Callback_guard(const Callback_guard&)            = delete;
		Callback_guard& operator=(const Callback_guard&) = delete;

		Callback_guard(Callback_guard&& other) noexcept
				: m_target(std::exchange(other.m_target, nullptr)) {}

		Callback_guard& operator=(Callback_guard&& other) noexcept {
			m_target = std::exchange(other.m_target, nullptr);
			return *this;
		}
	};

	// Deduction guides for making use of the guard easier.
	template <typename Callback_t>
	Callback_guard(Callback_t*, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	template <typename Callback_t>
	Callback_guard(std::function<Callback_t>&&, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	template <
			typename Callable, typename Callback_t,
			require<std::is_object_v<Callable>> = true>
	Callback_guard(Callable&&, CallbackStack<Callback_t>&)
			-> Callback_guard<Callback_t>;

	// A meta-list with all callback types.
	using Callback_list = detail::Type_list<
			BreakLoopCallback, GamepadAxisCallback, GamepadButtonCallback,
			KeyboardCallback, TextInputCallback, MouseButtonCallback,
			MouseMotionCallback, MouseWheelCallback, FingerMotionCallback,
			WindowEventCallback, AppEventCallback, DropFileCallback,
			QuitEventCallback, TouchInputCallback, ShortcutBarClickCallback>;

	// Meta function that generates the type of the main data structure of the
	// event manager.
	template <typename... Types>
	[[maybe_unused]] constexpr auto tuple_from_type_list(
			detail::Type_list<Types...>) -> std::tuple<CallbackStack<Types>...>;
	using Callback_tuple_t
			= decltype(tuple_from_type_list(std::declval<Callback_list>()));

	// Meta functions and types for constraining templates.
	template <typename F, typename... Ts>
	[[maybe_unused]] constexpr auto apply_compatible_with_filter1(
			detail::Type_list<Ts...>)
			-> detail::filter1<detail::compatible_with, F, Ts...>;

	template <typename F>
	using has_compatible_callback1 = std::integral_constant<
			bool,
			decltype(apply_compatible_with_filter1<F>(Callback_list{}))::size
					== 1>;

	template <typename F>
	[[maybe_unused]] constexpr inline const bool has_compatible_callback1_v
			= has_compatible_callback1<F>::value;

	template <typename F>
	using get_compatible_callback1 =
			typename decltype(apply_compatible_with_filter1<F>(
					Callback_list{}))::head;

	template <typename F, typename T, typename... Ts>
	[[maybe_unused]] constexpr auto apply_compatible_with_filter2(
			detail::Type_list<Ts...>)
			-> detail::filter2<detail::compatible_with, F, T, Ts...>;

	template <typename F, typename T>
	using has_compatible_callback2 = std::integral_constant<
			bool,
			decltype(apply_compatible_with_filter2<F, T>(Callback_list{}))::size
					== 1>;

	template <typename F, typename T>
	[[maybe_unused]] constexpr inline const bool has_compatible_callback2_v
			= has_compatible_callback2<F, T>::value;

	template <typename F, typename T>
	using get_compatible_callback2 =
			typename decltype(apply_compatible_with_filter2<F, T>(
					Callback_list{}))::head;

	template <typename T>
	struct remove_noexcept {
		using type = T;
	};

	template <typename Ret, typename... Args>
	struct remove_noexcept<Ret(Args...) noexcept> {
		using type = Ret(Args...);
	};

	template <typename Ret, typename... Args>
	struct remove_noexcept<Ret (*)(Args...) noexcept> {
		using type = Ret(Args...);
	};

	template <typename Ret, typename... Args>
	struct remove_noexcept<Ret (&)(Args...) noexcept> {
		using type = Ret(Args...);
	};

	template <typename T>
	using remove_noexcept_t = typename remove_noexcept<T>::type;

	// Meta function convert a non-member function signature to a pointer to
	// member function type.
	template <
			typename T, typename R, typename... Args,
			typename U = std::remove_reference_t<std::remove_pointer_t<T>>>
	[[maybe_unused]] auto member_cast_impl(T, R(Args...)) -> R (U::*)(Args...);

	template <typename T, typename Fs>
	using get_member_type_t
			= decltype(member_cast_impl(std::declval<T>(), std::declval<Fs>()));

	template <typename T, typename Fs>
	struct get_member_type {
		using type = get_member_type_t<T, Fs>;
	};

	template <typename T, typename Fs>
	[[maybe_unused]] auto get_call_operator() {
		using UnqualifiedT = std::remove_reference_t<std::remove_pointer_t<T>>;
		return static_cast<get_member_type_t<T, Fs>>(&UnqualifiedT::operator());
	}
}}    // namespace ::detail

class EventManager {
	template <typename Callback>
	using CallbackStack    = detail::CallbackStack<Callback>;
	using Callback_tuple_t = detail::Callback_tuple_t;
	Callback_tuple_t callbackStacks;

protected:
	template <typename Callback>
	CallbackStack<Callback>& get_callback_stack() {
		return std::get<CallbackStack<detail::remove_noexcept_t<Callback>>>(
				callbackStacks);
	}

	template <typename Callback>
	const CallbackStack<Callback>& get_callback_stack() const {
		return std::get<CallbackStack<detail::remove_noexcept_t<Callback>>>(
				callbackStacks);
	}

	template <typename T, typename... Fs>
	[[maybe_unused]] constexpr auto register_callback_object(
			T& data, detail::Type_list<Fs...>) {
		using tuple_t = decltype(std::make_tuple(register_one_callback(
				data, detail::get_call_operator<T, Fs>())...));

		struct opaque {
			tuple_t out;
		};

		return opaque{std::make_tuple(register_one_callback(
				data, detail::get_call_operator<T, Fs>())...)};
	}

	EventManager() = default;

public:
	static EventManager* getInstance();
	virtual ~EventManager()                      = default;
	EventManager(const EventManager&)            = delete;
	EventManager(EventManager&&)                 = delete;
	EventManager& operator=(const EventManager&) = delete;
	EventManager& operator=(EventManager&&)      = delete;

	// Registers one callback (based on type). For function pointers,
	// references, or functors.
	template <
			typename Callback,
			detail::require<detail::has_compatible_callback1_v<
					std::remove_reference_t<std::remove_pointer_t<Callback>>>>
			= true>
	[[nodiscard]] auto register_one_callback(Callback&& callback) {
		if constexpr (std::is_function_v<std::remove_reference_t<
							  std::remove_pointer_t<Callback>>>) {
			using Functor_t = detail::remove_noexcept_t<
					std::remove_reference_t<std::remove_pointer_t<Callback>>>;
			return detail::Callback_guard(
					std::forward<Callback>(callback),
					get_callback_stack<Functor_t>());
		}
		if constexpr (std::is_object_v<std::remove_reference_t<
							  std::remove_pointer_t<Callback>>>) {
			using Cb = std::remove_reference_t<std::remove_pointer_t<Callback>>;
			using Functor = detail::get_compatible_callback1<Cb>;
			std::function<Functor> fun(std::forward<Callback>(callback));
			return detail::Callback_guard(
					std::move(fun), get_callback_stack<Functor>());
		}
	}

	// Registers one callback (based on type). Accepts various callables, but
	// expects that they need a parameter of type T (pointer or reference) that
	// needs to be passed at the front. Also works with pointer-to-member
	// functions.
	template <
			typename T, typename F,
			detail::require<detail::has_compatible_callback2_v<F, T>> = true>
	[[nodiscard]] auto register_one_callback(T&& data, F&& callback) {
		using Callback = detail::get_compatible_callback2<F, T>;
		std::function<Callback> fun
				= detail::bind_front(std::forward<F>(callback), data);
		return detail::Callback_guard(
				std::move(fun), get_callback_stack<Callback>());
	}

	// Registers many callbacks (based on type). Forwards each parameter to its
	// own call to register_one_callback, then aggregates the results.
	template <
			typename... Fs,
			detail::require<std::conjunction_v<std::is_invocable<
					decltype(&EventManager::register_one_callback<Fs>),
					std::add_pointer_t<EventManager>, Fs>...>>
			= true>
	[[nodiscard]] auto register_callbacks(Fs&&... callback) {
		static_assert(sizeof...(Fs) != 0);
		if constexpr (sizeof...(Fs) == 1) {
			return register_one_callback(std::forward<Fs...>(callback...));
		} else {
			using tuple_t = decltype(std::make_tuple(
					register_one_callback(std::forward<Fs>(callback))...));

			struct opaque {
				tuple_t out;
			};

			return opaque{std::make_tuple(
					register_one_callback(std::forward<Fs>(callback))...)};
		}
	}

	// Registers many callbacks (based on type). Splits off the first argument,
	// then forwards it along with each of the remaining parameter to their own
	// calls to register_one_callback, then aggregates the results.
	template <
			typename T, typename... Fs,
			detail::require<std::conjunction_v<
					std::is_object<
							std::remove_reference_t<std::remove_pointer_t<T>>>,
					std::negation<detail::has_compatible_callback1<
							std::remove_reference_t<
									std::remove_pointer_t<T>>>>>>
			= true,
			detail::require<std::conjunction_v<std::is_invocable<
					decltype(&EventManager::register_one_callback<T, Fs>),
					std::add_pointer_t<EventManager>, T, Fs>...>>
			= true>
	[[nodiscard]] auto register_callbacks(T&& data, Fs&&... callback) {
		if constexpr (sizeof...(Fs) == 0) {
			using Callbacks = decltype(detail::apply_compatible_with_filter1<T>(
					detail::Callback_list{}));
			static_assert(
					Callbacks::size != 0,
					"Input object type has no usable callbacks");
			return register_callback_object(data, Callbacks{});
		} else if constexpr (sizeof...(Fs) == 1) {
			return register_one_callback(
					data, std::forward<Fs...>(callback...));
		} else {
			using tuple_t = decltype(std::make_tuple(register_one_callback(
					data, std::forward<Fs>(callback))...));

			struct opaque {
				tuple_t out;
			};

			return opaque{std::make_tuple(register_one_callback(
					data, std::forward<Fs>(callback))...)};
		}
	}

	virtual void handle_events() noexcept    = 0;
	virtual void enable_dropfile() noexcept  = 0;
	virtual void disable_dropfile() noexcept = 0;

	[[nodiscard]] bool any_events_pending() const noexcept;

	void start_text_input() const noexcept;
	void stop_text_input() const noexcept;
	void toggle_text_input() const noexcept;
};

#ifdef __clang__
#	pragma GCC diagnostic pop
#endif    // __clang__

#endif    // INPUT_MANAGER_H
