# 战国 iOS 最小包 · Codemagic 云端构建仓库

这个仓库只为一件事存在：**在一台没有 Mac 的机器上，把 iOS 客户端真正编译出来**。
上一轮（r120）就是用 Codemagic 跑通全量包，产出 1,676,135,986 B 未签名 IPA、
21 个 XCTest 全过。这一轮（r130 最小包）沿用同一条路，但省掉了当时那一整套
1.67 GB 分片加密传输 —— 资源现在只有 93,953,129 B，直接进仓库就行。

## 仓库里是什么

```
codemagic.yaml          # 两条工作流，见下
ios/                    # 完整可构建的 iOS 源树（112 MB 工作区，含最小资源）
  Zhanguo/Sources/Native/Micro/ZGMicroClient.swift     # ZGMU/1 协议客户端（新增）
  Zhanguo/Sources/Native/Micro/ZGMicroFetcher.swift    # 抓取/缓存/续传/校验（新增）
  Zhanguo/Sources/Native/Assets/ZGAssetStore.swift     # 改：允许 offset = -1 远端条目
  Zhanguo/Sources/Native/Assets/ZGAssetReader.swift    # 改：远端条目先落地再按 codec 解
  Zhanguo/Resources/MicroClient.json                   # 微端地址与口令
  Zhanguo/Resources/GameAssets/                        # 最小资产：pak 29,940,179 B
  ZhanguoTests/Micro/ZGMicroClientTests.swift          # 9 个用例，内存假服务端
  tools/cloud_step.sh                                  # 步骤日志 + 编译错误摘要
  tools/check_on_mac.sh                                # 模拟器原生测试（原样）
  tools/build_unsigned_ipa.sh                          # 未签名 IPA（原样）
verify_cloud_repo.py                                   # 推送前的本地自检
```

资源是从 Android `resources107.pak` 切出来的同一批字节，pak 的 sha256 是
`6d92cadb3d24a8e3478d544768c2b8a766944d94b52188228687e650c5c462fe`，与安卓 r129 最小包完全相同。

## 两条工作流

| 工作流 | 干什么 | 大概多久 | 什么时候用 |
| --- | --- | --- | --- |
| `swift-compile` | 只编译 App 和测试，不跑、不打包 | 4–8 分钟 | **先用它迭代编译错误** |
| `zhanguo-ios-minimal` | 跑模拟器 XCTest + 打未签名 IPA | 15–25 分钟 | 编译干净后再跑 |

两条都用 `mac_mini_m2` / Xcode 16.4，与 r120 那次一致。没有签名、没有 Apple 账号、
没有自动触发、没有上架。

## 操作步骤

1. 在 GitHub 建一个**私有空仓库**（名字随意，例如 `zhanguo-ios-cloud`）。
2. 解压 `zhanguo-ios-codemagic-r130.zip`，进入解压后的目录，然后：

   ```bash
   git remote add origin git@github.com:<你的账号>/zhanguo-ios-cloud.git
   git push -u origin main
   ```

   仓库里已经有一次提交，直接推即可。资源文件是被 `ios/.gitignore` 忽略后强制加入的，
   这是有意的：`.gitignore` 那几条是为了保护本机那份 1.67 GB 全量资源，不是针对这里。

3. Codemagic → Add application → 选这个仓库。它会自动读到根目录的 `codemagic.yaml`。
4. 先手工跑 **`swift-compile`**。
5. 如果失败：下载 artifact 里的 `ios/build/logs/compile.errors.txt` 发我。
   这个文件是去重后的 `error:` 行 + 最后 40 行，足够定位。
6. 编译干净后跑 **`zhanguo-ios-minimal`**，artifact 里拿 `Zhanguo-unsigned.ipa`
   和 `unsigned-ipa-audit.json`。

## 必须说清的三件事

1. **这份 Swift 从来没编译过。** 协议格式本身是对的 —— 用独立实现的 Python 客户端
   对真实 `zgmus` 服务器验过：6 条远端样本（含 24.4 MB 的 `native107/2047.glb`）
   取回字节与 pak 原段 sha256 全等，断点续传、4 连接并发、错误码全部正确。
   但 `ZGMicroClient.swift` 是另一份实现，第一次 `swiftc` 就是它的第一次体检，
   预期有 SDK 类型、并发标注、可选值解包一类的错要修。这就是为什么先跑 `swift-compile`。
2. **产出的 IPA 是未签名且 1676 MB 级别的**（资源少了 1.6 GB 后会小很多）。
   它不能直接安装，仍要过你的签名渠道。
3. **编译通过 ≠ 能进游戏。** 登录之后的链路（`scenes` 70,098 条、`native107` 6,682 条，
   占 pak 87.8%）没有一条在真实业务路径上被请求过，真机也没有任何测试。

## 推送前自检

```bash
python3 verify_cloud_repo.py
```

它检查 YAML 结构、资源完整性、以及云上那两道前置校验（`generate_project.py`
和 `verify_project.py`）能不能过 —— 后两道和 `check_on_mac.sh` 里跑的是同两个脚本，
在这里过不了的，云上一定过不了。
