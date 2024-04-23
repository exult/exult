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

#include <SDL3/SDL.h>
#import <UIKit/UIKit.h>

@protocol KeyInputDelegate
- (void)keydown:(SDL_Scancode)keycode;
- (void)keyup:(SDL_Scancode)keycode;
@end

@interface DPadView : UIView

@property(nonatomic, retain) UIImage*     backgroundImage;
@property(nonatomic, retain) NSArray*     images;
@property(nonatomic, assign) bool         vjoyIsActive;
@property(nonatomic, assign) CGPoint      vjoyCenter;
@property(nonatomic, assign) CGPoint      vjoyCurrent;
@property(nonatomic, assign) SDL_Gamepad* vjoyGamepad;
@property(nonatomic, weak) UITouch*       vjoyInputSource;

@end

typedef enum {
	GamePadButtonStyleRoundedRectangle,
	GamePadButtonStyleCircle,
} GamePadButtonStyle;

@class GamePadButton;

@protocol GamePadButtonDelegate
- (void)buttonDown:(GamePadButton*)btn;
- (void)buttonUp:(GamePadButton*)btn;
@end

@interface GamePadButton : UIView

@property(nonatomic, assign, unsafe_unretained) id<GamePadButtonDelegate>
												delegate;
@property(nonatomic, retain) UIColor*           textColor;
@property(nonatomic, retain) NSArray*           images;
@property(nonatomic, retain) NSString*          title;
@property(nonatomic, assign) BOOL               pressed;
@property(nonatomic, retain) NSArray*           keyCodes;
@property(nonatomic, assign) GamePadButtonStyle style;
@end
