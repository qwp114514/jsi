#!/usr/bin/env bash
# ============================================================
#  ANJIAN ESP — one-shot build
#  用法: bash build.sh
#  产物: dist/anjian-esp-<ver>.zip  (可直接 Magisk 刷入)
# ============================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
VERSION="1.0.4"
BUILD="$ROOT/.build"
DIST="$ROOT/dist"
NDK_VER="r26d"
NDK_DIR="$BUILD/android-ndk-$NDK_VER"
ABI="arm64-v8a"
API=29

cyan() { printf '\033[36m%s\033[0m\n' "$*"; }
ok()   { printf '\033[32m  ok  \033[0m%s\n' "$*"; }

mkdir -p "$BUILD" "$DIST"

# ---------- 1. NDK ----------
if [ ! -d "$NDK_DIR" ]; then
  cyan "[1/6] 下载 Android NDK $NDK_VER (~650MB, 只需一次)"
  URL="https://dl.google.com/android/repository/android-ndk-$NDK_VER-linux.zip"
  if ! curl -fL --retry 3 -o "$BUILD/ndk.zip" "$URL"; then
    echo "google 源失败, 切清华镜像"
    curl -fL --retry 3 -o "$BUILD/ndk.zip" \
      "https://mirrors.tuna.tsinghua.edu.cn/android/repository/android-ndk-$NDK_VER-linux.zip"
  fi
  unzip -q "$BUILD/ndk.zip" -d "$BUILD"
  rm -f "$BUILD/ndk.zip"
fi
export ANDROID_NDK_HOME="$NDK_DIR"
ok "NDK: $NDK_DIR"

# ---------- 2. ImGui ----------
if [ ! -d "$ROOT/third_party/imgui" ]; then
  cyan "[2/6] 拉取 ImGui"
  mkdir -p "$ROOT/third_party"
  git clone --depth 1 --branch v1.90.9 \
      https://github.com/ocornut/imgui.git "$ROOT/third_party/imgui"
fi
ok "ImGui ready"

# ---------- 3. zygisk.hpp ----------
if [ ! -f "$ROOT/third_party/zygisk.hpp" ]; then
  cyan "[3/6] 拉取 zygisk.hpp"
  mkdir -p "$ROOT/third_party"
  curl -fL --retry 3 -o "$ROOT/third_party/zygisk.hpp" \
    https://raw.githubusercontent.com/topjohnwu/Magisk/master/native/src/zygisk.hpp
fi
ok "zygisk.hpp ready"

# ---------- 4. 编译 ----------
cyan "[4/6] 编译 $ABI"
cmake -S "$ROOT" -B "$BUILD/cmake" \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM="android-$API" \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_STL=c++_static \
  -DIMGUI_DIR="$ROOT/third_party/imgui" \
  -DZYGISK_DIR="$ROOT/third_party" >/dev/null
cmake --build "$BUILD/cmake" -j"$(nproc)"
ok "libaux.so 编译完成"

# ---------- 5. 组装 ----------
cyan "[5/6] 组装 Magisk 模块"
STAGE="$BUILD/module"
rm -rf "$STAGE"
mkdir -p "$STAGE/zygisk" "$STAGE/common"
cp "$ROOT/module/module.prop"     "$STAGE/"
cp "$ROOT/module/customize.sh"    "$STAGE/"
cp "$ROOT/module/post-fs-data.sh" "$STAGE/"
cp "$ROOT/module/service.sh"      "$STAGE/"
cp "$ROOT/module/uninstall.sh"    "$STAGE/"
cp "$BUILD/cmake/libaux.so"       "$STAGE/zygisk/$ABI.so"
cp -r "$ROOT/res/." "$STAGE/common/" 2>/dev/null || true
sed -i "s/^version=.*/version=$VERSION/" "$STAGE/module.prop" 2>/dev/null || true
ok "模块组装完成"

# ---------- 6. 打包 ----------
cyan "[6/6] 打包"
OUT="$DIST/anjian-esp-$VERSION.zip"
rm -f "$OUT"
( cd "$STAGE" && zip -qr "$OUT" . -x '.*' )
ok "产物: $OUT"
echo
echo "  刷入方式:"
echo "    adb push $OUT /sdcard/"
echo "    Magisk -> 模块 -> 从本地安装 -> 选 $OUT -> 重启"
echo
echo "  操作: 三指同时按住屏幕 0.35 秒 = 呼出/收起菜单"
echo "  日志: adb logcat -s aux"
echo "  配置: /data/local/tmp/aux.ini"
