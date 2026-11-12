#!/bin/bash
# Builds the benchmark image set: public photos converted with macOS sips into
# every format the viewer reads. Usage: bench/fetch_data.sh [data_dir]
set -euo pipefail

DATA="${1:-$(dirname "$0")/data}"
DL="$DATA/download"
SRC="$DATA/source"
SET="$DATA/set"
mkdir -p "$DL" "$SRC" "$SET"/{png,jpg,heic,jpg_4032,heic_4032}

echo "[fetch] Kodak set (24 PNG)"
for i in $(seq -w 1 24); do
    f="$SRC/kodim$i.png"
    [ -s "$f" ] || curl -fsSL --retry 3 -o "$f" "https://r0k.us/graphics/kodak/kodak/kodim$i.png"
done

echo "[fetch] DIV2K validation HR (100 PNG, ~450 MB)"
ZIP="$DL/DIV2K_valid_HR.zip"
[ -s "$ZIP" ] || curl -fL --retry 3 -o "$ZIP" "https://data.vision.ee.ethz.ch/cvl/DIV2K/DIV2K_valid_HR.zip"
unzip -n -j -q "$ZIP" "*.png" -d "$SRC"

total=$(ls "$SRC"/*.png | wc -l | tr -d ' ')
n=0
for src in "$SRC"/*.png; do
    n=$((n + 1))
    b=$(basename "$src" .png)
    echo "[convert] $n/$total $b"
    [ -s "$SET/png/$b.png" ]         || cp "$src" "$SET/png/$b.png"
    [ -s "$SET/jpg/$b.jpg" ]         || sips -s format jpeg -s formatOptions 90 "$src" --out "$SET/jpg/$b.jpg" >/dev/null
    [ -s "$SET/heic/$b.heic" ]       || sips -s format heic -s formatOptions 80 "$src" --out "$SET/heic/$b.heic" >/dev/null
    [ -s "$SET/jpg_4032/$b.jpg" ]    || sips -Z 4032 -s format jpeg -s formatOptions 90 "$src" --out "$SET/jpg_4032/$b.jpg" >/dev/null
    [ -s "$SET/heic_4032/$b.heic" ]  || sips -Z 4032 -s format heic -s formatOptions 80 "$src" --out "$SET/heic_4032/$b.heic" >/dev/null
done

echo "[done] $(find "$SET" -type f | wc -l | tr -d ' ') files in $SET"
du -sh "$SET"/*
