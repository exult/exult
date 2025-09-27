#pragma once
#include "ConfigSetting_widget.h"
#include "Modal_gump.h"
#include "Scrollable_widget.h"

#include <functional>

class AdvancedOptions_gump : public Modal_gump {
	std::string           title;
	std::string           helpurl;
	Scrollable_widget*    scroll;
	Gump_button*          apply;
	Gump_button*          cancel;
	Gump_button*          help;

public:
	AdvancedOptions_gump(
			std::vector<ConfigSetting_widget::Definition>* settings,
			std::string&& title, std::string&& helpurl,
			std::function<void()> applycallback);
	~AdvancedOptions_gump() override;

	bool mouse_down(int mx, int my, MouseButton button) override;
	bool mouse_up(int mx, int my, MouseButton button) override;
	bool mouse_drag(int mx, int my) override;

	bool mousewheel_down(int mx, int my) override;

	bool mousewheel_up(int mx, int my) override;

	bool key_down(SDL_Keycode chr, SDL_Keycode unicode) override;

	void paint() override;

	bool run() override;

	// alternative
	void paint_elems() override;

	std::function<void()> applycallback;
	void                  on_apply();
	void                  on_cancel();
	void                  on_help();
};
