#!/usr/bin/env bash
#
# Build a self-contained Debian package (.deb) for MobilePhotoViewer.
#
# The Qt 6 runtime and the bundled heif/libde265 libraries are packed into the
# .deb under /opt/mobilephotoviewer (via linuxdeploy + its Qt plugin), so the
# package runs on Debian/Ubuntu without a matching system Qt. Only base system
# libraries (glibc, X11/xcb, OpenGL) are taken from the distro.
#
# Prereqs (Debian/Ubuntu):
#   - Qt 6 with qmake6/moc on PATH, or set CMAKE_PREFIX_PATH to the Qt dir.
#   - cmake, ninja-build, g++, wget, file, patchelf, desktop-file-utils.
#   - Runtime headers to build against Qt xcb: libgl1-mesa-dev and friends.
#
# Usage:  ./installer/linux/build_deb.sh 1.0.0
#
set -euo pipefail

VERSION="${1:-1.0.0}"
ARCH="amd64"
PKG="mobilephotoviewer"
BINARY="MobilePhotoViewer"
MAINTAINER="simfeo <simidimus@gmail.com>"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

echo "==> Configuring & building (version ${VERSION})"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION="${VERSION}"
cmake --build build

# ---- assemble an AppDir and let linuxdeploy bundle Qt ---------------------
APPDIR="$REPO_ROOT/AppDir"
rm -rf "$APPDIR"
install -Dm755 "build/${BINARY}"                              "$APPDIR/usr/bin/${BINARY}"
install -Dm644 "installer/linux/${PKG}.desktop"              "$APPDIR/usr/share/applications/${PKG}.desktop"
install -Dm644 "resources/appicon.png"                       "$APPDIR/usr/share/icons/hicolor/256x256/apps/${PKG}.png"

echo "==> Fetching linuxdeploy + Qt plugin"
TOOLS="$REPO_ROOT/.linuxdeploy"
mkdir -p "$TOOLS"
fetch() { [ -f "$TOOLS/$1" ] || wget -q -O "$TOOLS/$1" "$2"; chmod +x "$TOOLS/$1"; }
fetch linuxdeploy.AppImage        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
fetch linuxdeploy-plugin-qt.AppImage "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

echo "==> Bundling Qt into AppDir (rpath patched to \$ORIGIN/../lib)"
# --appimage-extract-and-run avoids needing FUSE on CI runners.
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
# heif.so / libde265.so are built into the build tree; add their dirs to the
# search path so linuxdeploy can resolve the binary's NEEDED entries and bundle
# them (otherwise it can't find them and the package would be missing them).
DEP_LIB_DIRS=$(find "$REPO_ROOT/build/third_party" -name '*.so*' -printf '%h\n' | sort -u | paste -sd: -)
export LD_LIBRARY_PATH="${DEP_LIB_DIRS}:${LD_LIBRARY_PATH:-}"
"$TOOLS/linuxdeploy.AppImage" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/usr/share/applications/${PKG}.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/${PKG}.png"

# ---- lay out the .deb tree -----------------------------------------------
echo "==> Building .deb"
PKGROOT="$REPO_ROOT/deb_root"
rm -rf "$PKGROOT"
mkdir -p "$PKGROOT/opt/${PKG}" "$PKGROOT/usr/bin" \
         "$PKGROOT/usr/share/applications" \
         "$PKGROOT/usr/share/icons/hicolor/256x256/apps" \
         "$PKGROOT/DEBIAN"

# Everything bundled goes under /opt; keep the internal usr/ layout so the
# rpath ($ORIGIN/../lib) resolves.
cp -a "$APPDIR/usr" "$PKGROOT/opt/${PKG}/"
# System-visible entry points.
ln -s "/opt/${PKG}/usr/bin/${BINARY}" "$PKGROOT/usr/bin/${PKG}"
cp "installer/linux/${PKG}.desktop" "$PKGROOT/usr/share/applications/${PKG}.desktop"
cp "resources/appicon.png" "$PKGROOT/usr/share/icons/hicolor/256x256/apps/${PKG}.png"

INSTALLED_KB=$(du -sk "$PKGROOT" | cut -f1)

cat > "$PKGROOT/DEBIAN/control" <<EOF
Package: ${PKG}
Version: ${VERSION}
Section: graphics
Priority: optional
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Installed-Size: ${INSTALLED_KB}
Depends: libc6, libstdc++6, libgl1, libglib2.0-0, libfontconfig1, libfreetype6, libx11-6, libx11-xcb1, libxcb1, libxkbcommon0, libxkbcommon-x11-0, libdbus-1-3
Description: Mobile photo viewer
 Viewer for phone photos including HEIC/HEIF, motion photos and panoramas.
 Qt runtime is bundled; installs under /opt/${PKG}.
EOF

DEB="${PKG}_${VERSION}_${ARCH}.deb"
dpkg-deb --root-owner-group --build "$PKGROOT" "$DEB"

echo ""
echo "Done:  $REPO_ROOT/$DEB"
echo "Inspect with:  dpkg-deb -I \"$DEB\"  &&  dpkg-deb -c \"$DEB\" | head"
