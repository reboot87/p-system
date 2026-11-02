#include "Format.h"
#include <windows.h>
#include <cstdio>

namespace fmt {

std::wstring RelTime(const std::wstring& created)
{
    SYSTEMTIME st = {}, now = {};
    if (swscanf_s(created.c_str(), L"%hd-%hd-%hd %hd:%hd:%hd",
                  &st.wYear, &st.wMonth, &st.wDay,
                  &st.wHour, &st.wMinute, &st.wSecond) < 3)
        return created;

    GetLocalTime(&now);

    FILETIME fa = {}, fb = {};
    if (!SystemTimeToFileTime(&st, &fa) || !SystemTimeToFileTime(&now, &fb))
        return created;

    ULARGE_INTEGER ua = { fa.dwLowDateTime, fa.dwHighDateTime };
    ULARGE_INTEGER ub = { fb.dwLowDateTime, fb.dwHighDateTime };
    long long diffSec = (long long)((ub.QuadPart - ua.QuadPart) / 10000000ULL);
    if (diffSec < 0) diffSec = 0;

    wchar_t buf[64];
    if (diffSec < 60)      return L"刚刚";
    if (diffSec < 3600)  { swprintf_s(buf, L"%d 分钟前", (int)(diffSec / 60));   return buf; }
    if (diffSec < 86400) { swprintf_s(buf, L"%d 小时前", (int)(diffSec / 3600)); return buf; }
    if (diffSec < 7 * 86400) { swprintf_s(buf, L"%d 天前", (int)(diffSec / 86400)); return buf; }
    if (st.wYear == now.wYear) {
        swprintf_s(buf, L"%d月%d日 %02d:%02d",
                   st.wMonth, st.wDay, st.wHour, st.wMinute);
        return buf;
    }
    swprintf_s(buf, L"%d年%d月%d日", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring SizeMB(long long mb)
{
    wchar_t b[32];
    if (mb >= 1024) swprintf_s(b, 32, L"%.1f GB", mb / 1024.0);
    else            swprintf_s(b, 32, L"%d MB", (int)mb);
    return b;
}

std::wstring SizeBytes(long long bytes)
{
    if (bytes <= 0) return L"—";
    return SizeMB(bytes / (1024 * 1024));
}

} // namespace fmt
