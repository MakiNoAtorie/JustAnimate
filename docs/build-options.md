Build Options {#build_options}
=============

## Commonly Used Options

These are options you might want to pass to CMake when configuring your build (`cmake -S . -B build -D<name>=<value>`).

- `-G Ninja`: Select the build generator
- `CMAKE_BUILD_TYPE=Debug` / `Release`: Development or optimised build (single-config generators)
- `CMAKE_PREFIX_PATH=<Qt dir>`: Where to find Qt 6 if it is not on the default search path
- `APP_VERSION=x.y.z.w`: Version string compiled into the app (default `0.0.0.0`, a development build)
- `JUSTANIMATE_FFMPEG=<path>`: An ffmpeg executable to bundle into the install and packages (optional)

## Targets

- `pencil2d`: The application
- `pencil2d_tests`: The unit tests
