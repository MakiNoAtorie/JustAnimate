# JustAnimate

**JustAnimate** is a free and open source 2D animation/drawing program for Windows, macOS, and Linux. It lets you create traditional hand-drawn animation using both bitmap and vector graphics.

JustAnimate is a fork of [Pencil2D](https://www.pencil2d.org/) that focuses on better drawing tools, a smoother animation workflow, UI polish, and performance. It opens Pencil2D project files (`.pclx` / `.pcl`).

## Building

JustAnimate uses CMake and Qt 6.

```bash
# Debian/Ubuntu dependencies
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-multimedia-dev \
                 qt6-svg-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/justanimate          # run the app
./build/pencil2d_tests       # run the unit tests
```

Platform-specific notes are in [`docs/`](docs/) (`build_linux.md`, `build_mac.md`, `build_win.md`).

## Using JustAnimate

Help > Quick Reference Guide gives an overview of the tools. Because JustAnimate is based on Pencil2D, the [Pencil2D User Manual](https://www.pencil2d.org/doc/user-manual.html) and [tutorials](https://www.pencil2d.org/doc/tutorials.html) also apply.

## Reporting issues

Please report bugs and request features in the [JustAnimate issue tracker](https://github.com/makinoatorie/justanimate/issues), not the Pencil2D one, unless you can reproduce the problem in Pencil2D itself.

## Credits and license

JustAnimate is based on Pencil2D, originally developed by Pascal Naidon, Patrick Corrieri, and Matt Chang, and maintained by the Pencil2D contributors. Source: <https://github.com/pencil2d/pencil>.

JustAnimate is distributed under the [GNU General Public License, version 2](LICENSE.TXT), the same license as Pencil2D.
