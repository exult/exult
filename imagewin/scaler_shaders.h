/* Native SDL GPU shader assets. GPL-2.0-or-later. */
#ifndef EXULT_SCALER_SHADERS_H
#define EXULT_SCALER_SHADERS_H
#include <SDL3/SDL.h>

#include <type_traits>
#if SDL_VERSION_ATLEAST(3, 4, 0)
// Only compile the native backend into each application. No shader compiler
// or shader translation library is needed on the player's machine.
#	if defined(__APPLE__)
#		include <TargetConditionals.h>
#		if TARGET_OS_IPHONE
#			include "shaders/scalers_ios.h"
#		else
#			include "shaders/scalers_metal.h"
#		endif
#	elif defined(_WIN32)
#		include "shaders/scalers_dxil.h"
#	else
#		include "shaders/scalers_spirv.h"
#	endif
namespace ScaleFX { namespace Shaders {
#	if defined(__APPLE__)
	constexpr inline SDL_GPUShaderFormat native_format = SDL_GPU_SHADERFORMAT_MSL;
	constexpr inline const char*         entry         = "main0";
#	elif defined(_WIN32)
	constexpr inline SDL_GPUShaderFormat native_format = SDL_GPU_SHADERFORMAT_DXIL;
	constexpr inline const char*         entry         = "main";
#	else
	constexpr inline SDL_GPUShaderFormat native_format = SDL_GPU_SHADERFORMAT_SPIRV;
	constexpr inline const char*         entry         = "main";
#	endif
	struct Code {
		const Uint8* bytes;
		size_t       size;
	};

	template <class T, size_t N>
	Code code(const T (&data)[N]) {
		return {reinterpret_cast<const Uint8*>(data), sizeof(data) - (std::is_same_v<T, char> ? 1 : 0)};
	}

	inline SDL_GPUShader* create(SDL_GPUDevice* device, Code source, SDL_GPUShaderStage stage, Uint32 samplers, Uint32 uniforms) {
		SDL_GPUShaderCreateInfo info{};
		info.code                = source.bytes;
		info.code_size           = source.size;
		info.entrypoint          = entry;
		info.format              = native_format;
		info.stage               = stage;
		info.num_samplers        = samplers;
		info.num_uniform_buffers = uniforms;
		return SDL_CreateGPUShader(device, &info);
	}
}}    // namespace ScaleFX::Shaders
#endif
#endif
