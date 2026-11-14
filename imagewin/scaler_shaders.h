/* Native SDL GPU shader assets. GPL-2.0-or-later. */
#ifndef EXULT_SCALER_SHADERS_H
#define EXULT_SCALER_SHADERS_H
#include <SDL3/SDL.h>

#include <type_traits>
#if SDL_VERSION_ATLEAST(3, 4, 0)
// Ship each supported format so the active SDL GPU device can choose its
// backend independently of the host OS. No runtime shader translator is needed.
#	include "shaders/scalers_dxil.h"
#	include "shaders/scalers_metal.h"
#	include "shaders/scalers_spirv.h"

namespace ScaleFX { namespace Shaders {
	enum Program {
		pass0,
		pass1,
		pass2,
		pass3,
		pass4,
		passreduce,
		passmatte,
		passvertex,
		scalers,
		scalersoutput,
		program_count
	};

	struct Code {
		const Uint8*        bytes  = nullptr;
		size_t              size   = 0;
		SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
		const char*         entry  = nullptr;
	};

	constexpr SDL_GPUShaderFormat available_formats
			= SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL;

	inline bool supports(SDL_GPUDevice* device) {
		return device && (SDL_GetGPUShaderFormats(device) & available_formats);
	}

	template <class T, size_t N>
	Code view(const T (&data)[N], SDL_GPUShaderFormat format, const char* entry) {
		return {reinterpret_cast<const Uint8*>(data), sizeof(data) - (std::is_same_v<T, char> ? 1 : 0), format, entry};
	}

	inline Code code_for_formats(SDL_GPUShaderFormat supported, Program program) {
		if (static_cast<unsigned>(program) >= program_count) {
			return {};
		}
		if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
			const Code programs[] = {
					view(SPIRV::pass0, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::pass1, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::pass2, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::pass3, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::pass4, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::passreduce, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::passmatte, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::passvertex, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::scalers, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
					view(SPIRV::scalersoutput, SDL_GPU_SHADERFORMAT_SPIRV, "main"),
			};
			return programs[program];
		}
		if (supported & SDL_GPU_SHADERFORMAT_DXIL) {
			const Code programs[] = {
					view(DXIL::pass0, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::pass1, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::pass2, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::pass3, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::pass4, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::passreduce, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::passmatte, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::passvertex, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::scalers, SDL_GPU_SHADERFORMAT_DXIL, "main"),
					view(DXIL::scalersoutput, SDL_GPU_SHADERFORMAT_DXIL, "main"),
			};
			return programs[program];
		}
		if (supported & SDL_GPU_SHADERFORMAT_MSL) {
			const Code programs[] = {
					view(METAL::pass0, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::pass1, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::pass2, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::pass3, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::pass4, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::passreduce, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::passmatte, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::passvertex, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::scalers, SDL_GPU_SHADERFORMAT_MSL, "main0"),
					view(METAL::scalersoutput, SDL_GPU_SHADERFORMAT_MSL, "main0"),
			};
			return programs[program];
		}
		return {};
	}

	inline Code code(SDL_GPUDevice* device, Program program) {
		return code_for_formats(device ? SDL_GetGPUShaderFormats(device) : SDL_GPU_SHADERFORMAT_INVALID, program);
	}

	inline SDL_GPUShader* create(SDL_GPUDevice* device, Code source, SDL_GPUShaderStage stage, Uint32 samplers, Uint32 uniforms) {
		if (!source.bytes) {
			SDL_SetError("No scaler shader format supported by the active GPU device");
			return nullptr;
		}
		SDL_GPUShaderCreateInfo info{};
		info.code                = source.bytes;
		info.code_size           = source.size;
		info.entrypoint          = source.entry;
		info.format              = source.format;
		info.stage               = stage;
		info.num_samplers        = samplers;
		info.num_uniform_buffers = uniforms;
		return SDL_CreateGPUShader(device, &info);
	}
}}    // namespace ScaleFX::Shaders
#endif
#endif
