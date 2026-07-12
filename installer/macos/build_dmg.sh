#!/usr/bin/env bash
#
# Build, sign, notarize and package MobilePhotoViewer into a .dmg — all locally
# on a Mac. Run over SSH or in Terminal. No GitHub Actions required.
#
# One-time setup on the Mac:
#   1. Install Xcode command line tools:  xcode-select --install
#   2. Install Qt 6 (matching what you build with locally), e.g. via the online
#      installer or `brew install qt`. Make sure qmake/macdeployqt are on PATH,
#      or point CMAKE_PREFIX_PATH at the Qt dir below.
#   3. Your "Developer ID Application" cert must be in the login keychain
#      (double-click the .p12 once). Check with:  security find-identity -v -p codesigning
#   4. Store a notarization profile once (uses an app-specific password from
#      appleid.apple.com), so you never type credentials again:
#        xcrun notarytool store-credentials MPV_NOTARY \
#          --apple-id "you@example.com" --team-id "ABCDE12345"
#      (it will prompt for the app-specific password)
#
# Then just:   ./installer/macos/build_dmg.sh 1.0.0
#
set -euo pipefail

# ---- config --------------------------------------------------------------
VERSION="${1:-1.0.0}"
SIGN_IDENTITY="${SIGN_IDENTITY:-Developer ID Application}"   # substring is enough if unambiguous
NOTARY_PROFILE="${NOTARY_PROFILE:-MPV_NOTARY}"
# If Qt isn't auto-found, set e.g.:
#   export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.1/macos"
# ---------------------------------------------------------------------------

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"
APP="build/MobilePhotoViewer.app"
DMG="MobilePhotoViewer-${VERSION}-macos.dmg"

echo "==> Configuring (version ${VERSION})"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION="${VERSION}"

echo "==> Building (macdeployqt runs via POST_BUILD)"
cmake --build build

echo "==> Codesigning with hardened runtime"
codesign --force --deep --options runtime --timestamp \
  --entitlements installer/macos/entitlements.plist \
  --sign "$SIGN_IDENTITY" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "==> Building DMG"
rm -rf dmg_staging "$DMG"
mkdir -p dmg_staging
cp -R "$APP" dmg_staging/
ln -s /Applications dmg_staging/Applications
hdiutil create -volname "MobilePhotoViewer" -srcfolder dmg_staging \
  -ov -format ULFO "$DMG"
codesign --force --sign "$SIGN_IDENTITY" --timestamp "$DMG"

echo "==> Notarizing (this waits for Apple; usually 1-5 min)"
xcrun notarytool submit "$DMG" --keychain-profile "$NOTARY_PROFILE" --wait

echo "==> Stapling the ticket"
xcrun stapler staple "$DMG"
xcrun stapler validate "$DMG"

echo ""
echo "Done:  $REPO_ROOT/$DMG"
echo "Verify Gatekeeper acceptance with:  spctl -a -t open --context context:primary-signature -v \"$DMG\""
