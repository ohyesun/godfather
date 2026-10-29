#!/usr/bin/env python3
"""Prepare immutable iOS assets from the exact signed r119 Android payload.

The source APK is never edited. Direct assets override identically named PAK
members, matching the patched Android package. The 1.64 GB PAK stays packed;
every indexed, decoded payload is streamed and hashed once during preparation.
Only this invocation's newly created staging directory can be removed on error.
An existing destination is verified or rejected, never overwritten or cleaned.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import tempfile
import zipfile
import zlib

R119_SHA256 = "62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44"
ROOT = Path(__file__).resolve().parents[2]
DEFAULT_APK = ROOT / "output/apk114-direct-game-45.124.77.131-login-r119.apk"
DEFAULT_DESTINATION = ROOT / "ios/Zhanguo/Resources/GameAssets"
MANIFEST = "assets-manifest.json"
OWNERSHIP = ".zg-assets-owned.json"
CHUNK = 256 * 1024
MAX_DECODED_PAYLOAD = 512 * 1024 * 1024


class AssetError(ValueError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while part := stream.read(CHUNK):
            digest.update(part)
    return digest.hexdigest()


def safe_name(name: str) -> str:
    if (not isinstance(name, str) or not name or "\\" in name or ":" in name or
            any(ord(c) < 32 or ord(c) == 127 for c in name) or name.startswith("/") or
            any(part in ("", ".", "..") for part in name.split("/")) or
            any(part.endswith((" ", ".")) for part in name.split("/"))):
        raise AssetError("Unsafe asset path: " + repr(name))
    if str(PurePosixPath(name)) != name:
        raise AssetError("Noncanonical asset path: " + repr(name))
    return name


def within(root: Path, name: str) -> Path:
    safe_name(name)
    target = root.joinpath(*name.split("/"))
    if not target.resolve().is_relative_to(root.resolve()):
        raise AssetError("Asset path escapes its managed root")
    return target


def unique_json(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise AssetError("Duplicate JSON path: " + str(key))
        result[key] = value
    return result


def indexed_payload(stream, offset: int, length: int, codec: int):
    """Yield decoded bytes in bounded chunks; reject truncated/trailing frames."""
    stream.seek(offset)
    remaining = length
    decoder = zlib.decompressobj(31 if codec == 1 else 15) if codec else None
    size = 0
    while remaining:
        part = stream.read(min(CHUNK, remaining))
        if not part:
            raise AssetError("Truncated PAK slice")
        remaining -= len(part)
        if decoder is None:
            size += len(part)
            if size > MAX_DECODED_PAYLOAD:
                raise AssetError("Decoded resource exceeds the explicit preparation bound")
            yield part
            continue
        pending = part
        while pending:
            try:
                output = decoder.decompress(pending, CHUNK)
            except zlib.error as error:
                raise AssetError("Corrupt compressed PAK member") from error
            pending = decoder.unconsumed_tail
            size += len(output)
            if size > MAX_DECODED_PAYLOAD:
                raise AssetError("Decoded resource exceeds the explicit preparation bound")
            if output:
                yield output
            if decoder.unused_data:
                raise AssetError("Trailing/concatenated compressed member is outside its declared payload")
    if decoder is not None and not decoder.eof:
        raise AssetError("Compressed PAK member has no complete end/CRC")


def validate_index(index, pack_size: int):
    if not isinstance(index, dict):
        raise AssetError("PAK index must be an object")
    for name, row in index.items():
        safe_name(name)
        if (not isinstance(row, list) or len(row) != 3 or
                any(type(n) is not int for n in row) or row[0] < 0 or row[1] < 0 or
                row[2] not in (0, 1) or row[0] + row[1] > pack_size):
            raise AssetError("Out-of-bounds or unsupported PAK record: " + name)


def read_manifest(destination: Path):
    marker_path, manifest_path = destination / OWNERSHIP, destination / MANIFEST
    if marker_path.is_symlink() or manifest_path.is_symlink() or not marker_path.is_file() or not manifest_path.is_file():
        raise AssetError("Existing destination is not a prepared, owned asset directory; refusing to overwrite")
    marker = json.loads(marker_path.read_text("utf8"), object_pairs_hook=unique_json)
    if marker.get("tool") != "zg-ios-prepare-assets-v1" or sha256_file(manifest_path) != marker.get("manifestSHA256"):
        raise AssetError("Existing manifest/ownership record was modified")
    manifest = json.loads(manifest_path.read_text("utf8"), object_pairs_hook=unique_json)
    if manifest.get("schemaVersion") != 1:
        raise AssetError("Unsupported prepared asset manifest")
    return manifest


def verify_existing(destination: Path, expected_sha256: str):
    manifest = read_manifest(destination)
    if manifest.get("source", {}).get("apkSHA256") != expected_sha256:
        raise AssetError("Existing assets come from another APK; use a new destination")
    files_root = destination / "files"
    if files_root.is_symlink() or not files_root.is_dir():
        raise AssetError("Missing/redirected files directory")
    for name, row in manifest["direct"].items():
        path = within(files_root, name)
        if (path.is_symlink() or not path.is_file() or path.stat().st_size != row[0] or sha256_file(path) != row[1]):
            raise AssetError("Prepared asset changed; refusing to overwrite: " + name)
    expected = {MANIFEST, OWNERSHIP} | {"files/" + name for name in manifest["direct"]}
    for path in destination.rglob("*"):
        if path.is_symlink():
            raise AssetError("Unexpected symlink inside prepared assets")
        if path.is_file() and path.relative_to(destination).as_posix() not in expected:
            raise AssetError("Unmanaged file inside prepared assets; refusing to modify it")
    pack_size = manifest["direct"]["resources107.pak"][0]
    for name, row in manifest["packed"].items():
        safe_name(name)
        if (len(row) != 5 or any(type(n) is not int for n in row[:4]) or min(row[:3]) < 0 or
                row[3] not in (0, 1, 2) or row[0] + row[1] > pack_size or
                not re.fullmatch(r"[0-9a-f]{64}", row[4])):
            raise AssetError("Invalid decoded payload manifest: " + name)
    return manifest


def prepare(apk: Path, destination: Path, *, expected_sha256=R119_SHA256, verify_only=False):
    apk, destination = Path(apk).resolve(strict=True), Path(destination).absolute()
    if not re.fullmatch(r"[0-9a-f]{64}", expected_sha256):
        raise AssetError("Expected source SHA256 must be exact")
    actual = sha256_file(apk)
    if actual != expected_sha256:
        raise AssetError("Source APK SHA256 does not match the pinned r119 build")
    if destination.is_symlink():
        raise AssetError("Destination cannot be a symlink")
    if destination.exists():
        manifest = verify_existing(destination, expected_sha256)
        return {"status": "verified-existing", "manifest": manifest, "destination": str(destination.resolve())}
    if verify_only:
        raise AssetError("Prepared destination does not exist")
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix="." + destination.name + ".prepare-", dir=destination.parent)).resolve()
    stage_parent = destination.parent.resolve()
    try:
        files_root = stage / "files"
        files_root.mkdir()
        direct = {}
        with zipfile.ZipFile(apk) as archive:
            assets = [entry for entry in archive.infolist() if entry.filename.startswith("assets/") and not entry.is_dir()]
            names, casefolded = set(), set()
            for entry in assets:
                if entry.orig_filename != entry.filename:
                    raise AssetError("Noncanonical raw APK asset path")
                name = safe_name(entry.filename[7:])
                if name in names or name.casefold() in casefolded or stat.S_ISLNK(entry.external_attr >> 16):
                    raise AssetError("Duplicate/case-colliding/symlink APK asset: " + name)
                names.add(name); casefolded.add(name.casefold())
                target = within(files_root, name)
                target.parent.mkdir(parents=True, exist_ok=True)
                digest, size = hashlib.sha256(), 0
                with archive.open(entry) as source, target.open("xb") as output:
                    while part := source.read(CHUNK):
                        output.write(part); digest.update(part); size += len(part)
                if size != entry.file_size:
                    raise AssetError("APK entry size mismatch: " + name)
                direct[name] = [size, digest.hexdigest()]
        pack_path, index_path = files_root / "resources107.pak", files_root / "resources107-index.json"
        if not pack_path.is_file() or not index_path.is_file() or "index.html" not in direct:
            raise AssetError("Required r119 entrypoint/PAK/index is missing")
        index = json.loads(index_path.read_text("utf8"), object_pairs_hook=unique_json)
        validate_index(index, pack_path.stat().st_size)
        packed, codecs = {}, Counter()
        with pack_path.open("rb") as stream:
            for name, (offset, length, compressed) in sorted(index.items(), key=lambda pair: pair[1][0]):
                stream.seek(offset); header = stream.read(min(2, length))
                codec = 0 if not compressed else 1 if header == b"\x1f\x8b" else 2
                digest, size = hashlib.sha256(), 0
                for part in indexed_payload(stream, offset, length, codec):
                    digest.update(part); size += len(part)
                packed[name] = [offset, length, size, codec, digest.hexdigest()]
                codecs[str(codec)] += 1
        manifest = {"schemaVersion": 1,
            "source": {"apkName": apk.name, "apkSHA256": actual, "apkBytes": apk.stat().st_size},
            "url": {"scheme": "zgasset", "host": "bundle", "entry": "index.html"},
            "recordFormat": {"direct": ["decodedBytes", "sha256"],
                "packed": ["offset", "storedBytes", "decodedBytes", "codec", "sha256"],
                "codecs": {"0": "identity", "1": "gzip", "2": "zlib"}, "directTakesPrecedence": True},
            "summary": {"directCount": len(direct), "packedCount": len(packed),
                "overriddenPackedCount": len(direct.keys() & packed.keys()),
                "visibleCount": len(direct.keys() | packed.keys()),
                "directBytes": sum(row[0] for row in direct.values()),
                "decodedPackedBytes": sum(row[2] for row in packed.values()), "codecs": dict(codecs)},
            "direct": dict(sorted(direct.items())), "packed": dict(sorted(packed.items()))}
        # Compact per-member arrays avoid 82k repeated object keys in the app.
        raw = (json.dumps(manifest, ensure_ascii=False, separators=(",", ":")) + "\n").encode("utf8")
        (stage / MANIFEST).write_bytes(raw)
        marker = {"tool": "zg-ios-prepare-assets-v1", "manifestSHA256": hashlib.sha256(raw).hexdigest(), "apkSHA256": actual}
        (stage / OWNERSHIP).write_text(json.dumps(marker, sort_keys=True) + "\n", "utf8")
        # mkdir is the exclusive publication reservation. Unlike POSIX
        # rename(stage, destination), it cannot replace an unrelated empty
        # directory that appears between an exists-check and publication.
        destination.mkdir(mode=0o700, exist_ok=False)
        for child in (files_root, stage / MANIFEST, stage / OWNERSHIP):
            child.rename(destination / child.name)
        return {"status": "prepared", "manifest": manifest, "destination": str(destination.resolve())}
    finally:
        # This exact fresh staging path is the only recursive removal target.
        if stage.exists():
            if stage.parent != stage_parent or not stage.name.startswith("." + destination.name + ".prepare-") or stage.is_symlink():
                raise AssetError("Staging cleanup target failed containment checks")
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, default=DEFAULT_APK)
    parser.add_argument("--destination", type=Path, default=DEFAULT_DESTINATION)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    result = prepare(args.apk, args.destination, verify_only=args.verify_only)
    print(json.dumps({"status": result["status"], "destination": result["destination"],
        "apkSHA256": result["manifest"]["source"]["apkSHA256"], "summary": result["manifest"]["summary"]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (AssetError, OSError, zipfile.BadZipFile) as error:
        print("Asset preparation failed: " + str(error), file=sys.stderr)
        raise SystemExit(1)
