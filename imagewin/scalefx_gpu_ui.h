/* ScaleFX UI GPU consensus matting. GPL-2.0-or-later. */
#ifndef EXULT_SCALEFX_GPU_UI_H
#define EXULT_SCALEFX_GPU_UI_H
#include "scalefx_gpu.h"

#include <memory>

namespace ScaleFX {
	class UIGPU {
		SDL_Renderer* renderer;
#if SDL_VERSION_ATLEAST(3, 4, 0)
		SDL_GPUDevice*                          device = nullptr;
		std::array<std::unique_ptr<GPU>, 3>     colours;
		SDL_Surface*                            view    = nullptr;
		SDL_GPUTexture*                         palette = nullptr;
		SDL_GPUTexture*                         final   = nullptr;
		std::vector<SDL_GPUTexture*>            flags;
		SDL_GPUTransferBuffer*                  transfer = nullptr;
		SDL_GPUSampler*                         sampler  = nullptr;
		SDL_GPUShader*                          vertex   = nullptr;
		std::array<SDL_GPUShader*, 2>           shaders{};
		std::array<SDL_GPUGraphicsPipeline*, 2> pipelines{};
		SDL_Texture*                            result = nullptr;
		int                                     width = 0, height = 0, factor = 0;
		bool                                    failed = false, ready = false;
		std::vector<Uint8>                      previous_pixels;
		std::array<SDL_Color, 256>              previous_base{}, previous_original{};
		std::array<bool, 256>                   used{};
		int                                     previous_transparent = -1;
		SDL_Rect                                previous_content{};

		void release() {
			SDL_FlushRenderer(renderer);
			for (auto& c : colours) {
				c.reset();
			}
			if (view) {
				SDL_DestroySurface(view);
			}
			view = nullptr;
			if (result) {
				SDL_DestroyTexture(result);
			}
			result = nullptr;
			if (final) {
				SDL_ReleaseGPUTexture(device, final);
			}
			final = nullptr;
			if (palette) {
				SDL_ReleaseGPUTexture(device, palette);
			}
			palette = nullptr;
			for (auto* t : flags) {
				SDL_ReleaseGPUTexture(device, t);
			}
			flags.clear();
			for (auto*& p : pipelines) {
				if (p) {
					SDL_ReleaseGPUGraphicsPipeline(device, p);
				}
				p = nullptr;
			}
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
			sampler = nullptr;
			if (transfer) {
				SDL_ReleaseGPUTransferBuffer(device, transfer);
			}
			transfer = nullptr;
			ready    = false;
			previous_pixels.clear();
		}

		SDL_GPUTexture* texture(int w, int h) {
			SDL_GPUTextureCreateInfo t{};
			t.type                 = SDL_GPU_TEXTURETYPE_2D;
			t.format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			t.usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
			t.width                = w;
			t.height               = h;
			t.layer_count_or_depth = 1;
			t.num_levels           = 1;
			return SDL_CreateGPUTexture(device, &t);
		}

		bool prepare(SDL_Surface* source, int f) {
			if (result && width == source->w && height == source->h && factor == f) {
				view->pixels = source->pixels;
				view->pitch  = source->pitch;
				return true;
			}
			release();
			device = SDL_GetGPURendererDevice(renderer);
			width  = source->w;
			height = source->h;
			factor = f;
			view   = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_INDEX8, source->pixels, source->pitch);
			if (!view || !SDL_CreateSurfacePalette(view)) {
				return false;
			}
			for (auto& c : colours) {
				c = std::make_unique<GPU>(renderer);
			}
			palette = texture(256, 1);
			final   = texture(width * f, height * f);
			if (!palette || !final) {
				return false;
			}
			flags.reserve(32);
			int w = width * f, h = height * f;
			do {
				w       = (w + 1) / 2;
				h       = (h + 1) / 2;
				auto* t = texture(w, h);
				if (!t) {
					return false;
				}
				flags.push_back(t);
			} while (w > 1 || h > 1);
			SDL_GPUTransferBufferCreateInfo ti{};
			ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
			ti.size  = 256 * 4;
			transfer = SDL_CreateGPUTransferBuffer(device, &ti);
			if (!transfer) {
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
			const uint32_t* codes[] = {Shaders::passreduce, Shaders::passmatte};
			const size_t    sizes[] = {sizeof(Shaders::passreduce), sizeof(Shaders::passmatte)};
			for (int i = 0; i < 2; ++i) {
				SDL_GPUShaderCreateInfo info{};
				info.code                = reinterpret_cast<const Uint8*>(codes[i]);
				info.code_size           = sizes[i];
				info.entrypoint          = "main";
				info.format              = SDL_GPU_SHADERFORMAT_SPIRV;
				info.stage               = SDL_GPU_SHADERSTAGE_FRAGMENT;
				info.num_samplers        = i == 0 ? 2 : 6;
				info.num_uniform_buffers = 1;
				shaders[i]               = SDL_CreateGPUShader(device, &info);
				if (!shaders[i]) {
					return false;
				}
				SDL_GPUColorTargetDescription target{};
				target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				SDL_GPUGraphicsPipelineCreateInfo pi{};
				pi.vertex_shader                         = vertex;
				pi.fragment_shader                       = shaders[i];
				pi.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
				pi.target_info.num_color_targets         = 1;
				pi.target_info.color_target_descriptions = &target;
				pipelines[i]                             = SDL_CreateGPUGraphicsPipeline(device, &pi);
				if (!pipelines[i]) {
					return false;
				}
			}
			auto props = SDL_CreateProperties();
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA32);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width * f);
			SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height * f);
			SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, final);
			result = SDL_CreateTextureWithProperties(renderer, props);
			SDL_DestroyProperties(props);
			return result && SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);
		}

		bool draw(
				SDL_GPUCommandBuffer* commands, int pipeline, SDL_GPUTexture* target, int w, int h,
				const SDL_GPUTextureSamplerBinding* bindings, int count, const int* params, int bytes) {
			SDL_GPUColorTargetInfo info{};
			info.texture  = target;
			info.load_op  = SDL_GPU_LOADOP_DONT_CARE;
			info.store_op = SDL_GPU_STOREOP_STORE;
			auto* pass    = SDL_BeginGPURenderPass(commands, &info, 1, nullptr);
			if (!pass) {
				return false;
			}
			SDL_BindGPUGraphicsPipeline(pass, pipelines[pipeline]);
			SDL_BindGPUFragmentSamplers(pass, 0, bindings, count);
			SDL_PushGPUFragmentUniformData(commands, 0, params, bytes);
			SDL_GPUViewport viewport{0, 0, float(w), float(h), 0, 1};
			SDL_SetGPUViewport(pass, &viewport);
			SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
			SDL_EndGPURenderPass(pass);
			return true;
		}
#endif
	public:
		explicit UIGPU(SDL_Renderer* r) : renderer(r) {}

		~UIGPU() {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			release();
#endif
		}

		UIGPU(const UIGPU&)            = delete;
		UIGPU& operator=(const UIGPU&) = delete;

		SDL_Texture* output() const {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			return ready && !failed ? result : nullptr;
#else
			return nullptr;
#endif
		}

		// Base colours include gamma and overrides; original holds fallback RGB
		// and intrinsic alpha. Content excludes guard bands, matching CPU consensus.
		bool update(
				SDL_Surface* source, int f, int transparent, const std::array<SDL_Color, 256>& base,
				const std::array<SDL_Color, 256>& original, SDL_Rect content) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
			if (!source || source->format != SDL_PIXELFORMAT_INDEX8 || (f != 3 && f != 4 && f != 6) || transparent < 0
				|| transparent > 255) {
				return false;
			}
			if (failed || !GPU::available(renderer)) {
				return false;
			}
			bool ok = false;
			try {
				ok = [&] {
					if (!prepare(source, f)) {
						return false;
					}
					bool identical = ready && previous_transparent == transparent && SDL_RectsEqual(&previous_content, &content);
					if (identical) {
						for (int y = 0; y < height && identical; ++y) {
							identical = SDL_memcmp(
												previous_pixels.data() + size_t(y) * width,
												static_cast<const Uint8*>(source->pixels) + y * source->pitch, size_t(width))
										== 0;
						}
						for (size_t i = 0; i < 256 && identical; ++i) {
							if (used[i]) {
								const auto a = base[i], b = previous_base[i];
								const auto c = original[i], d = previous_original[i];
								identical = (i == size_t(transparent) || (a.r == b.r && a.g == b.g && a.b == b.b)) && c.r == d.r
											&& c.g == d.g && c.b == d.b && c.a == d.a;
							}
						}
					}
					if (identical) {
						return true;
					}
					previous_pixels.resize(size_t(width) * height);
					used.fill(false);
					for (int y = 0; y < height; ++y) {
						const auto* row = static_cast<const Uint8*>(source->pixels) + y * source->pitch;
						SDL_memcpy(previous_pixels.data() + size_t(y) * width, row, size_t(width));
						for (int x = 0; x < width; ++x) {
							used[row[x]] = true;
						}
					}
					previous_base        = base;
					previous_original    = original;
					previous_transparent = transparent;
					previous_content     = content;

					auto  fills = base;
					auto* pal   = SDL_GetSurfacePalette(view);
					for (int i = 0; i < 3; ++i) {
						fills[transparent] = {Uint8(i == 0 ? 255 : 0), Uint8(i == 1 ? 255 : 0), Uint8(i == 2 ? 255 : 0), 255};
						if (!SDL_SetPaletteColors(pal, fills.data(), 0, 256)
							|| !colours[i]->update(view, {0, 0, width, height}, f)) {
							return false;
						}
					}
					if (!SDL_FlushRenderer(renderer)) {
						return false;
					}
					auto* mapped = SDL_MapGPUTransferBuffer(device, transfer, true);
					if (!mapped) {
						return false;
					}
					SDL_memcpy(mapped, original.data(), 256 * 4);
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
					SDL_GPUTextureTransferInfo ti{};
					ti.transfer_buffer = transfer;
					ti.pixels_per_row  = 256;
					ti.rows_per_layer  = 1;
					SDL_GPUTextureRegion region{};
					region.texture = palette;
					region.w       = 256;
					region.h = region.d = 1;
					SDL_UploadToGPUTexture(copy, &ti, &region, false);
					SDL_EndGPUCopyPass(copy);
					int w = width * f, h = height * f;
					for (size_t i = 0; i < flags.size(); ++i) {
						w                                = (w + 1) / 2;
						h                                = (h + 1) / 2;
						SDL_GPUTextureSamplerBinding b[] = {
								{i ? flags[i - 1] : colours[0]->gpu_output(), sampler},
                                {                   colours[1]->gpu_output(), sampler}
                        };
						int params[] = {i ? 1 : 0, 0, 0, 0, content.x * f, content.y * f, content.w * f, content.h * f};
						if (!draw(commands, 0, flags[i], w, h, b, 2, params, sizeof params)) {
							SDL_CancelGPUCommandBuffer(commands);
							return false;
						}
					}
					SDL_GPUTextureSamplerBinding b[] = {
							{    colours[0]->gpu_output(), sampler},
							{    colours[1]->gpu_output(), sampler},
							{    colours[2]->gpu_output(), sampler},
							{colours[0]->indexed_source(), sampler},
							{                     palette, sampler},
							{                flags.back(), sampler}
                    };
					int params[] = {width, height, f, transparent};
					if (!draw(commands, 1, final, width * f, height * f, b, 6, params, sizeof params)) {
						SDL_CancelGPUCommandBuffer(commands);
						return false;
					}
					if (!SDL_SubmitGPUCommandBuffer(commands)) {
						return false;
					}
					ready = true;
					return true;
				}();
			} catch (const std::bad_alloc&) {
				SDL_SetError("ScaleFX UI GPU allocation failed");
			}
			if (!ok) {
				failed = true;
				SDL_Log("ScaleFX UI GPU unavailable; using CPU: %s", SDL_GetError());
				release();
			}
			return ok;
#else
			(void)source;
			(void)f;
			(void)transparent;
			(void)base;
			(void)original;
			(void)content;
			return false;
#endif
		}
	};
}    // namespace ScaleFX
#endif
