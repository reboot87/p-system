# CHANGELOG

版本号单一来源：`CMakeLists.txt` 的 `project(AireRecord VERSION ...)`，见 `docs/adr/0002-version-numbering.md`。

## [v0.9.0] — 2026-11-15

### 工程化收敛（整改批次 P1）

- **P1-2 版本来源收敛到一处**：`project(AireRecord VERSION 0.9.0)` 成为唯一版本来源；CMake `configure_file` 生成 `Version.h`，暴露窄/宽宏 `AIRUI_VERSION` / `AIRUI_VERSION_W`；页脚文案改为唯一拼接点 `kAppFooterPrefix`；`res/app.rc` 补上 VERSIONINFO 版本块；新增 `CHANGELOG.md`。
- **P1-1** 统一构建目录与忽略规则，`build.bat` / `build.sh` 默认跑测试。
- **P1-4** 官方构建入口默认开启测试（`--no-test` 显式跳过）。
- **P1-6** 新增 `docs/adr/` 决策记录（工具链矩阵、版本号策略、服务/UI 生命周期契约、插件 ABI v2）。

### 架构与 ABI（整改批次 P0）

- **P0-1** 把 core 纯净门禁接入 CI，并加固检查脚本。
- **P0-2** 后台刷新/重连线程改为可 join，杜绝跨线程共享状态写、析构悬空。
- **P0-3** 插件 ABI v2：结构体带 `size` / `caps` / `painterIfaceRev`；`IPainter` 文本接口改为 `wchar_t* + len` 跨 DLL 字符串表示；发布顺序为先宿主兼容层、再 DLL、最后删兼容层。
- **P0-4** a11y `SetProp` 存储不再分配/泄漏。
- **P0-5** RAII 模态禁用 / 重绘冻结 / 更新锁防护。

### 渲染与性能

- GDI 只重绘脏区；跨帧复用窗口后备缓冲。
- 图库控件搜索按名称过滤；悬停控件装饰外边距失效处理。

### 服务 / IPC

- IPC 重构为持久化双向命名管道流；服务与每会话 UI+agent 双向心跳。
- 服务崩溃时 SCM 失败操作自动重启；`STOPPING` 为用户主动停止语义（记录在 `docs/adr/0003-service-ui-lifecycle.md`）。
- 离线韧性：UI 保持存活并显示红色告警，剪贴板记录暂存本地 `pending.db`，服务恢复后自动同步。

## [v0.8] — 2026-11-09

基线版本（tag `v0.8`）。详见 `docs/v0.8-baseline.md`：跨平台 GDI/Skia 双后端、核心/库分离、基础服务架构与 22 个 ctest 用例门禁。

[v0.8]: https://github.com/reboot87/p-system/releases/tag/v0.8