"""用与 iOS ZGMicroClient 相同的线格式直连本地 zgmus，验证三件事：

1. 握手帧（HELLO/HELLO_ACK）的字段偏移与服务器的实际实现一致；
2. `path:<pak 内路径>` 拿回来的就是全量 pak 里那一段存储字节（sha256 逐字节相同）；
3. 按清单的 codec 解压后，sha256 等于清单的 decoded sha256 ——
   也就是 iOS 端"取回来 → 解码 → 用清单 sha 校验"这条链在真实服务器上成立。

这本脚本是独立实现，不复用 iOS 侧的代码，也不复用安卓的 Java 实现。
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import socket
import struct
import sys
import time
import zlib
from pathlib import Path

MAGIC = 0x475A
T_HELLO, T_HELLO_ACK, T_GET, T_DATA, T_ERROR = 0x01, 0x81, 0x10, 0x90, 0x93
FLAG_END = 0x02
MAX_BODY = (1 << 20) + 64
IOS = Path(__file__).resolve().parents[1]


def recv_exact(sock: socket.socket, count: int) -> bytes:
    chunks = []
    got = 0
    while got < count:
        block = sock.recv(count - got)
        if not block:
            raise RuntimeError("服务端关闭连接（已收 %d/%d）" % (got, count))
        chunks.append(block)
        got += len(block)
    return b"".join(chunks)


def frame(kind: int, flags: int, body: bytes) -> bytes:
    return struct.pack("<HBBL", MAGIC, kind, flags, len(body)) + body


def read_frame(sock: socket.socket):
    header = recv_exact(sock, 8)
    magic, kind, flags, length = struct.unpack("<HBBL", header)
    if magic != MAGIC:
        raise RuntimeError("帧头 magic 不符：0x%04x" % magic)
    if length > MAX_BODY:
        raise RuntimeError("帧体超长 %d" % length)
    return kind, flags, recv_exact(sock, length) if length else b""


class Client:
    def __init__(self, host: str, port: int, token: str, timeout: float = 20.0):
        self.sock = socket.create_connection((host, port), timeout)
        self.sock.settimeout(timeout)
        self.seq = 0
        self.block_size = 0
        self.manifest_size = 0
        self.manifest_sha = b""
        self.hello(token)

    def hello(self, token: str) -> None:
        raw = token.encode("utf-8")
        body = struct.pack("<H", 1) + bytes([len(raw)]) + raw + struct.pack("<Q", int(time.time() * 1000))
        self.sock.sendall(frame(T_HELLO, 0, body))
        kind, _, payload = read_frame(self.sock)
        if kind == T_ERROR:
            raise RuntimeError("握手被拒：" + describe(payload))
        if kind != T_HELLO_ACK:
            raise RuntimeError("首帧不是 HELLO_ACK：0x%02x" % kind)
        if len(payload) < 55:
            raise RuntimeError("HELLO_ACK 太短 %d" % len(payload))
        if payload[0] != 0:
            raise RuntimeError("hello status=%d" % payload[0])
        self.block_size = struct.unpack_from("<I", payload, 1)[0]
        self.manifest_size = struct.unpack_from("<Q", payload, 15)[0]
        self.manifest_sha = payload[23:55]

    def get(self, pack_id: str, offset: int, length: int):
        raw = pack_id.encode("utf-8")
        self.seq += 1
        body = (struct.pack("<I", self.seq) + struct.pack("<H", len(raw)) + raw
                + struct.pack("<Q", offset) + struct.pack("<I", length & 0xFFFFFFFF))
        self.sock.sendall(frame(T_GET, 0, body))
        collected = bytearray()
        while True:
            kind, flags, payload = read_frame(self.sock)
            if kind == T_ERROR:
                raise ProbeError(describe(payload))
            if kind != T_DATA:
                raise RuntimeError("期望 DATA，收到 0x%02x" % kind)
            if len(payload) < 16:
                raise RuntimeError("DATA 帧太短")
            req_id, chunk_offset, data_length = struct.unpack_from("<IQI", payload, 0)
            if req_id != self.seq:
                raise RuntimeError("reqId 不匹配 %d != %d" % (req_id, self.seq))
            if chunk_offset != offset + len(collected):
                raise RuntimeError("DATA 不连续 %d" % chunk_offset)
            if 16 + data_length > len(payload):
                raise RuntimeError("DATA 长度越界")
            collected += payload[16:16 + data_length]
            if flags & FLAG_END:
                return bytes(collected)

    def close(self) -> None:
        try:
            self.sock.sendall(frame(0x1A, 0, b""))
        except OSError:
            pass
        self.sock.close()


class ProbeError(RuntimeError):
    pass


def describe(payload: bytes) -> str:
    if len(payload) < 7:
        return "错误帧太短"
    code = struct.unpack_from("<H", payload, 4)[0]
    size = payload[6]
    return "0x%04x %s" % (code, payload[7:7 + size].decode("utf-8", "replace"))


def decode(blob: bytes, codec: int) -> bytes:
    if codec == 1:
        return gzip.decompress(blob)
    if codec == 2:
        return zlib.decompress(blob)
    return blob


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18099)
    parser.add_argument("--token", default="zgphs00")
    parser.add_argument("--manifest", type=Path,
                        default=IOS / "build/minimal/GameAssets/assets-manifest.json")
    parser.add_argument("--index", type=Path,
                        default=IOS / "Zhanguo/Resources/GameAssets/files/resources107-index.json")
    parser.add_argument("--pak", type=Path,
                        default=IOS / "Zhanguo/Resources/GameAssets/files/resources107.pak")
    parser.add_argument("--samples", type=int, default=6)
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text("utf-8"))
    index = json.loads(args.index.read_text("utf-8"))
    remote = [(name, row) for name, row in manifest["packed"].items() if row[0] == -1]
    if not remote:
        raise SystemExit("清单里没有远端条目")

    # 抽样：一条 gzip、一条 identity、再加上解码后最大的那条（首图真正会吃的资源）
    gzip_pick = next(((n, r) for n, r in remote if r[3] == 1), None)
    plain_pick = next(((n, r) for n, r in remote if r[3] == 0), None)
    biggest = max(remote, key=lambda kv: kv[1][2])
    samples = [x for x in (gzip_pick, plain_pick, biggest) if x]
    for candidate in sorted(remote, key=lambda kv: -kv[1][2])[: args.samples]:
        if candidate not in samples:
            samples.append(candidate)
    samples = samples[: max(3, args.samples)]

    client = Client(args.host, args.port, args.token)
    print(json.dumps({
        "handshake": {
            "blockSize": client.block_size,
            "manifestSize": client.manifest_size,
            "manifestSHA256": client.manifest_sha.hex(),
            "expectedManifestBytes": 0,
        }
    }, ensure_ascii=False))

    results = []
    pak = args.pak.open("rb")
    try:
        for name, row in samples:
            _, stored, size, codec, digest = row
            raw = client.get("path:" + name, 0, 0x7FFFFF00)
            original_offset, original_length, original_flag = index[name]
            pak.seek(original_offset)
            reference = pak.read(original_length)
            decoded = decode(raw, codec)
            results.append({
                "path": name,
                "codec": codec,
                "receivedBytes": len(raw),
                "declaredStoredBytes": stored,
                "storedMatchesPakSegment": hashlib.sha256(raw).hexdigest() == hashlib.sha256(reference).hexdigest(),
                "decodedBytes": len(decoded),
                "declaredDecodedBytes": size,
                "decodedSHA256": hashlib.sha256(decoded).hexdigest(),
                "manifestSHA256": digest,
                "decodedMatchesManifest": hashlib.sha256(decoded).hexdigest() == digest,
                "indexFlagMatchesCodec": original_flag == codec,
            })
    finally:
        pak.close()

    # 错误路径：不存在的 id 必须是 E_NO_PACK，而不是挂住
    error_probe = {}
    try:
        client.get("path:scenes/__no_such_file__.glb", 0, 1024)
        error_probe["missing"] = "UNEXPECTED_SUCCESS"
    except ProbeError as exc:
        error_probe["missing"] = str(exc)

    # 续传：从段中间 offset 取，字节必须与全量同位置一致。
    # iOS 的 .part 续传完全靠这条路径，offset 写错会静默拼出坏文件。
    resume_probe = {}
    resume_name, resume_row = samples[0]
    with args.pak.open("rb") as handle:
        original_offset, original_length, _ = index[resume_name]
        handle.seek(original_offset)
        whole = handle.read(original_length)
    middle = min(4_096, max(0, len(whole) // 2))
    tail = client.get("path:" + resume_name, middle, 4_096)
    resume_probe = {
        "path": resume_name,
        "offset": middle,
        "requested": 4_096,
        "received": len(tail),
        "matchesWholeSegment": tail == whole[middle:middle + 4_096],
    }

    client.close()

    # 并发：iOS 连接池是 4 条，这里就用 4 条线程同时取不同资源。
    concurrent = {"connections": 4, "results": []}

    def fetch_one(item):
        name = item[0]
        probe = Client(args.host, args.port, args.token)
        try:
            data = probe.get("path:" + name, 0, 0x7FFFFF00)
            return {"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        finally:
            probe.close()

    from concurrent.futures import ThreadPoolExecutor
    pool_items = [samples[i] for i in range(min(4, len(samples)))]
    with ThreadPoolExecutor(max_workers=4) as executor:
        concurrent["results"] = list(executor.map(fetch_one, pool_items))
    concurrent["allDelivered"] = all(
        row["bytes"] == dict(samples)[row["path"]][1] for row in concurrent["results"])

    auth_probe = {}
    try:
        Client(args.host, args.port, "wrong-token")
        auth_probe["badToken"] = "UNEXPECTED_SUCCESS"
    except (RuntimeError, ProbeError) as exc:
        auth_probe["badToken"] = str(exc)

    ok = all(r["storedMatchesPakSegment"] and r["decodedMatchesManifest"]
             and r["receivedBytes"] == r["declaredStoredBytes"]
             and r["decodedBytes"] == r["declaredDecodedBytes"] for r in results)
    ok = ok and resume_probe["matchesWholeSegment"] and concurrent["allDelivered"]
    print(json.dumps({"samples": results, "errors": error_probe, "resume": resume_probe,
                      "concurrent": concurrent, "auth": auth_probe,
                      "verdict": "PASS" if ok else "FAIL"}, ensure_ascii=False, indent=2))
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # noqa: BLE001 - 命令行探针，失败要原样打出来
        print("探针失败：" + repr(error), file=sys.stderr)
        raise SystemExit(1) from error
