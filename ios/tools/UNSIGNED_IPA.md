# 未签名真机 IPA

这条链在 **Mac + 完整 Xcode 的 iPhoneOS SDK** 上编译 Release 真机 App，随后手工打成 `Payload/Zhanguo.app` 格式的 IPA，不需要 Apple 账号、开发者 Team、证书或描述文件。未签名 IPA 不能直接安装到 iPhone，需要交给签名渠道按实际设备和分发方式签名。

保留完整 `ios` 工程和已准备的资源，进入 `ios` 后执行：

```bash
bash tools/build_unsigned_ipa.sh
```

默认每次生成独立 `ios/build/Zhanguo-unsigned-时间-随机值/`，包含 `Zhanguo-unsigned.ipa`、`unsigned-ipa-audit.json`、`Zhanguo.xcarchive`、Xcode 版本和构建日志。不覆盖旧 IPA、归档或源码 ZIP。可用 `IOS_UNSIGNED_BUILD_ROOT=/另一个目录` 指定工作目录。资源约 1.6 GB；归档、DerivedData 和 IPA 会同时占用磁盘，执行前应留足空间。

脚本先运行现有工程生成器和资源校验，再用 `xcodebuild archive` 指定 `Release`、`iphoneos`、`generic/platform=iOS`、`arm64`、`SKIP_INSTALL=NO` 和 `CODE_SIGNING_ALLOWED=NO`。签名 identity 和 Team 显式为空；不执行 `exportArchive`，不自动注册设备或访问开发者平台。`ALWAYS_EMBED_SWIFT_STANDARD_LIBRARIES=YES` 让 Xcode 决定需要拷贝的 Swift 支持库。

独立 Python 校验器检查：

- 编译后 `Info.plist` 为 iPhoneOS/APPL，SDK 元数据正确，最低系统不低于 16.4，主可执行文件与部署版本一致。
- 主程序是无签名的 arm64 `MH_EXECUTE`。每个嵌入 Mach-O 都是 `LC_BUILD_VERSION platform=iOS(2)`；同为 arm64 的模拟器平台 7 和 Catalyst 均拒绝。
- 实际动态库依赖经 `@rpath`、`@loader_path`、`@executable_path` 解析；缺失的非系统库、无效的嵌入 runtime 会失败。iOS16.4 已提供的 Swift 系统库可以使用 SDK 对应库声明，不要求把 `libswiftCore` 重复放进 App。
- App 不残留旧描述文件或 `_CodeSignature`。ZIP 保留 Unix mode 和安全的相对符号链接，拒绝绝对、失效或逃出 App 的链接；大资源流式读写，全部 ZIP 条目会再次读回检查 CRC。

审计通过证明所提供 App 的平台/格式/依赖及封装检查通过，不能代替 Xcode 编译和 iPhone 运行结果。签名方需要保留资源、嵌入库和执行权限，按正确顺序签名嵌入代码与 App，并使用匹配的 Bundle Identifier、entitlements 和 provisioning profile。签名后还要验收冷启动、真实 WebKit/Keychain、区服列表、登录、游戏协议、资源加载与性能。

离线测试可在 Windows 或 Mac 运行：

```bash
python3 tools/test_unsigned_ipa.py
```

测试使用明确标记的合成 Mach-O/SDK fixture，验证真机与 arm64 模拟器的区别、损坏边界、库依赖、旧输出保护、ZIP 内容和符号链接；它不生成可交付的真机 App，也不宣称执行了 Apple SDK。当前工程的首次 Xcode 编译、Apple 原生 XCTest、实际 IPA 产出和真机验收仍待 Mac 环境完成。
