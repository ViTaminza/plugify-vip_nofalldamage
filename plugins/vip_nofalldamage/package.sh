#!/bin/bash
# Assembles build/package/vip_nofalldamage: the library and manifest from build/, everything else from dist/.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT="$ROOT/build/package/vip_nofalldamage"
rm -rf "$OUT"
mkdir -p "$OUT/bin"
cp "$ROOT/build/plugins/vip_nofalldamage/libvip_nofalldamage.so" "$OUT/bin/"
cp "$ROOT/build/plugins/vip_nofalldamage/vip_nofalldamage.pplugin" "$OUT/"
if [ -d "$HERE/dist" ]; then cp -r "$HERE/dist/." "$OUT/"; fi
echo "package: $OUT"
