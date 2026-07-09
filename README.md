# MobilePhotoViewer

A cross-platform (Windows / macOS / Linux) **Qt 6** photo browser and viewer. It
works as an Explorer/ACDSee-style file browser for a folder tree and, on top of
that, is a dedicated viewer for **motion photos** — the still-plus-embedded-video
format produced by modern phones — and 360° panoramas.

Supported still/motion formats:

- Everything `QImageReader` supports (JPEG, PNG, WebP, BMP, GIF, TIFF, …)
- **JPEG** motion photos (Google/Samsung `MicroVideo` and the newer `MotionPhoto`
  container format) — plays the embedded clip
- **HEIC / HEIF** stills and motion photos (decoded via bundled libheif + libde265)
- **Equirectangular panoramas** (GPano XMP) — viewed as an interactive sphere

## Features

**Browsing**

- Folder tree + thumbnail grid, with a live **Preview** and EXIF **Metadata** panel.
- Thumbnails decode **asynchronously** off the UI thread (`QThreadPool`), centred
  on a fixed square so captions line up in a clean grid.
- **Thumbnail zoom** 64–384 px: `Ctrl`+mouse-wheel, trackpad **pinch** (macOS),
  the `+` / `-` keys, and the `−` / `+` toolbar buttons.
- **Navigation toolbar**: Back / Forward (with history), Up, a clickable
  **breadcrumb** address bar (click a segment to jump, or click to type a path),
  and a **Sort** menu — by Name, Date Created, Date Modified, Size, or Extension,
  ascending or descending (folders always listed first, sorted by name).

**File management**

- **Explorer-style selection**: Ctrl / Shift / rubber-band multi-select.
- **Drag images out** of the app to copy them to a folder or share to another app
  (Telegram, WhatsApp, …) via a standard `text/uri-list` drag.
- **Right-click context menu** and matching **Edit** menu / shortcuts:
  Cut, Copy, Paste, Delete (to Recycle Bin / Trash, with confirmation), Rename
  (inline), Copy as Path, Show in Explorer / Finder, Refresh.

**Viewing**

- Zoom / pan, previous / next (arrow keys or wheel), immersive full screen.
- Motion-photo playback with a play button; **Save Video As…** exports the clip.
- One-click 360° button for equirectangular panoramas.

## Architecture

The UI is decomposed into focused components that communicate via signals;
`MainWindow` is a thin coordinator (central stack + menus + drag-and-drop routing).

| File(s) | Responsibility |
|---|---|
| `main.cpp` | App entry point; opens a file/folder passed as the first CLI argument. |
| `mainwindow.{h,cpp,ui}` | Window shell: central `QStackedWidget` (grid / image / panorama), menus, full screen, drag-and-drop, and image loading (`SetImage`, `stepImage`, panorama detection). |
| `folderbrowserpanel.{h,cpp}` | The browser: folder tree (`QFileSystemModel`) + thumbnail grid, navigation & history, sorting, thumbnail zoom, and all file operations. |
| `locationbar.{h,cpp}` | Breadcrumb address bar that toggles to an editable path field. |
| `gridcontextmenu.{h,cpp}` | The grid's right-click menu (item vs. empty-space variants). |
| `thumbnailmodel.{h,cpp}` | List model for the grid: async thumbnail generation, folder icons, drag-out payload (`mimeData`), and inline rename (`setData`). |
| `metadatapanel.{h,cpp}` | Preview image + EXIF/property table for the selected/opened file. |
| `explorerselectiondelegate.{h,cpp}` | Draws the translucent Explorer-style selection highlight. |
| `imagerenderwidget.{h,cpp,ui}` | Still-image view with zoom/pan, the motion-photo play button, and the 360° button. |
| `videowidget.{h,cpp,ui}` | Embedded video playback surface used by the render widget. |
| `panoramawidget.{h,cpp}` | OpenGL sphere for equirectangular panoramas. |
| `jpegmanager.{h,cpp}`, `heifmanager.{h,cpp}` | Decode a still and extract the embedded motion-photo video clip (JPEG / HEIF). |
| `Tools/imagewithvideobase.*` | Common base for the managers (`HasMicrovideo`, `getVideoMemory`, …). |
| `Tools/heifboxes.*`, `Tools/heifreader.*`, `Tools/ramfile.*` | HEIF/ISO-BMFF box parsing and in-memory file helpers. |

## Keyboard shortcuts

| Context | Shortcut | Action |
|---|---|---|
| File | `Ctrl+O` / `Ctrl+Shift+O` | Open file / folder |
| File | `Ctrl+S` | Save motion-photo video |
| File | `Ctrl+Q` | Quit |
| Browse | `Ctrl+X` / `Ctrl+C` / `Ctrl+V` | Cut / Copy / Paste |
| Browse | `Delete` / `F2` | Delete (to Recycle Bin) / Rename |
| Browse | `Ctrl+Shift+C` / `F5` | Copy as Path / Refresh |
| Browse | `Ctrl` + wheel, `+` / `-` | Zoom thumbnails |
| Navigation | `Alt+←` / `Alt+→` | Back / Forward |
| View | `Ctrl+B` / `Esc` | Back to thumbnails |
| View | `+` / `-` / `Ctrl+0` | Zoom in / out / fit |
| View | `←` / `→` / wheel | Previous / next image |
| View | `Space` / `H` | Play video / toggle play button |
| View | `F` / `F11` | Full screen |

## Prerequisites

- **Qt 6.5 or newer** (6.11 recommended). The build is tested with
  **Qt 6.11.1 MSVC 2022 64-bit** on Windows and Qt 6 for macOS.
  Required modules: Core, Gui, Widgets, Multimedia, MultimediaWidgets, OpenGL,
  OpenGLWidgets.
- **CMake ≥ 3.21** and **Ninja** (both ship with Qt under `Tools/`).
- A C++17 compiler: **MSVC 2022** (Windows), **Apple Clang / Xcode** (macOS), or
  GCC/Clang (Linux).
- **Git** — the image codecs are pulled in as submodules.

## Dependencies

libheif and libde265 are bundled as git submodules under `third_party/` and are
built automatically as part of the project — nothing to install separately.
TinyEXIF and tinyxml2 are vendored directly in the source tree.

## Getting the source

The submodules must be present before building:

```bash
git clone --recurse-submodules <repo-url>
# or, in an existing clone:
git submodule update --init --recursive
```

## Building

### Qt Creator (all platforms, recommended)

1. **File → Open File or Project…** and select `CMakeLists.txt`.
2. Pick a Qt 6 kit and a build type (**Release** / **Debug**), then
   **Configure Project**.
3. Build with **Ctrl+B**, run with **Ctrl+R**.

Qt Creator provides its own CMake and Ninja.

### Windows — command line

From a shell with the MSVC environment loaded (run `vcvars64.bat`, or use the
*x64 Native Tools Command Prompt for VS 2022*):

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build build
```

A `POST_BUILD` step runs **windeployqt** and copies every runtime dependency
(`heif.dll`, `libde265.dll`, the Qt DLLs, and the multimedia/FFmpeg plugins) next
to `build/MobilePhotoViewer.exe`, so it runs in place with no extra setup.

### macOS — command line

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=~/Qt/6.11.1/macos
cmake --build build
```

The bundled libheif/libde265 submodules compile from source with Clang. To
produce a self-contained bundle for distribution, run **macdeployqt** on the
built app (the macOS analogue of the automatic Windows deployment step):

```bash
~/Qt/6.11.1/macos/bin/macdeployqt build/MobilePhotoViewer.app
```

macOS-specific behaviour is wired in: **pinch-to-zoom** on the thumbnail grid and
**Show in Finder** in the context menu.

### Linux — command line

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=~/Qt/6.11.1/gcc_64
cmake --build build
```

"Show in Explorer" opens the containing folder via `xdg-open`.

The first configure compiles libde265 and libheif once; later rebuilds only
recompile the application.

## Running

```bat
MobilePhotoViewer "D:\path\to\photo.jpg"     # open a file
MobilePhotoViewer "D:\path\to\folder"        # open a folder in the browser
```

Launched with no arguments, it opens the browser at the filesystem root.

## Notes

- Use `CMakeLists.txt`; the old qmake `MobilePhotoViewer.pro` is superseded.
- `QImageReader`'s default 128 MB allocation cap is lifted at startup so very
  large phone stills (e.g. 108 MP) decode instead of being rejected.
- Automatic dependency deployment is configured for Windows only (windeployqt);
  on macOS/Linux use macdeployqt or your platform's packaging as shown above.
