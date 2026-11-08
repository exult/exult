// Standalone SDL 3.4 GPU/CPU conformance test. No game assets or window required.
#include "imagewin/scale_scalefx.h"
#include "imagewin/scalefx_gpu.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#if SDL_VERSION_ATLEAST(3, 4, 0)
void require(bool ok, const char* what) {
	if (!ok) {
		throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
	}
}

namespace ScaleFX {
	struct GPUProbe {
		static void diagnose(GPU& gpu, const Workspace& cpu, SDL_Surface* source) {
			// Inspect the inputs independently of shader arithmetic. Use aligned
			// rows so this diagnostic also works with D3D12 texture readback.
			for (int input = 0; input < 2; ++input) {
				const int                       w = input ? 256 : source->w, h = input ? 256 : source->h;
				const int                       stride = (w + 63) & ~63;
				SDL_GPUTransferBufferCreateInfo info{};
				info.usage   = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
				info.size    = Uint32(stride * h * 4);
				auto* buffer = SDL_CreateGPUTransferBuffer(gpu.device, &info);
				require(buffer != nullptr, "input diagnostic transfer");
				auto* commands = SDL_AcquireGPUCommandBuffer(gpu.device);
				require(commands != nullptr, "input diagnostic commands");
				auto* pass = SDL_BeginGPUCopyPass(commands);
				require(pass != nullptr, "input diagnostic copy");
				SDL_GPUTextureRegion region{};
				region.texture = input ? gpu.metrics : gpu.source;
				region.w       = w;
				region.h       = h;
				region.d       = 1;
				SDL_GPUTextureTransferInfo transfer{};
				transfer.transfer_buffer = buffer;
				transfer.pixels_per_row  = stride;
				transfer.rows_per_layer  = h;
				SDL_DownloadFromGPUTexture(pass, &region, &transfer);
				SDL_EndGPUCopyPass(pass);
				require(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(gpu.device), "input diagnostic readback");
				const auto* values = static_cast<const Uint8*>(SDL_MapGPUTransferBuffer(gpu.device, buffer, false));
				require(values != nullptr, "input diagnostic map");
				size_t differences = 0;
				for (int y = 0; y < h; ++y) {
					for (int x = 0; x < w; ++x) {
						Uint32 expected, actual;
						if (input) {
							SDL_memcpy(&expected, &gpu.distances[y * 256 + x], 4);
						} else {
							const Uint8 index   = static_cast<const Uint8*>(source->pixels)[y * source->pitch + x];
							const auto  c       = SDL_GetSurfacePalette(source)->colors[index];
							const Uint8 bytes[] = {c.r, c.g, c.b, index};
							SDL_memcpy(&expected, bytes, 4);
						}
						SDL_memcpy(&actual, values + (y * stride + x) * 4, 4);
						if (actual != expected) {
							if (!differences) {
								std::cerr << "::error title=ScaleFX GPU input::input=" << input << " xy=" << x << ',' << y
										  << " GPU=" << std::hex << actual << " CPU=" << expected << std::dec << '\n';
							}
							++differences;
						}
					}
				}
				std::cerr << "ScaleFX input " << input << " differing pixels: " << differences << '\n';
				SDL_UnmapGPUTransferBuffer(gpu.device, buffer);
				SDL_ReleaseGPUTransferBuffer(gpu.device, buffer);
			}
			for (int stage = 0; stage < 2; ++stage) {
				SDL_GPUTransferBufferCreateInfo info{};
				info.usage   = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
				info.size    = Uint32(size_t(source->w) * source->h * 16);
				auto* buffer = SDL_CreateGPUTransferBuffer(gpu.device, &info);
				require(buffer != nullptr, "diagnostic transfer");
				auto* commands = SDL_AcquireGPUCommandBuffer(gpu.device);
				require(commands != nullptr, "diagnostic commands");
				auto* pass = SDL_BeginGPUCopyPass(commands);
				require(pass != nullptr, "diagnostic copy");
				SDL_GPUTextureRegion region{};
				region.texture = gpu.targets[stage];
				region.w       = source->w;
				region.h       = source->h;
				region.d       = 1;
				SDL_GPUTextureTransferInfo transfer{};
				transfer.transfer_buffer = buffer;
				transfer.pixels_per_row  = source->w;
				transfer.rows_per_layer  = source->h;
				SDL_DownloadFromGPUTexture(pass, &region, &transfer);
				SDL_EndGPUCopyPass(pass);
				require(SDL_SubmitGPUCommandBuffer(commands) && SDL_WaitForGPUIdle(gpu.device), "diagnostic readback");
				auto* values = static_cast<const float*>(SDL_MapGPUTransferBuffer(gpu.device, buffer, false));
				require(values != nullptr, "diagnostic map");
				size_t count = 0;
				for (int y = 0; y < source->h; ++y) {
					for (int x = 0; x < source->w; ++x) {
						for (int channel = 0; channel < 4; ++channel) {
							const auto expected = (stage == 0 ? cpu.metric.get(x, y) : cpu.strengths.get(x, y))[channel];
							const auto actual   = values[(y * source->w + x) * 4 + channel];
							if (actual != expected) {
								if (!count) {
									std::cerr << "::error title=ScaleFX intermediate::stage=" << stage << " xy=" << x << "," << y
											  << " channel=" << channel << " GPU=" << std::hexfloat << actual << " CPU=" << expected
											  << std::defaultfloat << "\n";
								}
								++count;
							}
						}
					}
				}
				std::cerr << "ScaleFX stage " << stage << " differing components: " << count << "\n";
				SDL_UnmapGPUTransferBuffer(gpu.device, buffer);
				SDL_ReleaseGPUTransferBuffer(gpu.device, buffer);
			}
		}
	};
}    // namespace ScaleFX

static const char* update_kind  = "full";
static int         test_pattern = 0;

void compare(SDL_Renderer* renderer, ScaleFX::GPU& gpu, SDL_Surface* source, int f, SDL_Texture* cached = nullptr) {
	auto*                         palette = SDL_GetSurfacePalette(source);
	std::array<ScaleFX::RGB, 256> rgb;
	std::array<Uint32, 256>       colours;
	auto*                         format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);
	for (size_t i = 0; i < 256; ++i) {
		auto c     = palette->colors[i];
		rgb[i]     = {c.r, c.g, c.b};
		colours[i] = SDL_MapRGBA(format, nullptr, c.r, c.g, c.b, 255);
	}
	std::vector<Uint32> cpu(size_t(source->w * source->h * f * f));
	ScaleFX::Workspace  workspace;
	ScaleFX::scale(
			static_cast<const Uint8*>(source->pixels), source->pitch, source->w, source->h, 0, 0, source->w, source->h, rgb,
			cpu.data(), source->w * f, colours, f, workspace);
	auto* saved_target = SDL_GetRenderTarget(renderer);
	require(SDL_SetRenderTarget(renderer, cached ? cached : gpu.output()), "read target");
	auto* pixels = SDL_RenderReadPixels(renderer, nullptr);
	require(pixels != nullptr, "read pixels");
	auto* rgba = SDL_ConvertSurface(pixels, SDL_PIXELFORMAT_RGBA32);
	require(rgba != nullptr, "convert pixels");
	size_t differences = 0;
	for (int y = 0; y < rgba->h; ++y) {
		for (int x = 0; x < rgba->w; ++x) {
			Uint32 actual;
			SDL_memcpy(&actual, static_cast<const Uint8*>(rgba->pixels) + y * rgba->pitch + x * 4, 4);
			if (actual != cpu[size_t(y * rgba->w + x)]) {
				if (differences == 0) {
					std::cerr << "::error title=ScaleFX comparison::source=" << source->w << "x" << source->h
							  << " pattern=" << test_pattern << " update=" << update_kind << " " << f << "x first difference " << x
							  << ',' << y << " GPU " << std::hex << actual << " CPU " << cpu[size_t(y * rgba->w + x)] << std::dec
							  << '\n';
				}
				++differences;
			}
		}
	}
	SDL_DestroySurface(rgba);
	SDL_DestroySurface(pixels);
	SDL_SetRenderTarget(renderer, saved_target);
	if (differences) {
		ScaleFX::GPUProbe::diagnose(gpu, workspace, source);
		throw std::runtime_error(std::to_string(differences) + " GPU/CPU differences");
	}
}

int main(int argc, char**) {
	try {
		require(SDL_Init(SDL_INIT_VIDEO), "SDL init");
		auto* renderer = SDL_CreateGPURenderer(nullptr, nullptr);
		if (!renderer) {
			std::cerr << "GPU unavailable: " << SDL_GetError() << '\n';
			return 77;
		}
		std::cerr << "GPU test backend=" << SDL_GetGPUDeviceDriver(SDL_GetGPURendererDevice(renderer))
				  << " platform=" << SDL_GetPlatform() << "\n";
		// SDL's windowless GPU renderer requires a valid target even when
		// flushing state commands; there is no swapchain/backbuffer.
		auto* anchor = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1);
		require(anchor != nullptr && SDL_SetRenderTarget(renderer, anchor), "offscreen target");
		Uint32 random = 12345;
		auto   next   = [&] {
			random = 1664525u * random + 1013904223u;
			return Uint8(random >> 24);
		};
		{
			ScaleFX::GPU gpu(renderer);
			for (int pattern = 0; pattern < 3; ++pattern) {
				test_pattern = pattern;
				for (int size : {1, 7, 32, 64}) {
					for (int factor : {3, 4, 6}) {
						auto* source = SDL_CreateSurface(size, size + 2, SDL_PIXELFORMAT_INDEX8);
						require(source != nullptr, "source");
						auto* pal = SDL_CreateSurfacePalette(source);
						require(pal != nullptr, "palette");
						std::array<SDL_Color, 256> colours;
						for (auto& c : colours) {
							c = {next(), next(), next(), 255};
						}
						if (pattern) {
							colours[0] = {0, 0, 0, 255};
							colours[1] = {255, 255, 255, 255};
							colours[2] = {255, 40, 40, 255};
							colours[3] = {20, 110, 240, 255};
						}
						require(SDL_SetPaletteColors(pal, colours.data(), 0, 256), "set palette");
						for (int y = 0; y < source->h; ++y) {
							for (int x = 0; x < source->w; ++x) {
								static_cast<Uint8*>(source->pixels)[y * source->pitch + x] = pattern == 1
																									 ? Uint8((x / (y / 4 + 1)) % 4)
																							 : pattern == 2 ? Uint8(x >= y)
																											: next();
							}
						}
						update_kind = "full";
						require(gpu.update(source, {0, 0, source->w, source->h}, factor), "GPU full update");
						compare(renderer, gpu, source, factor);
						if (pattern == 0 && size == 32 && factor == 6) {
							// Check zero-copy texture sharing and submission ordering:
							// the cached SDL frame must survive a later GPU update.
							auto* snapshot = SDL_DuplicateSurface(source);
							require(snapshot != nullptr, "snapshot");
							auto* cache = SDL_CreateTexture(
									renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, size * factor, source->h * factor);
							require(cache != nullptr, "SDL cache target");
							auto* saved = SDL_GetRenderTarget(renderer);
							require(SDL_SetRenderTarget(renderer, cache)
											&& SDL_RenderTexture(renderer, gpu.output(), nullptr, nullptr)
											&& SDL_RenderPresent(renderer),
									"SDL GPU composition");
							static_cast<Uint8*>(source->pixels)[0] ^= 7;
							require(gpu.update(source, {0, 0, size, source->h}, factor), "next GPU frame");
							compare(renderer, gpu, snapshot, factor, cache);
							require(SDL_RenderTexture(renderer, gpu.output(), nullptr, nullptr) && SDL_RenderPresent(renderer),
									"next SDL frame");
							compare(renderer, gpu, source, factor, cache);
							SDL_SetRenderTarget(renderer, saved);
							SDL_DestroyTexture(cache);
							SDL_DestroySurface(snapshot);
						}
						int x = size / 2, y = source->h / 2;
						static_cast<Uint8*>(source->pixels)[y * source->pitch + x] ^= 3;
						SDL_Rect dirty{std::max(0, x - 6), std::max(0, y - 6), 0, 0};
						dirty.w     = std::min(source->w, x + 7) - dirty.x;
						dirty.h     = std::min(source->h, y + 7) - dirty.y;
						update_kind = "dirty";
						require(gpu.update(source, dirty, factor), "GPU dirty update");
						compare(renderer, gpu, source, factor);
						for (const auto edge : {
									 std::array<int, 2>{       0,             0},
                                      {size - 1, source->h - 1},
                                      {       0,             y},
                                      {       x,             0}
                        }) {
							static_cast<Uint8*>(source->pixels)[edge[1] * source->pitch + edge[0]] ^= 1;
							SDL_Rect region{std::max(0, edge[0] - 6), std::max(0, edge[1] - 6), 0, 0};
							region.w    = std::min(size, edge[0] + 7) - region.x;
							region.h    = std::min(source->h, edge[1] + 7) - region.y;
							update_kind = "boundary";
							require(gpu.update(source, region, factor), "GPU boundary update");
							compare(renderer, gpu, source, factor);
						}
						const Uint8 changed = static_cast<const Uint8*>(source->pixels)[0];
						colours[changed].r ^= 127;
						require(SDL_SetPaletteColors(pal, colours.data(), 0, 256), "palette change");
						update_kind = "palette";
						require(gpu.update(source, {x, y, 1, 1}, factor), "GPU palette update");
						compare(renderer, gpu, source, factor);
						SDL_DestroySurface(source);
					}
				}
			}
		}
		if (argc > 1) {
			auto* source = SDL_CreateSurface(320, 200, SDL_PIXELFORMAT_INDEX8);
			require(source != nullptr, "benchmark source");
			auto*                      pal = SDL_CreateSurfacePalette(source);
			std::array<SDL_Color, 256> p{};
			p[1] = {240, 240, 225, 255};
			p[2] = {235, 75, 35, 255};
			p[3] = {45, 145, 230, 255};
			SDL_SetPaletteColors(pal, p.data(), 0, 256);
			for (int y = 0; y < 200; ++y) {
				for (int x = 0; x < 320; ++x) {
					static_cast<Uint8*>(source->pixels)[y * source->pitch + x] = Uint8(((x - y + 400) / 8) % 4);
				}
			}
			std::array<ScaleFX::RGB, 256> rgb;
			std::array<Uint32, 256>       colours;
			for (size_t i = 0; i < 256; ++i) {
				rgb[i]     = {p[i].r, p[i].g, p[i].b};
				colours[i] = SDL_MapRGBA(SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32), nullptr, p[i].r, p[i].g, p[i].b, 255);
			}
			auto time = [](auto call) {
				call();
				std::array<double, 9> samples;
				for (auto& v : samples) {
					auto begin = std::chrono::steady_clock::now();
					for (int i = 0; i < 3; ++i) {
						call();
					}
					v = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count() / 3;
				}
				std::sort(samples.begin(), samples.end());
				return samples[4];
			};
			std::cout << "factor,cpu_ms,gpu_completed_ms,gpu_dirty_completed_ms\n";
			{
				ScaleFX::GPU       gpu(renderer);
				ScaleFX::Workspace workspace;
				for (int f : {3, 4, 6}) {
					std::vector<Uint32> dst(size_t(320 * 200 * f * f));
					auto                cpu = time([&] {
						ScaleFX::scale(
								static_cast<const Uint8*>(source->pixels), source->pitch, 320, 200, 0, 0, 320, 200, rgb, dst.data(),
								320 * f, colours, f, workspace);
					});
					auto                run = [&](SDL_Rect rect) {
						require(gpu.update(source, rect, f), "benchmark GPU");
						require(SDL_WaitForGPUIdle(SDL_GetGPURendererDevice(renderer)), "GPU wait");
					};
					auto full  = time([&] {
						run({0, 0, 320, 200});
					});
					auto dirty = time([&] {
						run({152, 92, 16, 16});
					});
					std::cout << f << ',' << cpu << ',' << full << ',' << dirty << '\n';
				}
			}
			SDL_DestroySurface(source);
		}
		SDL_DestroyRenderer(renderer);
		auto* surface  = SDL_CreateSurface(17, 19, SDL_PIXELFORMAT_RGBA32);
		auto* software = SDL_CreateSoftwareRenderer(surface);
		require(software != nullptr, "software renderer");
		{
			ScaleFX::GPU fallback(software);
			require(!ScaleFX::GPU::available(software), "software fallback available");
			require(!fallback.update(surface, {0, 0, 17, 19}, 3) && fallback.output() == nullptr, "software CPU fallback");
		}
		SDL_DestroyRenderer(software);
		SDL_DestroySurface(surface);
		SDL_Quit();
		std::cout << "GPU/CPU 3x/4x/6x, resize, dirty rectangles and palette animation agree\n";
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
#else
int main() {
	std::cerr << "GPU test requires SDL 3.4+\n";
	return 77;
}
#endif
