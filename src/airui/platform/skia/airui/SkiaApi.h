#pragma once
// ============================================================
// AirUI - SkiaApi.h
// Single include point for the Skia API used by this backend.
//
// Two modes:
//   * normal build  -> real Skia headers (vcpkg `skia` installs them
//     under include/skia/include/core, so <include/core/...> is the
//     documented include form).
//   * AIRUI_SKIA_MOCK -> a tiny API-shaped stand-in used ONLY by
//     scripts/check-skia-syntax.sh to compile SkiaPainter.cpp on a
//     machine without an SDK. The mock never renders anything.
//
// Keeping all Skia includes behind this one header makes Skia version
// churn (a documented risk, cross-platform.md R5) a one-file fix.
// ============================================================
#if defined(_WIN32) && !defined(AIRUI_SKIA_MOCK)
// SkiaSurface.h pulls in <windows.h> on Windows, whose min/max macros
// expand inside std::numeric_limits<T>::max() / std::max(...) in Skia's
// own headers (MSVC C2589/C2059, e.g. SkRect.h, SkSamplingOptions.h).
// SkiaApi.h is the only place Skia headers enter the build, so disable
// the macros here. It affects only the few backend translation units;
// the legacy Win32 code keeps its usual <windows.h> behaviour.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#ifdef AIRUI_SKIA_MOCK
#include "SkiaApiMock.h"
#else
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSpan.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkDashPathEffect.h>
#endif
