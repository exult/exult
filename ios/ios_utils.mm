/*
 * Copyright (C) 2015 Chaoji Li
 * Copyright (C) 2020-2022 The Exult Team
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

#include "ios_utils.h"

#include "Configuration.h"
#import "GamePadView.h"
#include "ignore_unused_variable_warning.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <cassert>

namespace {
	char gDocsDir[512];
}

// NOLINTNEXTLINE(google-objc-function-naming)
// extern "C" int SDL_SendKeyboardKey(
//		Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode keycode, Uint8 state);

// This should be equivalent to :
//   SDL_Event event;
//   event.type = SDL_EVENT_KEY_DOWN ( state == SDL_PRESSED ) or
//                SDL_EVENT_KEY_UP   ( state == SDL_RELEASED );
//   event.common.timestamp = timestamp; // 0 in here
//   event.key.scancode = aSDL_Scancode;
//   event.key.key      = SDL_GetKeyFromScancode(aSDL_Scancode, SDL_KMOD_NONE);
//   event.key.mod      = 0;
//   event.key.raw      = rawcode;       // 0 in here
//   event.key.state    = state;
//   event.key.repeat   = SDL_FALSE;
//   event.key.windowID = 0;             // It should be keyboard->focus ? keyboard->focus->id : 0;
//   event.key.which    = keyboardID;    // 0 in here
//   SDL_PushEvent(&event);

@interface UIManager : NSObject <KeyInputDelegate, GamePadButtonDelegate>

@property(nonatomic, assign) TouchUI_iOS*   touchUI;
@property(nonatomic, retain) DPadView*      dpad;
@property(nonatomic, retain) GamePadButton* btn1;
@property(nonatomic, retain) GamePadButton* btn2;
@property(nonatomic, assign) SDL_Scancode   recurringKeycode;

- (void)promptForName:(NSString*)name;

@end

@implementation UIManager
@synthesize     touchUI;
@synthesize     dpad;
@synthesize     btn1;
@synthesize     btn2;
@synthesize     recurringKeycode;

- (void)sendRecurringKeycode {
//	SDL_SendKeyboardKey(0, 0, 0, self.recurringKeycode, SDL_PRESSED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_DOWN;
	event.key.scancode = self.recurringKeycode;
	event.key.key      = SDL_GetKeyFromScancode(self.recurringKeycode, SDL_KMOD_NONE);
	event.key.state    = SDL_PRESSED;
	event.key.repeat   = SDL_FALSE;
	event.key.windowID = 0; // It should be keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
	[self performSelector:@selector(sendRecurringKeycode)
			   withObject:nil
			   afterDelay:.5];
}

- (void)keydown:(SDL_Scancode)keycode {
//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_PRESSED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_DOWN;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE);
	event.key.state    = SDL_PRESSED;
	event.key.repeat   = SDL_FALSE;
	event.key.windowID = 0; // It should be keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
	self.recurringKeycode = keycode;
	[NSObject cancelPreviousPerformRequestsWithTarget:self
											 selector:@selector
											 (sendRecurringKeycode)
											   object:nil];
	[self performSelector:@selector(sendRecurringKeycode)
			   withObject:nil
			   afterDelay:.5];
}

- (void)keyup:(SDL_Scancode)keycode {
	[NSObject cancelPreviousPerformRequestsWithTarget:self
											 selector:@selector
											 (sendRecurringKeycode)
											   object:nil];
//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_RELEASED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_UP;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE);
	event.key.state    = SDL_RELEASED;
	event.key.repeat   = SDL_FALSE;
	event.key.windowID = 0; // It should be keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
}

- (void)buttonDown:(GamePadButton*)btn {
	NSNumber*  code     = btn.keyCodes[0];
	const auto keycode = static_cast<SDL_Scancode>([code integerValue]);
//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_PRESSED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_DOWN;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE);
	event.key.state    = SDL_PRESSED;
	event.key.repeat   = SDL_FALSE;
	event.key.windowID = 0; // It should be keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
}

- (void)buttonUp:(GamePadButton*)btn {
	NSNumber*  code     = btn.keyCodes[0];
	const auto keycode = static_cast<SDL_Scancode>([code integerValue]);
//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_RELEASED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_UP;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE);
	event.key.state    = SDL_RELEASED;
	event.key.repeat   = SDL_FALSE;
	event.key.windowID = 0; // It should be keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
}

- (void)promptForName:(NSString*)name {
	UIWindow* alertWindow =
			[[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
	alertWindow.windowLevel        = UIWindowLevelAlert;
	alertWindow.rootViewController = [[UIViewController alloc] init];
	[alertWindow makeKeyAndVisible];

	UIAlertController* alert = [UIAlertController
			alertControllerWithTitle:@""
							 message:@""
					  preferredStyle:UIAlertControllerStyleAlert];

	UIAlertAction* ok = [UIAlertAction
			actionWithTitle:@"OK"
					  style:UIAlertActionStyleDefault
					handler:^(UIAlertAction* action) {
					  ignore_unused_variable_warning(action);
					  UITextField* textField = alert.textFields.firstObject;
					  TouchUI::onTextInput(textField.text.UTF8String);
					  alertWindow.hidden = YES;
					  [alert dismissViewControllerAnimated:YES completion:nil];
					}];

	UIAlertAction* cancel = [UIAlertAction
			actionWithTitle:@"Cancel"
					  style:UIAlertActionStyleDefault
					handler:^(UIAlertAction* action) {
					  ignore_unused_variable_warning(action);
					  alertWindow.hidden = YES;
					  [alert dismissViewControllerAnimated:YES completion:nil];
					}];
	[alert addAction:ok];
	[alert addAction:cancel];
	[alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
	  textField.placeholder = @"";
	  if (name != nullptr) {
		  [textField setText:name];
	  }
	}];

	[alertWindow.rootViewController presentViewController:alert
												 animated:YES
											   completion:nil];
}

- (CGRect)calcRectForDPad {
	UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController* controller = window.rootViewController;
	CGRect            rcScreen   = controller.view.bounds;
	CGSize            sizeDpad   = CGSizeMake(100, 100);
	double            margin     = 100;

	std::string str;
	double      left = rcScreen.size.width - sizeDpad.width - margin;
	config->value("config/touch/dpad_location", str, "right");
	if (str == "no") {
		return CGRectZero;
	}
	if (str == "left") {
		left = 0;
	}
	CGRect rcDpad = CGRectMake(
			left, rcScreen.size.height - sizeDpad.height - margin,
			sizeDpad.width, sizeDpad.height);
	return rcDpad;
}

- (void)onDpadLocationChanged {
	self.dpad.frame = [self calcRectForDPad];
}

- (GamePadButton*)createButton:(NSString*)title
					   keycode:(int)keycode
						  rect:(CGRect)rect {
	GamePadButton* btn = [[GamePadButton alloc] initWithFrame:rect];
	btn.keyCodes       = @[@(keycode)];

	auto* button        = [UIImage imageNamed:@"btn.png"];
	auto* buttonPressed = [UIImage imageNamed:@"btnpressed.png"];
	NSAssert(button != nil, @"Failed to load image 'btn.png'");
	NSAssert(buttonPressed != nil, @"Failed to load image 'btnpressed.png'");

	btn.images = @[
		button,
		buttonPressed,
	];
	btn.textColor = [UIColor whiteColor];
	btn.title     = title;
	btn.delegate  = self;
	return btn;
}

- (void)showGameControls {
	if (self.dpad == nil) {
		self.dpad = [[DPadView alloc] initWithFrame:CGRectZero];
	}
	UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController* controller = window.rootViewController;

	self.dpad.frame = [self calcRectForDPad];
	[controller.view addSubview:self.dpad];

	self.dpad.alpha = 1;
}

- (void)hideGameControls {
	self.dpad.alpha = 0;
}

- (void)showButtonControls {
	if (self.btn1 == nil) {
		self.btn1 = [self createButton:@"ESC"
							   keycode:static_cast<int>(SDL_SCANCODE_ESCAPE)
								  rect:CGRectZero];
	}

	UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController* controller = window.rootViewController;

	CGRect rcScreen   = controller.view.bounds;
	CGSize sizeButton = CGSizeMake(60, 30);

	CGRect rcButton = CGRectMake(
			10, rcScreen.size.height - sizeButton.height, sizeButton.width,
			sizeButton.height);
	self.btn1.frame = rcButton;
	[controller.view addSubview:self.btn1];

	self.btn1.alpha = 1;
}

- (void)hideButtonControls {
	self.btn1.alpha = 0;
}

@end

namespace {
	UIManager* gDefaultManager;
}

/* ---------------------------------------------------------------------- */
#pragma mark TouchUI iOS

TouchUI_iOS::TouchUI_iOS() {
	if (gDefaultManager == nil) {
		gDefaultManager = [[UIManager alloc] init];
	}
}

void TouchUI_iOS::promptForName(const char* name) {
	if (name == nullptr) {
		[gDefaultManager promptForName:nil];
	} else {
		[gDefaultManager promptForName:[NSString stringWithUTF8String:name]];
	}
}

void TouchUI_iOS::showGameControls() {
	[gDefaultManager showGameControls];
}

void TouchUI_iOS::hideGameControls() {
	[gDefaultManager hideGameControls];
}

void TouchUI_iOS::showButtonControls() {
	[gDefaultManager showButtonControls];
}

void TouchUI_iOS::hideButtonControls() {
	[gDefaultManager hideButtonControls];
}

void TouchUI_iOS::onDpadLocationChanged() {
	[gDefaultManager onDpadLocationChanged];
}

/* ---------------------------------------------------------------------- */

const char* IOSGetDocumentsDir() {
	if (gDocsDir[0] == 0) {
		NSArray* paths = NSSearchPathForDirectoriesInDomains(
				NSDocumentDirectory, NSUserDomainMask, YES);
		NSString* docDirectory = [paths objectAtIndex:0];
		strcpy(gDocsDir, docDirectory.UTF8String);
		printf("Documents: %s\n", gDocsDir);
		// chdir(gDocsDir);
		// *strncpy(gDocsDir, , sizeof(gDocsDir) - 1) = 0;
	}
	return gDocsDir;
}
