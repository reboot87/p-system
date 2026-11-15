# ADR-0003: 服务 / UI 生命周期契约（离线谁存活）

- **状态**：生效（取代 `466b688` 的旧决定）
- **日期**：2026-11-15
- **关联**：`src/core/App.h`（`ServiceDied` / `SetServiceOffline` / `StartReconnectWatch` / `m_suppressRestartUntil`）、`src/ipc/PipeClient.cpp`（SCM 静默启动）、`src/agent/AgentHost.cpp`（`pending.db`）、`src/service/main.cpp`（STOPPING 广播 + SCM 失败操作）、提交 `466b688` / `c209f7e`

## 背景

服务生命周期决策在近期连续反转：`466b688`「服务停则 UI 退出」→ `c209f7e`「离线 UI 保持存活」，且前一条的注释仍留在 `App.h` 里（「统一退出」与「UI 保持存活」并列），读者无法判断哪个是当前契约。P1-6 要求把当前契约固定成 ADR，并把反转记为「旧决定被替代」。

## 决定（当前契约，来自 `c209f7e` 及其后续实现）

- **离线谁存活**：服务停止 / 崩溃 / 断开 / 启动失败时，**UI 保持存活**，进入离线态（红色告警 + 状态栏提示），不再随服务退出。这是从「服务停则 UI 退出」逆转而来的契约。
- **离线暂存**：服务不可用时，剪贴板记录**不丢弃**，写入本会话本地库 `%APPDATA%\AireRecord\pending.db`；服务恢复后由 `AgentHost` 同步回系统服务（`pending.db` 归属本会话 agent，UI 只读暂存库）。
- **自动拉起**：服务已安装但被停止时，UI 用 SCM 静默启动（无需 UAC）；服务崩溃时由 **SCM 失败操作（failure-actions）** 自动重启（Session 0，仅进程异常退出触发）。离线态下禁止 UI 擅自重启服务（`m_ipc.SetAutoStart(!off)`），避免 UI 在服务停机时抢拉起。
- **STOPPING 广播**：用户主动 `sc stop` 时，服务端先广播 `STOPPING` 事件再关管道；UI 收到后暂停自动重启（`m_suppressRestartUntil`），避免「停止」与「启动」竞争（SCM 立即报无法停止）。SCM 失败操作**不**触发干净的 `sc stop`。
- **退出路径**：唯一让 UI 随服务退出的是用户显式「退出」（托盘/退出菜单 → `ExitAll()`：停止服务 best-effort + 强制退出本 UI）。

## 后果

- 收益：离线时用户不丢剪贴板记录；服务崩溃自动恢复，UI 持续可用；语义单一、可解释。
- 代价：需维护 SCM 失败操作、STOPPING 暂停窗口、pending 同步三条机制，任一配合错位（如停止瞬间被 UI 拉起）都需处理竞争。

## 被替代的旧决定

- `466b688`（"service stops/crashes, UI exits too"）：服务停止/崩溃时 UI 一并退出。被 `c209f7e` 的「离线 UI 保持存活」取代；`App.h` 中与之冲突的旧注释需清理。