/* ScaleFX Vulkan acceleration using SDL 3.4's GPU renderer.
 * Copyright (C) 2026 Exult Team. GPL-2.0-or-later.
 * Shader algorithm attribution and MIT license: scale_scalefx.h.
 */
#ifndef EXULT_SCALEFX_GPU_H
#define EXULT_SCALEFX_GPU_H
#include "scale_scalefx.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <new>
#include <vector>
#if SDL_VERSION_ATLEAST(3, 4, 0)
#	include "shaders/scalefx_spirv.h"
#endif
namespace ScaleFX {
	class GPU {
		[[maybe_unused]] SDL_Renderer* renderer;
		[[maybe_unused]] bool          failed = false;
#if SDL_VERSION_ATLEAST(3, 4, 0)
		SDL_GPUDevice*                          device  = nullptr;
		SDL_GPUSampler*                         sampler = nullptr;
		SDL_GPUShader*                          vertex  = nullptr;
		std::array<SDL_GPUShader*, 5>           shaders{};
		std::array<SDL_GPUGraphicsPipeline*, 5> pipelines{};
		std::array<SDL_GPUTexture*, 5>          targets{};
		SDL_GPUTexture*                         source   = nullptr;
		SDL_GPUTexture*                         metrics  = nullptr;
		SDL_GPUTransferBuffer*                  transfer = nullptr;
		SDL_Texture*                            result   = nullptr;
		int                                     width = 0, height = 0, factor = 0;
		bool                                    initialized = false;
		std::array<SDL_Color, 256>              palette{};
		std::vector<uint32_t>                   upload;
		std::vector<float>                      distances;

		void release() {
			if (renderer) {
				SDL_FlushRenderer(renderer);
			}
			if (result) {
				SDL_DestroyTexture(result);
			}
			result = nullptr;
			for (auto*& p : pipelines) {
				if (p) {
					SDL_ReleaseGPUGraphicsPipeline(device, p);
				}
				p = nullptr;
			}
			for (auto*& t : targets) {
				if (t) {
					SDL_ReleaseGPUTexture(device, t);
				}
				t = nullptr;
			}
			if (source) {
				SDL_ReleaseGPUTexture(device, source);
			}
			source = nullptr;
			if (metrics) {
				SDL_ReleaseGPUTexture(device, metrics);
			}
			metrics = nullptr;
			if (transfer) {
				SDL_ReleaseGPUTransferBuffer(device, transfer);
			}
			transfer = nullptr;
			for (auto*& s : shaders) {
				if (s) {
					SDL_ReleaseGPUShader(device, s);
				}
				s = nullptr;
			}
			if (vertex) {
				SDL_ReleaseGPUShader(device, vertex);
			}
			vertex = nullptr;
			if (sampler) {
				SDL_ReleaseGPUSampler(device, sampler);
			}
			sampler     = nullptr;
			initialized = false;
		}

		SDL_GPUTexture* texture(SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage, int w, int h) {
			SDL_GPUTextureCreateInfo info{};
			info.type                 = SDL_GPU_TEXTURETYPE_2D;
			info.format               = format;
			info.usage                = usage;
			info.width                = w;
			info.height               = h;
			info.layer_count_or_depth = 1;
			info.num_levels           = 1;
			return SDL_CreateGPUTexture(device, &info);
		}

		bool prepare(int w, int h, int f) {
			if (source && width == w && height == h && factor == f) {
				return true;
			}
			release();
			width  = w;
			height = h;
			factor = f;
			device = SDL_GetGPURendererDevice(renderer);
			if (!device || !(SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV)) {
				return false;
			}
			SDL_GPUSamplerCreateInfo si{};
			si.min_filter = si.mag_filter = SDL_GPU_FILTER_NEAREST;
			si.mipmap_mode                = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
			si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
			sampler                                                   = SDL_CreateGPUSampler(device, &si);
			if (!sampler) {
				return false;
			}
			source = texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, w, h);
			if (!source) {
				return false;
			}
			SDL_GPUTransferBufferCreateInfo ti{};
			ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			ti.size  = Uint32(size_t(w) * size_t(h) * 4 + 256 * 256 * sizeof(float));
			metrics  = texture(SDL_GPU_TEXTUREFORMAT_R32_FLOAT, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, 256);
			if (!metrics) {
				return false;
			}
			transfer = SDL_CreateGPUTransferBuffer(device, &ti);
			if (!transfer) {
				return false;
			}
			for (int i = 0; i < 5; ++i) {
				const auto format  = (i == 0 || i == 1 || i == 3) ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT
																  : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				targets[size_t(i)] = texture(
						format, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, i == 4 ? w * f : w,
						i == 4 ? h * f : h);
				if (!targets[size_t(i)]) {
					return false;
				}
			}
			SDL_GPUShaderCreateInfo vi{};
			vi.code       = reinterpret_cast<const Uint8*>(Shaders::passvertex);
			vi.code_size  = sizeof(Shaders::passvertex);
			vi.entrypoint = "main";
			vi.format     = SDL_GPU_SHADERFORMAT_SPIRV;
			vi.stage      = SDL_GPU_SHADERSTAGE_VERTEX;
			vertex        = SDL_CreateGPUShader(device, &vi);
			if (!vertex) {
				return false;
			}
			const uint32_t* code[] = {Shaders::pass0, Shaders::pass1, Shaders::pass2, Shaders::pass3, Shaders::pass4};
			const size_t    length[]
					= {sizeof(Shaders::pass0), sizeof(Shaders::pass1), sizeof(Shaders::pass2), sizeof(Shaders::pass3),
					   sizeof(Shaders::pass4)};
			for (int i = 0; i < 5; ++i) {
				SDL_GPUShaderCreateInfo info{};
				info.code                = reinterpret_cast<const Uint8*>(code[i]);
				info.code_size           = length[i];
				info.entrypoint          = "main";
				info.format              = SDL_GPU_SHADERFORMAT_SPIRV;
				info.stage               = SDL_GPU_SHADERSTAGE_FRAGMENT;
				info.num_samplers        = (i == 0 || i == 2 || i == 4) ? 2 : 1;
				info.num_uniform_buffers = 1;
				shaders[size_t(i)]       = SDL_CreateGPUShader(device, &info);
				if (!shaders[size_t(i)]) {
					return false;
				}
				SDL_GPUColorTargetDescription target{};
				target.format = (i == 0 || i == 1 || i == 3) ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT
															 : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				SDL_GPUGraphicsPipelineCreateInfo pi{};
				pi.vertex_shader                         = vertex;
				pi.fragment_shader                       = shaders[size_t(i)];
				pi.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
				pi.target_info.num_color_targets         = 1;
				pi.target_info.color_target_descriptions = &target;
				pipelines[size_t(i)]                     = SDL_CreateGPUGraphicsPipeline(device, &pi);
				if (!pipelines[size_t(i)]) {
					return false;
				}
			}
			auto props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA32);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, w * f);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, h * f);
			SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, targets[4]);
			result = SDL_CreateTextureWithProperties(renderer, props);
			SDL_DestroyProperties(props);
			if (!result) {
				return false;
			}
			SDL_SetTextureBlendMode(result, SDL_BLENDMODE_NONE);
			SDL_SetTextureScaleMode(result, SDL_SCALEMODE_NEAREST);
			upload.resize(size_t(w) * size_t(h));
			distances.resize(256 * 256);
			return true;
		}
#endif
	public:
		explicit GPU(SDL_Renderer* r) : renderer(r) {}

		GPU(const GPU&)            = delete;
		GPU& operator=(const GPU&) = delete;

		~GPU() {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			release();
#endif
		}

		static bool available(SDL_Renderer* r) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			if (!r || SDL_strcmp(SDL_GetRendererName(r), "gpu") != 0) {
				return false;
			}
			auto* d = SDL_GetGPURendererDevice(r);
			return d && (SDL_GetGPUShaderFormats(d) & SDL_GPU_SHADERFORMAT_SPIRV);
#else
			(void)r;
			return false;
#endif
		}

#if SDL_VERSION_ATLEAST(3, 4, 0)
		SDL_GPUTexture* gpu_output() const {
			return targets[4];
		}

		SDL_GPUTexture* indexed_source() const {
			return source;
		}
#endif
		SDL_Texture* output() const {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			return initialized && !failed ? result : nullptr;
#else
			return nullptr;
#endif
		}

		// Dirty is an output-damage rectangle in source coordinates; caller must
		// expand actual source edits by six pixels, as for the CPU implementation.
		bool update(SDL_Surface* input, SDL_Rect dirty, int f) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			if (failed || !available(renderer)) {
				return false;
			}
			if (!input || input->format != SDL_PIXELFORMAT_INDEX8 || (f != 3 && f != 4 && f != 6)) {
				return false;
			}

			bool ok = false;
			try {
				ok = [&] {
					if (!prepare(input->w, input->h, f)) {
						return false;
					}
					auto* pal = SDL_GetSurfacePalette(input);
					if (!pal || pal->ncolors < 256) {
						return false;
					}
					bool                  changed = !initialized;
					std::array<bool, 256> changed_entries{};
					for (size_t i = 0; i < 256; ++i) {
						const auto a = palette[i], b = pal->colors[i];
						changed_entries[i] = !initialized || a.r != b.r || a.g != b.g || a.b != b.b;
						changed            = changed || changed_entries[i];
					}
					if (changed) {
						std::copy_n(pal->colors, 256, palette.begin());
						dirty = {0, 0, width, height};
					}
					SDL_Rect bounds{0, 0, width, height};
					if (!SDL_GetRectIntersection(&dirty, &bounds, &dirty)) {
						return true;
					}
					// Refresh the complete input halo too: guard-band pixels
					// can change when a logical image edge is repainted.
					SDL_Rect input_area{std::max(0, dirty.x - 6), std::max(0, dirty.y - 6), 0, 0};
					input_area.w = std::min(width, dirty.x + dirty.w + 6) - input_area.x;
					input_area.h = std::min(height, dirty.y + dirty.h + 6) - input_area.y;
					std::array<uint32_t, 256> colours;
					for (size_t i = 0; i < 256; ++i) {
						const auto  c       = palette[i];
						const Uint8 bytes[] = {c.r, c.g, c.b, Uint8(i)};
						SDL_memcpy(&colours[i], bytes, 4);
					}
					for (int y = input_area.y; y < input_area.y + input_area.h; ++y) {
						const auto* row = static_cast<const Uint8*>(input->pixels) + y * input->pitch;
						auto*       dst = upload.data() + size_t(y) * size_t(width);
						for (int x = input_area.x; x < input_area.x + input_area.w; ++x) {
							dst[x] = colours[row[x]];
						}
					}
					if (!SDL_FlushRenderer(renderer)) {
						return false;
					}
					auto* mapped = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transfer, true));
					if (!mapped) {
						return false;
					}
					for (int y = input_area.y; y < input_area.y + input_area.h; ++y) {
						SDL_memcpy(
								mapped + size_t(y * width + input_area.x) * 4, upload.data() + size_t(y * width + input_area.x),
								size_t(input_area.w) * 4);
					}
					if (changed) {
						auto* distances = mapped + size_t(width) * size_t(height) * 4;
						for (size_t a = 0; a < 256; ++a) {
							for (size_t b = 0; b <= a; ++b) {
								if (!changed_entries[a] && !changed_entries[b]) {
									continue;
								}
								const auto  ca = palette[a], cb = palette[b];
								const float d                = detail::distance({ca.r, ca.g, ca.b}, {cb.r, cb.g, cb.b});
								this->distances[a * 256 + b] = this->distances[b * 256 + a] = d;
							}
						}
						SDL_memcpy(distances, this->distances.data(), 256 * 256 * sizeof(float));
					}
					SDL_UnmapGPUTransferBuffer(device, transfer);
					auto* commands = SDL_AcquireGPUCommandBuffer(device);
					if (!commands) {
						return false;
					}
					auto* copy = SDL_BeginGPUCopyPass(commands);
					if (!copy) {
						SDL_CancelGPUCommandBuffer(commands);
						return false;
					}
					SDL_GPUTextureTransferInfo transfer_info{};
					transfer_info.transfer_buffer = transfer;
					transfer_info.offset          = Uint32(size_t(input_area.y * width + input_area.x) * 4);
					transfer_info.pixels_per_row  = width;
					transfer_info.rows_per_layer  = height;
					SDL_GPUTextureRegion region{};
					region.texture = source;
					region.x       = input_area.x;
					region.y       = input_area.y;
					region.w       = input_area.w;
					region.h       = input_area.h;
					region.d       = 1;
					SDL_UploadToGPUTexture(copy, &transfer_info, &region, false);
					if (changed) {
						SDL_GPUTextureTransferInfo mt{};
						mt.transfer_buffer = transfer;
						mt.offset          = Uint32(size_t(width) * size_t(height) * 4);
						mt.pixels_per_row  = 256;
						mt.rows_per_layer  = 256;
						SDL_GPUTextureRegion mr{};
						mr.texture = metrics;
						mr.w = mr.h = 256;
						mr.d        = 1;
						SDL_UploadToGPUTexture(copy, &mt, &mr, false);
					}
					SDL_EndGPUCopyPass(copy);
					const int params[] = {width, height, f, -1};
					for (int i = 0; i < 5; ++i) {
						const int halo = i == 0 ? 5 : i == 1 ? 4 : i == 2 ? 3 : 0;
						SDL_Rect  clip{std::max(0, dirty.x - halo), std::max(0, dirty.y - halo), 0, 0};
						clip.w = std::min(width, dirty.x + dirty.w + halo) - clip.x;
						clip.h = std::min(height, dirty.y + dirty.h + halo) - clip.y;
						if (i == 4) {
							clip.x *= f;
							clip.y *= f;
							clip.w *= f;
							clip.h *= f;
						}
						SDL_GPUColorTargetInfo target{};
						target.texture  = targets[size_t(i)];
						target.load_op  = initialized ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
						target.store_op = SDL_GPU_STOREOP_STORE;
						auto* pass      = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
						if (!pass) {
							SDL_CancelGPUCommandBuffer(commands);
							return false;
						}
						SDL_BindGPUGraphicsPipeline(pass, pipelines[size_t(i)]);
						SDL_GPUTextureSamplerBinding bindings[2]{};
						bindings[0] = {i == 0 ? source : targets[size_t(i - 1)], sampler};
						if (i == 0 || i == 2 || i == 4) {
							bindings[1] = {i == 0 ? metrics : i == 2 ? targets[0] : source, sampler};
						}
						SDL_BindGPUFragmentSamplers(pass, 0, bindings, (i == 0 || i == 2 || i == 4) ? 2 : 1);
						SDL_PushGPUFragmentUniformData(commands, 0, params, sizeof params);
						SDL_GPUViewport viewport{0, 0, float(i == 4 ? width * f : width), float(i == 4 ? height * f : height),
												 0, 1};
						SDL_SetGPUViewport(pass, &viewport);
						SDL_SetGPUScissor(pass, &clip);
						SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
						SDL_EndGPURenderPass(pass);
					}
					if (!SDL_SubmitGPUCommandBuffer(commands)) {
						return false;
					}
					initialized = true;
					return true;
				}();
			} catch (const std::bad_alloc&) {
				SDL_SetError("ScaleFX GPU upload allocation failed");
			}
			if (!ok) {
				failed = true;
				SDL_Log("ScaleFX GPU unavailable; using CPU: %s", SDL_GetError());
				release();
				std::vector<uint32_t>().swap(upload);
			}
			return ok;
#else
			(void)input;
			(void)dirty;
			(void)f;
			return false;
#endif
		}
	};
}    // namespace ScaleFX
#endif
