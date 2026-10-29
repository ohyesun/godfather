# 战国 iOS 第一版工程（r119 基底）

这是一份已准备好游戏资源的原生 iOS 工程。当前 Windows 已完成源码、资源准备、JavaScript/协议契约与浏览器验证；**尚未经过 Xcode 编译、iOS Simulator XCTest 或 iPhone 实服验证，也没有生成可安装 IPA。**

## 先看这个：全量包还是最小包

仓库里这一份是**全量包**（`GameAssets` 带完整的 1.64 GB PAK，安装包约 1.67 GB）。
r130 起另有一条**最小包**路线：本地只留到登录界面为止的 1,887 条资源（PAK 29.9 MB），
其余 80,457 条改成按需从微端服务器取，源包 64 MB 左右。

- 改造内容、体积对比、验收清单、未验证项：[MINIMAL-IOS.md](MINIMAL-IOS.md)
- 裁剪与校验：`python tools/build_min_assets.py [--verify-only]`
- 出最小包源包：`python tools/package_minimal_source.py`

最小包需要微端服务器在线才能进游戏，全量包不需要。两条路线的源码是同一份，
差别只在 `GameAssets` 与 `MicroClient.json` 是否被使用。

## 工程包含的内容

- 原 r119 游戏画面、交互和协议 JavaScript。iOS 仅替换原生传输适配器，不改 Android 源码和原 APK。
- UIKit + WKWebView 容器，最低 iOS/iPadOS 16.4，iPhone/iPad 横屏，按安全区域布局。App 切到后台会取消连接，回来后由玩家点击重新进入，不伪装离线托管。
- `Lab.request95` 原调用契约、document nonce、主框架和入口校验。每个 TCP 连接持有自身 connectionId；旧页面、旧连接、迟到回包和异步解压队列均隔离。
- Network.framework 原始 TCP、列表服务器及注册协议；游戏 RC5/DES、帧处理、GBK 和 zlib 继续使用原 JavaScript。
- Keychain 异步存储。只允许设备能力和既有租约字段；不保存角色登录密码或普通游戏会话 token。未提供假同步离线存储或假成功返回。
- `zgasset://bundle/index.html` 同源资源服务。原 1.64 GB PAK 保持打包，每次请求独立读取/解压，64 KB 分块、三路资源并发、Range/HEAD 和取消处理。不是在手机首次启动时展开八万文件。
- 可直接打开的 `Zhanguo.xcodeproj`，包含原生 XCTest、Simulator 检查脚本和 IPA 导出脚本，无需 CocoaPods/XcodeGen。
- `tools/build_unsigned_ipa.sh`：无需证书的真机 Release 编译及 IPA 封装；`CloudBuild` 提供无本地 Mac 时可用的云构建模板。

原资源来源是签名 r119：
`62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44`。

`GameAssets` 保存 1,781 个 APK 直存资源与 82,344 个 PAK 记录。所有 PAK payload 已逐一流式解码并计算 SHA256，重名时 APK 直存补丁优先。iOS 的四个适配文件放在独立 `GameOverlay`，原始资源不覆盖。

## 在 Mac 上继续

需要已安装 iOS SDK 的 Xcode 和 Python 3.9 或更新版本。完整工程应保留 `Zhanguo/Resources/GameAssets`、`GameOverlay`、`NativeConfig.json`，不要只复制 Swift 文件。

1. 解压完整源码包，进入 `ios` 目录，先运行：

   ```bash
   bash tools/check_on_mac.sh
   ```

   脚本会校验资源、重新生成项目，选择本机可用 iPhone Simulator，编译并运行 XCTest。也可设置 `IOS_DESTINATION='platform=iOS Simulator,id=你的模拟器UDID'`。测试宿主带 `--zg-tests`，只运行假网络通道和本地 WK/Keychain 测试，不会自动进入游戏或连接实服。

2. 用 Xcode 打开 `Zhanguo.xcodeproj`，选择 `Zhanguo` target。在 Signing & Capabilities 选择自己的 Team；如自己的账号需要，修改 Bundle Identifier。连接 iPhone，选择该设备并运行。账号登录由本人完成。

3. 真机优先验收：冷启动区服列表 → 四区登录 → 选角色 → 进场景；再检查移动/技能/路障、自动索敌、摊位属性、宠物、电影43远景烽火台与点燃、结束后电影45和恢复控制。还需观察内存、资源加载、发热、切后台与重新进入。Windows 的测试不替代这些设备结果。

4. 真机验证完成、Apple 签名环境就绪后，可导出测试 IPA：

   ```bash
   DEVELOPMENT_TEAM=你的TeamID bash tools/export_ipa.sh
   ```

   默认导出方法为 `debugging`。已配好相应签名和分发方式时，可用 `IOS_EXPORT_METHOD=release-testing` 或 `app-store-connect`。脚本默认不让 Xcode自动改开发者平台配置；需要时显式设置 `IOS_ALLOW_PROVISIONING_UPDATES=1`。Archive、ExportOptions 和 IPA 输出到 `ios/build`，不覆盖旧归档。

本阶段以开发安装和运行验收为目标，尚未进行 App Store 提交、商店图标/元数据和审核材料准备。

## 已有签名渠道：先生成未签名 IPA

你不需要在编译阶段提供 Apple 签名证书。把完整工程交给有 Mac/Xcode 的构建方，先通过原生测试，再运行：

```bash
bash tools/build_unsigned_ipa.sh
```

脚本编译 `iPhoneOS Release / arm64`，在新的 `build/Zhanguo-unsigned-时间-随机值/` 目录输出 `Zhanguo-unsigned.ipa` 和 `unsigned-ipa-audit.json`。它会拒绝模拟器平台、错误的 Mach-O、缺失依赖和签名残留；封装保留执行权限。此 IPA **需要由你的渠道签名后安装**。详见 [未签名 IPA 说明](tools/UNSIGNED_IPA.md)。

没有本地 Mac 时，用 [CloudBuild/README.md](CloudBuild/README.md) 的小型私有仓库模板，在自己的 Codemagic 账户手工运行。完整资源 ZIP 通过你授权的私有 HTTPS 地址和 SHA256 取入，仓库本身不含 1.68 GB 大资源或原生认证配置；不需要 Apple 账号、签名证书或私钥。模板没有执行过云端构建，也没有生成或发布 IPA。

本次为该链补了两处资源服务初始化修正，避免 Swift 对象初始化完成前通过存储属性访问 `self`。本机已运行构造 Mach-O/ZIP 和云源码下载校验的离线测试；Windows 无法验证 Xcode 编译、实际 Apple SDK 链接和 iPhone 运行，必须等真实构建日志确认。

## 已完成的本地验证

- `tests/asset-preparation-audit.json`：完整原资源解码核验、原 PAK 和直存资源不变、准备器幂等/越界/截断等负例。
- `tests/js/contract-report.json`：直接执行真实生成模块，验证 GBK、RC5 压缩碎帧、DES 会话、旧连接/旧页面/旧解码队列隔离；WK 和原生响应明确使用测试输入。
- `tests/js/cold-start-report.json`：实际浏览器两种尺寸加载真实入口/游戏代码和193次资源请求；动态七区解析、PNG 解码、缺原生桥的明确失败状态。没有登录账号或打开游戏 TCP。
- `tests/review/host-bridge-review.md`：独立检查消息字段、Keychain 接线、主文档换 nonce、旧导航异步销毁和弹框 once completion。
- `tests/project-verification.json`：资源与 overlay hash、Swift/Test 文件收录、Info.plist/Storyboard/Scheme 结构和私有原生配置存在性验证。

`ZhanguoTests` 中的原生网络假通道、真实 WK custom scheme/ESM/fetch/纹理/音频/wasm、Keychain 往返用例已经写好，**当前未执行**。首次 Mac 编译若发现 SDK 类型、链接或 WebKit 行为差异，应按实际构建日志修复后再出 IPA。

## 复现与目录

- `tools/prepare_assets.py`：从固定 r119 APK 提取和验证不可变资源。已有目录仅验证或拒绝，不覆盖未知文件。
- `Web/patch_client.py`：生成 iOS 独立 overlay，不修改原资源。
- `tools/prepare_native_config.py`：从现有原生容器读取已有列表服务器配置/认证字段，仅写本地 NativeConfig，不输出认证值；不读取玩家密码。
- `tools/generate_project.py`：新增 Swift 文件后重新生成 Xcode project；不修改业务源码。
- `tools/verify_project.py`：完整工程与资源自检，可在 Windows 或 Mac 运行，不宣称执行了 Apple SDK。

平台依据：[WKWebView](https://developer.apple.com/documentation/webkit/wkwebview)、[WKURLSchemeHandler](https://developer.apple.com/documentation/webkit/wkurlschemehandler)、[Network.framework](https://developer.apple.com/documentation/network/nwconnection)、[Safari 16.4 的 Compression Streams 与 import maps](https://webkit.org/blog/13966/webkit-features-in-safari-16-4/)。
