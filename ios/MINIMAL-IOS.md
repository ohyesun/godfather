# iOS 最小包改造（r130）

r119~r126 的 IPA 都是**全量包**：`resources107.pak` 1,641,386,457 B 原样打进
`GameAssets/files/`，安装包 1.67 GB。这份改动把它换成"本地只留到登录界面为止
的资源，其余按需从微端服务器取"，与安卓 r129 是同一套口径。

**当前状态：源码与资源已就绪，但一行 Swift 都没有编译过，一个测试都没有执行过。**
Windows 上没有 Apple SDK，这一条在 Mac 构建通过之前不会改变。

## 1. 体积

| 部分 | 全量包 | 最小包 |
| --- | --- | --- |
| `resources107.pak` | 1,641,386,457 | **29,940,179** |
| `resources107-index.json` | 3,993,453 | 3,381,727 |
| 散文件（1,780 条） | 51,086,203 | 51,086,203（不变） |
| `assets-manifest.json` | 10,156,264 | 9,545,020 |
| **GameAssets 合计** | **1,706,622,377** | **93,953,129**（89.6 MiB） |
| 源包 ZIP（含源码） | 1.68 GB | **64,113,555** |

pak 里保留 1,887 条（首屏到登录为止），其余 80,457 条改成远端条目。
裁出来的 pak 与安卓 r129 的 `min/resources107.pak` **sha256 完全相同**：

```
6d92cadb3d24a8e3478d544768c2b8a766944d94b52188228687e650c5c462fe
```

两端裁出同一份字节，说明判定表落地一致，不是在各自解释同一份规则。

## 2. 运行期怎么取资源

`assets-manifest.json` 仍是完整的 82,344 条。区别只在 `packed` 的五个字段：

```jsonc
// 本地有：offset 指向裁剪后 pak 里的位置
"bridge/index.json": [123456, 4096, 8192, 1, "…"],
// 本地没有：offset = -1，其余四样原样保留
"scenes/1001.glb":   [-1,     265180, 913244, 1, "…"]
```

`offset = -1` 就是"向微端服务器取"的哨兵，与服务器索引、安卓客户端同一个值。
`storedBytes` / `decodedBytes` / `codec` / `sha256` 一条都不能少 —— 取回来之后
靠它们做长度、编码与哈希三重校验。

调用链：

```
WKURLSchemeHandler (zgasset://bundle/<path>)
  └ ZGAssetStore.asset(path)
      ├ 本地 direct / overlay / pak 段  → 直接读文件（与 r126 完全一致）
      └ offset = -1                     → ZGAssetSource(remoteId: path)
          └ ZGAssetReader.stream
              └ ZGMicroFetcher.resolve(remoteId:, storedSize:, decodedSize:, codec:, sha256:)
                  ├ 缓存命中  → 立刻返回本地 .bin
                  └ 未命中    → ZGMU/1 取 path:<path> → 落地 .part → 解码 → 校验 → .bin
```

新增四个文件：

| 文件 | 职责 |
| --- | --- |
| `Sources/Native/Micro/ZGMicroClient.swift` | ZGMU/1 线格式、连接、连接池（4 条） |
| `Sources/Native/Micro/ZGMicroFetcher.swift` | 缓存、断点续传、长度/sha256 校验、预算清理 |
| `ZhanguoTests/Micro/ZGMicroClientTests.swift` | 9 个离线用例，用内存假服务端 |
| `Resources/MicroClient.json` | 服务器地址/口令，与安卓同名同义 |

改动的既有文件只有两个：`ZGAssetStore.swift`（允许 `offset = -1`、按 path 生成远端源）
和 `ZGAssetReader.swift`（远端条目先落地再按普通本地资源走）。

几个刻意的取舍：

- **解码后才落缓存**。WebKit 发的是解码表示的 `Range` 请求，若每次 range 都重新
  解压，一个 24 MB 的 glb 会被解压几十次。现在解码一次落地成 `.bin`，后续全是本地 seek。
- **`.part` 才承载续传**。它存的是服务端原始存储字节（压缩仍是压缩），中断后从
  已有长度继续，不重来。
- **拒绝可执行资源**。`.js/.mjs/.html/.htm/.css/.svg` 一律不通过微端通道下发 ——
  裸 TCP 能下发脚本就等于给页面开了一个代码注入面。这些路径本来就在 `direct` 里（随包）。
- **失败不静默**。配置缺失、校验不符、长度不符都抛异常，WebKit 收到明确失败，
  而不是空流白屏。

## 3. 复现

裁剪（Windows 可跑，10 秒）：

```bash
cd ios
python tools/build_min_assets.py            # -> build/minimal/GameAssets
python tools/build_min_assets.py --verify-only   # 期望 verdict = PASS
```

`--verify-only` 会把每一条本地段重新解压、比对 sha256，并逐条复刻客户端
`ZGAssetStore.init` 的断言（路径合法性、codec 与长度关系、offset 是否越界、
source sha256 是否等于 r119 基线）。当前结果：

```json
{"verdict":"PASS","localEntries":1887,"remoteEntries":80457,
 "pakBytes":29940179,"indexEntries":82344,"problems":[]}
```

打包（源码 + 裁剪资产，不含 Apple SDK 产物）：

```bash
python tools/package_minimal_source.py      # -> ../output/zhanguo-ios-r130-minimal-source.zip
```

工程文件（新增源码与 `MicroClient.json` 后必须重跑）：

```bash
python tools/generate_project.py
python tools/verify_project.py
```

## 4. 在 Mac 上编译

```bash
unzip zhanguo-ios-r130-minimal-source.zip && cd ios
bash tools/check_on_mac.sh          # 编译 + XCTest（含本次新增的 9 个用例）
bash tools/build_unsigned_ipa.sh    # iPhoneOS Release / arm64 -> Zhanguo-unsigned.ipa
```

`check_on_mac.sh` 里的 XCTest 是这批代码的**第一次真编译**。预期要修的是
SDK 类型与并发标注这类问题（`SWIFT_STRICT_CONCURRENCY = minimal`），
不建议在没跑测试之前直接出 IPA。

## 5. 协议实测（Windows 上真连了一次服务器）

Swift 编译不了，但协议层可以在 Windows 上真跑：起一台本地 `zgmus`（207 个旧包用
NTFS 联接 + 全量 pak），再用一份**独立实现**的 Python 客户端按同一套线格式握手取件。

```bash
# 1) 搭合并目录（旧包 + pak107），起服
zgmus.exe --port 18099 --bind 127.0.0.1 --root <合并目录> \
  --manifest <patch-20261029.1/manifest.json> --token zgphs00 \
  --pak <resources107.pak> --pak-index <resources107-index.json> \
  --pak-files <pakfiles-manifest.json>

# 2) 探针
python tools/probe_micro_protocol.py --port 18099
```

实测结果（探针自己拼帧，不复用 iOS 代码，也不复用安卓的 Java 实现）：

| 检查项 | 结果 |
| --- | --- |
| 握手 `blockSize` / 清单长度 / 清单 sha256 | 65,536 / 128,840 B / `cb586314d72e1350…`，与信封文件一致 |
| 6 条远端样本（含 `native107/2047.glb`，24.4 MB 解压后） | 取回字节与 pak 原段 **sha256 全等** |
| 同一批样本解码后 | sha256 与清单 `decodedSHA256` **全等**，长度全等 |
| 索引 flag 与清单 codec | 6/6 一致 |
| 断点续传（`offset=485` 取 4096） | 返回 485 B，与全段同位置**逐字节相同** |
| 4 条连接并发取件 | 4/4 完整交付，长度全对 |
| 不存在的 id / 错误口令 | `0x0010 no such pack` / `0x0020 auth failed`，不挂连接 |

结论：**iOS 端"取回来 → 解码 → 用清单 sha256 校验"这条链在真实服务器上成立**，
帧偏移、END 位、虚拟 id 都与服务器对得上。仍未覆盖的是 Swift 那一侧的实现本身
（编译、并发、内存），那要等 Mac。

## 6. 验收清单

编译通过后必须按顺序走一遍，任何一步失败都别发：

1. 冷启动到区服列表 —— 这一屏全在本地包里，**断网也应该能到**；
2. 登录 → 选角 → 进场景 —— 这是第一次真正请求 `scenes/`（70,098 条，占远端
   1.53 GB 的 96%），此时才验证微端链路；
3. 第一张地图加载完成后的内存与发热；
4. 切后台再回来，缓存是否命中（第二次进同一张图不应该再走网络）；
5. 弱网（限速 1 Mbps / 200 ms RTT）下的首图加载时长；
6. 杀掉进程重进，`.part` 续传是否生效。

## 7. 未验证项（必须原样带进交付说明）

- **Swift 未编译**。本机没有 Apple SDK，`ZGMicroClient.swift` / `ZGMicroFetcher.swift`
  以及两个改动文件都只做过结构审查与括号平衡检查。
- **测试未执行**。9 个用例是为 Mac 上第一次 `xcodebuild test` 准备的，不是"已通过"。
- **Swift 侧的协议实现没有跑过**。帧格式本身已经用 Python 独立实现和真实服务器
  验过（见 §5），但 `ZGMicroClient.swift` 里那 500 行 Swift 代码是另一份实现，
  需要在 Mac 上用 `ZGMicroClientTests` 再验一遍。
- **真机零测试**。安卓侧也只在 MuMu 12 / Android 12 / x86_64 上跑过。
- **登录之后全链路未验证**。`scenes/` 与 `native107/` 共 76,780 条（占远端 96%）
  一条都没有在真实业务路径上被请求过。
- **服务器仍是待部署状态**。生产上的微端还是 20261028.1，`patch-20261029.1`
  在本轮开始时仍在传输。

## 8. 已知限制

- 远端条目**必须**有网络才能加载；离线只能到登录界面。这是最小包的固有代价，
  不接受就别用最小包（用 `package_source.py` 出全量包）。
- 缓存目录在 `Library/Caches/zgmicro`，系统在磁盘紧张时可能整体清掉，属预期行为。
- 服务端口令是明文写在 `MicroClient.json` 里的，与安卓一致；它不是玩家账号凭据，
  但改端口令要同时改两端。
- 3 路并发取资源（与 `ZGAssetSchemeHandler` 的 worker 数一致），连接池 4 条。
  首图时如果一次要几十个模型，会排队。
