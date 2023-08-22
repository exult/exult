/* 
* Copyright (C) 2010, 2011, 2015  Chaoji Li
* Copyright (C) 2016-2022 The Exult Team
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
#import "GamePadView.h"


#define CENTER_OF_RECT(r) CGPointMake(r.size.width/2,r.size.height/2)
#define DISTANCE_BETWEEN(a,b) sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y))

#define CHOP(x) \
if ((x) > 1.0) { \
	(x) = 1.0; \
} else if ((x) < -1.0) { \
	(x) = -1.0; \
} else {}

/** DPadView */
@implementation DPadView
@synthesize backgroundImage;
@synthesize images;

- (id)initWithFrame:(CGRect)frame {
	if ((self = [super initWithFrame:frame])) {
		self.backgroundColor = [UIColor clearColor];
		self.backgroundImage = [UIImage imageNamed:@"joythumb-glass.png"];
		int vjoy_index = SDL_AttachVirtualJoystick(
			SDL_JOYSTICK_TYPE_GAMEPAD,
			SDL_GAMEPAD_AXIS_MAX,
			SDL_GAMEPAD_BUTTON_MAX,
			0
		);
		if (vjoy_index < 0) {
			printf("SDL_AttachVirtualJoystick failed: %s\n", SDL_GetError());
		} else {
			vjoy_controller = SDL_OpenGamepad(vjoy_index);
			if (!vjoy_controller) {
				printf("SDL_OpenGamepad failed for virtual joystick: %s\n", SDL_GetError());
				SDL_DetachVirtualJoystick(vjoy_index);
			}
		}
		[self reset];
		// printf("VJOY INIT, controller=%p\n", vjoy_controller);
	}
	return self;
}

- (void)drawRect:(CGRect)rect
{
	if (backgroundImage) {
		[backgroundImage drawInRect:rect];
	}
}

- (void)dealloc {
	if (vjoy_controller) {
		const SDL_JoystickID vjoy_controller_id = SDL_GetJoystickInstanceID(
			SDL_GetGamepadJoystick(vjoy_controller)
		);
		SDL_CloseGamepad(vjoy_controller);
		for (int i = 0, n = SDL_NumJoysticks(); i < n; ++i) {
			const SDL_JoystickID current_id = SDL_JoystickGetDeviceInstanceID(i);
			if (current_id == vjoy_controller_id) {
				// printf("detach virtual at id:%d, index:%d\n", current_id, i);
				SDL_DetachVirtualJoystick(i);
				break;
			}
		}
	}

	[backgroundImage release];
	[images release];
	[super dealloc];
}

- (void)reset
{
	vjoy_is_active = false;
	SDL_SetJoystickVirtualAxis(
		SDL_GetGamepadJoystick(vjoy_controller),
		SDL_GAMEPAD_AXIS_LEFTX,
		0
	);
	SDL_SetJoystickVirtualAxis(
		SDL_GetGamepadJoystick(vjoy_controller),
		SDL_GAMEPAD_AXIS_LEFTY,
		0
	);
	vjoy_center = vjoy_current = CGPointMake(0,0);
	vjoy_input_source = nil;
	[self updateViewTransform];
}

- (void)updateViewTransform
{
	if (!vjoy_is_active) {
		self.transform = CGAffineTransformIdentity;
//        printf("updateViewTransform: reset to identity\n");
	} else {
		
		// Calculate the position of vjoy_center within this view's
		// parent/super-view.  This requires first resetting this view's
		// UIKit 'transform' to an untransformed state, as UIView's
		// method, 'convertPoint:toView:', will apply any existing
		// transform.
		self.transform = CGAffineTransformIdentity;
		CGPoint vjoy_center_in_parent_view = [self convertPoint:vjoy_center toView:self.superview];
		const CGPoint translation = CGPointMake(
			vjoy_center_in_parent_view.x - self.center.x,
			vjoy_center_in_parent_view.y - self.center.y
		);
		self.transform = CGAffineTransformMakeTranslation(translation.x, translation.y);
	}
	[self setNeedsDisplay];
}


- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	UITouch *touch = [touches anyObject];
	// printf("touchesBegan, %p\n", touch);

	if (!vjoy_is_active && touch != nil) {
		vjoy_input_source = touch;
		vjoy_center = vjoy_current = [touch locationInView:self];
		vjoy_is_active = true;
		SDL_SetJoystickVirtualAxis(
			SDL_GetGamepadJoystick(vjoy_controller),
			SDL_GAMEPAD_AXIS_LEFTX,
			0
		);
		SDL_SetJoystickVirtualAxis(
			SDL_GetGamepadJoystick(vjoy_controller),
			SDL_GAMEPAD_AXIS_LEFTY,
			0
		);
		[self updateViewTransform];
		// printf("VJOY START\n");
	}
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
//    printf("touchesMoved, vjoy_input_source:%p, touch:%p\n", vjoy_input_source, touch);

	UITouch * __strong current_input_source = vjoy_input_source;
	if (vjoy_is_active && [touches containsObject:current_input_source]) {

		// Calculate new vjoy_current, but first, reset this view's
		// UIKit-transform, lest the call to UITouch's 'locationInView:'
		// method reports incorrect values (due to a previously-applied
		// transform).
		self.transform = CGAffineTransformIdentity;
		vjoy_current = [current_input_source locationInView:self];

		float dx = vjoy_current.x - vjoy_center.x;
		float dy = vjoy_current.y - vjoy_center.y;

		// Move the vjoy's center if it's outside of its radius
		float dlength = sqrt((dx * dx) + (dy * dy));
		if (dlength > vjoy_radius) {
			vjoy_center.x = vjoy_current.x - (dx * (vjoy_radius / dlength));
			vjoy_center.y = vjoy_current.y - (dy * (vjoy_radius / dlength));
			dx = vjoy_current.x - vjoy_center.x;
			dy = vjoy_current.y - vjoy_center.y;
		}

		// Update vjoy state
		const Sint16 joy_axis_x_raw = (Sint16)((dx / vjoy_radius) * SDL_JOYSTICK_AXIS_MAX);
		SDL_SetJoystickVirtualAxis(
			SDL_GetGamepadJoystick(vjoy_controller),
			SDL_GAMEPAD_AXIS_LEFTX,
			joy_axis_x_raw
		);
		const Sint16 joy_axis_y_raw = (Sint16)((dy / vjoy_radius) * SDL_JOYSTICK_AXIS_MAX);
		SDL_SetJoystickVirtualAxis(
			SDL_GetGamepadJoystick(vjoy_controller),
			SDL_GAMEPAD_AXIS_LEFTY,
			joy_axis_y_raw
		);

		// Update visuals
		[self updateViewTransform];
		
		// printf("VJOY MOVE: %d, %d\n", (int)joy_axis_x_raw, (int)joy_axis_y_raw);
	}
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	// printf("touchesEnded, vjoy_input_source:%p, touch:%p\n", vjoy_input_source, touch);

	if ([touches containsObject:vjoy_input_source]) {
		// Mark vjoy as inactive
		[self reset];
	}
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
	CGPoint ptCenter = CENTER_OF_RECT(self.bounds);
	return DISTANCE_BETWEEN(point, ptCenter) < self.bounds.size.width/2;
}

@end

/** GamePadButton */
@implementation GamePadButton
@synthesize pressed;
@synthesize keyCodes;
@synthesize style;
@synthesize title;
@synthesize images;
@synthesize textColor;
@synthesize delegate;

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
	if (style == GamePadButtonStyleCircle) {
		CGPoint ptCenter = CENTER_OF_RECT(self.bounds);
		return DISTANCE_BETWEEN(point, ptCenter) < self.bounds.size.width/2;
	} else {
		return [super pointInside:point withEvent:event];
	}
}

- (void)setTitle:(NSString *)s
{
	if (title) [title release];
	title = s;
	[title retain];
	[self setNeedsDisplay];
}

- (void)setPressed:(BOOL)b
{
	if (pressed != b) {
		pressed = b;
		if (delegate) {
			if (b)
				[delegate buttonDown:self];
			else
				[delegate buttonUp:self];
		}
		[self setNeedsDisplay];
	}
}

- (void)setStyle:(GamePadButtonStyle)s
{
	style = s;
	[self setNeedsDisplay];
}

- (id)initWithFrame:(CGRect)frame {
	if ((self = [super initWithFrame:frame])) {
		self.backgroundColor = [UIColor clearColor];
		self.textColor       = [UIColor colorWithRed:0 green:0 blue:0 alpha:1];
		keyCodes = [[NSMutableArray alloc] initWithCapacity:2];
	}
	return self;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	self.pressed = YES;
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	self.pressed = NO;
//	[[self superview] touchesEnded:touches withEvent:event];
}

-(void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
//	[[self superview] touchesMoved:touches withEvent:event];
}

- (void)drawRect:(CGRect)rect
{
	int bkgIndex;
	UIColor *color = nil;

	if (!pressed) {
		color = [textColor colorWithAlphaComponent:0.3];
		bkgIndex = 0;
	} else {
		color = [textColor colorWithAlphaComponent:0.8];
		bkgIndex = 1;
	}

	if ([images count] > bkgIndex) {
		UIImage *image = [images objectAtIndex:bkgIndex];
		[image drawInRect:rect];
	}

	if (title) {
		float fontSize = MIN(14, rect.size.height/4);
		UIFont *fnt = [UIFont systemFontOfSize:fontSize];
		CGSize size = [title sizeWithAttributes:@{NSFontAttributeName:fnt}];
		CGRect rc = CGRectMake((rect.size.width-size.width)/2,
					(rect.size.height-size.height)/2,
					size.width, size.height);
		[color setFill];
		[title drawInRect:rc withAttributes:@{NSFontAttributeName:fnt, 
											  NSForegroundColorAttributeName: color}];
	}
}

- (void)dealloc {
	[images release];
	[title release];
	[textColor release];
	[keyCodes release];
	[super dealloc];
}

@end
/** End */
