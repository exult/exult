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
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "GamePadView.h"
#include "Configuration.h"

#include <cassert>

static char docs_dir[512];
extern "C" int SDL_SendKeyboardKey(Uint64 timestamp, Uint8 state, SDL_Scancode scancode);

@interface UIManager : NSObject<KeyInputDelegate, GamePadButtonDelegate>
{
	TouchUI_iOS *touchUI;
	DPadView *dpad;
	GamePadButton *btn1;
	GamePadButton *btn2;
	SDL_Scancode recurringKeycode;
};

- (void)promptForName:(NSString*)name;

@end

@implementation UIManager

- (void)sendRecurringKeycode
{
	SDL_SendKeyboardKey(0, SDL_PRESSED, recurringKeycode);
	[self performSelector:@selector(sendRecurringKeycode) withObject:nil afterDelay:.5];
}

- (void)keydown:(SDL_Scancode)keycode
{
	SDL_SendKeyboardKey(0, SDL_PRESSED, keycode);
	recurringKeycode = keycode;
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendRecurringKeycode) object:nil];
	[self performSelector:@selector(sendRecurringKeycode) withObject:nil afterDelay:.5];
}

- (void)keyup:(SDL_Scancode)keycode
{
	[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendRecurringKeycode) object:nil];
	SDL_SendKeyboardKey(0, SDL_RELEASED, keycode);
}

- (void)buttonDown:(GamePadButton*)btn
{
	SDL_SendKeyboardKey(0, SDL_PRESSED, (SDL_Scancode) [btn.keyCodes[0] integerValue] );
}

- (void)buttonUp:(GamePadButton*)btn
{
	SDL_SendKeyboardKey(0, SDL_RELEASED, (SDL_Scancode) [btn.keyCodes[0] integerValue] );
}

- (void)promptForName:(NSString*)name
{
	UIWindow *alertWindow = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
	alertWindow.windowLevel = UIWindowLevelAlert;
	alertWindow.rootViewController = [UIViewController new];
	[alertWindow makeKeyAndVisible];

	UIAlertController *alert = [UIAlertController
										alertControllerWithTitle:@""
										message:@""
										preferredStyle:UIAlertControllerStyleAlert
	];
	UIAlertAction* ok = [UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault
										handler:^(UIAlertAction *action) {
											UITextField *textField = alert.textFields.firstObject;
											TouchUI::onTextInput(textField.text.UTF8String);
											alertWindow.hidden = YES;
											[alert dismissViewControllerAnimated:YES completion:nil];
	}];
	UIAlertAction* cancel = [UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleDefault
										handler:^(UIAlertAction * action) {
											alertWindow.hidden = YES;
											[alert dismissViewControllerAnimated:YES completion:nil];
	}];
	[alert addAction:ok];
	[alert addAction:cancel];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		textField.placeholder = @"";
		if (name)
			[textField setText:name];
	}];

	[alertWindow.rootViewController presentViewController:alert animated:YES completion:nil];
}

- (CGRect)calcRectForDPad
{
	UIWindow *window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController *controller = window.rootViewController;
	CGRect rcScreen = controller.view.bounds;
	CGSize sizeDpad = CGSizeMake(100, 100);
	float margin = 100;

	std::string str;
	float left = rcScreen.size.width - sizeDpad.width - margin;
	config->value("config/touch/dpad_location", str, "right");
	if (str == "no") {
		return CGRectZero;
	} else if (str == "left") {
		left = 0;
	}
	CGRect rcDpad = CGRectMake(
		left,
		rcScreen.size.height - sizeDpad.height - margin,
		sizeDpad.width,
		sizeDpad.height);
	return rcDpad;
}

- (void)onDpadLocationChanged
{
	dpad.frame = [self calcRectForDPad];
}


- (GamePadButton*)createButton:(NSString*)title keycode:(int)keycode rect:(CGRect)rect
{
	GamePadButton *btn = [[GamePadButton alloc] initWithFrame:rect];
	btn.keyCodes = @[@(keycode)];
	btn.images = @[
		[UIImage imageNamed:@"btn.png"],
		[UIImage imageNamed:@"btnpressed.png"],
	];
	btn.textColor = [UIColor whiteColor];
	btn.title = title;
	btn.delegate = self;
	return btn;
}

- (void)showGameControls
{
	if (dpad == nil) {
		dpad = [[DPadView alloc] initWithFrame:CGRectZero];
	}
	UIWindow *window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController *controller = window.rootViewController;

	dpad.frame = [self calcRectForDPad];
	[controller.view addSubview:dpad];

	dpad.alpha = 1;
}

- (void)hideGameControls
{
	dpad.alpha = 0;
}

- (void)showButtonControls
{
	if (btn1 == nil) {
		btn1 = [self createButton:@"ESC" keycode:(int)SDL_SCANCODE_ESCAPE rect:CGRectZero];
	}

	UIWindow *window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController *controller = window.rootViewController;

	CGRect rcScreen = controller.view.bounds;
	CGSize sizeButton = CGSizeMake(60,30);
	CGRect rcButton = CGRectMake(10, rcScreen.size.height-sizeButton.height, sizeButton.width, sizeButton.height);
	btn1.frame = rcButton;
	[controller.view addSubview:btn1];

	btn1.alpha = 1;
}

- (void)hideButtonControls
{
	btn1.alpha = 0;
}
@end


static UIManager *_defaultManager;

/* ---------------------------------------------------------------------- */
#pragma mark TouchUI iOS

TouchUI_iOS::TouchUI_iOS() : TouchUI()
{
	if (_defaultManager == nil) {
		_defaultManager = [[UIManager alloc] init];
	}
}

void TouchUI_iOS::promptForName(const char *name)
{
	if (name == NULL) {
		[_defaultManager promptForName:nil];
	} else {
		[_defaultManager promptForName:[NSString stringWithUTF8String:name]];
	}
}

void TouchUI_iOS::showGameControls()
{
	[_defaultManager showGameControls];
}

void TouchUI_iOS::hideGameControls()
{
	[_defaultManager hideGameControls];
}

void TouchUI_iOS::showButtonControls()
{
	[_defaultManager showButtonControls];
}

void TouchUI_iOS::hideButtonControls()
{
	[_defaultManager hideButtonControls];
}

void TouchUI_iOS::onDpadLocationChanged()
{
	[_defaultManager onDpadLocationChanged];
}

/* ---------------------------------------------------------------------- */

const char* ios_get_documents_dir()
{
	if (docs_dir[0] == 0) {
		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
		NSString *docDirectory = [paths objectAtIndex:0];
		strcpy(docs_dir, docDirectory.UTF8String);
		printf("Documents: %s\n", docs_dir);
	//	chdir(docs_dir);
//		*strncpy(docs_dir, , sizeof(docs_dir)-1) = 0;
	}
	return docs_dir;
}
