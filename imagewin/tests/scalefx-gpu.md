# ScaleFX GPU acceleration on Linux

Build with **SDL 3.4 or newer** and a working Vulkan driver. Select ScaleFX and
3x, 4x or 6x normally. GPU acceleration defaults to `auto`; no shader compiler
is needed to build or run Exult because the validated SPIR-V is embedded.

```xml
<video>
  <scale_method>ScaleFX</scale_method>
  <scale>6</scale>
  <scalefx_gpu>auto</scalefx_gpu>
</video>
```

Use `<scalefx_gpu>no</scalefx_gpu>` to force CPU scaling, then restart Exult.
An explicit non-GPU `SDL_RENDER_DRIVER` override is respected. Console messages
identify the selected device and report `GPU shaders active at 3x/4x/6x` after
a successful first frame. SDL 3.2 builds remain supported and report that GPU
acceleration requires SDL 3.4. They continue using the CPU implementation.

## Rendering and fallback

The world image, opaque scenes, and transparent/translucent UI layers use GPU
ScaleFX, including the mouse cursor with universal UI settings. UI uses the same
red/green/blue consensus-difference matting and integer alpha reconstruction as
the CPU implementation. A GPU reduction determines whether the CPU algorithm
would request the blue pass; no scaled pixels or reduction flags are read back.
The blue scaling pass is submitted speculatively to avoid a CPU/GPU round trip.
Palette overrides, gamma, intrinsic translucency, layer opacity and composition
ordering are preserved. Other scalers retain their existing rendering paths.

UI input and used palette entries are cached, so unrelated palette animation
and unchanged layer refreshes reuse the existing GPU texture. Empty transparent
layers are skipped before allocating scaling textures. Palette distance tables
recalculate only pairs affected by changed colours. Resources are retained
between updates and released before their renderer. Shader/resource failure
returns the affected layer to its existing CPU matting implementation.

SDL_GPU runs the five source-metric, strength, junction, selector and output
passes. Intermediate metrics and selectors use full 32-bit float attachments;
half-float targets can change edge decisions and are not used. Palette-pair
colour distances are computed on the CPU only when RGB palette values change,
then uploaded once. This avoids GPU-vendor sqrt differences. Embedded SPIR-V
also prohibits floating-point contraction in the edge arithmetic. 3x retains
the original selectors; 4x/6x use the same native contour geometry and tie rules
as the CPU implementation.

The final GPU texture is imported into the **same SDL renderer** through SDL
3.4's GPU texture property. There is no scaled-frame CPU readback or CPU pixel
output in the normal GPU path. Existing world caching, fill filtering, overlay
composition, final presentation and screenshots remain in SDL_Renderer. Readback
occurs only when a screenshot or test explicitly requests it.

Textures, shaders, pipelines and staging storage are retained between updates.
Dirty source regions refresh their input halo, including changing guard bands.
Each shader pass uses the required progressively smaller scissor rectangle,
with final writes restricted to output damage. A palette change refreshes the
whole source and output. Resize/factor changes rebuild resources, and resources
are released before their renderer/device is destroyed.

If GPU renderer creation is unavailable, Exult creates its regular renderer.
If shader/resource setup or submission fails, the GPU scaler is disabled for
that renderer and the CPU path repaints the **whole** staging image before
resuming partial updates. This prevents old GPU frames from leaving stale CPU
fill surfaces. Software/other SDL renderers also use CPU scaling. GPU resources
are released on failure, resize and shutdown.

This implementation targets Vulkan/SPIR-V, tested on Linux. Metal/DXIL shader
variants are not provided; unsupported backends fall back to CPU. The previous
SDL 3.2 investigation was accurate for that API, but SDL 3.4 adds the missing
GPU device and external-texture integration needed by this implementation.

## Verification

From the repository root, using the desired SDL installation:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. imagewin/tests/test_scalefx.cc -o /tmp/test-scalefx
/tmp/test-scalefx
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. imagewin/tests/test_scalefx_gpu.cc $(pkg-config --cflags --libs sdl3) -o /tmp/test-scalefx-gpu
SDL_VIDEODRIVER=offscreen /tmp/test-scalefx-gpu
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. imagewin/tests/test_scalefx_gpu_ui.cc $(pkg-config --cflags --libs sdl3) -o /tmp/test-scalefx-gpu-ui
SDL_VIDEODRIVER=offscreen /tmp/test-scalefx-gpu-ui
# Optional CPU versus GPU completed-latency measurements:
SDL_VIDEODRIVER=offscreen /tmp/test-scalefx-gpu --benchmark
SDL_VIDEODRIVER=offscreen /tmp/test-scalefx-gpu-ui --benchmark
```

The GPU test checks all factors on tiny/padded images, random 256-colour images,
four-colour slopes and diagonals; full, dirty and boundary updates; visible
palette changes; resize/factor changes; software-renderer fallback; and shared
texture composition across successive SDL/GPU submissions. GPU output must
match the CPU pixel-for-pixel. A valid offscreen render target is used, as
required by SDL's windowless GPU renderer. Exit 77 means the GPU test did not
run because SDL/GPU support was unavailable; it is not a passing test.

The UI test compares CPU and GPU RGBA output at all factors, including fully
transparent layers, saturated fill colours, diagonal outlines, random colours,
palette changes, intrinsic alpha, alpha-only changes, cached updates, software-renderer fallback and resized
resources. UI activation is logged as `ScaleFX: GPU UI shaders active at 4x`.

Shader regeneration is a development-only step:

```sh
python3 imagewin/shaders/compile_scalefx.py
```

It requires `glslangValidator`; `spirv-val`, when available, also validates the
actual decorated modules embedded in the generated header. Shader sources and
the original ScaleFX MIT notice are distributed alongside the embedded code.
Linux CI builds SDL 3.4 and runs both CPU and Vulkan conformance tests using Mesa.

## Local test limits

The cloud host has Mesa **lavapipe**, a CPU-emulated Vulkan device, not a physical
GPU. It successfully ran the shader conformance suite and actual Exult Vulkan
window presentation at all three factors, plus the explicit CPU setting. An
SDL 3.2 application build also passed. Licensed game data is absent, so these
are renderer and algorithm checks, not a full gameplay test.

The optional benchmark includes upload, submission and a GPU-idle wait, so it
measures completed latency rather than just asynchronous submission time.
Warm-up excludes initial pipeline compilation. Software Vulkan timings must
not be interpreted as a hardware speedup; it may be slower than the CPU scaler.
Run the same benchmark and compare in-game frame times on the target GPU. If
SDL selects a software Vulkan adapter, forcing CPU is generally preferable.

For reference, one idle-host run with GCC 14.2 `-O2` on a 320x200 striped
scene gave these median completed times. **This is CPU-emulated Vulkan**,
not a measurement of physical GPU acceleration:

| Factor | CPU (ms) | Software Vulkan full (ms) | Software Vulkan 16x16 dirty (ms) |
| --- | ---: | ---: | ---: |
| 3x | 6.14 | 10.87 | 1.03 |
| 4x | 7.94 | 12.09 | 1.06 |
| 6x | 12.11 | 23.13 | 1.24 |

The host also passed address/undefined-behaviour sanitizer checks for the GPU
upload/resource path and all GPU/CPU conformance cases. Window presentation was
checked through a private X server. Hardware FPS and full gameplay remain for
testing on a Linux machine with a physical Vulkan GPU and game data.

The UI benchmark compares three CPU colour-scaling passes (a lower bound that
excludes CPU matting) against completed GPU scaling plus matting on a 420x263
transparent UI. It separately measures cached updates. All buffers/pipelines
are warmed before timing. These are standalone costs, not in-game FPS; use a
physical GPU to assess the acceleration on the target machine.
