# BL 原生 JAR 频道

BL 将用户提供的 BBLL 1.5.2 界面资源、布局层次与 newBV 的 Web API 合同移植到 nTv 的原生频道插件。唯一入口是 `BlPlugin.create()` 返回的根 View，页面、播放器、二维码和弹层均为其子 View；JAR 不注册 Activity、Fragment、Service 或 WebView。

## 导入

宿主需具备 BL 首版加入的 `ChannelPlugin.NativeHost.httpClient()`；已经安装配套宿主的设备可以直接更新 JAR。更早的宿主会提示更新 APK。正式包名、测试包名都可以使用相同的 JAR。

在线订阅：

https://raw.githubusercontent.com/buhanzhe/NativeWasmTv/master/plugins/bl/dist/example.m3u

`dist/example.m3u` 包含 HTTPS JAR 地址及 SHA-256。宿主下载、校验并缓存 DEX JAR。发布的订阅固定 JAR 所在的 Git 提交，避免 CDN 新旧缓存造成哈希不匹配。

## 兼容目标

| 项目 | 说明 |
| --- | --- |
| 最低目标 | Android 4.0 / API 14；UI 源码用 Google Android API 14 平台包校验，R8 `--min-api 14` 裁剪并生成 DEX |
| 已实测设备 | Android 4.4.4 / API 19、Android 7.1.1 / API 25 |
| 已知保留问题 | 4.4 强制系统 MediaPlayer 时，切分 P 仍偶发准备超时；IJK 完整回归通过。详见 [VALIDATION.md](VALIDATION.md) |
| Android 4.0 实测 | 设备未连接，尚未完成真机验证；编译通过不等于全部 ROM、解码器都已经验证 |
| WebView | 完全不依赖 WebView，没有最低 WebView 版本要求 |
| API 14–15 播放 | `BlSystemPlayer`，只引用 API 14 的平台 MediaPlayer；视频经共享 OkHttp 的本机代理输入 |
| API 16+ 播放 | 默认优先 DASH；Exo core 2.9.6 合并分离的 AVC/AAC 音视频，DataSource 使用同一个宿主 OkHttp。MP4/FLV 复用宿主 IJK，可切换系统播放器 |
| ABI | JAR 不包含 `.so`；IJK 播放取决于宿主 APK 与设备 ABI 匹配。平台播放无需 JAR 携带原生库 |

没有直接打包 newBV 的 Compose/Media3 主应用，也没有把 BBLL 的 Activity 搬进 JAR。平台播放适配器特意避免 IJK `AndroidMediaPlayer` 中 API 16 `OnTimedTextListener` 的引用。

## 本版功能

- 推荐、热门、分区、番剧列表/剧集、直播列表及播放。
- 搜索视频/番剧/直播/UP，识别 BV 号与含 BV 号的链接；支持系统输入法和遥控器字母键盘。
- 详情、分 P、相关推荐、UP 投稿、评论阅读。
- 扫码登录、Cookie 本地保存、历史、收藏夹、稍后再看、关注列表、动态；账号接口需要有效登录。
- 点赞、收藏、投币确认、加入稍后再看。
- 视频播放、全屏控制、画质、分 P、进度、继续上次播放、连续分 P、VOD 弹幕和 JSON 字幕。
- BBLL 底部渐变控制栏、标题/分 P 信息、白色图标、紫色焦点/进度条、右上角时钟、右侧设置和居中选集弹层。
- DASH、画质、AAC 音轨、AVC/HEVC 选择、备用 CDN 线路；直播 v2 可用画质/线路和旧接口回退；0.25–2 倍速、五种画面缩放、字幕字号/默认字幕、控制按钮显示、底部进度、调试信息。
- 正序/倒序、单集循环、随机、播完停止，以及全部播完退出播放器。系统 MediaPlayer 保持 1 倍速。
- VOD protobuf 分段弹幕，支持滚动/顶部/底部固定；字号、透明度、速度、占屏比、密度、服务端 weight 等级、关键词过滤及固定弹幕开关。接口不兼容时回退 XML。
- Android 5.0+ 直播 WebSocket 弹幕、认证、心跳、zlib 解包、限量批处理和退避重连；4.x 使用近期弹幕接口每 3 秒轮询兼容宿主旧 TLS 的串行读写。后台停止、前台恢复，所有连接仍复用宿主 OkHttp。
- 遥控器方向/确认、MENU、播放暂停/快进快退/上下集媒体键；返回先逐层关闭弹层，播放中双击返回详情，再按页面堆栈返回。频道根页面双击返回交给宿主退出。

这是第二版，优先对齐 BBLL 的播放器与弹幕，完整差异见 [PARITY.md](PARITY.md)。仍未达到 BBLL/newBV 全部功能等价：会员渐变/高级脚本弹幕、T9 键盘、服务端播放进度同步等未实现；不提供发送评论/弹幕、多账号、离线下载。关注/取消关注和三连已接入确认操作，真实账号扫码确认及写操作尚未验证。账号、地区、付费及接口限制仍有效。

真实接口可读取推荐/详情并播放；未登录搜索在本次测试网络可能返回空结果或 `v_voucher` 验证要求，界面明确提示登录后重试或稍后重试，不会显示成“没有搜索结果”。

## 运行与性能

- 所有 API、图片、弹幕、字幕与 CDN 请求都使用宿主 `NetworkClient.sharedClient()` 的同一个实例。JAR 不创建其他 OkHttpClient、不携带重复 OkHttp/Okio/IJK 类或原生库。
- API 由 2 个有界工作线程处理，图片由 2 个线程处理；GridView 复用卡片，单列表最多 144 项，返回堆栈最多 12 层。
- 位图按显示尺寸采样，缓存为最大堆的 1/10 且不超过 6 MiB；历史最多 100 项、保存进度最多 256 项。
- MP4/FLV/HLS 通过带随机令牌的 `127.0.0.1` 流式代理接入，支持 Range/HEAD，以及 HLS 相对地址、Key/Map URI 重写；最大 4 条并发流、每条 32 KiB 传输缓冲、URL 表最多 1024 项。每次播放有独立会话，旧会话取消后才创建下一解码器，释放过程不阻塞 UI。
- DASH 视频与音频直接使用共享 OkHttp 的 DataSource，播放器目标缓冲 3–10 秒、目标大小 8 MiB；不包含 Exo UI、下载服务或额外原生库。
- 画面 SurfaceView、字幕 TextView、弹幕 Canvas。VOD 最多缓存 3 段、每段最多 6000 条，XML 最多 12000 条；同屏 8–48 条，默认 24 条；活动播放约 30fps，暂停/后台停止绘制定时器。直播消息队列最多 200 条、历史最多 400 条，每 100ms 批量提交 UI。
- 弹幕轨道按实际文字宽度和移动速度避碰，底部弹幕及字幕避让控制栏。播放器设置仅改变 View 和偏好，不启动 Activity。
- 直播自动尝试接口返回的备用线路，v2 流不可播放时回退旧直播接口；菜单显示接口实际可用画质，直播不显示可拖动进度条。
- 关闭频道会取消 HTTP、关闭代理端口、停止线程池和定时器、释放播放器与位图缓存。HTTP/2 响应在后台关闭，避免退出时在主线程发送 RST 导致崩溃。

## 构建

Python 3.9+、JDK 8、Android SDK Build Tools 30.0.3：

```powershell
python plugins/bl/build.py --sdk D:/android/sdk --java-home C:/path/to/jdk8
```

也可设置 `ANDROID_HOME`、`JAVA_HOME`。脚本只在 `.codex-tmp/bl/build` 写中间产物。依赖版本、SDK 来源及 SHA-256 固定在 `dependencies.json`，优先使用 Gradle 缓存，否则从 Maven Central、Google SDK 和宿主使用的 JCenter 镜像获取。ZXing 3.3.0 只从源码编译 QRCodeWriter 及其依赖，不包含摄像头或扫码 Activity。Exo core 经 `shrink.pro` 裁剪，无下载 Service/Activity 子类；只在 API 16+ 创建 DASH 引擎。ZIP 条目时间固定，方便复核构建产物。

输出 `dist/bl.jar`、`dist/bl.sha256`、`dist/example.m3u`。宿主接口为 API 1 的可选扩展，既有 Folia BrowserHost/DirectoryHost 不变。插件可使用 M3U 查询参数 `apiOrigin`、`passportOrigin`、`liveOrigin` 指向本地测试接口；正式订阅不包含这些参数。

## 来源

`reference/source.json` 记录 APK 哈希、提取布局和图标、newBV 提交。`reference/` 保存解码后的布局 XML 和图标 SVG，`assets/` 保存供旧 Android 使用的 PNG。见 `NOTICE`、`LICENSE-newBV`、`LICENSE-ZXing`、`LICENSE-ExoPlayer`；BBLL 图片/图标的原作者权利不因 newBV 的 MIT 许可而改变。

## 验证方式

`python plugins/bl/tests/verify_artifact.py` 检查 JAR 哈希、API 14 元数据、DEX 类归属，拒绝 Activity/WebView/重复宿主库。Android 实机测试入口是 `app/src/androidTest/java/xiao/bu/tv/BlPluginInstrumentation.java`。

使用独立测试包名构建（以下命令显式采用 Android 调试证书，不用于正式发布）：

```powershell
./gradlew.bat :app:assembleArm32Release :app:assembleArm32DebugAndroidTest -PapplicationIdOverride=xiao.bu.tv.bltest -PtestRunner=xiao.bu.tv.BlPluginInstrumentation -I plugins/bl/tests/test-signing.gradle
```

测试服务需要一段 40 秒 H.264 Baseline/AAC 的 MP4、开发 HTTPS 证书及私钥：

```powershell
python plugins/bl/tests/fixture.py --video path/to/video.mp4 --cert path/to/cert.pem --key path/to/key.pem
python plugins/bl/tests/run_device.py --serial DEVICE_SERIAL --package xiao.bu.tv.bltest --host-apk app/build/outputs/apk/arm32/release/app-arm32-release.apk --test-apk app/build/outputs/apk/androidTest/arm32/debug/app-arm32-debug-androidTest.apk
```

设备需可访问电脑局域网的 39626/39627 端口。加 `--real` 使用真实站点 API，`--dash` 测试 DASH，`--system` 强制走 API 14 系统播放器适配器。默认测试 IJK。DASH 测试需在视频旁准备 `video-only.mp4`（无音频）和 `audio-only.m4a`（无视频），例如用 ffmpeg 的 `-an -c:v copy` 和 `-vn -c:a copy` 分离，并加 `-movflags +faststart`。

结果及截图输出到 `output/bl/tests/`。测试覆盖共享客户端身份、触摸/遥控器、解码时间推进、媒体键、画质/连续分 P/备用线路/字幕、默认字幕和按钮显示设置、protobuf 弹幕实际绘制、zlib 直播弹幕、CDN 403 自动换线、后台暂停恢复、Range/HEAD、HLS URI 重写、二维码、历史、逐层返回及退出后的线程和端口关闭。`--smoke` 跳过解码器切换压力场景，只检查基础播放、面板、导航、直播弹幕和退出；不能代替完整回归。真实接口测试只做匿名读取，不操作真实账号。发布验证记录见 [VALIDATION.md](VALIDATION.md)。

UI 提取可复核：先用 apktool 解码用户 APK（`apktool d -s`），再运行 `python plugins/bl/extract_ui.py DECODED_DIR --sharp SHARP_MODULE`；只提取资源，没有导入 APK 的 DEX。
