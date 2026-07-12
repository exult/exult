/*
 *  Copyright (C) 2000-2026  The Exult Team
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

#ifndef CHEAT_SCREEN_H
#define CHEAT_SCREEN_H

#include "imagebuf.h"
#include "palette.h"
#include "rect.h"
#include "singles.h"
#include "span.h"

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wold-style-cast"
#	pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#	if !defined(__llvm__) && !defined(__clang__)
#		pragma GCC diagnostic ignored "-Wuseless-cast"
#	endif
#endif    // __GNUC__
#include <SDL3/SDL.h>
#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif    // __GNUC__

#include <any>
#include <array>
#include <climits>
#include <forward_list>
#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

class Game_window;
class Image_buffer8;
class Font;
class Game_clock;
class Game_object;
class Actor;
class Scene_layer;

// #define CHEAT_SCREEN_TEST_MOBILE 1

class CheatScreen : protected Game_singletons {
	static CheatScreen* cscreen;
	Actor*              grabbed = nullptr;

	std::vector<std::string> global_flag_names;
	bool                     global_flag_names_loaded = false;
	void                     load_global_flag_names();

public:
	void show_screen();

	void SetGrabbedActor(Actor* g) {
		grabbed = g;
	}

	void ClearThisGrabbedActor(Actor* g) const {
		if (g == grabbed) {
			g = nullptr;
		}
	}

	void clear_buttons() {
		buttons_down.clear();
	}

	// When thrown Causes RunMenu to show the message if there is one and optionally leave the current MenuCommand returning to the
	// last Menu
	struct MenuCommandException {
		std::string msg     = "";      // Message to display
		bool return_to_menu = true;    // if true leave the current MenuCommand returning menu, if false, retry current input phase
	};

private:
	// Get a 1 to 3 char name for a keycode
	static const char* getKeyName(SDL_Keycode keycode);

	// Hotspot is used to associate Label to a Keycodes and are used to determine what key press to simulate in response to
	// mouse clicks and touch events. Position a hotspot using the coordinates passed to the constructor or by setting x and y
	// fields or by using the PositionLeftOf() method. A hotspot may have upto 2 keycodes associated with it. If both keycodes are 0
	// Only the label will be shown
	class Hotspot {
		SDL_Keycode keycode[2] = {0};
		int         namew[2]   = {0};    // Width of the of each keycode's Name in chars
		bool        hide[2]    = {false, false};

	public:
		const std::string label = {};    // This is drawn to the right of above
		bool label_only = false;      // Don't hit check keycodes. Only draw the label. When this is set the label is always painted
		std::string label_rw = {};    // if this is set it is used instead of label
		int         x, y;

		const std::string& get_label() const {
			return label_rw.empty() ? label : label_rw;
		}

		// Paint the Hotspot
		void Paint(SDL_Keycode hilighted, int hoverx, int hovery) const;

		Hotspot(int x, int y, SDL_Keycode keycode, std::string&& label, SDL_Keycode keycode2 = SDLK_UNKNOWN);

		void setKeycode(SDL_Keycode code, unsigned index) {
			if (index >= std::size(keycode)) {
				return;
			}
			keycode[index] = code;
			namew[index]   = GetKeyNameWidth(code);
		}

		// Used to dynamically hide or show a Hotspot's keycodes. When a hidden a keycode will be ignored when checking input
		// If all keycodes are hidden the label will also be hidden
		// This has no effect if label_only is set
		void setHidden(unsigned index, bool hidden) {
			if (index >= std::size(keycode)) {
				return;
			}
			hide[index] = hidden;
		}

		static int GetKeyNameWidth(SDL_Keycode keycode) {
			switch (keycode) {
				// Arrows are drawn 1 char wide
			case SDLK_UP:
			case SDLK_DOWN:
			case SDLK_LEFT:
			case SDLK_RIGHT:
				return 1;
				break;
			default:
				break;
			}
			return strlen(getKeyName(keycode));
		}

		TileRect GetRect(unsigned key, bool inclabel) const {
			if (key >= std::size(keycode) || !keycode[key] || hide[key]) {
				return {};
			}
			TileRect r(x, y, 0, 8);
			if (key == 0) {
				r.w = 8 + 8 * namew[0] + (namew[1] ? 0 : 8);
			} else {
				r.x += 8 + 8 * namew[0];
				r.w = 8 + 8 * namew[1];
			}

			// Include label only if only 1 keycode
			if (inclabel && !keycode[1 - key]) {
				r.w += 8 * get_label().size();
			}

			return r;
		}

		// Get the number of Non Zero non hidden keycodes
		unsigned GetNumkeycodes() const {
			unsigned num = 0;
			for (size_t i = 0; i < std::size(keycode); i++) {
				if (keycode[i] && !hide[i]) {
					num++;
				}
			}
			return num;
		}

		// Get the total width of the hotspot on screen
		// Includes brackets and label
		int getWidth() {
			if (label_only) {
				return get_label().size() * 8;
			}

			auto rect0 = GetRect(0, true);
			auto rect1 = GetRect(1, true);

			if (!rect0) {
				return rect1.w;
			} else if (!rect1) {
				return rect0.w;
			}

			if (rect1.intersects(rect0)) {
				return rect1.add(rect0).w;
			}

			return std::max(rect0.w, rect1.w);
		}

		// Position this hotspot to the left of another
		// Or from the right edge of screen if hotspot argument is null
		// Copies y of hotspot, if not null
		void PositionLeftOf(const Hotspot* hotspot = nullptr);

		// Hit check a vector of HotSpots within (distance) of point (mx,my)
		// Returns the Keycode at the point
		// Uses TileRect::distance for calculating distance
		static SDL_Keycode HitCheck(const std::vector<Hotspot*>& hotspots, int mx, int my, int distance = 4);

		// Check if this HotSpot has a keycode matching code. Mask is a bit mask used to select which keycode to check
		bool IsKeycode(SDL_Keycode code, unsigned mask = 0xFFFFFFFF) const {
			if (!label_only && code) {
				code = FixUppercaseKeycode(code);
				for (unsigned i = 0; i < std::size(keycode); i++) {
					if (!hide[i] && (mask & 1 << i) && FixUppercaseKeycode(keycode[i]) == code) {
						return true;
					}
				}
			}
			return false;
		}

		// if the code is an uppercase letter make it lowercase
		// so it is a valid SDL_Keycode
		static SDL_Keycode FixUppercaseKeycode(SDL_Keycode code) {
			return code >= 'A' && code <= 'Z' ? std::tolower(code) : code;
		}
	};

	// The Base InputHandler class
	// This doesn't do much by itself beyond defining the InputHandler interface
	class InputHandler {
	protected:
		// Actual text input
		char input[31] = {0};    // Input size of 31 is the limit of how much can fit on screen at 320 pixel screen width with the
								 // prompt from the english exultmsg
		size_t curlen = 0;       // the current length of the input string. This should must match strlen(input) and be less than
								 // std::size(input)
		bool empty_allowed = false;    // if false it is an error if the user doesn't input anything and presses enter
		bool was_empty     = false;    // set if empty_allowed and there was no input

		std::string promptmsg;    // The prompt message to be shown at the bottom of the screen below the input line

	public:
		virtual ~InputHandler() {}

		// Hotspots are automatically placed to the right of the Prompt Text
		// Generally there would only be room for 2 hotspots on screen.
		std::vector<Hotspot> hotspots;

		// Change the prompt message dynamically
		void SetPromptMessage(std::string&& promptmsg) {
			this->promptmsg = std::move(promptmsg);
		}

		virtual void GetPromptMessage(char* buf, size_t buf_size) {
			snprintf(buf, buf_size, "%s", promptmsg.c_str());
		}

		// Handle Keypress. Returns true if input handler wants no more input
		virtual bool OnInput(SDL_Keycode key_sym) = 0;
		// Parse the input, throws InputException on error. This is called immediately after OnInput returns true
		virtual void Parse();

		// Arranges the hotspots that are at coord {0,0}
		// Adding additonal hotspots that need auto arranging after this method has been called would not be a good idea
		// This method is called by RunMenu every frame after PaintPrompt is called
		virtual void Arrange00Hotspots(int x, int y, unsigned lines = 1);
		// Paint The Prompt and Input, return pixel width drawn
		virtual int PaintPrompt(int x, int y, SDL_Keycode lastkey);

		// Returns if input buffer is completely filled
		virtual bool input_full() {
			// 1 char is always left for nul terminating
			return curlen >= std::size(input) - 1;
		}

		// Get raw input buffer and its size
		const char* getInputRAW(size_t& len) {
			len = curlen;
			return input;
		}

		// Get input as null terminated string
		const char* getInput() {
			// Null terminate it just to be sure
			input[curlen] = 0;
			return input;
		}

		// Clear input and reset the Input Handler back to inital State
		virtual void clear() {
			curlen = 0;
			memset(input, 0, sizeof(input));
			was_empty = false;
		}

		virtual void SetInput(const char* new_input) {
			curlen = std::min<size_t>(strlen(new_input), std::size(input) - 1);
			memcpy(input, new_input, curlen);
			input[curlen] = 0;
		}

		InputHandler(bool empty_allowed, std::string&& promptmsg) : empty_allowed(empty_allowed), promptmsg(std::move(promptmsg)) {}

		// Construct with an initial vector of hotspots
		InputHandler(bool empty_allowed, std::string&& promptmsg, std::vector<Hotspot>&& hotspots)
				: empty_allowed(empty_allowed), promptmsg(std::move(promptmsg)), hotspots(std::move(hotspots)) {}

		// Add pointers to the hotspots to the vector
		void GatherHotspots(std::vector<Hotspot*>& gathered) {
			gathered.reserve(gathered.size() + hotspots.size());
			for (auto& hs : hotspots) {
				gathered.push_back(&hs);
			}
		}
	};

	// InputHandlers class contains all the InputHandler subclasses
	class InputHandlers {
	public:
		// Input Handler to Capture a single keypress.
		// If InputHandler::hotspots is not empty any keypress that doesn't correspond to a hotspot will display invalid command
		// key_sym is set to the key that was pressed
		class KeyOnly : public InputHandler {
		public:
			SDL_Keycode key_sym = SDLK_UNKNOWN;

			// Construct with no hotspots. Any keypress is allowed and will be capture
			KeyOnly(std::string&& promptmsg) : InputHandler(true, std::move(promptmsg)) {}

			// Constructs with a vector of hotspots
			KeyOnly(std::string&& promptmsg, std::vector<Hotspot>&& hotspots)
					: InputHandler(true, std::move(promptmsg), std::move(hotspots)) {}

			void GetPromptMessage(char* buf, size_t buf_size) override {
				snprintf(buf, buf_size, "%s", promptmsg.c_str());
			}

			// On Input just records the key
			bool OnInput(SDL_Keycode key_sym) override {
				this->key_sym = Hotspot::FixUppercaseKeycode(key_sym);
				return true;
			}

			bool check_key(SDL_Keycode key_sym) {
				return this->key_sym == Hotspot::FixUppercaseKeycode(key_sym);
			}

			// Parse makes sure the pressed key is in hotspots
			void Parse() override;

			void clear() override {
				InputHandler::clear();
				key_sym = 0;
			}
		};

		// PressAKey is the same as KeyOnly except it adds "Press a Key" to the end of the prompt message
		// This is used by RunMenu to show messages from MenuCommandExceptions
		class PressAKey : public KeyOnly {
		public:
			PressAKey(std::string&& promptmsg = {}) : KeyOnly(std::move(promptmsg)) {}

			void GetPromptMessage(char* buf, size_t buf_size) override;
		};

		class String : public InputHandler {
			std::string invalidmsg;

		public:
			String(bool empty_allowed, std::string&& promptmsg, std::string&& invalidmsg);

			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;
		};

		// Integer Input Handler
		// Used to get a integer numeric input from the user
		// val_min and val_max are used to do bounds checking
		// The prompt message will be painted showing the valid range
		// Use INT_MIN and INT_MAX to suppress bound checking and showing the range
		// If parsing fails for whatever reason a MenuCommandException will be thrown with the message set to invalidmsg
		// optional special is an out of bounds value that will be allowed and not fail parsing if the user enters it
		// Validate function is called at the end of parsing if it returns false Parsing will fail
		class Integer : public InputHandler {
		protected:
			bool                          hexonly;
			int                           val_min, val_max;
			std::string                   invalidmsg;
			std::optional<int>            special;
			std::function<bool(Integer*)> validate;

		public:
			int value = 0;

			void GetPromptMessage(char* buf, size_t buf_size) override;

			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override {
				InputHandler::clear();
				value = val_min;
			}

			void setMax(int new_max) {
				val_max = new_max;
			}

			Integer(bool empty_allowed, int min, int max, bool hex);
			Integer(bool empty_allowed, int min, int max, bool hex, std::string&& promptmsg);
			Integer(bool empty_allowed, int min, int max, bool hex, std::string&& promptmsg, std::string&& invalidmsg,
					std::optional<int> special = {}, std::function<bool(Integer*)> validate = {});
		};

		// Shape Input Handler is a sub class of Integer
		// It allows the user to enter a numeric value for a shape  from shapes.vga
		// Or the user can choose to use the Shape Browser to pick a shape there
		// This class can also get a frame number at the same time. If wantframenum is true, After Entering a shape number, the user
		// will be prompted for a frame number unless they used the shape browser where the frame number will also be taken from
		// there
		class Shape : public Integer {
		public:
			int  shapenum     = -1;
			int  framenum     = -1;
			bool wantframenum = false;
			Shape(bool empty_allowed, bool wantframenum = false);
			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override;
		};

		// GameObject is a subclass of Integer so user can enter a npc number for the object
		// Alternatively they can Pick an object from the Game World in Target Mode
		// If empty_allowed is true, empty input will use the grabbed NPC if NPC grabbing is enabled
		class GameObject : public Integer {
		public:
			Game_object* object = nullptr;
			GameObject(bool empty_allowed);
			GameObject(bool empty_allowed, std::string&& promptmsg);
			GameObject(bool empty_allowed, std::string&& promptmsg, std::string&& invalidmsg);
			bool OnInput(SDL_Keycode key_sym) override;
			void Parse() override;

			void clear() override {
				Integer::clear();
				object = nullptr;
			}
		};

		// Actor is a simple subclass of GameObject with only real the difference that when using pick mode only Actors are allowed
		// Message are also changed to be specific to NPCs
		class Actor : public GameObject {
		public:
			Actor(bool empty_allowed);
			Actor(bool empty_allowed, std::string&& promptmsg);
			::Actor* actor = nullptr;
			void     Parse() override;

			void clear() override {
				GameObject::clear();
				actor = nullptr;
			}
		};
	};

	struct Menu;

	// MenuCommand the class everything is built from
	// Uses a simple event model. Events can be handled by subclassing or by using callback in the Events struct
	// Menu Commands have a vector on Input handler that will process input from the user before the MenuCommand is Activated
	// Events are
	// Activate: Called once all input handlers have finished processing input. Returns a Menu Command that should take over and be
	// pushed to the top of the stack. if null is returned  this command will be popped. If this is returned, input processing will
	// be reset
	// paint_display: Called to paint the display region when a Menu Command is top of the stack or if the Menu Command
	// above has forwarded the event down the stack to us. Returns true if event handled. returns false to forward to below
	// run: Called once per frame when a Menu Command or it's owner Menu is top of the stack and processing input
	// cancelled: called if input processing for the Menu Command was cancelled because the user pressed Escape or
	// if the MenuCommand was popped from the stack because of a MenuCommandException
	// begin_phase: called when a new input phase has begun
	// end_phase: called when an input phase is ending
	class MenuCommand : public std::enable_shared_from_this<MenuCommand> {
		bool input_active = false;

	public:
		size_t phase = 0;    // The current input phase inputs[phase] is the input handler for the phase
		std::vector<std::shared_ptr<InputHandler>>
									 inputs;    // The input handlers that should recieve input before this command is activated
		std::shared_ptr<MenuCommand> below;     // Back reference to the MenuCommand below us in the stack

	protected:
		// If set this is a pointer to our hotspot in the menu we are in
		Hotspot* hotspot = nullptr;

	public:
		MenuCommand() {}

		virtual ~MenuCommand() {}

		Hotspot* getHotspot() {
			return hotspot;
		}

		// Function Callbacks for Events
		struct Events {
			std::function<std::shared_ptr<MenuCommand>(MenuCommand*, SDL_Keycode keycode)> Activate      = {};
			std::function<bool(MenuCommand*)>                                              paint_display = {};
			std::function<void(MenuCommand*)>                                              run           = {};
			std::function<void(MenuCommand*)>                                              cancelled     = {};
			std::function<void(MenuCommand*)>                                              begin_phase   = {};
			std::function<void(MenuCommand*)>                                              end_phase     = {};
		} events;

		// Virtual methods for input handlers
		virtual std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) {
			if (events.Activate) {
				return events.Activate(this, keycode);
			}
			return {};
		}

		virtual void paint_display() {
			// let event handler paint the display first and if it doesn't forward to below
			if (!events.paint_display || !events.paint_display(this)) {
				below->paint_display();
			}
		}

		virtual void cancelled() {
			if (events.cancelled) {
				events.cancelled(this);
			}
		}

		InputHandler* GetInputHandler() const {
			if (phase >= inputs.size()) {
				return {};
			}
			return inputs[phase].get();
		}

		virtual void GatherHotspots(std::vector<Hotspot*>& gathered) {
			if (below) {
				below->GatherHotspots(gathered);
			}
		}

		virtual void run() {
			if (events.run) {
				events.run(this);
			}
		}

		// Returns true if we are top of the stack, active and receiving input
		bool isInputActive() {
			return input_active;
		}

		// Returns false if no more phases
		virtual bool BeginPhase() {
			if (phase >= inputs.size()) {
				input_active = false;
				return false;
			}
			inputs[phase]->clear();
			input_active = true;
			if (events.begin_phase) {
				events.begin_phase(this);
			}
			return true;
		}

		virtual void EndPhase() {
			if (events.end_phase) {
				events.end_phase(this);
			}
			++phase;
			if (phase >= inputs.size()) {
				input_active = false;
			}
		}

		void ResetPhase() {
			phase = 0;
			// This is set false until BeginPhase is called
			input_active = false;
		}

		// Returns true if the command is ready to be activated, that is there are no more phases
		bool Ready() {
			return phase >= inputs.size();
		}

		// Unready goes back an input phase
		void UnReady() {
			if (inputs.size()) {
				phase--;
				input_active = false;
			}
		}

		size_t NumPhases() {
			return inputs.size();
		}

	private:
		friend Menu;
		// If set this is the menu that we are in
		Menu* owner = nullptr;

	public:
		std::shared_ptr<Menu> GetMyMenu() {
			if (owner) {
				return std::static_pointer_cast<Menu>(owner->shared_from_this());
			}
			if (below) {
				auto menu = std::dynamic_pointer_cast<Menu>(below);
				if (menu) {
					return menu;
				}
				return below->GetMyMenu();
			}
			return {};
		}

		// Helper Methods to get and set data. This is used by menus to keep track of the current object (actor, flag num) being
		// edited

	protected:
		std::any data;

	public:
		template <typename T>
		void setData(T value) {
			data = std::make_any<T>(value);
		}

		template <typename T>
		T getDataOrDefault(T defaultValue = {}) const {
			if (!data.has_value() || data.type() != typeid(T)) {
				return defaultValue;
			}

			return std::any_cast<T>(data);
		}

		template <typename T>
		static T getDataOrDefault(MenuCommand* self, T defaultValue = {}) {
			return self ? self->getDataOrDefault<T>(defaultValue) : defaultValue;
		}

		template <typename T>
		static T getDataOrDefault(std::shared_ptr<MenuCommand> self, T defaultValue = {}) {
			return self ? self->getDataOrDefault<T>(defaultValue) : defaultValue;
		}
	};

	// LeftRightIntegerCommand subclass of MenuCommand
	// A menu command that scrolls a value between a min an max value. Must have a hotspot with 2 keycodes
	// First keycode decrements and second keycode increments
	// Hotspot label is updated to reflect the current value
	struct LeftRightIntegerCommand : public MenuCommand {
		int  min, max, currentval;
		bool update_label;

		LeftRightIntegerCommand(int min, int max, int initial, bool update_label = true)
				: MenuCommand(), min(std::min(min, max)), max(std::max(min, max)), currentval(initial), update_label(update_label) {
		}

		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override {
			// first hotspot key is decrement
			if (hotspot && hotspot->IsKeycode(keycode, 1 << 0) && currentval > min) {
				currentval--;
				if (currentval > max) {
					currentval = max;
				}
			}
			// second key is increment
			if (hotspot && hotspot->IsKeycode(keycode, 1 << 1) && currentval < max) {
				currentval++;
				if (currentval < min) {
					currentval = min;
				}
			}

			return MenuCommand::Activate(keycode);
		}

		void run() override {
			MenuCommand::run();
			// If we don't have a hotspot we can't do anything
			if (!hotspot) {
				return;
			}
			// Set each key hidden based on comparing the currentvalue to min and max
			hotspot->setHidden(0, currentval <= min);
			hotspot->setHidden(1, currentval >= max);
			if (update_label) {
				char valstring[16];
				snprintf(valstring, std::size(valstring), "%4i", currentval);
				hotspot->label_rw = (hotspot->label + valstring);
			}
		}
	};

	// ToggleCommand subclass of MenuCommand
	// A simple menu command that manages and updates its Hotspot label to refect a boolean state using the supplied string array
	// when activated the boolean state is toggled
	struct ToggleCommand : public MenuCommand {
		bool                              state;
		const std::array<const char*, 2>& state_strings;

		ToggleCommand(bool state, const std::array<const char*, 2>& state_strings)
				: MenuCommand(), state(state), state_strings(state_strings) {}

		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override {
			state = !state;

			return MenuCommand::Activate(keycode);
		}

		void run() override {
			MenuCommand::run();
			// Set the hotspot label based on the currentstate

			if (hotspot) {
				hotspot->label_rw = hotspot->label + state_strings[int(state)];
			}
		}
	};

	// A menu is a MenuCommand and can be directly used as a menu item to create multi level menus
	// If Menu needs additional input before it is shown it is recommended to return the Menu from the activate event of a
	// MenuCommand instead. Adding InputHandlers to a Menu object to require additional input before the Menu is shown should work
	// but it is untested. The Menu Constructor adds a KeyOnly input handler to activate keypresses that select Menu Items and
	// should be the last input handler so do not push back or emplace back additional input handlers to a menu
	struct Menu : public MenuCommand {
		// Type Alias for a Menu Item
		// The Hotspot defines the label and keycode of the Menu Item and the MenuCommand is what the it should do when the user
		// selects it
		using Item = std::pair<Hotspot, std::shared_ptr<MenuCommand>>;

	protected:
		// hThe menu items. Changing this out of the Menu constructor is not a good idea
		std::forward_list<Item> items;

	public:
		// Construct a Menu from a moved std::forward_list of Items
		Menu(std::forward_list<Item>&& items);

		// Respond to Activate event. This checks the Menu's items and returns the MenuCommand associated with the Keycode that
		// caused activation If enter is pressed it returns itself which causes the keypress to be silently ignored. Otherwise it
		// throws MenuCommandException to show the Invalid Command Message
		std::shared_ptr<MenuCommand> Activate(SDL_Keycode keycode) override;

		void GatherHotspots(std::vector<Hotspot*>& gathered) override {
			for (auto& it : items) {
				gathered.push_back(&it.first);
			}
		}

		void run() override {
			if (isInputActive()) {
				// Forward to our children if we are receiving input. If we are not receiving input the event was forward to us
				for (auto& it : items) {
					if (it.second) {
						// Make sure hotspot pointer is set
						if (!it.second->hotspot) {
							it.second->hotspot = &(it.first);
						}
						//  Make sure owner is set
						if (!it.second->owner) {
							it.second->owner = this;
						}
						it.second->run();
					}
				}
			}
		}
	};

	SDL_Keycode GetKey(const std::vector<Hotspot*>& hotspots, SDL_Keycode& unicode);

	void RunMenu(std::shared_ptr<Menu> menu);

	// Map display/event coordinates to cheat-screen (scene layer) coordinates.
	// The cheat screen is drawn into a scaled scene layer, so raw mouse events
	// must be mapped through that layer rather than the game area.
	void screen_to_cheat(int ex, int ey, int& cx, int& cy) const;

	struct {
		Uint32 last = 0;
		// Accumulated swipe deltas. We treat these as a vector
		float dx = 0;
		float dy = 0;
	} swipe;

	Image_buffer8* ibuf = nullptr;
	// The cheat screen renders into a full-screen scene layer (owned by a local
	// in show_screen); this is a non-owning pointer valid only while the screen
	// is shown. Null if no layer is available.
	Scene_layer* scene = nullptr;
	// Current mouse position in cheat-screen (scene) coordinates, used for
	// hotspot hit-testing and hover.
	int                   cheat_mousex = 0, cheat_mousey = 0;
	std::shared_ptr<Font> font  = nullptr;
	Game_clock*           clock = nullptr;
	int                   maxx = 0, maxy = 0;
	int                   centerx = 0, centery = 0;
	Palette               pal            = {};
	Xform_palette         highlighttable = {};
	Xform_palette         hovertable     = {};
	Xform_palette         fontcolor      = {};
	Xform_palette         fontcolor2     = {};

	const float  swipe_threshold  = 0.075f;    // Threshold for Swipes to be converted into key inputs.
	const Uint32 highlighttimeout = 500;       // Timeout (ms) for how long hotspots should highlight for after a keypress
	const Uint32 cursorflashtime  = 500;       // Duty cycle time (ms) for cursor flash
	const Uint32 messagetimeout
			= 1000;    // Timeout (ms) for how long Press a Key messages should stay before Automatically dismissing

	std::shared_ptr<Menu> RootMenu();

	// Paint an arrow using the font, type is one of '^' 'v' '<' '>'
	int PaintArrow(int offsetx, int offsety, int type);

	int PaintKeyName(int offsetx, int offsety, SDL_Keycode key_sym);

	std::shared_ptr<Menu> UsecodeMenu();
	std::shared_ptr<Menu> GlobalFlagMenu(unsigned num);

	std::shared_ptr<Menu> NPCMenu(Actor* actor);

	std::shared_ptr<Menu> NPCFlagMenu(Actor* actor);
	std::shared_ptr<Menu> AdvancedFlagMenu(unsigned flagnum, Actor* actor);

	std::shared_ptr<Menu> BusinessMenu(Actor* actor);

	std::shared_ptr<Menu> PalEffectMenu(Actor* actor);

	std::shared_ptr<Menu> TeleportMenu();

	int highest_map = INT_MIN;
	int Get_highest_map();

	//
	static inline const int      button_down_finger = -1;
	std::unordered_multiset<int> buttons_down       = {};
	void                         WaitButtonsUp(bool silent = false);
};

#endif
