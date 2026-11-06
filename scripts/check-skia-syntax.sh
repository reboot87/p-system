#!/usr/bin/env bash
# ============================================================
# Compile the real Skia backend sources against the API contract.
#
# This runs WITHOUT a Skia SDK: SkiaApi.h is switched to the API-shaped
# mock in tests/skia_contract. It is a syntax / interface-shape gate,
# not a rendering test. The authoritative link+run check is the
# vcpkg-backed Skia job in .github/workflows/ci.yml.
#
# Works with g++ and with clang (macOS, where g++ is an alias).
# ============================================================
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cxx="${CXX:-g++}"

inc=(
  -I "$root/tests/skia_contract"
  -I "$root/src/airui/platform/skia"
  -I "$root/src/airui/core/include"
)

srcs=(
  "$root/src/airui/platform/skia/airui/SkiaTypes.cpp"
  "$root/src/airui/platform/skia/airui/SkiaPainter.cpp"
  "$root/src/airui/platform/skia/airui/SkiaSurface.cpp"
  "$root/tests/test_skia_types.cpp"
  "$root/tests/test_skia_backend.cpp"
)

for src in "${srcs[@]}"; do
  echo "SYNTAX $(basename "$src")"
  "$cxx" -std=c++17 -Wall -Wextra -DAIRUI_SKIA_MOCK \
    "${inc[@]}" -fsyntax-only "$src"
done

echo "OK: Skia backend sources compile against the API contract"
