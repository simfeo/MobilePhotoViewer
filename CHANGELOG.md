# Changelog

All notable changes to MobilePhotoViewer are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/).

## [1.2.0] - 2026-11-12

### Faster HEIF browsing and viewing
- Grid thumbnails, the preview panel and the first view of a photo now use the
  thumbnail phones embed in every HEIC instead of decoding the full 24 MP image.
  Filling the grid with iPhone photos is 10 to 40 times faster.
- Opening a HEIC no longer freezes the window: the full photo decodes in the
  background and replaces the preview as soon as it is ready.
- Full HEIC decoding is about 3.5 times faster on Apple Silicon (a 24 MP
  iPhone 18 Pro Max photo: 296 ms before, 83 ms now).
- Every photo is read from disk once; opening one used to read the file up to
  seven times.

### Fixed
- Clicking a folder in the path bar did nothing on macOS. The path bar also
  gains a "/" segment for the root of the disk.
- macOS: the 1.1.1 bundle carried a build-machine path in libheif, which made
  macOS reject the app with "Bad Load Command" on first launch on some Macs.
- A double free when a HEIF image reports an undefined bit depth.

### Under the hood
- macOS decodes HEVC with the FFmpeg that Qt already bundles (NEON optimized);
  libde265 stays as the fallback. libheif comes from a fork that adds a NEON
  YCbCr to RGBA conversion, bit-exact with the original code.
- Built with Qt 6.12.

### Requirements
- macOS 14.4 or later (previously 14.0), as required by Qt 6.12.

## [1.1.1] - 2026-09-03

### Added
- **Samsung multi-shot images**: pictures that carry extra frames in their SEF
  metadata now expose those frames in the viewer, so every shot in a burst can
  be inspected instead of only the primary image.
- **Single-shot images** are now recognised as their own case rather than being
  treated as an incomplete multi-shot file.
- Motion-panorama sweep direction is read from the `Motion_Panorama_Info` SEF
  block instead of being guessed from the block name, so panoramas built by
  other Samsung camera versions scroll the right way.

### Fixed
- **macOS: the app no longer needs `sudo xattr -rd com.apple.quarantine`.**
  Releases previously stapled the notarization ticket only onto the `.dmg`, so
  an app dragged to `/Applications` kept its quarantine flag with no local
  ticket and Gatekeeper had to fall back to an online check. The `.app` bundle
  is now notarized and stapled in its own right before the DMG is built.
- Depth metadata is oriented correctly — depth maps are no longer inverted (or
  double-inverted) on single-shot images.

### Changed
- Updated the bundled TinyEXIF to a newer revision.
- macOS builds are signed inside-out (nested dylibs, Qt plugins and frameworks
  first, then the bundle) instead of with the deprecated `codesign --deep`.
- Internal refactoring of the JPEG manager and the shared image+video base, and
  the whole tree is now formatted with a checked-in `.clang-format`.
- README updated.

## [1.1.0] - 2026-08-18

### Changed
- Source tree restructured into `src/app`, `src/browser`, `src/formats` and
  `src/widgets`.

## [1.0.0]

- First release.
