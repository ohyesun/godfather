import Foundation

enum ZGAssetError: Error { case invalidManifest, invalidPath, missingFile, cancelled, truncated, corruptCompressedData(Int32) }

struct ZGDirectAssetRecord: Decodable {
    let size: Int64
    let sha256: String
    init(from decoder: Decoder) throws {
        var row = try decoder.unkeyedContainer()
        size = try row.decode(Int64.self)
        sha256 = try row.decode(String.self)
        guard row.isAtEnd, size >= 0, sha256.count == 64 else { throw ZGAssetError.invalidManifest }
    }
}

struct ZGPackedAssetRecord: Decodable {
    let offset: Int64
    let storedSize: Int64
    let size: Int64
    let codec: Int32
    let sha256: String
    init(from decoder: Decoder) throws {
        var row = try decoder.unkeyedContainer()
        offset = try row.decode(Int64.self)
        storedSize = try row.decode(Int64.self)
        size = try row.decode(Int64.self)
        codec = try row.decode(Int32.self)
        sha256 = try row.decode(String.self)
        // offset == -1 是"不在本地 pak 里、按 path 向微端服务器取"的哨兵，
        // 与服务器索引和安卓客户端同一口径；其余负值一律视为坏清单。
        guard row.isAtEnd, offset >= -1, storedSize > 0, size >= 0,
              (0...2).contains(codec), sha256.count == 64 else { throw ZGAssetError.invalidManifest }
    }
}

private struct ZGAssetManifest: Decodable {
    struct Source: Decodable { let apkSHA256: String }
    let schemaVersion: Int
    let source: Source
    let direct: [String: ZGDirectAssetRecord]
    let packed: [String: ZGPackedAssetRecord]
}

private struct ZGAssetOverlay: Decodable {
    let schemaVersion: Int
    let sourceAPK_SHA256: String
    let files: [String: ZGDirectAssetRecord]
}

struct ZGAssetSource: Sendable {
    let fileURL: URL?
    let offset: Int64
    let storedSize: Int64
    let size: Int64
    /// 0 = unchanged bytes; 1 = gzip member; 2 = zlib member.
    let codec: Int32
    let sha256: String
    /// 非 nil 表示这段不在本地 pak 里，要按这个 id（`path:<pak 内路径>`）
    /// 从微端服务器取；`fileURL` 此时为 nil，`offset` 为 -1。
    let remoteId: String?

    var isRemote: Bool { remoteId != nil }
}

struct ZGAssetResponsePlan {
    let url: URL
    let status: Int
    let headers: [String: String]
    let source: ZGAssetSource?
    let range: Range<Int64>
    let smallBody: Data?
    let head: Bool
}

/// Prepared manifests and the signed bundle are immutable after initialization.
/// PAK members stay compressed on disk. Direct APK assets take precedence;
/// this is essential for the r119 client/view patches over older PAK copies.
final class ZGAssetStore: @unchecked Sendable {
    static let scheme = "zgasset"
    static let host = "bundle"
    static let entryURL = URL(string: "zgasset://bundle/index.html")!
    static let sourceAPKHash = "62a968e2d80bdaf95b6ddd80a038de3dd29622aa2e1f55cddde5f8e887800c44"
    private let root: URL
    private let overlayRoot: URL?
    private let direct: [String: ZGDirectAssetRecord]
    private let packed: [String: ZGPackedAssetRecord]
    private let overlay: [String: ZGDirectAssetRecord]
    private let packURL: URL
    private let packSize: Int64

    init(rootURL: URL, overlayURL: URL? = nil) throws {
        guard rootURL.isFileURL else { throw ZGAssetError.invalidPath }
        let canonicalRoot = rootURL.standardizedFileURL.resolvingSymlinksInPath()
        let manifestURL = canonicalRoot.appendingPathComponent("assets-manifest.json", isDirectory: false)
        let manifest = try JSONDecoder().decode(ZGAssetManifest.self,
            from: Data(contentsOf: manifestURL, options: .mappedIfSafe))
        guard manifest.schemaVersion == 1, manifest.source.apkSHA256 == Self.sourceAPKHash,
              let pack = manifest.direct["resources107.pak"], manifest.direct["index.html"] != nil else {
            throw ZGAssetError.invalidManifest
        }
        let packFile = try Self.containedFile(root: canonicalRoot.appendingPathComponent("files", isDirectory: true),
                                            path: "resources107.pak", size: pack.size)
        for (path, record) in manifest.direct {
            guard Self.validPath(path), record.sha256.allSatisfy({ $0.isHexDigit }) else { throw ZGAssetError.invalidManifest }
        }
        for (path, record) in manifest.packed {
            // 远端条目（offset == -1）没有本地字节可比对，但仍要求长度、
            // 编码与 sha256 合法：取回来之后要按这三样校验。
            let local = record.offset >= 0
            guard Self.validPath(path), record.sha256.allSatisfy({ $0.isHexDigit }),
                  record.size <= 512 * 1024 * 1024,
                  record.codec != 0 || record.storedSize == record.size,
                  !local || (record.offset <= pack.size && record.storedSize <= pack.size - record.offset)
            else { throw ZGAssetError.invalidManifest }
        }
        let preparedOverlayRoot: URL?
        let preparedOverlay: [String: ZGDirectAssetRecord]
        if let overlayURL {
            guard overlayURL.isFileURL else { throw ZGAssetError.invalidPath }
            let canonical = overlayURL.standardizedFileURL.resolvingSymlinksInPath()
            let manifest = try JSONDecoder().decode(ZGAssetOverlay.self,
                from: Data(contentsOf: canonical.appendingPathComponent("overlay-manifest.json")))
            guard manifest.schemaVersion == 1, manifest.sourceAPK_SHA256 == Self.sourceAPKHash else { throw ZGAssetError.invalidManifest }
            for (path, record) in manifest.files {
                guard Self.validPath(path), record.sha256.allSatisfy({ $0.isHexDigit }) else { throw ZGAssetError.invalidManifest }
                _ = try Self.containedFile(root: canonical, path: path, size: record.size)
            }
            preparedOverlayRoot = canonical; preparedOverlay = manifest.files
        } else { preparedOverlayRoot = nil; preparedOverlay = [:] }
        root = canonicalRoot
        direct = manifest.direct
        packed = manifest.packed
        packSize = pack.size
        packURL = packFile
        overlayRoot = preparedOverlayRoot
        overlay = preparedOverlay
    }

    static func validPath(_ path: String) -> Bool {
        guard !path.isEmpty, !path.hasPrefix("/"), !path.contains("\\"), !path.contains(":"),
              !path.unicodeScalars.contains(where: { $0.value < 32 || $0.value == 127 }) else { return false }
        return path.split(separator: "/", omittingEmptySubsequences: false).allSatisfy {
            !$0.isEmpty && $0 != "." && $0 != ".." && !$0.hasSuffix(".") && !$0.hasSuffix(" ")
        }
    }

    private static func containedFile(root: URL, path: String, size: Int64) throws -> URL {
        guard validPath(path) else { throw ZGAssetError.invalidPath }
        let base = root.standardizedFileURL.resolvingSymlinksInPath()
        let file = base.appendingPathComponent(path).standardizedFileURL.resolvingSymlinksInPath()
        guard file.path.hasPrefix(base.path + "/") else { throw ZGAssetError.invalidPath }
        let values = try file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey])
        guard values.isRegularFile == true, Int64(values.fileSize ?? -1) == size else { throw ZGAssetError.missingFile }
        return file
    }

    private func asset(_ path: String) throws -> ZGAssetSource? {
        if let record = overlay[path], let overlayRoot {
            return ZGAssetSource(fileURL: try Self.containedFile(root: overlayRoot, path: path, size: record.size),
                offset: 0, storedSize: record.size, size: record.size, codec: 0,
                sha256: record.sha256, remoteId: nil)
        }
        if let record = direct[path] {
            return ZGAssetSource(fileURL: try Self.containedFile(root: root.appendingPathComponent("files", isDirectory: true),
                path: path, size: record.size), offset: 0, storedSize: record.size, size: record.size,
                codec: 0, sha256: record.sha256, remoteId: nil)
        }
        guard let record = packed[path] else { return nil }
        if record.offset < 0 {
            // 最小包里没有这段：交给微端通道按原始存储字节取回。
            return ZGAssetSource(fileURL: nil, offset: -1, storedSize: record.storedSize,
                size: record.size, codec: record.codec, sha256: record.sha256, remoteId: path)
        }
        guard record.offset <= packSize, record.storedSize <= packSize - record.offset else { return nil }
        return ZGAssetSource(fileURL: packURL, offset: record.offset, storedSize: record.storedSize,
            size: record.size, codec: record.codec, sha256: record.sha256, remoteId: nil)
    }

    /// 诊断用：清单里有多少条要在运行时向微端服务器取。
    var remoteEntryCount: Int { packed.values.reduce(0) { $0 + ($1.offset < 0 ? 1 : 0) } }
    var localPackedEntryCount: Int { packed.count - remoteEntryCount }

    static func mimeType(path: String) -> String {
        switch (path as NSString).pathExtension.lowercased() {
        case "html", "htm": return "text/html; charset=utf-8"
        case "js", "mjs": return "text/javascript; charset=utf-8"
        case "css": return "text/css; charset=utf-8"
        case "json", "map": return "application/json; charset=utf-8"
        case "wasm": return "application/wasm"
        case "glb": return "model/gltf-binary"
        case "gltf": return "model/gltf+json"
        case "png": return "image/png"
        case "jpg", "jpeg": return "image/jpeg"
        case "webp": return "image/webp"
        case "gif": return "image/gif"
        case "svg": return "image/svg+xml"
        case "mp3": return "audio/mpeg"
        case "wav": return "audio/wav"
        case "ogg": return "audio/ogg"
        case "mp4", "m4v": return "video/mp4"
        case "m4a": return "audio/mp4"
        case "woff": return "font/woff"
        case "woff2": return "font/woff2"
        case "ttf": return "font/ttf"
        default: return "application/octet-stream"
        }
    }

    /// Single RFC byte ranges over DECODED representations. Multipart ranges
    /// are rejected with416; no range is ever passed to the compressed slice.
    static func byteRange(_ header: String, size: Int64) -> Range<Int64>? {
        let value = header.trimmingCharacters(in: .whitespacesAndNewlines)
        guard value.hasPrefix("bytes="), !value.contains(","), size > 0 else { return nil }
        let parts = value.dropFirst(6).split(separator: "-", omittingEmptySubsequences: false)
        guard parts.count == 2, parts.allSatisfy({ $0.allSatisfy({ $0 >= "0" && $0 <= "9" }) }) else { return nil }
        if parts[0].isEmpty {
            guard let suffix = Int64(parts[1]), suffix > 0 else { return nil }
            return max(0, size - suffix)..<size
        }
        guard let start = Int64(parts[0]), start >= 0, start < size else { return nil }
        if parts[1].isEmpty { return start..<size }
        guard let end = Int64(parts[1]), end >= start else { return nil }
        return start..<(min(end, size - 1) + 1)
    }

    func response(for request: URLRequest) throws -> ZGAssetResponsePlan {
        guard let url = request.url, let components = URLComponents(url: url, resolvingAgainstBaseURL: false),
              components.scheme?.lowercased() == Self.scheme, components.host?.lowercased() == Self.host,
              components.user == nil, components.password == nil, components.port == nil else { throw ZGAssetError.invalidPath }
        let encoded = components.percentEncodedPath
        guard let decoded = encoded.removingPercentEncoding, decoded.hasPrefix("/") else { throw ZGAssetError.invalidPath }
        let path = decoded == "/" ? "index.html" : String(decoded.dropFirst())
        guard Self.validPath(path) else { throw ZGAssetError.invalidPath }
        let method = (request.httpMethod ?? "GET").uppercased(), head = method == "HEAD"
        var headers = ["Access-Control-Allow-Origin": "*", "Access-Control-Allow-Methods": "GET, HEAD, OPTIONS",
            "Access-Control-Allow-Headers": "Range, If-Range", "Access-Control-Expose-Headers": "Content-Length, Content-Range, Accept-Ranges, ETag",
            "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff", "Accept-Ranges": "bytes"]
        func empty(_ status: Int, _ body: String = "") -> ZGAssetResponsePlan {
            var h = headers; let bytes = Data(body.utf8)
            h["Content-Length"] = String(bytes.count); h["Content-Type"] = "text/plain; charset=utf-8"
            return ZGAssetResponsePlan(url: url, status: status, headers: h, source: nil, range: 0..<0,
                smallBody: bytes.isEmpty ? nil : bytes, head: head)
        }
        if method == "OPTIONS" { return empty(204) }
        guard method == "GET" || head else { headers["Allow"] = "GET, HEAD, OPTIONS"; return empty(405, "Method not allowed") }
        guard let source = try asset(path) else { return empty(404, "Resource not found") }
        headers["Content-Type"] = Self.mimeType(path: path)
        headers["ETag"] = "\"" + source.sha256 + "\""
        var range = Int64(0)..<source.size, status = 200
        if !head, let requested = request.value(forHTTPHeaderField: "Range"),
           request.value(forHTTPHeaderField: "If-Range") == nil || request.value(forHTTPHeaderField: "If-Range") == headers["ETag"] {
            guard let selected = Self.byteRange(requested, size: source.size) else {
                headers["Content-Range"] = "bytes */\(source.size)"; return empty(416)
            }
            range = selected; status = 206
            headers["Content-Range"] = "bytes \(range.lowerBound)-\(range.upperBound - 1)/\(source.size)"
        }
        headers["Content-Length"] = String(range.upperBound - range.lowerBound)
        // Decompressed resources are ordinary representations: deliberately
        // no Content-Encoding header, otherwise WebKit would inflate twice.
        return ZGAssetResponsePlan(url: url, status: status, headers: headers, source: source,
            range: range, smallBody: nil, head: head)
    }
}
