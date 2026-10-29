"""把 iOS 工程打成"最小包源包"：源码 + 裁剪后的 GameAssets + 客户端补丁层。

与 package_source.py 的差别只有一处 —— `Zhanguo/Resources/GameAssets` 用
`build_min_assets.py` 产出的裁剪版替换。其余（源码、GameOverlay、NativeConfig、
MicroClient.json）原样带走，因此在 Mac / 云构建上解压即编，不需要再跑一遍裁剪。

产物不是 IPA：这里没有 Apple SDK，也没有签名。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path

IOS = Path(__file__).resolve().parents[1]
OUTPUT = IOS.parent / "output/zhanguo-ios-r130-minimal-source.zip"
EXCLUDED_PARTS = {"__pycache__", "DerivedData", "build", "xcuserdata", ".DS_Store"}
ASSET_PREFIX = "Zhanguo/Resources/GameAssets"


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for part in iter(lambda: stream.read(1 << 20), b""):
            value.update(part)
    return value.hexdigest()


def package(minimal: Path, output: Path = OUTPUT) -> dict:
    output = Path(output).resolve()
    if output.exists():
        raise FileExistsError("不覆盖已交付的源包：" + str(output))
    if not (minimal / "assets-manifest.json").is_file():
        raise FileNotFoundError("先跑 build_min_assets.py：" + str(minimal))

    entries: list[tuple[Path, str]] = []
    for path in sorted(IOS.rglob("*")):
        relative = path.relative_to(IOS)
        if any(part in EXCLUDED_PARTS for part in relative.parts):
            continue
        if relative.as_posix() == "source-manifest.json":
            continue
        if relative.as_posix().startswith(ASSET_PREFIX + "/") or relative.as_posix() == ASSET_PREFIX:
            continue
        if path.is_symlink():
            raise ValueError("不打包符号链接：" + str(relative))
        if path.is_file():
            entries.append((path, "ios/" + relative.as_posix()))

    for path in sorted(minimal.rglob("*")):
        if path.is_symlink():
            raise ValueError("裁剪目录里有符号链接：" + str(path))
        if path.is_file():
            relative = path.relative_to(minimal).as_posix()
            entries.append((path, "ios/" + ASSET_PREFIX + "/" + relative))

    files = {name: [path.stat().st_size, digest(path)] for path, name in entries}
    manifest = {
        "format": 1,
        "artifactKind": "complete-Xcode-source-with-minimal-game-assets",
        "baseRevision": "r119",
        "assetMode": "minimal",
        "appleSDKBuildExecuted": False,
        "installableIPA": False,
        "files": files,
    }
    manifest_path = IOS / "build/source-manifest-minimal.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    entries.append((manifest_path, "ios/source-manifest.json"))

    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "x", allowZip64=True) as archive:
        for path, name in entries:
            # pak 与已压缩的图集再走一次 DEFLATE 只会烧 CPU，直接存储。
            stored = path.suffix.lower() in (".pak", ".glb", ".png", ".jpg", ".jpeg", ".mp3") \
                or path.stat().st_size > 64 << 20
            archive.write(path, name,
                          compress_type=zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED,
                          compresslevel=6 if not stored else None)

    record = {
        "archive": str(output),
        "sha256": digest(output),
        "bytes": output.stat().st_size,
        "fileCount": len(entries),
        "assetMode": "minimal",
        "gameAssetsManifest": json.loads((minimal / "assets-manifest.json").read_text("utf-8")).get("minimal"),
        "installableIPA": False,
        "appleSDKBuildExecuted": False,
        "gameAccountOrLiveServerTested": False,
    }
    output.with_suffix(".json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--minimal", type=Path, default=IOS / "build/minimal/GameAssets")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    print(json.dumps(package(args.minimal, args.output), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
