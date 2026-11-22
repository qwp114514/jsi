#!/usr/bin/env bash
# 在 arm64 Ubuntu(proot) 上用 apt 的 clang 交叉编译到 aarch64-linux-android
# 不跑 x86_64 的 NDK 编译器，只借 NDK 的 sysroot（头文件 + bionic 桩库）
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/build-src"
NDK="${NDK:-/root/ndk-extract/android-ndk-r26d}"
SYS="$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot"
API=29
OUT="$SRC/.out"
mkdir -p "$OUT"

[ -d "$SYS" ] || { echo "sysroot 缺失: $SYS"; exit 1; }

CXX="clang++ --target=aarch64-linux-android$API --sysroot=$SYS -D__ANDROID_API__=$API"
CFLAGS="-fPIC -O2 -fvisibility=hidden -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections"
INC="-I$SRC/src -I$SRC/third_party/imgui -I$SRC/third_party/imgui/backends -I$SRC/third_party -I$SYS/usr/include/c++/v1"
DEF="-DIMGUI_IMPL_OPENGL_ES3 -DAUX_VERSION=\"1.0.4\""

SRCS=$(ls "$SRC"/src/*.cpp)
IMGUI="$SRC/third_party/imgui/imgui.cpp $SRC/third_party/imgui/imgui_draw.cpp $SRC/third_party/imgui/imgui_tables.cpp $SRC/third_party/imgui/imgui_widgets.cpp $SRC/third_party/imgui/backends/imgui_impl_opengl3.cpp"

OBJS=""
for f in $SRCS $IMGUI; do
  b=$(basename "$f" .cpp)
  o="$OUT/$b.o"
  echo "  CC $b"
  $CXX $CFLAGS $INC $DEF -c "$f" -o "$o"
  OBJS="$OBJS $o"
done

LIBS="$SYS/usr/lib/aarch64-linux-android/libc++_static.a"
LDFLAGS="-shared -fuse-ld=lld -Wl,-soname,libaux.so -Wl,--gc-sections -Wl,-s"
$CXX $CFLAGS $LDFLAGS $OBJS -o "$OUT/libaux.so" \
  "$LIBS" -L"$SYS/usr/lib/aarch64-linux-android/$API" -llog -landroid -lEGL -lGLESv3 -ldl -lm -lc

ls -lh "$OUT/libaux.so"
echo "构建完成: $OUT/libaux.so"
