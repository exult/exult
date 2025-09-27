/*
 * Copyright (C) 2015 Chaoji Li
 * Copyright (C) 2020-2025 The Exult Team
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
	char      gDocsDir[512];
	NSString* gLaunchGameFlag = nil;    // Store launch flag globally
}    // namespace

// NOLINTNEXTLINE(google-objc-function-naming)
// extern "C" int SDL_SendKeyboardKey(
//		Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode,
//		SDL_Scancode keycode, Uint8 state);

// This should be equivalent to :
//   SDL_Event event;
//   event.type = SDL_EVENT_KEY_DOWN ( state == SDL_PRESSED ) or
//                SDL_EVENT_KEY_UP   ( state == SDL_RELEASED );
//   event.common.timestamp = timestamp; // 0 in here
//   event.key.scancode = aSDL_Scancode;
//   event.key.key      = SDL_GetKeyFromScancode(
//           aSDL_Scancode, SDL_KMOD_NONE, false);
//   event.key.mod      = 0;
//   event.key.raw      = rawcode;       // 0 in here
//   event.key.down     = true  ( state == SDL_PRESSED ) or
//                        false ( state == SDL_RELEASED );
//   event.key.repeat   = false;
//   event.key.windowID = 0; // keyboard->focus ? keyboard->focus->id : 0;
//   event.key.which    = keyboardID;    // 0 in here
//   SDL_PushEvent(&event);

@interface UIManager : NSObject <KeyInputDelegate, GamePadButtonDelegate>

@property(nonatomic, assign) TouchUI_iOS*   touchUI;
@property(nonatomic, retain) DPadView*      dpad;
@property(nonatomic, retain) GamePadButton* btn1;
@property(nonatomic, retain) GamePadButton* btn2;
@property(nonatomic, assign) SDL_Scancode   recurringKeycode;

- (void)promptForName:(NSString*)name;
- (void)showPauseControls;
- (void)hidePauseControls;

@end

// Forward declare the interface we'll need
@interface ExultQuickActionManager : NSObject
+ (instancetype)sharedManager;
- (void)setupQuickActions;
- (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem;
- (BOOL)isGameInstalled:(NSString*)gameType;
@end

@implementation ExultQuickActionManager

+ (instancetype)sharedManager {
	static ExultQuickActionManager* instance = nil;
	static dispatch_once_t          onceToken;
	dispatch_once(&onceToken, ^{
	  instance = [[ExultQuickActionManager alloc] init];
	});
	return instance;
}

- (void)setupQuickActions {
	NSString* documentsPath
			= IOSGetDocumentsDir()
					  ? [NSString stringWithUTF8String:IOSGetDocumentsDir()]
					  : nil;
	if (!documentsPath) {
		return;
	}

	NSMutableArray* shortcuts = [NSMutableArray array];

	if ([self isGameInstalled:@"bg"]) {
		UIApplicationShortcutItem* bgShortcut = [[UIApplicationShortcutItem
				alloc]
					 initWithType:@"launch_bg"
				   localizedTitle:@"Black Gate"
				localizedSubtitle:@"Launch Ultima VII"
							 icon:[UIApplicationShortcutIcon
										  iconWithType:
												  UIApplicationShortcutIconTypePlay]
						 userInfo:nil];
		[shortcuts addObject:bgShortcut];
	}

	if ([self isGameInstalled:@"si"]) {
		UIApplicationShortcutItem* siShortcut = [[UIApplicationShortcutItem
				alloc]
					 initWithType:@"launch_si"
				   localizedTitle:@"Serpent Isle"
				localizedSubtitle:@"Launch Ultima VII Part 2"
							 icon:[UIApplicationShortcutIcon
										  iconWithType:
												  UIApplicationShortcutIconTypePlay]
						 userInfo:nil];
		[shortcuts addObject:siShortcut];
	}

	if ([self isGameInstalled:@"fov"]) {
		UIApplicationShortcutItem* fovShortcut = [[UIApplicationShortcutItem
				alloc]
					 initWithType:@"launch_fov"
				   localizedTitle:@"Forge of Virtue"
				localizedSubtitle:@"Launch BG with add-on"
							 icon:[UIApplicationShortcutIcon
										  iconWithType:
												  UIApplicationShortcutIconTypePlay]
						 userInfo:nil];
		[shortcuts addObject:fovShortcut];
	}

	if ([self isGameInstalled:@"ss"]) {
		UIApplicationShortcutItem* ssShortcut = [[UIApplicationShortcutItem
				alloc]
					 initWithType:@"launch_ss"
				   localizedTitle:@"Silver Seed"
				localizedSubtitle:@"Launch SI with add-on"
							 icon:[UIApplicationShortcutIcon
										  iconWithType:
												  UIApplicationShortcutIconTypePlay]
						 userInfo:nil];
		[shortcuts addObject:ssShortcut];
	}

	if ([self isGameInstalled:@"sib"]) {
		UIApplicationShortcutItem* sibShortcut = [[UIApplicationShortcutItem
				alloc]
					 initWithType:@"launch_sib"
				   localizedTitle:@"Serpent Isle Beta"
				localizedSubtitle:@"Launch SI Beta"
							 icon:[UIApplicationShortcutIcon
										  iconWithType:
												  UIApplicationShortcutIconTypePlay]
						 userInfo:nil];
		[shortcuts addObject:sibShortcut];
	}

	[UIApplication sharedApplication].shortcutItems = shortcuts;
}

- (BOOL)handleShortcutItem:(UIApplicationShortcutItem*)shortcutItem {
	NSString* type     = shortcutItem.type;
	NSString* gameFlag = nil;

	if ([type isEqualToString:@"launch_bg"]) {
		gameFlag = @"--bg";
	} else if ([type isEqualToString:@"launch_si"]) {
		gameFlag = @"--si";
	} else if ([type isEqualToString:@"launch_fov"]) {
		gameFlag = @"--fov";
	} else if ([type isEqualToString:@"launch_ss"]) {
		gameFlag = @"--ss";
	} else if ([type isEqualToString:@"launch_sib"]) {
		gameFlag = @"--sib";
	}

	if (gameFlag) {
		NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
		[defaults setObject:gameFlag forKey:@"ExultLaunchGameFlag"];
		[defaults synchronize];

		gLaunchGameFlag = [gameFlag copy];
		return YES;
	}

	return NO;
}

- (BOOL)isGameInstalled:(NSString*)gameType {
	NSString* documentsPath
			= IOSGetDocumentsDir()
					  ? [NSString stringWithUTF8String:IOSGetDocumentsDir()]
					  : nil;
	if (!documentsPath) {
		return NO;
	}

	NSString* gamePath = [documentsPath stringByAppendingPathComponent:@"game"];

	// Helper block to check for both static and STATIC
	BOOL (^checkStaticFile)(NSString*) = ^BOOL(NSString* basePath) {
	  NSString* staticLower =
			  [basePath stringByAppendingPathComponent:@"static"];
	  NSString* staticUpper =
			  [basePath stringByAppendingPathComponent:@"STATIC"];

	  BOOL lowerExists =
			  [[NSFileManager defaultManager] fileExistsAtPath:staticLower];
	  BOOL upperExists =
			  [[NSFileManager defaultManager] fileExistsAtPath:staticUpper];

	  return lowerExists || upperExists;
	};

	if ([gameType isEqualToString:@"bg"]) {
		NSString* bgPath =
				[gamePath stringByAppendingPathComponent:@"blackgate"];
		return checkStaticFile(bgPath);
	} else if ([gameType isEqualToString:@"si"]) {
		NSString* siPath =
				[gamePath stringByAppendingPathComponent:@"serpentisle"];
		return checkStaticFile(siPath);
	} else if ([gameType isEqualToString:@"fov"]) {
		NSString* bgPath =
				[gamePath stringByAppendingPathComponent:@"blackgate"];
		NSString* fovPath =
				[gamePath stringByAppendingPathComponent:@"forgeofvirtue"];
		return checkStaticFile(bgPath) && checkStaticFile(fovPath);
	} else if ([gameType isEqualToString:@"ss"]) {
		NSString* siPath =
				[gamePath stringByAppendingPathComponent:@"serpentisle"];
		NSString* ssPath =
				[gamePath stringByAppendingPathComponent:@"silverseed"];
		return checkStaticFile(siPath) && checkStaticFile(ssPath);
	} else if ([gameType isEqualToString:@"sib"]) {
		NSString* sibPath =
				[gamePath stringByAppendingPathComponent:@"serpentbeta"];
		return checkStaticFile(sibPath);
	}

	return NO;
}

- (void)handleApplicationDidFinishLaunching:(NSNotification*)notification {
	// Check if this was a Quick Action launch
	NSDictionary* launchOptions = notification.userInfo;
	if (launchOptions) {
		UIApplicationShortcutItem* shortcutItem = [launchOptions
				objectForKey:UIApplicationLaunchOptionsShortcutItemKey];
		if (shortcutItem) {
			[self handleShortcutItem:shortcutItem];
		}
	}
}

- (void)dealloc {
#if !__has_feature(objc_arc)
	[super dealloc];
#endif
}

@end

@implementation UIManager
@synthesize touchUI;
@synthesize dpad;
@synthesize btn1;
@synthesize btn2;
@synthesize recurringKeycode;
// Toggle glyphs for the Pause button
static NSString* const kGlyphPause = @"\u23F8\uFE0F";    // ⏸
static NSString* const kGlyphPlay  = @"\u25B6\uFE0F";    // ▶

- (void)sendRecurringKeycode {
	//	SDL_SendKeyboardKey(0, 0, 0, self.recurringKeycode, SDL_PRESSED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_DOWN;
	event.key.scancode = self.recurringKeycode;
	event.key.key      = SDL_GetKeyFromScancode(
            self.recurringKeycode, SDL_KMOD_NONE, false);
	event.key.down     = true;
	event.key.repeat   = false;
	event.key.windowID = 0;    // keyboard->focus ? keyboard->focus->id : 0;
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
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE, false);
	event.key.down     = true;
	event.key.repeat   = false;
	event.key.windowID = 0;    // keyboard->focus ? keyboard->focus->id : 0;
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
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE, false);
	event.key.down     = false;
	event.key.repeat   = false;
	event.key.windowID = 0;    // keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
}

- (void)buttonDown:(GamePadButton*)btn {
	NSNumber*  code    = btn.keyCodes[0];
	const auto keycode = static_cast<SDL_Scancode>([code integerValue]);
	//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_PRESSED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_DOWN;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE, false);
	event.key.down     = true;
	event.key.repeat   = false;
	event.key.windowID = 0;    // keyboard->focus ? keyboard->focus->id : 0;
	SDL_PushEvent(&event);
}

- (void)buttonUp:(GamePadButton*)btn {
	NSNumber*  code    = btn.keyCodes[0];
	const auto keycode = static_cast<SDL_Scancode>([code integerValue]);
	if (btn == self.btn2) {
		NSString* cur = btn.title;
		btn.title =
				[cur isEqualToString:kGlyphPause] ? kGlyphPlay : kGlyphPause;
	}
	//	SDL_SendKeyboardKey(0, 0, 0, keycode, SDL_RELEASED);
	SDL_Event event;
	SDL_zero(event);
	event.type         = SDL_EVENT_KEY_UP;
	event.key.scancode = keycode;
	event.key.key      = SDL_GetKeyFromScancode(keycode, SDL_KMOD_NONE, false);
	event.key.down     = false;
	event.key.repeat   = false;
	event.key.windowID = 0;    // keyboard->focus ? keyboard->focus->id : 0;
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
	const CGFloat     margin     = 100;

	std::string str;
	double      left = rcScreen.size.width - sizeDpad.width - margin;
	config->value("config/touch/dpad_location", str, "right");
	if (str == "no") {
		return CGRectZero;
	}
	if (str == "left") {
		left = margin;
	}
	CGRect rcDpad = CGRectMake(
			left, rcScreen.size.height - sizeDpad.height - margin,
			sizeDpad.width, sizeDpad.height);
	return rcDpad;
}

- (void)onDpadLocationChanged {
	self.dpad.frame = [self calcRectForDPad];
	[self layoutButtonsForDpadLocation];
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

	btn.images    = @[button, buttonPressed];
	btn.textColor = [UIColor blackColor];
	btn.title     = title;
	btn.delegate  = self;
	return btn;
}

- (void)layoutButtonsForDpadLocation {
	UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController* controller = window.rootViewController;
	CGRect            rcScreen   = controller.view.bounds;
	CGSize            sizeButton = CGSizeMake(60, 40);
	const CGFloat     btnMargin  = 10.0;    // ESC margin from edge
	const CGFloat     gap        = 12.0;    // gap between ESC and Pause

	std::string dpadLoc;
	config->value("config/touch/dpad_location", dpadLoc, "right");

	// ESC goes opposite the D-Pad
	BOOL escOnLeft = YES;
	if (dpadLoc == "left") {
		escOnLeft = NO;    // D-Pad on left -> ESC on right
	} else if (dpadLoc == "right") {
		escOnLeft = YES;    // D-Pad on right -> ESC on left
	} else {                // "no" or unknown
		escOnLeft = YES;
	}

	CGRect rcEsc;
	if (escOnLeft) {
		rcEsc = CGRectMake(
				btnMargin, rcScreen.size.height - sizeButton.height - btnMargin,
				sizeButton.width, sizeButton.height);
	} else {
		rcEsc = CGRectMake(
				rcScreen.size.width - sizeButton.width - btnMargin,
				rcScreen.size.height - sizeButton.height - btnMargin,
				sizeButton.width, sizeButton.height);
	}

	if (self.btn1) {
		self.btn1.frame = rcEsc;
		// Ensure it is on-screen when visible
		if (self.btn1.alpha > 0.0 && self.btn1.superview == nil) {
			[controller.view addSubview:self.btn1];
		}
	}

	// If Pause is visible, keep it adjacent to ESC
	if (self.btn2 && self.btn2.alpha > 0.0) {
		CGRect rcPause;
		if (escOnLeft) {
			rcPause = CGRectMake(
					CGRectGetMaxX(rcEsc) + gap, rcEsc.origin.y,
					sizeButton.width, sizeButton.height);
		} else {
			rcPause = CGRectMake(
					CGRectGetMinX(rcEsc) - gap - sizeButton.width,
					rcEsc.origin.y, sizeButton.width, sizeButton.height);
		}
		self.btn2.frame = rcPause;
		if (self.btn2.superview == nil) {
			[controller.view addSubview:self.btn2];
		}
	}
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

	// Lay out based on current D-Pad location
	[self layoutButtonsForDpadLocation];

	// Ensure ESC is visible and on view
	if (self.btn1.superview == nil) {
		[controller.view addSubview:self.btn1];
	}
	self.btn1.alpha = 1;

	// If Pause is already visible, keep it placed next to ESC
	if (self.btn2 && self.btn2.alpha > 0.0 && self.btn2.superview == nil) {
		[controller.view addSubview:self.btn2];
	}
}

- (void)hideButtonControls {
	if (self.btn1) {
		self.btn1.alpha = 0;
	}
	if (self.btn2) {
		self.btn2.alpha = 0;
	}
}

- (void)showPauseControls {
	UIWindow* window = [[[UIApplication sharedApplication] delegate] window];
	UIViewController* controller = window.rootViewController;
	if (self.btn2 == nil) {
		self.btn2 = [self createButton:kGlyphPause
							   keycode:static_cast<int>(SDL_SCANCODE_SPACE)
								  rect:CGRectZero];
	}

	// Ensure ESC is laid out to anchor Pause next to it
	if (self.btn1 == nil || self.btn1.alpha == 0.0) {
		[self showButtonControls];
	}

	// Place next to ESC respecting D-Pad side
	[self layoutButtonsForDpadLocation];

	if (self.btn2.superview == nil) {
		[controller.view addSubview:self.btn2];
	}
	self.btn2.title = kGlyphPause;    // start with Pause glyph
	self.btn2.alpha = 1;
}

- (void)hidePauseControls {
	if (self.btn2) {
		self.btn2.alpha = 0;
	}
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

void TouchUI_iOS::showPauseControls() {
	[gDefaultManager showPauseControls];
}

void TouchUI_iOS::hidePauseControls() {
	[gDefaultManager hidePauseControls];
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

extern "C" {
void iOS_SetupQuickActions() {
	[[ExultQuickActionManager sharedManager] setupQuickActions];
}

const char* iOS_GetLaunchGameFlag() {
	if (gLaunchGameFlag) {
		return [gLaunchGameFlag UTF8String];
	}

	NSUserDefaults* defaults   = [NSUserDefaults standardUserDefaults];
	NSString*       storedFlag = [defaults objectForKey:@"ExultLaunchGameFlag"];
	if (storedFlag) {
		gLaunchGameFlag = [storedFlag copy];
		[defaults removeObjectForKey:@"ExultLaunchGameFlag"];
		[defaults synchronize];
		return [gLaunchGameFlag UTF8String];
	}

	return nullptr;
}

void iOS_ClearLaunchGameFlag() {
	gLaunchGameFlag          = nil;
	NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
	[defaults removeObjectForKey:@"ExultLaunchGameFlag"];
	[defaults synchronize];
}
}

@implementation NSObject (ExultQuickActionNotifications)

+ (void)load {
	// Register for app lifecycle notifications to handle Quick Actions
	[[NSNotificationCenter defaultCenter]
			addObserver:[ExultQuickActionManager sharedManager]
			   selector:@selector(handleApplicationDidFinishLaunching:)
				   name:UIApplicationDidFinishLaunchingNotification
				 object:nil];
}

@end
