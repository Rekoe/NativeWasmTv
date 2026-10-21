# BL JAR 发布验证

## v3 验证（2026-10-21）

新增会话内错误详情、可复制的脱敏诊断、API/媒体原因链、后台回退记录和准备超时提示。
构建仍通过 API 14 框架编译与产物归属校验；保留源文件和行号，已在设备异常栈确认可定位到 Java 源码。
最终 JAR 591,288 字节，SHA-256 `1895555bb1e5220c42055aedf8ef80fe0ea4d01cdff646df5ca14f046a7d9844`。

- JVM：脱敏、重复脱敏、HTTP/业务上下文、原因链、DNS/TLS/超时建议、20 条记录边界通过。
- 实际共享 OkHttp + 本地故障服务：HTTP 503 与追踪号、非法 JSON、业务 -352、媒体 403、恢复请求及响应释放通过。
- Android 9 / API 28 辅助验证：开发阶段错误详情显示/复制/返回、失败后恢复；DASH 完整回归与额外 20 秒播放观察通过。
- Android 7.1.1 / API 25，小米 6：按用户要求覆盖正式包 `xiao.bu.tv`，保留应用数据和正式 BL 偏好。
  系统拒绝新测试包，改用原签名同包名的临时内置 instrumentation；测试偏好单独存于 `ntv-bl-instrumentation`。
  验证结束重新覆盖无测试入口的正式 release APK，签名与原包一致。

| 7.1 测试路径 | 结果与范围 |
| --- | --- |
| DASH 完整回归，两轮 | 通过；最终轮含额外 20 秒播放观察，画质/连续切 P/备用线路/字幕/倍速/缩放/后台恢复/弹幕/直播换线/搜索/历史/二维码/退出释放 |
| IJK 完整回归，两轮 | 通过；初轮含额外 20 秒观察，最终轮重复全部操作 |
| 故障注入 | HTTP 503/追踪号、非法 JSON、业务 -352、媒体 403、原始媒体错误、详情复制/脱敏/嵌套返回及请求恢复通过 |
| 系统播放器基础回归 | 通过；点播与直播、403 换线、弹幕、暂停/恢复、MENU/返回、搜索/历史/二维码及退出；未把 smoke 表述为连续切 P 压力验收 |
| 真实站点匿名读取 | 推荐/详情、DASH 点播、分段弹幕、本地历史、直播、二维码与退出通过；载入 486 条点播弹幕，记录 368 次绘制；搜索返回站点验证提示；直播连接认证成功，本次短测 0 条真实聊天消息 |

实测修复：

- 宿主 R8 改名 `IjkLibLoader` 导致下载 JAR 的 `loadLibrariesOnce` 找不到，已保留该公共参数类型。
  最终 APK 的 ABI 检查通过，也验证宿主没有 Exo 与临时测试入口。此故障需更新宿主 APK，单换 JAR 无法修复宿主接口。
- Android 系统 MediaPlayer 对 403 长时间内部重试：准备中检测共享网络层已有的失败并及时换线/显示错误，避免无限加载。
- 新版 JDK 使用 `-target 8` 仍可能生成旧 Android 没有的 ByteBuffer 协变方法调用：JDK 9+ 自动使用 `--release 8`，
  产物检查拒绝这类方法描述符。JDK 8 与 JDK 17 均构建并通过检查，发布产物使用 JDK 8。

最终 DASH 轮次读取 Java 堆约 40.4 MiB、native 堆约 27.2 MiB，包含整个宿主进程；未做长时间功耗或帧率基准。
本轮没有重新验收 Android 4.4，也未连接 Android 4.0；下面 v2 记录保留原先范围。没有执行真实账号写操作。

本地证据：`output/bl/tests/ff0316c7-fixture-dash-formal-v3-final/`、
`ff0316c7-fixture-ijk-formal-v3-final/`、`ff0316c7-fixture-ijk-errors-formal-v3-final/`、
`ff0316c7-fixture-system-smoke-formal-v3-system-compatible/`、`ff0316c7-real-dash-formal-v3-official/`。

## v2 发布记录

日期：2026-10-21。JAR 版本 2、API 14 目标，539 KiB 以下，具体校验值以同目录 `dist/bl.sha256` 为准。测试使用独立包名 `xiao.bu.tv.channeltest` 的 release 宿主，不覆盖正式包。

| 设备/路径 | 结果 | 证据与范围 |
| --- | --- | --- |
| Android 7.1.1，DASH 确定性服务 | 通过 | 独立 H.264 视频和 AAC 音频请求、播放时钟、暂停/恢复、MENU/嵌套返回、画质请求、连续切 P、备用线路、字幕与默认字幕、倍速/缩放、按钮隐藏、真实 Canvas 弹幕绘制、直播画质、WebSocket 认证及 zlib 弹幕、403 自动换线、退出释放 |
| Android 7.1.1，IJK 确定性服务 | 通过 | 同上；点播使用宿主 IJK，直播仍复用 IJK 与共享网络 |
| Android 7.1.1，真实站点 | 通过，含下述限制 | 推荐/详情、DASH 点播、真实分段弹幕、本地历史、匿名直播播放、WebSocket 认证连接、扫码生成/轮询、逐层返回与退出 |
| Android 4.4.4，IJK 确定性服务 | 通过 | 完整回归；连续切 P、字幕/备用线路、点播弹幕，直播画质/失败换线和每 3 秒轮询的兼容弹幕 |
| Android 4.4.4，系统播放器基础测试 | 通过 | 基础播放、媒体键、设置面板、逐级返回、直播及轮询弹幕、搜索/历史/二维码和退出；使用 `--system --smoke`，不包含切 P 压力场景 |
| Android 4.4.4，系统播放器完整测试 | 未通过 | 切 P 偶发卡在准备阶段；有通过的轮次，最终压力回归仍复现，不能据此承诺系统解码器切 P 已彻底修复 |
| Android 4.0 | 未测试 | 本轮未连接真机；源码 API 14 校验与 R8 min-api 14 不能替代真机验收 |

真实接口样本：7.1 一次点播加载 479 条 protobuf 弹幕并实际绘制；早期样本还取得 459/840 条。真实直播能播放，弹幕连接认证成功，最终短测窗口收到 0 条聊天消息，因此不把该次连接结果表述为真实直播消息已验收。消息解包、绘制与认证包格式由测试服务验证。未登录搜索返回站点验证提示，测试确认提示正确，未绕过验证。

测试服务的直播首条线路故意返回 403，备用线路提供可解码视频；WebSocket 服务验证客户端 op=7、roomid、protover=2、token 后才返回认证成功，再发送 zlib 压缩消息。4.x 使用 newBV 的近期消息接口替代 WebSocket，原因是宿主旧 TLS 实现串行读写；兼容模式有轮询延迟，可能漏过高密度消息。

资源与性能边界：7.1 最终 DASH 回归读取到宿主进程 Java 堆约 42.7 MiB、native 堆约 19.9 MiB，包含宿主、界面、图片和解码器，不是 JAR 单独占用。弹幕定时器目标约 30fps；没有做长时间帧率/功耗基准，不声称所有旧设备恒定 30fps。UI 按用户 APK 的布局和图标核对，没有导入它的 Activity/DEX。

构建检查：UI 源码以官方 Android API 14 平台包编译；R8 裁剪 Exo core 后生成 min-api 14 DEX。`verify_artifact.py` 检查 SHA-256、入口/版本元数据、类归属，无 Activity/Fragment/Service/WebView 子类，不重复打包宿主 OkHttp/Okio/IJK，不包含新 `.so`。DASH 仅在 API 16+ 创建，所有 HTTP 和 WebSocket 使用同一宿主 OkHttp。

本地完整日志和截图：`output/bl/tests/` 中的 `ff0316c7-fixture-dash`、`ff0316c7-fixture-ijk`、`ff0316c7-real-dash`、`8ba8ceb8-fixture-ijk`、`8ba8ceb8-fixture-system-smoke`；系统完整测试失败保留在 `8ba8ceb8-fixture-system`。真实账号扫码确认、点赞/投币/收藏/关注/三连没有执行，功能差异见 [PARITY.md](PARITY.md)。
