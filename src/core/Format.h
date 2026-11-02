#pragma once
// ============================================================
// Format.h — 展示层格式化工具
// ============================================================
#include <string>

namespace fmt {

// 相对时间：刚刚 / N 分钟前 / N 小时前 / N 天前 / M月D日 HH:MM / Y年M月D日
std::wstring RelTime(const std::wstring& createdAt);

// 容量：MB → "512 MB" / "1.5 GB"
std::wstring SizeMB(long long mb);

// 容量：字节 → 同上（<=0 返回 "—"）
std::wstring SizeBytes(long long bytes);

} // namespace fmt
