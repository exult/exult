// Shader format selection is based on device capabilities, on every host OS.
#include "imagewin/scaler_shaders.h"

#include <iostream>
#include <stdexcept>
#if SDL_VERSION_ATLEAST(3, 4, 0)
int main() {
	using namespace ScaleFX::Shaders;
	try {
		for (const auto format : {SDL_GPU_SHADERFORMAT_SPIRV, SDL_GPU_SHADERFORMAT_DXIL, SDL_GPU_SHADERFORMAT_MSL}) {
			for (int i = 0; i < program_count; ++i) {
				const auto shader = code_for_formats(format, static_cast<Program>(i));
				if (!shader.bytes || !shader.size || shader.format != format) {
					throw std::runtime_error("Missing shader for supported device format");
				}
				if (format == SDL_GPU_SHADERFORMAT_MSL) {
					if (SDL_strcmp(shader.entry, "main0") || shader.bytes[shader.size] != 0
						|| SDL_strncmp(reinterpret_cast<const char*>(shader.bytes), "#pragma", 7)) {
						throw std::runtime_error("Incorrect Metal source or entry point");
					}
				} else {
					const Uint8 spirv_magic[] = {3, 2, 35, 7};
					const auto* signature
							= format == SDL_GPU_SHADERFORMAT_SPIRV ? spirv_magic : reinterpret_cast<const Uint8*>("DXBC");
					if (SDL_strcmp(shader.entry, "main") || SDL_memcmp(shader.bytes, signature, 4)) {
						throw std::runtime_error("Incorrect binary format or entry point");
					}
				}
			}
		}
		if (code_for_formats(SDL_GPU_SHADERFORMAT_INVALID, pass0).bytes || code_for_formats(SDL_GPU_SHADERFORMAT_DXBC, pass0).bytes
			|| code_for_formats(available_formats, program_count).bytes) {
			throw std::runtime_error("Unsupported format/program did not reject selection");
		}
		std::cout << "All shader programs select SPIR-V, DXIL and Metal from device formats on this host\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
#else
int main() {
	return 0;
}
#endif
