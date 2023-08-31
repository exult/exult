/*
 * Copyright (C) 2015 Chaoji Li
 * Copyright (C) 2017-2022 The Exult Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#import <UIKit/UIKit.h>


#include <SDL3/SDL.h>

@protocol KeyInputDelegate

- (void)keydown:(SDL_Scancode)keycode;
- (void)keyup:(SDL_Scancode)keycode;

@end


const float vjoy_radius = 80.f;  // max-radius of vjoy

@interface DPadView : UIView
{
	UIImage *backgroundImage;
	bool vjoy_is_active; 		            // true when the vjoy is active
	CGPoint vjoy_center;     	            // center of the vjoy
	CGPoint vjoy_current;     	            // current position of the vjoy
	SDL_Gamepad *vjoy_controller;    // the vjoy's SDL_Gamepad
	UITouch * __weak vjoy_input_source;	    // where vjoy input is actively coming from
}

@property (nonatomic, retain) NSArray *images;
@property (nonatomic, retain) UIImage *backgroundImage;

@end


typedef enum
{
	GamePadButtonStyleRoundedRectangle,
	GamePadButtonStyleCircle,
} GamePadButtonStyle;

@class GamePadButton;

@protocol GamePadButtonDelegate
- (void)buttonDown:(GamePadButton*)btn;
- (void)buttonUp:(GamePadButton*)btn;
@end

@interface GamePadButton : UIView
{
	/* Bindings */
	NSArray *keyCodes;

	NSString *title;
	BOOL pressed;
	GamePadButtonStyle style;

	NSArray *images;
	UIColor *textColor;
	id<GamePadButtonDelegate> delegate;
}

@property (nonatomic, assign) id<GamePadButtonDelegate> delegate;
@property (nonatomic, retain) UIColor *textColor;
@property (nonatomic, retain) NSArray *images;
@property (nonatomic, retain) NSString *title;
@property (nonatomic, assign) BOOL pressed;
@property (nonatomic, retain) NSArray *keyCodes;
@property (nonatomic, assign) GamePadButtonStyle style;

@end
