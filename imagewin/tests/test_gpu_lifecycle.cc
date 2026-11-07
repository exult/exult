// Production renderer regression: link with engine objects, replacing main.
#include "conf/Configuration.h"
#include "imagewin/iwin8.h"
#include "imagewin/scalefx_gpu.h"

#include <iostream>
#include <stdexcept>

class Window : public Image_window8 {
public:
	using Image_window8::Image_window8;

	bool enabled() const {
		return scalefx_gpu_allowed();
	}

	bool gpu() const {
		return ScaleFX::GPU::available(screen_renderer);
	}
};

int main() {
	try {
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			throw std::runtime_error(SDL_GetError());
		}
		config = new Configuration;
		config->set("config/video/scalefx_gpu", "auto", false);
		config->set("config/video/vsync", 0, false);
		{
			Window window(128, 96, 32, 24, 4, false, Image_window::ScaleFX, Image_window::Fit, Image_window::point);
			window.clear_clip();
			if (!window.gpu()) {
				throw std::runtime_error("No GPU renderer");
			}
			unsigned char palette[768];
			for (int i = 0; i < 768; ++i) {
				palette[i] = i % 256;
			}
			window.set_palette(palette, 255);
			window.fill8(1);
			window.show();
			Image_buffer8 scene(20, 16);
			scene.fill8(1);
			auto* saved = window.set_render_buffer(&scene);
			window.set_scene_mode(true, 20, 16);
			window.show();
			window.set_scene_mode(false);
			window.set_render_buffer(saved);
			if (!window.enabled()) {
				throw std::runtime_error("Scene permanently disabled GPU acceleration");
			}
			window.set_ui_config(32, 24, Image_window::ScaleFX, Image_window::Fit, Image_window::point);
			int   layer  = window.create_layer("Lifecycle UI", 16, 12);
			auto* pixels = window.get_layer_ibuf(layer);
			pixels->fill8(255);
			pixels->fill8(2, 5, 5, 2, 2);
			window.layer_set_visible(layer, true);
			window.layer_set_dirty(layer);
			window.show();
			if (!window.enabled()) {
				throw std::runtime_error("UI disabled GPU acceleration");
			}
		}
		delete config;
		config = nullptr;
		SDL_Quit();
		std::cout << "GPU world -> scene -> transparent UI transition passed\n";
		return 0;
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
