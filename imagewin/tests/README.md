# ScaleFX tests

From the repository root, with assertions enabled:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. imagewin/tests/test_scalefx.cc -o /tmp/test-scalefx
/tmp/test-scalefx
```

The suite checks the production scaler against independent shader output, with
padded input/output pitches, 32-bit and 16-bit RGB/BGR output tables, tiny images,
tiled partial updates, damage expansion, and palette changes. It also checks that
partial updates leave other pixels and padding untouched. It needs no game data,
SDL, display, or OpenGL. AddressSanitizer and UndefinedBehaviorSanitizer may be
added to the compiler command for bounds and arithmetic checks.

`slopes-32-24.txt` and `noise-17-19.txt` contain width/height, four RGB colours,
source rows of palette indices, and the expected 3x output rows. Expected output
was rendered with the unmodified five-pass `scalefx/scalefx.glslp` preset in
[libretro/glsl-shaders](https://github.com/libretro/glsl-shaders/tree/1aaabbebdd88a8a89a82edfdc94e10580c99ffdd/scalefx),
revision `1aaabbebdd88a8a89a82edfdc94e10580c99ffdd`, using Mesa llvmpipe.
Passes 0/1 use floating-point framebuffers; passes 2/3 use RGBA8, all textures
use nearest sampling and clamp-to-edge, with default shader parameters
(`SFX_CLR=0.5`, `SFX_SAA=1`, `SFX_SCN=1`).

The slopes source is `(x / (y / 4 + 1)) % 4` with integer division; noise uses
four colours. Input and expected output are stored together so the tests do not
need a random-number generator or network downloads. The CPU implementation
also matched the upstream shader on 24 diagonal, slope, and noise images
from 1x1 through 32x24 during development, using both a four-colour palette
and a randomized 256-colour palette.

## Native factors and performance

The suite also validates native 4x/6x patterns, an independent straight-diagonal
oracle (including reflections/rotations), all factors with dirty/tiled updates,
palette invalidation/restoration, and retained workspace storage.
[Implementation and GPU investigation](scalefx-notes.md) describes the geometry,
compatibility guarantees, renderer constraints and performance methodology.

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I. imagewin/tests/benchmark_scalefx.cc -o /tmp/benchmark-scalefx
/tmp/benchmark-scalefx /tmp/scalefx > /tmp/scalefx-performance.csv
# Optional visual report; Pillow is the only extra dependency:
python3 imagewin/tests/compare_scalefx.py /tmp/scalefx /tmp/scalefx-comparison.png
```

To compare against the previous implementation at commit `b6ccac7ea`, extract
that header outside the checkout, rename its namespace and include guard, and
pass its absolute path as `SCALEFX_BASELINE_HEADER` to both binaries. For example:

```sh
git show b6ccac7ea:imagewin/scale_scalefx.h > /tmp/scalefx-old.h
python3 - <<'PY'
from pathlib import Path
p = Path('/tmp/scalefx-old.h')
s = p.read_text().replace('ScaleFX', 'BaselineScaleFX')
s = s.replace('EXULT_SCALE_SCALEFX_H', 'EXULT_SCALE_BASELINE_SCALEFX_H')
p.write_text(s)
PY
c++ -std=c++17 -O2 -I. -DSCALEFX_BASELINE_HEADER='"/tmp/scalefx-old.h"' imagewin/tests/test_scalefx.cc -o /tmp/test-scalefx
/tmp/test-scalefx
c++ -std=c++17 -O2 -I. -DSCALEFX_BASELINE_HEADER='"/tmp/scalefx-old.h"' imagewin/tests/benchmark_scalefx.cc -o /tmp/benchmark-scalefx
/tmp/benchmark-scalefx /tmp/scalefx
```

The optional baseline tests compare old/new 3x output on random 256-colour
palettes, source pitches, dimensions and palette edits, in addition to the
committed upstream shader fixtures. Sanitizer builds use the same source with
`-fsanitize=address,undefined -fno-omit-frame-pointer -g` (keep assertions on).

## Optional GPU path

[GPU setup and validation](scalefx-gpu.md) describes the SDL 3.4/Vulkan path,
CPU fallback, GPU/CPU world and transparent UI pixel conformance tests, shader regeneration, and completed
GPU latency benchmark. Older SDL builds keep the CPU implementation.
