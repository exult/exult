/* GPU scalers using SDL 3.4's GPU renderer.
 * Copyright (C) 2026 Exult Team. GPL-2.0-or-later.
 * Shader algorithm attribution and MIT license: scale_scalefx.h.
 */
#ifndef EXULT_SCALEFX_GPU_H
#define EXULT_SCALEFX_GPU_H
#include "scale_scalefx.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <climits>
#include <new>
#include <vector>
#if SDL_VERSION_ATLEAST(3, 4, 0)
#	include "scaler_shaders.h"
#	include "shaders/scalers_hq_lut.h"
#endif
namespace ScaleFX {
	enum class Algorithm {
		ScaleFX,
		HQ2x,
		HQ3x,
		HQ4x,
		SaI,
		SuperSaI,
		SuperEagle,
		XBR
	};

	class GPU {
		friend struct GPUProbe;
		[[maybe_unused]] SDL_Renderer* renderer;
		Algorithm                      algorithm;
		[[maybe_unused]] bool          failed = false;
#if SDL_VERSION_ATLEAST(3, 4, 0)
		SDL_GPUDevice*                          device        = nullptr;
		bool                                    full_analysis = false;
		SDL_GPUSampler*                         sampler       = nullptr;
		SDL_GPUShader*                          vertex        = nullptr;
		std::array<SDL_GPUShader*, 5>           shaders{};
		std::array<SDL_GPUGraphicsPipeline*, 5> pipelines{};
		std::array<SDL_GPUTexture*, 5>          targets{};
		SDL_GPUTexture*                         source   = nullptr;
		SDL_GPUTexture*                         metrics  = nullptr;
		SDL_GPUTexture*                         lookup   = nullptr;
		SDL_GPUTransferBuffer*                  transfer = nullptr;
		SDL_Texture*                            result   = nullptr;
		int                                     width = 0, height = 0, factor = 0;
		bool                                    initialized = false;
		std::array<SDL_Color, 256>              palette{};
		std::vector<uint32_t>                   upload;
		std::vector<float>                      distances;
		size_t                                  palette_offset = 0;

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
			if (lookup) {
				SDL_ReleaseGPUTexture(device, lookup);
			}
			lookup = nullptr;
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
			if (w <= 0 || h <= 0 || !SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D, usage)) {
				SDL_SetError("GPU scaler texture format or size is unsupported");
				return nullptr;
			}
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
			const uint64_t output_width = uint64_t(w) * f, output_height = uint64_t(h) * f;
			const uint64_t row_bytes    = (uint64_t(w) * 4 + 255) & ~uint64_t(255);
			const uint64_t source_bytes = (row_bytes * uint64_t(h) + 511) & ~uint64_t(511);
			const uint64_t upload_bytes = source_bytes + 256 * 256 * sizeof(float) + sizeof(Shaders::hq_lookup);
			if (w <= 0 || h <= 0 || output_width > INT_MAX || output_height > INT_MAX || upload_bytes > UINT32_MAX) {
				SDL_SetError("GPU scaler image exceeds supported allocation sizes");
				return false;
			}
			release();
			width  = w;
			height = h;
			factor = f;
			device = SDL_GetGPURendererDevice(renderer);
			if (!device || !(SDL_GetGPUShaderFormats(device) & Shaders::native_format)) {
				return false;
			}
			// D3D12 dirty analysis passes produce incorrect edge values even
			// with verified source/LUT uploads. Rebuild the low-resolution
			// intermediates there; source uploads and output damage stay partial.
			full_analysis = SDL_strcmp(SDL_GetGPUDeviceDriver(device), "direct3d12") == 0;
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
			ti.usage       = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			palette_offset = size_t(source_bytes);
			ti.size        = Uint32(palette_offset + 256 * 256 * sizeof(float));
			metrics        = algorithm == Algorithm::ScaleFX
									 ? texture(SDL_GPU_TEXTUREFORMAT_R32_FLOAT, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, 256)
									 : texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, 1);
			if (algorithm != Algorithm::ScaleFX) {
				ti.size += sizeof(Shaders::hq_lookup);
				lookup = texture(SDL_GPU_TEXTUREFORMAT_R16_UINT, SDL_GPU_TEXTUREUSAGE_SAMPLER, 512, 384);
				if (!lookup) {
					return false;
				}
			}
			if (!metrics) {
				return false;
			}
			transfer = SDL_CreateGPUTransferBuffer(device, &ti);
			if (!transfer) {
				return false;
			}
			for (int i = 0; i < 5; ++i) {
				const auto format  = i == 4                            ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
									 : algorithm != Algorithm::ScaleFX ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT
									 : (i == 0 || i == 1 || i == 3)    ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT
																	   : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				targets[size_t(i)] = texture(
						format, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, i == 4 ? w * f : w,
						i == 4 ? h * f : h);
				if (!targets[size_t(i)]) {
					return false;
				}
			}
			vertex = Shaders::create(device, Shaders::code(Shaders::passvertex), SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
			if (!vertex) {
				return false;
			}
			const Shaders::Code code[]
					= {Shaders::code(Shaders::pass0), Shaders::code(Shaders::pass1), Shaders::code(Shaders::pass2),
					   Shaders::code(Shaders::pass3), Shaders::code(Shaders::pass4)};
			for (int i = 0; i < (algorithm == Algorithm::ScaleFX ? 5 : 2); ++i) {
				const auto   shader_code = algorithm == Algorithm::ScaleFX ? code[i]
										   : i == 0                        ? Shaders::code(Shaders::scalers)
																		   : Shaders::code(Shaders::scalersoutput);
				const Uint32 samplers = algorithm == Algorithm::ScaleFX ? ((i == 0 || i == 2 || i == 4) ? 2 : 1) : i == 0 ? 3 : 4;
				shaders[i]            = Shaders::create(device, shader_code, SDL_GPU_SHADERSTAGE_FRAGMENT, samplers, 1);
				if (!shaders[i]) {
					return false;
				}
				SDL_GPUColorTargetDescription descriptions[4]{};
				const Uint32                  count = algorithm != Algorithm::ScaleFX && i == 0 ? 2 : 1;
				for (Uint32 k = 0; k < count; ++k) {
					descriptions[k].format = algorithm == Algorithm::ScaleFX
													 ? ((i == 0 || i == 1 || i == 3) ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT
																					 : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
											 : i == 0 ? SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT
													  : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				}
				SDL_GPUGraphicsPipelineCreateInfo pi{};
				pi.vertex_shader                         = vertex;
				pi.fragment_shader                       = shaders[i];
				pi.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
				pi.target_info.num_color_targets         = count;
				pi.target_info.color_target_descriptions = descriptions;
				pipelines[i]                             = SDL_CreateGPUGraphicsPipeline(device, &pi);
				if (!pipelines[i]) {
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
		explicit GPU(SDL_Renderer* r, Algorithm a = Algorithm::ScaleFX) : renderer(r), algorithm(a) {}

		Algorithm method() const {
			return algorithm;
		}

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
			return d && (SDL_GetGPUShaderFormats(d) & Shaders::native_format);
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
		bool update(SDL_Surface* input, SDL_Rect dirty, int f, SDL_Rect content = {0, 0, 0, 0}, SDL_Point origin = {-1, -1}) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			if (failed || !available(renderer)) {
				return false;
			}
			if (!input || input->format != SDL_PIXELFORMAT_INDEX8
				|| (algorithm == Algorithm::ScaleFX ? (f != 3 && f != 4 && f != 6) : (f < 2 || f > 4))) {
				return false;
			}

			if (origin.x < 0 || origin.y < 0) {
				origin = {dirty.x, dirty.y};
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
					// Pack damage at offset zero with a D3D12-compatible row
					// pitch. Palette/LUT uploads have 512-byte aligned offsets.
					// This avoids SDL's temporary upload repacking and preserves
					// partial texture updates on every backend.
					const size_t row_bytes = (size_t(input_area.w) * 4 + 255) & ~size_t(255);
					for (int row = 0; row < input_area.h; ++row) {
						SDL_memcpy(
								mapped + size_t(row) * row_bytes, upload.data() + size_t(input_area.y + row) * width + input_area.x,
								size_t(input_area.w) * 4);
					}
					if (changed && algorithm == Algorithm::ScaleFX) {
						auto* distances = mapped + palette_offset;
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

					if (algorithm != Algorithm::ScaleFX) {
						if (changed) {
							auto* dst = mapped + palette_offset;
							for (size_t i = 0; i < 256; ++i) {
								auto        c       = palette[i];
								const Uint8 bytes[] = {c.r, c.g, c.b, 255};
								SDL_memcpy(dst + i * 4, bytes, 4);
							}
						}
						if (!initialized) {
							SDL_memcpy(
									mapped + palette_offset + 256 * 256 * sizeof(float), Shaders::hq_lookup,
									sizeof(Shaders::hq_lookup));
						}
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
					transfer_info.offset          = 0;
					transfer_info.pixels_per_row  = Uint32(row_bytes / 4);
					transfer_info.rows_per_layer  = input_area.h;
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
						mt.offset          = Uint32(palette_offset);
						mt.pixels_per_row  = 256;
						mt.rows_per_layer  = algorithm == Algorithm::ScaleFX ? 256 : 1;
						SDL_GPUTextureRegion mr{};
						mr.texture = metrics;
						mr.w       = 256;
						mr.h       = algorithm == Algorithm::ScaleFX ? 256 : 1;
						mr.d       = 1;
						SDL_UploadToGPUTexture(copy, &mt, &mr, false);
					}
					if (algorithm != Algorithm::ScaleFX && !initialized) {
						SDL_GPUTextureTransferInfo lt{};
						lt.transfer_buffer = transfer;
						lt.offset          = Uint32(palette_offset + 256 * 256 * sizeof(float));
						lt.pixels_per_row  = 512;
						lt.rows_per_layer  = 384;
						SDL_GPUTextureRegion lr{};
						lr.texture = lookup;
						lr.w       = 512;
						lr.h       = 384;
						lr.d       = 1;
						SDL_UploadToGPUTexture(copy, &lt, &lr, false);
					}
					SDL_EndGPUCopyPass(copy);
					if (algorithm != Algorithm::ScaleFX) {
						if (content.w <= 0 || content.h <= 0) {
							content = {0, 0, width, height};
						}
						int params[]
								= {width, height, f, int(algorithm), width, content.y + content.h, origin.x, origin.y, 0, 0, 0, 0};
						// Two 128-bit colour targets stay within the 32-byte
						// attachment budget of older mobile Metal GPUs. Larger
						// patterns are emitted in two groups before resolving.
						const int groups = f == 2 ? 1 : 2;
						for (int draw = 0; draw <= groups; ++draw) {
							const int stage = draw < groups ? 0 : 1;
							params[8]       = draw;
							SDL_GPUColorTargetInfo colours[4]{};
							const int              count = stage == 0 ? 2 : 1;
							for (int k = 0; k < count; ++k) {
								colours[k].texture  = targets[stage == 0 ? draw * 2 + k : 4];
								colours[k].load_op  = initialized ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_DONT_CARE;
								colours[k].store_op = SDL_GPU_STOREOP_STORE;
							}
							auto* pass = SDL_BeginGPURenderPass(commands, colours, count, nullptr);
							if (!pass) {
								SDL_CancelGPUCommandBuffer(commands);
								return false;
							}
							SDL_BindGPUGraphicsPipeline(pass, pipelines[stage]);
							SDL_GPUTextureSamplerBinding bindings[4]{};
							if (stage == 0) {
								bindings[0] = {source, sampler};
								bindings[1] = {metrics, sampler};
								bindings[2] = {lookup, sampler};
							} else {
								for (int k = 0; k < 4; ++k) {
									bindings[k] = {targets[k], sampler};
								}
							}
							SDL_BindGPUFragmentSamplers(pass, 0, bindings, stage == 0 ? 3 : 4);
							SDL_PushGPUFragmentUniformData(commands, 0, params, sizeof params);
							const int       multiplier = stage == 0 ? 1 : f;
							SDL_GPUViewport viewport{0, 0, float(width * multiplier), float(height * multiplier), 0, 1};
							SDL_SetGPUViewport(pass, &viewport);
							SDL_Rect clip{dirty.x * multiplier, dirty.y * multiplier, dirty.w * multiplier, dirty.h * multiplier};
							if (full_analysis && stage == 0) {
								clip = {0, 0, width, height};
							}
							SDL_SetGPUScissor(pass, &clip);
							SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
							SDL_EndGPURenderPass(pass);
						}
						if (!SDL_SubmitGPUCommandBuffer(commands)) {
							return false;
						}
						initialized = true;
						return true;
					}
					const int params[] = {width, height, f, -1};
					for (int i = 0; i < 5; ++i) {
						const int halo = i == 0 ? 5 : i == 1 ? 4 : i == 2 ? 3 : 0;
						SDL_Rect  clip{std::max(0, dirty.x - halo), std::max(0, dirty.y - halo), 0, 0};
						clip.w = std::min(width, dirty.x + dirty.w + halo) - clip.x;
						clip.h = std::min(height, dirty.y + dirty.h + halo) - clip.y;
						if (full_analysis && i < 4) {
							clip = {0, 0, width, height};
						}
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
				SDL_SetError("GPU scaler upload allocation failed");
			}
			if (!ok) {
				failed = true;
				SDL_Log("GPU scaler unavailable; using CPU: %s", SDL_GetError());
				release();
				std::vector<uint32_t>().swap(upload);
			}
			return ok;
#else
			(void)input;
			(void)dirty;
			(void)f;
			(void)content;
			(void)origin;
			return false;
#endif
		}
	};
}    // namespace ScaleFX
#endif
