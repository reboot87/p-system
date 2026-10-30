#pragma once
// ============================================================
// MachineConfig.h — 机器级配置（%PROGRAMDATA%\AireRecord\service.ini）
//
// ── AireRecord 数据分层契约 ──────────────────────────────────
// 【用户级】%APPDATA%\AireRecord\  ← 必须在用户会话内访问
//   · airecord.db         剪贴板条目（个人数据，按用户隔离）
//   · 用户偏好            autostart、未来 UI 偏好（DB settings 表）
//   归属：会话代理（当前 airecordd / 未来的用户会话代理）
//
// 【机器级】%PROGRAMDATA%\AireRecord\  ← SYSTEM 服务可读写，
//   所有用户可读（未来 SCM 系统服务的正确数据位置）
//   · service.ini         服务配置（管道名、运行模式、日志级别）
//   · 未来：用户注册表（SID→DB 路径）、全局策略、服务级数据库
// ────────────────────────────────────────────────────────────
#include <string>

namespace machine
{
    // %PROGRAMDATA%\AireRecord 目录（不存在则创建）
    std::wstring ConfigDir();

    // service.ini 完整路径
    std::wstring IniPath();

    // 读取配置项；不存在返回 def
    std::wstring Get(const wchar_t* section, const wchar_t* key,
                     const wchar_t* def);

    // 写入配置项（自动建目录）
    void Set(const wchar_t* section, const wchar_t* key,
             const wchar_t* value);

    // 配置文件是否已初始化（首次运行时由服务端写入默认值）
    bool Exists();
}
