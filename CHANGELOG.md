# Changelog

All notable changes to MobilePhotoViewer are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/).

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

## [1.1.0] - 2026-08

### Changed
- Source tree restructured into `src/app`, `src/browser`, `src/formats` and
  `src/widgets`.

## [1.0.0]

- First release.
