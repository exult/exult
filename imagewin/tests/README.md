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
