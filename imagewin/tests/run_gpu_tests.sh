#!/bin/sh
# Standalone CPU/GPU conformance tests. No game data is required.
set -eu
cd "$(dirname "$0")/../.."
CXX=${CXX:-c++}
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
sdl_flags=$(pkg-config --cflags --libs sdl3)
"$CXX" -std=c++17 -O2 -I. imagewin/tests/test_scalefx.cc -o "$test_dir/cpu"
if ! "$test_dir/cpu"; then
    echo "::error title=CPU scaler fixtures::ScaleFX CPU regression failed on $(uname -m)"
    exit 1
fi
"$CXX" -std=c++17 -O2 -I. -Iheaders imagewin/tests/test_gpu_fallback.cc $sdl_flags -o "$test_dir/fallback"
SDL_VIDEODRIVER=dummy "$test_dir/fallback"
if [ "${1:-}" = --fallback-only ]; then exit 0; fi
if ! pkg-config --atleast-version=3.4 sdl3; then
    echo "SKIP: native GPU tests require SDL 3.4+; CPU fallback passed"
    exit 0
fi
for factor in 2 3 4; do
    "$CXX" -std=c++17 -O2 -I. -Iheaders $(pkg-config --cflags sdl3) -DTEST_HQ_FACTOR="$factor" \
        -c imagewin/tests/hq_reference.cc -o "$test_dir/hq$factor.o"
done
references="imagewin/tests/scaler_reference.cc $test_dir/hq2.o $test_dir/hq3.o $test_dir/hq4.o"
for test in test_scalefx_gpu test_scalefx_gpu_ui test_gpu_scalers; do
    extra=
    if [ "$test" != test_scalefx_gpu ]; then extra=$references; fi
    "$CXX" -std=c++17 -O2 -I. -Iheaders "imagewin/tests/$test.cc" $extra $sdl_flags -o "$test_dir/$test"
    status=0
    "$test_dir/$test" || status=$?
    if [ "$status" = 77 ] && [ "${1:-}" = --allow-no-gpu ]; then
        echo "SKIP: $test: no native GPU device on this runner"
    elif [ "$status" != 0 ]; then
        exit "$status"
    fi
done
