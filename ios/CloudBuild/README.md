# 无本地 Mac：生成未签名真机 IPA

此目录是一个小型 Codemagic 构建仓库模板。完整游戏源码和资源另放在私有 HTTPS 下载地址，通过 SHA256 校验后取入临时构建目录，避免把约 1.68 GB 的资源包提交到普通 Git。

**当前模板尚未在云端执行，当前没有实际生成的 IPA。** 不需要在此流程里提供 Apple 账号、证书或私钥；生成的未签名 IPA 交给签名者处理，签名完成前不能直接安装。真实 Xcode 编译及原生 XCTest 若失败，必须先修复失败，不能把源码 ZIP 或模拟器 `.app` 改后缀当 IPA。

## 使用 Codemagic

1. 将本目录的**内容**复制到一个小型私有 Git 仓库根目录，使根目录有 `codemagic.yaml` 和 `scripts/fetch_source.py`，再由自己关联到 Codemagic。
2. 将新交付的 `zhanguo-ios-r119-cloud-ready-source.zip` 放到你授权的私有文件存储。需要能直接下载 ZIP 的 HTTPS 地址，可以是限时签名 URL，不是需要网页手工登录的网盘分享页。源码包包含客户端已有的列表服务认证配置，请按私有源码保管。
3. 在 Codemagic 的应用环境变量中创建组 **`ios_unsigned_source`**：

   | 变量 | 值 |
   | --- | --- |
   | `IOS_SOURCE_URL` | 该完整 ZIP 的私有 HTTPS 下载地址；勾选 Secret |
   | `IOS_SOURCE_SHA256` | 同名 `.json` 记录中的 `sha256`，64 位十六进制 |

4. 手工运行 **`zhanguo-ios-unsigned`**。配置使用 Apple Silicon Mac 和固定 Xcode 16.4，先验证源码、执行 Simulator 原生测试，再编译 **iPhoneOS Release / arm64**，封装未签名 IPA。未配置自动触发、签名、上架或外发通知。
5. 成功后在该次构建的 Artifacts 下载 `.ipa`、校验报告和构建日志。将 **IPA** 交给你的签名者；签名者需要用自己的有效证书和匹配描述文件签名，必要时调整 Bundle ID。源码包或这个小模板本身都不是安装包。

默认构建最长 120 分钟。使用前在自己的服务账户里确认可用额度；此模板不启用付费、不购买额度，也不保证某个账户有免费分钟。测试宿主带 `--zg-tests`，不会自动登录游戏或连接真实游戏服务；真机联网验收仍需另做。

## 如果签名者有 Mac

可以直接把完整源码包交给对方。解压、进入 `ios` 后运行：

```bash
bash tools/check_on_mac.sh
bash tools/build_unsigned_ipa.sh
```

两步都通过后，`ios/build` 中的未签名 IPA 可进入对方的签名流程。也可在配置好自己的 Team 后使用现有 `tools/export_ipa.sh` 直接导出签名版。构建脚本默认拒绝 Windows/Linux、模拟器产物、已有输出覆盖和错误平台的 Mach-O。

## 模板离线测试

```bash
python3 scripts/test_fetch_source.py
```

此测试使用构造的小 ZIP，不访问网络。覆盖下载 URL 错误信息去敏、SHA256、manifest、文件列表、路径越界、符号链接、大小上限和已有目标保护；不代替 Xcode 编译。

依据：[Codemagic 原生 iOS 无签名构建](https://docs.codemagic.io/yaml-quick-start/first-signed-build/)、[YAML 配置和构建产物](https://docs.codemagic.io/yaml-basic-configuration/yaml-getting-started/)、[环境变量](https://docs.codemagic.io/yaml-basic-configuration/configuring-environment-variables/)。
