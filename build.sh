#!/usr/bin/env bash
# ============================================================
# AireRecord / AirUI - Linux build (CMake + Unix Makefiles)
#
# 平台无关核心 airui-core 可在 Linux 下用 CMake + Unix 生成器
# 构建（仅核心静态库）。完整应用依赖 Win32 / GDI / DWM，
# 只能在 Windows + MSVC (VS2017) 下构建，见 build.bat。
#
# 用法:
#   ./build.sh            # cmake -G 'Unix Makefiles' 构建 airui-core
#   ./build.sh clean      # 删除 build-linux 并重新配置
#   ./build.sh test       # 构建后运行核心冒烟测试 (scripts/check-core-linux-smoke.sh)
#   ./build.sh clean test # 清空 + 构建 + 冒烟测试
# ============================================================
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"

BUILD_DIR="build-linux"
GEN="Unix Makefiles"
CXX=${CXX:-g++}
DO_CLEAN=0
DO_TEST=0

for arg in "$@"; do
  case "$arg" in
    clean) DO_CLEAN=1 ;;
    test)  DO_TEST=1 ;;
    *) echo "[warn] unknown arg: $arg" ;;
  esac
done

# Ninja 优先（更快），回退 Unix Makefiles
if command -v ninja >/dev/null 2>&1; then
  GEN="Ninja"
fi

if [ "$DO_CLEAN" = "1" ]; then
  echo "[clean] removing $BUILD_DIR ..."
  rm -rf "$BUILD_DIR"
fi

echo "[1/3] Configuring (CMake + $GEN) ..."
cmake -S . -B "$BUILD_DIR" -G "$GEN" -DCMAKE_BUILD_TYPE=Release

echo "[2/3] Building airui-core ..."
cmake --build "$BUILD_DIR" --config Release --target airui-core

echo "[3/3] airui-core artifact:"
if [ -f "$BUILD_DIR/libairui-core.a" ]; then
  ls -l "$BUILD_DIR/libairui-core.a"
elif [ -f "$BUILD_DIR/lib/libairui-core.a" ]; then
  ls -l "$BUILD_DIR/lib/libairui-core.a"
else
  find "$BUILD_DIR" -name 'libairui-core.a' -print -exec ls -l {} \;
fi

if [ "$DO_TEST" = "1" ]; then
  echo "[test] running core smoke test (CMake + Unix built core) ..."
  ./scripts/check-core-linux-smoke.sh
fi

echo
echo "Linux build OK (core only)."
echo "完整应用请在 Windows 上执行 build.bat (CMake + Visual Studio 2017)。"
