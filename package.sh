#!/usr/bin/env bash
# 组装 Magisk 模块 zip
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/build-src"
SO="$SRC/.out/libaux.so"
STAGE="$ROOT/pkg"
OUT="$ROOT/dist/aux-hardened-1.0.5.zip"

[ -f "$SO" ] || { echo "缺少 $SO，先跑 build-android.sh"; exit 1; }

rm -rf "$STAGE"; mkdir -p "$STAGE/zygisk" "$ROOT/dist"
cp "$ROOT/hardened/module/module.prop"    "$STAGE/"
cp "$ROOT/hardened/module/customize.sh"   "$STAGE/"
cp "$ROOT/hardened/module/post-fs-data.sh" "$STAGE/"
cp "$ROOT/hardened/module/service.sh"     "$STAGE/"
cp "$ROOT/hardened/module/uninstall.sh"   "$STAGE/"
cp "$SO" "$STAGE/zygisk/arm64-v8a.so"
[ -d "$SRC/res" ] && cp -r "$SRC/res/." "$STAGE/common/" 2>/dev/null || true
chmod 755 "$STAGE"/*.sh
rm -f "$OUT"
cd "$STAGE" && zip -qr "$OUT" . -x '.*'
ls -lh "$OUT"
echo "产物: $OUT"
