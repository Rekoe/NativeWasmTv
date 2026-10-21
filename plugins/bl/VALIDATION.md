# BL JAR v2 发布验证

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
