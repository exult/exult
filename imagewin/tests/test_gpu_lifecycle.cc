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
		return gpu_scalers_allowed();
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
		config->set("config/video/scaler_gpu", "auto", false);
		config->set("config/video/vsync", 0, false);
		for (const char* name :
			 {"ScaleFX", "Hq2x", "Hq3x", "Hq4x", "2xSaI", "Super2xSaI", "SuperEagle", "2xBR", "3xBR", "4xBR", "Point"}) {
			const int         method = Image_window::get_scaler_for_name(name);
			const std::string label  = name;
			const int         factor
					= (label == "Hq2x" || label == "2xSaI" || label == "Super2xSaI" || label == "SuperEagle" || label == "2xBR") ? 2
					  : (label == "Hq3x" || label == "3xBR") ? 3
															 : 4;
			Window window(128, 96, 32, 24, factor, false, method, Image_window::Fit, Image_window::point);
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
			window.set_ui_config(
					32, 24, label == "Point" ? int(Image_window::ScaleFX) : method, Image_window::Fit, Image_window::point);
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
		std::cout << "GPU scaler world -> scene -> transparent UI transitions and runtime UI selection passed\n";
		return 0;
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
