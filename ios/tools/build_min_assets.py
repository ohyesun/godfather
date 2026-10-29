"""把 iOS 的 GameAssets 裁成"最小可用包"：本地只留到登录界面为止的资源，
其余条目改成 offset = -1（"不在本地、按 path 向微端服务器取"），与安卓 r129
用同一套判定表，保证两端对同一个路径给出同一个结论。

三条不可让步的约束：

* 不重编码。保留下来的每一段都是从原 pak 原样复制的存储字节，落地后与
  签名 pak 逐字节相同；
* 清单仍是完整的 82,344 条。远端条目只把 offset 换成 -1，storedSize /
  decodedBytes / codec / sha256 全部保留 —— 客户端取回来之后要靠这四样做
  长度、编码与哈希校验；
* 被散文件遮蔽的 pak 条目一律丢。客户端 `direct` 优先，这些段就算留着
  也永远不会被读到（iOS 有 1,722 条属于这种情况）。

用法：
    python build_min_assets.py --source <GameAssets> --out <out-dir>
    python build_min_assets.py --verify-only --out <out-dir>
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
import shutil
import sys
import time
import zlib
from pathlib import Path

# 判定表与 E:/zg-m9-work/build_min_pak.py 逐项一致：两端必须裁出同一个集合。
BOOT_PREFIXES = ("bridge/", "ui111/", "ui110/", "novice114/", "icons/", "navigation109/",
                 "characters84/", "companions84/", "season84/", "vendor/")
BOOT_FILES = {"scenes/catalogue.json"}
HEAVY_PREFIXES = ("scenes/", "native107/", "audio107/")

REMOTE_OFFSET = -1
CHUNK = 1 << 20
BASE_APK_SHA = "62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44"


def valid_path(path: str) -> bool:
    """复刻 ZGAssetStore.validPath：绝对路径、反斜杠、盘符、控制字符、.. 全部拒绝。"""
    if not path or path.startswith("/") or "\\" in path or ":" in path:
        return False
    if any(ord(ch) < 32 or ord(ch) == 127 for ch in path):
        return False
    for part in path.split("/"):
        if not part or part in (".", "..") or part.endswith(".") or part.endswith(" "):
            return False
    return True


class BuildError(Exception):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(CHUNK)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def decode(blob: bytes, codec: int) -> bytes:
    if codec == 1:
        return gzip.decompress(blob)
    if codec == 2:
        return zlib.decompress(blob)
    return blob


def boot_policy(path: str) -> str | None:
    if path in BOOT_FILES:
        return "boot-file"
    if path.startswith(HEAVY_PREFIXES):
        return None
    if path.startswith(BOOT_PREFIXES):
        return "boot-prefix"
    return "boot-catchall"


def build(source: Path, out: Path) -> dict:
    started = time.time()
    manifest_path = source / "assets-manifest.json"
    files_root = source / "files"
    pak_path = files_root / "resources107.pak"
    index_path = files_root / "resources107-index.json"
    for required in (manifest_path, pak_path, index_path):
        if not required.is_file():
            raise BuildError("缺少输入文件：" + str(required))

    manifest = json.loads(manifest_path.read_text("utf-8"))
    packed: dict[str, list] = manifest["packed"]
    direct: dict[str, list] = manifest["direct"]
    index = json.loads(index_path.read_text("utf-8"))
    pak_size = pak_path.stat().st_size

    if len(index) != len(packed):
        raise BuildError("索引 %d 条与清单 %d 条不一致" % (len(index), len(packed)))

    kept: dict[str, list] = {}
    dropped: dict[str, list] = {}
    shadowed: dict[str, dict] = {}
    for path, record in packed.items():
        offset, stored, size, codec, digest = record
        why = boot_policy(path)
        if why and path in direct:
            # 散文件优先：pak 里这份永远读不到，留着纯属占体积。
            shadowed[path] = {"reason": why, "storedBytes": stored,
                              "directBytes": direct[path][0], "codec": codec}
            why = None
        if why:
            kept[path] = [offset, stored, size, codec, digest, why]
        else:
            dropped[path] = [offset, stored, size, codec, digest]
    if set(kept) & set(dropped):
        raise BuildError("keep/drop 集合重叠")

    if out.exists():
        shutil.rmtree(out)
    (out / "files").mkdir(parents=True)

    # ---- 1) 其余散文件用硬链接复用：同卷不占空间，zip 时读到的仍是真实字节 ----
    linked = copied = 0
    for entry in files_root.iterdir():
        if entry.name in ("resources107.pak", "resources107-index.json"):
            continue
        target = out / "files" / entry.name
        if entry.is_dir():
            shutil.copytree(entry, target)
            copied += 1
            continue
        try:
            os.link(entry, target)
            linked += 1
        except OSError:
            shutil.copy2(entry, target)
            copied += 1

    # ---- 2) 重排保留段，写新 pak ----
    new_pak = out / "files" / "resources107.pak"
    new_index: dict[str, list] = {}
    failures: list[dict] = []
    cursor = 0
    stored_total = 0
    decoded_total = 0
    codes = {0: 0, 1: 0, 2: 0}
    reasons = {"boot-prefix": 0, "boot-file": 0, "boot-catchall": 0}
    by_prefix: dict[str, dict] = {}

    with pak_path.open("rb", buffering=0) as src, new_pak.open("wb", buffering=0) as dst:
        for path in sorted(kept):
            offset, stored, size, codec, digest, why = kept[path]
            if offset < 0 or offset + stored > pak_size:
                raise BuildError("原始 offset 越界：" + path)
            src.seek(offset)
            blob = src.read(stored)
            if len(blob) != stored:
                raise BuildError("短读 %s %d/%d" % (path, len(blob), stored))
            try:
                decoded = decode(blob, codec)
            except Exception as exc:  # noqa: BLE001 - 记录具体条目
                failures.append({"path": path, "error": "解压失败：" + str(exc)})
                decoded = b""
            actual = sha256_bytes(decoded)
            if len(decoded) != size or actual != digest:
                failures.append({"path": path, "expectedBytes": size, "actualBytes": len(decoded),
                                 "expectedSha": digest, "actualSha": actual})
            dst.write(blob)
            new_index[path] = [cursor, stored, codec]
            kept[path] = [cursor, stored, size, codec, digest]
            cursor += stored
            stored_total += stored
            decoded_total += size
            codes[codec] = codes.get(codec, 0) + 1
            reasons[why] += 1
            group = by_prefix.setdefault(path.split("/")[0] if "/" in path else "(root)",
                                         {"entries": 0, "storedBytes": 0})
            group["entries"] += 1
            group["storedBytes"] += stored

    # ---- 3) 远端条目：offset = -1，其余四元组原样保留 ----
    for path, record in dropped.items():
        offset, stored, size, codec, digest = record
        new_index[path] = [REMOTE_OFFSET, stored, codec]
        dropped[path] = [REMOTE_OFFSET, stored, size, codec, digest]

    if failures:
        raise BuildError("保留段校验失败 %d 条，首条：%s" % (len(failures), failures[0]))

    new_pak_size = new_pak.stat().st_size
    if new_pak_size != cursor:
        raise BuildError("新 pak 尺寸 %d 与游标 %d 不一致" % (new_pak_size, cursor))

    new_index_path = out / "files" / "resources107-index.json"
    new_index_path.write_text(json.dumps(new_index, ensure_ascii=False, separators=(",", ":")),
                              encoding="utf-8")

    # ---- 4) 改写清单 ----
    merged: dict[str, list] = {}
    merged.update(kept)
    merged.update(dropped)
    if len(merged) != len(packed):
        raise BuildError("改写后条目数变化 %d -> %d" % (len(packed), len(merged)))

    pak_sha = sha256_file(new_pak)
    index_sha = sha256_file(new_index_path)
    manifest["packed"] = dict(sorted(merged.items()))
    manifest["direct"]["resources107.pak"] = [new_pak_size, pak_sha]
    manifest["direct"]["resources107-index.json"] = [new_index_path.stat().st_size, index_sha]
    summary = manifest.setdefault("summary", {})
    summary["directBytes"] = sum(row[0] for row in manifest["direct"].values())
    summary["decodedPackedBytes"] = sum(row[2] for row in manifest["packed"].values())
    manifest["minimal"] = {
        "mode": "minimal",
        "policy": "boot-to-login（与安卓 r129 同一判定表）",
        "localEntries": len(kept),
        "remoteEntries": len(dropped),
        "shadowedByDirect": len(shadowed),
        "localPakBytes": new_pak_size,
        "localStoredBytes": stored_total,
        "localDecodedBytes": decoded_total,
        "remoteStoredBytes": sum(row[1] for row in dropped.values()),
        "remoteIdScheme": "path:<pak 内路径>",
        "remoteServesStoredBytes": True,
        "sourcePak": {"bytes": pak_size, "sha256": sha256_file(pak_path)},
        "localCodes": {str(k): v for k, v in codes.items()},
    }
    manifest_raw = (json.dumps(manifest, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf-8")
    (out / "assets-manifest.json").write_bytes(manifest_raw)

    marker = {
        "tool": "zg-ios-minimal-assets-v1",
        "manifestSHA256": sha256_bytes(manifest_raw),
        "apkSHA256": manifest["source"]["apkSHA256"],
        "minimal": True,
    }
    (out / ".zg-assets-owned.json").write_text(json.dumps(marker, sort_keys=True) + "\n", "utf-8")

    report = {
        "mode": "minimal",
        "source": str(source),
        "output": str(out),
        "indexEntries": len(index),
        "localEntries": len(kept),
        "remoteEntries": len(dropped),
        "shadowedByDirect": len(shadowed),
        "localPakBytes": new_pak_size,
        "localDecodedBytes": decoded_total,
        "remoteStoredBytes": manifest["minimal"]["remoteStoredBytes"],
        "localPakSha256": pak_sha,
        "indexBytes": new_index_path.stat().st_size,
        "indexSha256": index_sha,
        "manifestBytes": len(manifest_raw),
        "manifestSha256": marker["manifestSHA256"],
        "fullPakBytes": pak_size,
        "reasons": reasons,
        "localCodes": {str(k): v for k, v in codes.items()},
        "localBytesByPrefix": dict(sorted(by_prefix.items(), key=lambda kv: -kv[1]["storedBytes"])),
        "linkedFiles": linked,
        "copiedFiles": copied,
        "seconds": round(time.time() - started, 1),
    }
    (out / "minimal-report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), "utf-8")
    return report


def verify(out: Path) -> dict:
    manifest = json.loads((out / "assets-manifest.json").read_text("utf-8"))
    marker = json.loads((out / ".zg-assets-owned.json").read_text("utf-8"))
    files_root = out / "files"
    pak_path = files_root / "resources107.pak"
    index_path = files_root / "resources107-index.json"
    manifest_path = out / "assets-manifest.json"
    problems: list[str] = []

    if marker["manifestSHA256"] != sha256_file(manifest_path):
        problems.append("清单哈希与 .zg-assets-owned.json 不符")

    pak_size = pak_path.stat().st_size
    record = manifest["direct"].get("resources107.pak")
    if not record:
        problems.append("清单缺少 direct/resource107.pak 记录")
    elif record[0] != pak_size or record[1] != sha256_file(pak_path):
        problems.append("本地 pak 与清单记录不符")

    index = json.loads(index_path.read_text("utf-8"))
    if len(index) != len(manifest["packed"]):
        problems.append("索引 %d 条与清单 %d 条不一致" % (len(index), len(manifest["packed"])))

    local = remote = 0
    covered = 0
    expected_offset = 0
    decode_failures: list[str] = []
    with pak_path.open("rb") as stream:
        for path, entry in index.items():
            offset, stored, flag = entry
            packed = manifest["packed"].get(path)
            if packed is None:
                problems.append("清单缺少 packed 记录：" + path)
                continue
            if [offset, stored, flag] != [packed[0], packed[1], 0 if packed[3] == 0 else 1]:
                if packed[3] == 2:
                    problems.append("zlib 条目无法用索引 flag 表达：" + path)
                    continue
                problems.append("索引与清单不一致：" + path)
                continue
            if offset == REMOTE_OFFSET:
                remote += 1
                continue
            local += 1
            if offset != expected_offset:
                problems.append("本地段不连续：%s（%d != %d）" % (path, offset, expected_offset))
            expected_offset = offset + stored
            covered += stored
            stream.seek(offset)
            blob = stream.read(stored)
            try:
                decoded = decode(blob, packed[3])
            except Exception as exc:  # noqa: BLE001
                decode_failures.append(path + "：" + str(exc))
                continue
            if len(decoded) != packed[2] or sha256_bytes(decoded) != packed[4]:
                problems.append("段内容与清单不符：" + path)

    if covered != pak_size:
        problems.append("本地段总长 %d 与 pak 尺寸 %d 不符" % (covered, pak_size))

    for name, (size, digest) in manifest["direct"].items():
        target = files_root / name
        if not target.is_file():
            problems.append("散文件缺失：" + name)
        elif target.stat().st_size != size or sha256_file(target) != digest:
            problems.append("散文件与清单不符：" + name)

    # ---- 客户端初始化会做的断言，这里逐条复刻一遍 ----
    if manifest.get("schemaVersion") != 1:
        problems.append("schemaVersion 不是 1")
    if manifest["source"]["apkSHA256"] != BASE_APK_SHA:
        problems.append("source.apkSHA256 与客户端硬编码的 r119 基线不符")
    if "index.html" not in manifest["direct"]:
        problems.append("缺少入口 index.html")

    for name, record in manifest["direct"].items():
        if not valid_path(name):
            problems.append("direct 路径非法：" + name)
        if len(record) != 2 or len(record[1]) != 64 or any(c not in "0123456789abcdefABCDEF" for c in record[1]):
            problems.append("direct 记录格式不符：" + name)

    for name, record in manifest["packed"].items():
        if len(record) != 5:
            problems.append("packed 记录长度不是 5：" + name)
            continue
        offset, stored, size, codec, digest = record
        if not valid_path(name):
            problems.append("packed 路径非法：" + name)
        if offset < REMOTE_OFFSET:
            problems.append("offset 小于 -1：" + name)
        if stored <= 0 or size < 0 or size > 512 * 1024 * 1024:
            problems.append("长度字段越界：" + name)
        if codec not in (0, 1, 2):
            problems.append("codec 非法：" + name)
        if codec == 0 and stored != size:
            problems.append("identity 条目 stored != decoded：" + name)
        if len(digest) != 64 or any(c not in "0123456789abcdefABCDEF" for c in digest):
            problems.append("sha256 格式不符：" + name)
        if offset >= 0 and (offset + stored > pak_size):
            problems.append("本地段越界：" + name)

    remote_count = sum(1 for row in manifest["packed"].values() if row[0] == REMOTE_OFFSET)

    return {
        "output": str(out),
        "verdict": "PASS" if not problems else "FAIL",
        "localEntries": local,
        "remoteEntries": remote,
        "remoteRecords": remote_count,
        "pakBytes": pak_size,
        "indexEntries": len(index),
        "manifestBytes": manifest_path.stat().st_size,
        "clientInitAssertions": "replicated",
        "problems": problems[:50],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path,
                        default=Path(__file__).resolve().parents[1] / "Zhanguo/Resources/GameAssets")
    parser.add_argument("--out", type=Path,
                        default=Path(__file__).resolve().parents[1] / "build/minimal/GameAssets")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    try:
        if args.verify_only:
            result = verify(args.out)
            print(json.dumps(result, ensure_ascii=False, indent=2))
            raise SystemExit(0 if result["verdict"] == "PASS" else 1)
        print(json.dumps(build(args.source, args.out), ensure_ascii=False, indent=2))
    except (BuildError, OSError) as error:
        print("构建失败：" + str(error), file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == "__main__":
    main()
