# MobilePhotoViewer

A Qt 6 desktop viewer for **motion photos** — the still-plus-embedded-video format
produced by modern phones. It displays the still image and plays the embedded
clip on demand, for both:

- **JPEG** motion photos (Google/Samsung `MicroVideo` and the newer `MotionPhoto`
  container format)
- **HEIC / HEIF** motion photos (decoded via libheif + libde265)

Drag-and-drop a file to open it, use the mouse wheel to move through the folder,
and click the play button to play the embedded video.

## Prerequisites

- **Qt 6.7.0** (MSVC 2019 64-bit build) — e.g. `C:\Qt\6.7.0\msvc2019_64`
- **Visual Studio 2022** (MSVC toolchain) or the Build Tools
- **CMake ≥ 3.21** and **Ninja** — both ship with Qt (`C:\Qt\Tools\CMake_64`,
  `C:\Qt\Tools\Ninja`)
- **Git** (the image codecs are pulled in as submodules)

## Dependencies

libheif and libde265 are bundled as git submodules under `third_party/` and are
built automatically as part of the project — there is nothing to build or install
separately. `tinyexif` is vendored directly in the source tree.

## Getting the source

The submodules must be present before building:

```bash
# fresh clone
git clone --recurse-submodules <repo-url>

# or, in an existing clone
git submodule update --init --recursive
```

## Building

### Option A — Qt Creator (recommended)

1. **File → Open File or Project…** and select `CMakeLists.txt` (not the legacy
   `.pro` file).
2. Select the **Qt 6.7.0 MSVC2019 64-bit** kit and a build type (**Release** or
   **Debug**), then **Configure Project**.
3. Build with **Ctrl+B** and run with **Ctrl+R**.

Qt Creator provides its own CMake and Ninja, so no extra tooling is needed.

### Option B — Command line

From a shell with the MSVC environment loaded (run `vcvars64.bat`, or use the
*x64 Native Tools Command Prompt for VS 2022*):

```bat
set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.7.0\msvc2019_64\bin;%PATH%

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2019_64

cmake --build build
```

The first configure compiles libde265 and libheif once; later rebuilds only
recompile the application.

## Output

The build produces `build/MobilePhotoViewer.exe` with every runtime dependency
copied next to it — `heif.dll`, `libde265.dll`, the Qt DLLs, and the Qt
multimedia (FFmpeg) plugins — so the executable runs in place with no extra setup.

You can pass an image path as the first argument to open it directly:

```bat
build\MobilePhotoViewer.exe "D:\path\to\photo.jpg"
```

## Notes

- Use `CMakeLists.txt`; the old qmake `MobilePhotoViewer.pro` is superseded.
- The app forces `QT_DISABLE_HW_TEXTURES_CONVERSION=1` at startup to work around a
  Qt 6.7.0 FFmpeg-backend bug that mis-crops hardware-decoded 1080p HEVC frames.
