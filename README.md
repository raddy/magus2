# magus2

Lean CMake scaffold optimized for aggressive performance builds with a simple test mode.

## Build (perf)

```sh
cmake --preset perf
cmake --build --preset perf
./build/perf/apps/magus2_hello
```

If `mold` is not installed, disable it with:

```sh
cmake --preset perf -DMAGUS2_USE_MOLD=OFF
```

If `ccache` is not installed, either install it or disable it with:

```sh
cmake --preset perf -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER=
```

## Build + test

```sh
cmake --preset tests
cmake --build --preset tests
ctest --preset tests
```
