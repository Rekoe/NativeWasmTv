# BL 原生 JAR 频道

BL 将用户提供的 BBLL 1.5.2 界面资源、布局层次与 newBV 的 Web API 合同移植到 nTv 的原生频道插件。唯一入口是 `BlPlugin.create()` 返回的根 View，页面、播放器、二维码和弹层均为其子 View；JAR 不注册 Activity、Fragment、Service 或 WebView。

## 导入

先安装本次配套宿主 APK；旧宿主没有 `ChannelPlugin.NativeHost.httpClient()`，单独替换 JAR 会提示更新宿主。正式包名、测试包名都可以使用相同的 JAR。

在线订阅：

https://raw.githubusercontent.com/buhanzhe/NativeWasmTv/master/plugins/bl/dist/example.m3u

`dist/example.m3u` 包含 HTTPS JAR 地址及 SHA-256。宿主下载、校验并缓存 DEX JAR。发布的订阅固定 JAR 所在的 Git 提交，避免 CDN 新旧缓存造成哈希不匹配。

## 兼容目标

| 项目 | 说明 |
| --- | --- |
| 最低目标 | Android 4.0 / API 14；使用 Google Android API 14 平台包编译 UI，并以 D8 `--min-api 14` 生成 DEX |
| 已实测设备 | Android 4.4.4 / API 19、Android 7.1.1 / API 25 |
| Android 4.0 实测 | 设备未连接，尚未完成真机验证；编译通过不等于全部 ROM、解码器都已经验证 |
| WebView | 完全不依赖 WebView，没有最低 WebView 版本要求 |
| API 14–15 播放 | `BlSystemPlayer`，只引用 API 14 的平台 MediaPlayer；视频经共享 OkHttp 的本机代理输入 |
| API 16+ 播放 | 默认复用宿主 IJK Java 和原生库，可在设置切换系统播放器 |
| ABI | JAR 不包含 `.so`；IJK 播放取决于宿主 APK 与设备 ABI 匹配。平台播放无需 JAR 携带原生库 |

没有直接打包 newBV 的 Compose/Media3 主应用，也没有把 BBLL 的 Activity 搬进 JAR。平台播放适配器特意避免 IJK `AndroidMediaPlayer` 中 API 16 `OnTimedTextListener` 的引用。

## 本版功能

- 推荐、热门、分区、番剧列表/剧集、直播列表及播放。
- 搜索视频/番剧/直播/UP，识别 BV 号与含 BV 号的链接；支持系统输入法和遥控器字母键盘。
- 详情、分 P、相关推荐、UP 投稿、评论阅读。
- 扫码登录、Cookie 本地保存、历史、收藏夹、稍后再看、关注列表、动态；账号接口需要有效登录。
- 点赞、收藏、投币确认、加入稍后再看。
- 视频播放、全屏控制、画质、分 P、进度、继续上次播放、连续分 P、VOD 弹幕和 JSON 字幕。
- 遥控器方向/确认、播放暂停媒体键；返回先关闭弹层，再返回上一层页面，频道根页面双击返回交给宿主退出。

这是首版原生移植，未达到 BBLL/newBV 全部功能等价。当前播放请求优先返回带音频的 MP4/FLV `durl`；尚未实现 DASH 音视频分离合成，账号/地区/付费/接口限制仍有效。不包含直播 WebSocket 弹幕、发送评论、发送弹幕、关注增删、完整服务端观看进度同步。扫码确认与账号写操作未使用真实账号验证。

真实接口可读取推荐/详情并播放；未登录搜索在本次测试网络可能返回空结果或 `v_voucher` 验证要求，界面明确提示登录后重试或稍后重试，不会显示成“没有搜索结果”。

## 运行与性能

- 所有 API、图片、弹幕、字幕与 CDN 请求都使用宿主 `NetworkClient.sharedClient()` 的同一个实例。JAR 不创建其他 OkHttpClient、不携带重复 OkHttp/Okio/IJK 类或原生库。
- API 由 2 个有界工作线程处理，图片由 2 个线程处理；GridView 复用卡片，单列表最多 144 项，返回堆栈最多 12 层。
- 位图按显示尺寸采样，缓存为最大堆的 1/10 且不超过 6 MiB；历史最多 100 项、保存进度最多 256 项。
- 媒体通过带随机令牌的 `127.0.0.1` 流式代理接入，支持 Range/HEAD，以及 HLS 相对地址、Key/Map URI 重写；最大 4 条并发流、每条 32 KiB 传输缓冲、URL 表最多 1024 项。
- 画面 SurfaceView、字幕 TextView、弹幕 Canvas。弹幕最多保留 12000 条、同时绘制 8 条，活动播放时约 30fps，暂停/后台停止绘制定时器。
- 关闭频道会取消 HTTP、关闭代理端口、停止线程池和定时器、释放播放器与位图缓存。HTTP/2 响应在后台关闭，避免退出时在主线程发送 RST 导致崩溃。

## 构建

Python 3.9+、JDK 8、Android SDK Build Tools 30.0.3：

```powershell
python plugins/bl/build.py --sdk D:/android/sdk --java-home C:/path/to/jdk8
```

也可设置 `ANDROID_HOME`、`JAVA_HOME`。脚本只在 `.codex-tmp/bl/build` 写中间产物。依赖版本、SDK 来源及 SHA-256 固定在 `dependencies.json`，优先使用 Gradle 缓存，否则从 Maven Central、Google SDK 和宿主使用的 JCenter 镜像获取。ZXing 3.3.0 只从源码编译 QRCodeWriter 及其依赖，不包含摄像头或扫码 Activity。ZIP 条目时间固定，方便复核构建产物。

输出 `dist/bl.jar`、`dist/bl.sha256`、`dist/example.m3u`。宿主接口为 API 1 的可选扩展，既有 Folia BrowserHost/DirectoryHost 不变。插件可使用 M3U 查询参数 `apiOrigin`、`passportOrigin`、`liveOrigin` 指向本地测试接口；正式订阅不包含这些参数。

## 来源

`reference/source.json` 记录 APK 哈希、提取布局和图标、newBV 提交。`reference/` 保存解码后的布局 XML 和图标 SVG，`assets/` 保存供旧 Android 使用的 PNG。见 `NOTICE`、`LICENSE-newBV`、`LICENSE-ZXing`；BBLL 图片/图标的原作者权利不因 newBV 的 MIT 许可而改变。

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

设备需可访问电脑局域网的 39626/39627 端口。加 `--real` 使用真实站点 API，`--system` 强制走 API 14 系统播放器适配器。结果及截图输出到 `output/bl/tests/`。测试覆盖共享客户端身份、触摸/遥控器、解码时间推进、媒体键、画质/分 P/字幕、后台暂停恢复、Range/HEAD、HLS URI 重写、二维码、历史、返回及退出后的线程和端口关闭。

UI 提取可复核：先用 apktool 解码用户 APK（`apktool d -s`），再运行 `python plugins/bl/extract_ui.py DECODED_DIR --sharp SHARP_MODULE`；只提取资源，没有导入 APK 的 DEX。
