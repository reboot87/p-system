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
#include <include/core/SkPathEffect.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkDashPathEffect.h>
#endif
