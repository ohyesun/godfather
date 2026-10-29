import CryptoKit
import Foundation

// MARK: - 配置

/// 打包进 App 的微端服务器配置。与安卓 `assets/micro-client.json` 字段同名，
/// 便于两端共用一份运维口径。文件缺失视为"未启用微端"：远端条目会明确报错，
/// 而不是安静地返回空流。
struct ZGMicroSettings: Decodable {
    let host: String
    let port: Int
    let token: String?
    let connectTimeoutMs: Int?
    let readTimeoutMs: Int?
    let maxAttempts: Int?
    let cacheBudgetMb: Int?

    static let resourceName = "MicroClient"

    init(host: String, port: Int, token: String? = nil, connectTimeoutMs: Int? = nil,
         readTimeoutMs: Int? = nil, maxAttempts: Int? = nil, cacheBudgetMb: Int? = nil) {
        self.host = host
        self.port = port
        self.token = token
        self.connectTimeoutMs = connectTimeoutMs
        self.readTimeoutMs = readTimeoutMs
        self.maxAttempts = maxAttempts
        self.cacheBudgetMb = cacheBudgetMb
    }

    static func load(bundle: Bundle = .main) -> ZGMicroSettings? {
        guard let url = bundle.url(forResource: resourceName, withExtension: "json"),
              let data = try? Data(contentsOf: url),
              let value = try? JSONDecoder().decode(Self.self, from: data) else { return nil }
        guard !value.host.isEmpty, (1...65_535).contains(value.port) else { return nil }
        return value
    }

    var endpoint: ZGNetworkEndpoint { ZGNetworkEndpoint(host: host, port: port) }
    var connectTimeout: TimeInterval { TimeInterval(connectTimeoutMs ?? 5_000) / 1000 }
    var readTimeout: TimeInterval { TimeInterval(readTimeoutMs ?? 20_000) / 1000 }
    var attempts: Int { max(1, maxAttempts ?? 3) }
    var cacheBudget: Int64 { Int64(max(64, cacheBudgetMb ?? 512)) * 1_048_576 }
}

// MARK: - 抓取与缓存

/// 远端资源的本地副本。
///
/// 缓存分两段：
///   `<hash>.part`  —— 服务端原始存储字节（`path:` 模式就是 pak 里那段，
///                     压缩仍然是压缩），断点续传只认它；
///   `<hash>.bin`   —— 解码后的表示，`ZGAssetReader` 的 range 请求直接读它。
///
/// 为什么一定要落到解码后：WebKit 发的是 `bytes=0-1023` 这种**解码表示**的
/// range，而远端只给压缩块。若每次 range 都重新解压，一个 24 MB 的 glb 会被
/// 反复解压几十次。解码一次、落地一次，后续全是本地 seek。
///
/// 语义边界：`path:` 拿回的就是签名 pak 里的字节，落地后按清单的
/// `decodedBytes` 与 `sha256` 双重校验；任一不符就删文件并抛异常，
/// 绝不把半截内容交给 WebKit。
final class ZGMicroFetcher: @unchecked Sendable {
    static let shared = ZGMicroFetcher()

    /// 可从微端通道下发的内容不包含可执行资源：裸 TCP 通道一旦能下发
    /// .js/.html，就等于给游戏页面开了一个代码注入面。
    private static let forbiddenExtensions = [".js", ".mjs", ".html", ".htm", ".css", ".svg"]

    private let condition = NSCondition()
    private var slots: [String: Slot] = [:]
    private var pool: ZGMicroPool?
    private var cacheRoot: URL?
    private var settings: ZGMicroSettings?
    private var attemptedLoad = false
    private var injectedFactory: ZGChannelFactory?

    private enum Slot {
        case running
        case done(URL)
    }

    private init() {}

    // MARK: 对外

    /// 把远端条目解析成本地**解码后**文件。命中缓存不连网。
    func resolve(remoteId: String, storedSize: Int64, decodedSize: Int64, codec: Int32,
                 sha256: String, cancelled: () -> Bool) throws -> URL {
        try refuseExecutable(remoteId)
        let key = Self.cacheKey(remoteId)
        let decodedURL = try cacheDirectory().appendingPathComponent(key + ".bin")

        if Self.fileSize(decodedURL) == decodedSize {
            Self.touch(decodedURL)
            return decodedURL
        }

        // 同一个资源被多路并发请求时只下载一次，其余等待同一份结果。
        condition.lock()
        while true {
            if case .some(.running) = slots[key] {
                if cancelled() { condition.unlock(); throw ZGMicroFailure.cancelled }
                if !condition.wait(until: Date().addingTimeInterval(60)) {
                    condition.unlock()
                    throw ZGMicroFailure.timeout
                }
                continue
            }
            if case .some(.done(let ready)) = slots[key] {
                condition.unlock()
                return ready
            }
            slots[key] = .running
            condition.unlock()
            break
        }

        do {
            let url = try produce(key: key, remoteId: remoteId, storedSize: storedSize,
                                  decodedSize: decodedSize, codec: codec,
                                  sha256: sha256, cancelled: cancelled)
            condition.lock()
            slots[key] = .done(url)
            condition.broadcast()
            condition.unlock()
            return url
        } catch {
            condition.lock()
            slots.removeValue(forKey: key)
            condition.broadcast()
            condition.unlock()
            throw error
        }
    }

    /// 缓存的解码文件是否已在本地（不校验完整性，只给日志和预热用）。
    func cachedFile(remoteId: String) -> URL? {
        guard let key = try? Self.cacheKey(remoteId),
              let root = try? cacheDirectory() else { return nil }
        let url = root.appendingPathComponent(key + ".bin")
        return Self.fileSize(url) > 0 ? url : nil
    }

    func reset() {
        condition.lock()
        let existing = pool
        pool = nil
        cacheRoot = nil
        settings = nil
        attemptedLoad = false
        injectedFactory = nil
        slots.removeAll()
        condition.unlock()
        existing?.closeAll()
    }

    /// 测试专用注入口：绕开 App Bundle 里的配置文件，直接给连接参数、
    /// 缓存目录和通道工厂。生产路径永远走 `ZGMicroSettings.load()`。
    func configure(settings: ZGMicroSettings, cacheRoot: URL,
                   factory: @escaping ZGChannelFactory) throws {
        do {
            try FileManager.default.createDirectory(at: cacheRoot, withIntermediateDirectories: true)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        condition.lock()
        let existing = pool
        pool = nil
        self.settings = settings
        self.cacheRoot = cacheRoot
        self.attemptedLoad = true
        self.injectedFactory = factory
        slots.removeAll()
        condition.unlock()
        existing?.closeAll()
    }

    // MARK: 生产

    private func produce(key: String, remoteId: String, storedSize: Int64, decodedSize: Int64,
                         codec: Int32, sha256: String, cancelled: () -> Bool) throws -> URL {
        let directory = try cacheDirectory()
        let decodedURL = directory.appendingPathComponent(key + ".bin")
        let partURL = directory.appendingPathComponent(key + ".part")

        if max(0, Self.fileSize(partURL)) > storedSize {
            try? FileManager.default.removeItem(at: partURL)
        }
        if max(0, Self.fileSize(partURL)) < storedSize {
            try download(remoteId: remoteId, to: partURL, storedSize: storedSize, cancelled: cancelled)
        }
        let stored = max(0, Self.fileSize(partURL))
        guard stored == storedSize else {
            throw ZGMicroFailure.sizeMismatch(expected: storedSize, actual: stored)
        }
        if cancelled() { throw ZGMicroFailure.cancelled }

        if codec == 0 {
            // identity：存储字节就是内容。清单里的 sha256 是解码后的，
            // 两个表示既然是同一份字节，顺便当作传输校验。
            let actual = try Self.hashFile(partURL)
            guard actual == sha256.lowercased() else {
                try? FileManager.default.removeItem(at: partURL)
                throw ZGMicroFailure.checksum(expected: sha256, actual: actual)
            }
            try replace(partURL, with: decodedURL)
            prune()
            return decodedURL
        }

        // gzip / zlib：逐块解压到临时文件，边解压边算 sha256。
        let temporary = directory.appendingPathComponent(key + ".tmp")
        try? FileManager.default.removeItem(at: temporary)
        var writeFailure: Error?
        var hasher = SHA256()
        do {
            let asset = ZGAssetSource(fileURL: partURL, offset: 0, storedSize: storedSize,
                                      size: decodedSize, codec: codec,
                                      sha256: sha256, remoteId: nil)
            try ZGAssetReader.stream(asset, range: 0..<decodedSize, cancelled: cancelled) { data in
                do {
                    try Self.append(data, to: temporary)
                    hasher.update(data: data)
                    return true
                } catch {
                    writeFailure = error
                    return false
                }
            }
        } catch {
            if let writeFailure = writeFailure {
                try? FileManager.default.removeItem(at: temporary)
                throw writeFailure
            }
            try? FileManager.default.removeItem(at: temporary)
            throw error
        }
        let actualHash = hasher.finalize().map { String(format: "%02x", $0) }.joined()
        guard actualHash == sha256.lowercased() else {
            try? FileManager.default.removeItem(at: temporary)
            throw ZGMicroFailure.checksum(expected: sha256, actual: actualHash)
        }
        let decoded = max(0, Self.fileSize(temporary))
        guard decoded == decodedSize else {
            try? FileManager.default.removeItem(at: temporary)
            throw ZGMicroFailure.sizeMismatch(expected: decodedSize, actual: decoded)
        }
        try replace(temporary, with: decodedURL)
        try? FileManager.default.removeItem(at: partURL)
        prune()
        return decodedURL
    }

    private func download(remoteId: String, to partURL: URL, storedSize: Int64,
                          cancelled: () -> Bool) throws {
        let pool = try requirePool()
        let settings = try requireSettings()
        var attempt = 0
        var last: Error = ZGMicroFailure.connection("未知失败")

        while attempt < settings.attempts {
            attempt += 1
            if cancelled() { throw ZGMicroFailure.cancelled }
            let connection: ZGMicroConnection
            do {
                connection = try pool.borrow()
            } catch {
                last = error
                if let micro = error as? ZGMicroFailure, !micro.isRetryable { throw micro }
                Self.backoff(attempt)
                continue
            }
            do {
                let have = max(0, Self.fileSize(partURL))
                if have < storedSize {
                    let want = Int(min(Int64(0x7FFF_FF00), storedSize - have))
                    try poolWrite(connection: connection, remoteId: remoteId,
                                  offset: have, length: want, to: partURL)
                }
                pool.give(connection)
                let now = max(0, Self.fileSize(partURL))
                if now >= storedSize { return }
                last = ZGMicroFailure.protocolViolation(
                    "续传未完成 " + String(now) + "/" + String(storedSize))
                Self.backoff(attempt)
            } catch {
                // 出错后帧边界不可信，这条连接不能回池。
                pool.drop(connection)
                if let micro = error as? ZGMicroFailure {
                    if !micro.isRetryable { throw micro }
                }
                last = error
                Self.backoff(attempt)
            }
        }
        throw last
    }

    private func poolWrite(connection: ZGMicroConnection, remoteId: String, offset: Int64,
                           length: Int, to partURL: URL) throws {
        let handle: FileHandle
        do {
            if !FileManager.default.fileExists(atPath: partURL.path) {
                FileManager.default.createFile(atPath: partURL.path, contents: nil)
            }
            handle = try FileHandle(forWritingTo: partURL)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        defer { try? handle.close() }
        do {
            try handle.seekToEnd()
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        try connection.fetch(id: remoteId, offset: offset, length: length) { _, data in
            do {
                try handle.write(contentsOf: data)
            } catch {
                throw ZGMicroFailure.disk(error.localizedDescription)
            }
        }
        try? handle.synchronize()
    }

    // MARK: 依赖

    private func requireSettings() throws -> ZGMicroSettings {
        condition.lock()
        if let ready = settings { condition.unlock(); return ready }
        if attemptedLoad { condition.unlock(); throw ZGMicroFailure.notConfigured }
        attemptedLoad = true
        condition.unlock()
        let loaded = ZGMicroSettings.load()
        condition.lock()
        settings = loaded
        condition.unlock()
        guard let loaded = loaded else { throw ZGMicroFailure.notConfigured }
        return loaded
    }

    private func requirePool() throws -> ZGMicroPool {
        let current = try requireSettings()
        condition.lock()
        if let ready = pool { condition.unlock(); return ready }
        let injected = injectedFactory
        condition.unlock()
        let factory: ZGChannelFactory = injected ?? { endpoint, pinned in
            try ZGNWByteChannel(endpoint: endpoint, pinnedCertificate: pinned)
        }
        let created = ZGMicroPool(endpoint: current.endpoint, token: current.token ?? "",
                                  connectTimeout: current.connectTimeout,
                                  readTimeout: current.readTimeout,
                                  factory: factory)
        condition.lock()
        if let existing = pool { condition.unlock(); return existing }
        pool = created
        condition.unlock()
        return created
    }

    private func cacheDirectory() throws -> URL {
        condition.lock()
        if let ready = cacheRoot { condition.unlock(); return ready }
        condition.unlock()
        let base: URL
        do {
            base = try FileManager.default.url(for: .cachesDirectory, in: .userDomainMask,
                                               appropriateFor: nil, create: true)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        let root = base.appendingPathComponent("zgmicro", isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        condition.lock()
        cacheRoot = root
        condition.unlock()
        return root
    }

    // MARK: 工具

    private func refuseExecutable(_ remoteId: String) throws {
        let lower = remoteId.lowercased()
        for suffix in Self.forbiddenExtensions where lower.hasSuffix(suffix) {
            throw ZGMicroFailure.refused("拒绝从微端通道下发可执行资源：" + remoteId)
        }
    }

    /// 缓存预算清理：按最后访问时间从旧到新删 `.bin`；`.part` 不动，
    /// 它代表一个未完成的续传，删掉等于白下载。
    private func prune() {
        guard let budget = settings?.cacheBudget, budget > 0,
              let directory = cacheRoot else { return }
        let keys: [URLResourceKey] = [.fileSizeKey, .contentModificationDateKey, .isRegularFileKey]
        guard let entries = try? FileManager.default.contentsOfDirectory(
            at: directory, includingPropertiesForKeys: keys, options: [.skipsHiddenFiles]) else { return }
        var files: [(url: URL, size: Int64, at: Date)] = []
        var total: Int64 = 0
        for entry in entries where entry.pathExtension == "bin" {
            guard let values = try? entry.resourceValues(forKeys: Set(keys)),
                  values.isRegularFile == true else { continue }
            let size = Int64(values.fileSize ?? 0)
            files.append((entry, size, values.contentModificationDate ?? .distantPast))
            total += size
        }
        guard total > budget else { return }
        for item in files.sorted(by: { $0.at < $1.at }) {
            if total <= budget { break }
            if (try? FileManager.default.removeItem(at: item.url)) != nil { total -= item.size }
        }
    }

    private func replace(_ source: URL, with destination: URL) throws {
        do {
            if FileManager.default.fileExists(atPath: destination.path) {
                try FileManager.default.removeItem(at: destination)
            }
            try FileManager.default.moveItem(at: source, to: destination)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
    }

    static func cacheKey(_ remoteId: String) -> String {
        let digest = SHA256.hash(data: Data(remoteId.utf8))
        return digest.map { String(format: "%02x", $0) }.joined()
    }

    static func fileSize(_ url: URL) -> Int64 {
        guard let values = try? url.resourceValues(forKeys: [.fileSizeKey, .isRegularFileKey]),
              values.isRegularFile == true else { return -1 }
        return Int64(values.fileSize ?? -1)
    }

    static func touch(_ url: URL) {
        try? FileManager.default.setAttributes([.modificationDate: Date()], ofItemAtPath: url.path)
    }

    static func hashFile(_ url: URL) throws -> String {
        let handle: FileHandle
        do { handle = try FileHandle(forReadingFrom: url) } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        defer { try? handle.close() }
        var hasher = SHA256()
        while true {
            let data: Data
            do { data = try handle.read(upToCount: 1 << 20) ?? Data() } catch {
                throw ZGMicroFailure.disk(error.localizedDescription)
            }
            if data.isEmpty { break }
            hasher.update(data: data)
        }
        return hasher.finalize().map { String(format: "%02x", $0) }.joined()
    }

    static func append(_ data: Data, to url: URL) throws {
        if !FileManager.default.fileExists(atPath: url.path) {
            FileManager.default.createFile(atPath: url.path, contents: nil)
        }
        let handle: FileHandle
        do { handle = try FileHandle(forWritingTo: url) } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
        defer { try? handle.close() }
        do {
            try handle.seekToEnd()
            try handle.write(contentsOf: data)
        } catch {
            throw ZGMicroFailure.disk(error.localizedDescription)
        }
    }

    static func backoff(_ attempt: Int) {
        let milliseconds = min(2_000, 200 * max(1, attempt))
        Thread.sleep(forTimeInterval: TimeInterval(milliseconds) / 1000)
    }
}
