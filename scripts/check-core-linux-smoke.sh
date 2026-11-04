#!/usr/bin/env bash
# ============================================================
# AirUI - phase 0 Linux compile smoke test
#
# Proves that airui-core is platform-free: it compiles with a
# plain POSIX C++17 compiler and links into a static library,
# with no Win32 headers anywhere.
#
# Target platform: Ubuntu 22.04 / GCC 11 (CI baseline).
# Any g++ >= 9 works for the smoke test.
#
#   ./scripts/check-core-linux-smoke.sh
# ============================================================
set -eu

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
core="$root/src/airui/core"
cxx="${CXX:-g++}"
out="$root/build-linux-smoke"

echo "compiler: $("$cxx" --version | head -1)"

rm -rf "$out"
mkdir -p "$out"

for src in "$core"/src/*.cpp; do
    obj="$out/$(basename "${src%.cpp}").o"
    echo "CXX $(basename "$src")"
    "$cxx" -std=c++17 -Wall -Wextra -Werror -c \
        -I "$core/include" "$src" -o "$obj"
done

ar rcs "$out/libairui-core.a" "$out"/*.o

# --- HAL header check: the interfaces must compile and be usable ---
cat > "$out/hal_check.cpp" <<'CPP'
#include "airui/types.h"
#include "airui/hal.h"
#include "airui/Theme.h"
#include "airui/Layout.h"
#include "airui/Animation.h"
#include "airui/Observable.h"

struct TestClock : air::ui::IClock
{
    uint64_t NowMs() override { return 0; }
};

struct TestItem : air::ui::ILayoutItem
{
    air::ui::Rect rect;
    void Move(const air::ui::Rect& rc) override { rect = rc; }
};

int main()
{
    air::ui::Rect r(0, 0, 10, 10);
    air::ui::Point p{ 5, 5 };
    air::ui::Color c = air::ui::MakeColor(1, 2, 3);

    TestItem item;
    air::ui::Layout().Row({ { &item, 1 } }, 8).Apply(r);

    TestClock clock;
    air::ui::anim::AnimationManager::SetClock([&clock] { return clock.NowMs(); });

    air::ui::ObservableValue<int> value(0);
    value = 1;

    return (r.Contains(p) && !item.rect.Empty() && c != 0 && value.Get() == 1) ? 0 : 1;
}
CPP
echo "CXX hal_check.cpp (HAL header usability)"
"$cxx" -std=c++17 -Wall -Wextra -Werror -c \
    -I "$core/include" "$out/hal_check.cpp" -o "$out/hal_check.o"

echo "LINK hal_check"
"$cxx" "$out/hal_check.o" "$out/Theme.o" "$out/Layout.o" "$out/Animation.o" \
    -o "$out/hal_check"

echo "RUN hal_check (layout + HAL behaviour)"
"$out/hal_check"

# ---- Core controls smoke: PaintButton / PaintCheckbox / PaintRadio ----
echo "CXX test_controls_core (core controls)"
"$cxx" -std=c++17 -Wall -Wextra -Werror -c \
    -I "$core/include" "$root/tests/test_controls_core.cpp" -o "$out/test_controls_core.o"
echo "LINK test_controls_core"
"$cxx" "$out/test_controls_core.o" "$out/libairui-core.a" -o "$out/test_controls_core"
echo "RUN test_controls_core"
"$out/test_controls_core"

echo "--- artifacts ---"
ls -l "$out"
echo "linux-smoke: PASSED (airui-core built with $cxx, no platform headers)"
