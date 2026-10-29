import XCTest
@testable import Zhanguo

/// 全部在内存里跑，不连任何真实主机。
///
/// 这里的假服务端刻意**不复用** `ZGMicroWire` 的读写函数，而是自己按字节
/// 偏移拼帧：如果客户端把 idLen 或 offset 的偏移写错了，两边对不上就会
/// 立刻暴露，而不是被同一份工具函数一起改错。
final class ZGMicroClientTests: XCTestCase {

    // MARK: 假服务端

    private final class FakeServer {
        var token = "zgphs00"
        var manifestSize: UInt64 = 659_000
        var manifestSHA = [UInt8](repeating: 0x11, count: 32)
        var resources: [String: Data] = [:]
        var splitChunk = 0
        var failWith: (code: UInt16, message: String)?
        var helloStatus: UInt8 = 0
        private(set) var requests: [(id: String, offset: Int64, length: UInt32)] = []

        func handle(_ raw: Data) -> Data? {
            let bytes = [UInt8](raw)
            guard bytes.count >= 8, bytes[0] == 0x5A, bytes[1] == 0x47 else { return nil }
            let type = bytes[2]
            let length = Int(u32(bytes, 4))
            guard bytes.count >= 8 + length else { return nil }
            let body = Array(bytes[8..<(8 + length)])
            switch type {
            case 0x01: return hello(body)
            case 0x10: return get(body)
            default: return nil
            }
        }

        private func hello(_ body: [UInt8]) -> Data? {
            guard body.count >= 3 else { return nil }
            let tokenLength = Int(body[2])
            guard body.count >= 3 + tokenLength else { return nil }
            let supplied = String(decoding: body[3..<(3 + tokenLength)], as: UTF8.self)
            if supplied != token { return error(rid: 0, code: 0x0020, message: "auth failed") }
            var ack = [UInt8](repeating: 0, count: 55)
            ack[0] = helloStatus
            put32(&ack, 1, 65_536)
            put16(&ack, 5, 4)
            put64(&ack, 7, 1_700_000_000_000)
            put64(&ack, 15, manifestSize)
            ack.replaceSubrange(23..<55, with: manifestSHA)
            return frame(0x81, 0, ack)
        }

        private func get(_ body: [UInt8]) -> Data? {
            guard body.count >= 6 else { return nil }
            let rid = u32(body, 0)
            let idLength = Int(u16(body, 4))
            guard body.count >= 6 + idLength + 12 else { return nil }
            let id = String(decoding: body[6..<(6 + idLength)], as: UTF8.self)
            let offset = Int64(bitPattern: u64(body, 6 + idLength))
            let length = u32(body, 14 + idLength)
            requests.append((id, offset, length))
            if let failure = failWith { return error(rid: rid, code: failure.code, message: failure.message) }
            guard let data = resources[id] else { return error(rid: rid, code: 0x0010, message: "no such pack") }
            guard offset < Int64(data.count) else { return error(rid: rid, code: 0x0011, message: "offset beyond end") }
            let end = min(Int64(data.count), offset + Int64(length))
            let slice = data.subdata(in: Int(offset)..<Int(end))
            var out = Data()
            var position = 0
            let step = splitChunk > 0 ? splitChunk : max(1, slice.count)
            while position < slice.count {
                let take = min(step, slice.count - position)
                let part = slice.subdata(in: position..<(position + take))
                let last = position + take >= slice.count
                var payload = [UInt8](repeating: 0, count: 16 + part.count)
                put32(&payload, 0, rid)
                put64(&payload, 4, UInt64(offset + Int64(position)))
                put32(&payload, 12, UInt32(part.count))
                payload.replaceSubrange(16..<(16 + part.count), with: [UInt8](part))
                out.append(frame(0x90, last ? 0x02 : 0, payload))
                position += take
            }
            return out
        }

        private func frame(_ type: UInt8, _ flags: UInt8, _ body: [UInt8]) -> Data {
            var out = [UInt8](repeating: 0, count: 8 + body.count)
            out[0] = 0x5A; out[1] = 0x47; out[2] = type; out[3] = flags
            put32(&out, 4, UInt32(body.count))
            if !body.isEmpty {
                out.replaceSubrange(8..<(8 + body.count), with: body)
            }
            return Data(out)
        }

        private func error(rid: UInt32, code: UInt16, message: String) -> Data {
            let text = [UInt8](message.utf8)
            var body = [UInt8](repeating: 0, count: 7 + text.count)
            put32(&body, 0, rid)
            put16(&body, 4, code)
            body[6] = UInt8(min(255, text.count))
            if !text.isEmpty {
                body.replaceSubrange(7..<(7 + min(255, text.count)), with: text.prefix(255))
            }
            return frame(0x93, 0, body)
        }

        private func u16(_ b: [UInt8], _ o: Int) -> UInt16 { UInt16(b[o]) | (UInt16(b[o + 1]) << 8) }
        private func u32(_ b: [UInt8], _ o: Int) -> UInt32 {
            UInt32(b[o]) | (UInt32(b[o + 1]) << 8) | (UInt32(b[o + 2]) << 16) | (UInt32(b[o + 3]) << 24)
        }
        private func u64(_ b: [UInt8], _ o: Int) -> UInt64 {
            var v: UInt64 = 0
            for i in stride(from: 7, through: 0, by: -1) { v = (v << 8) | UInt64(b[o + i]) }
            return v
        }
        private func put16(_ b: inout [UInt8], _ o: Int, _ v: UInt16) {
            b[o] = UInt8(v & 0xFF); b[o + 1] = UInt8((v >> 8) & 0xFF)
        }
        private func put32(_ b: inout [UInt8], _ o: Int, _ v: UInt32) {
            for i in 0..<4 { b[o + i] = UInt8((v >> (8 * UInt32(i))) & 0xFF) }
        }
        private func put64(_ b: inout [UInt8], _ o: Int, _ v: UInt64) {
            for i in 0..<8 { b[o + i] = UInt8((v >> (8 * UInt64(i))) & 0xFF) }
        }
    }

    private final class FakeChannel: ZGByteChannel {
        private let lock = NSLock()
        private let server: FakeServer
        private var outbound = Data()
        private var pending: ((Data, Bool, Error?) -> Void)?
        private var pendingMaximum = 0
        private var cancelled = false

        init(server: FakeServer) { self.server = server }

        func start(_ completion: @escaping (Error?) -> Void) { completion(nil) }

        func send(_ data: Data, completion: @escaping (Error?) -> Void) {
            lock.lock()
            let reply = server.handle(data)
            if let reply = reply { outbound.append(reply) }
            lock.unlock()
            completion(nil)
            pump()
        }

        func receive(maximum: Int, completion: @escaping (Data, Bool, Error?) -> Void) {
            lock.lock()
            pending = completion
            pendingMaximum = maximum
            lock.unlock()
            pump()
        }

        func cancel() {
            lock.lock(); cancelled = true; lock.unlock()
        }

        private func pump() {
            lock.lock()
            guard let callback = pending, !outbound.isEmpty, !cancelled else { lock.unlock(); return }
            let size = min(pendingMaximum, outbound.count)
            let chunk = Data(outbound.prefix(size))
            outbound.removeFirst(size)
            pending = nil
            lock.unlock()
            callback(chunk, false, nil)
        }
    }

    // MARK: 夹具

    private static let plainFixture = Data("ZGMU-IDENTITY-BLOCK-0123456789".utf8)
    private static let plainFixtureSHA = "c195450d9e85a3dc85503f0c95aec2e5dd9906c00e646875532dcc15f64f2fb7"
    private static let gzipFixture = Data(base64Encoded: "H4sIAAAAAAAC/4ty9w3V9fX08/R19NF184wICQ1y1S0zBABkTKm0FwAAAA==")!
    private static let gzipDecoded = Data("ZGMU-MINIMAL-FIXTURE-v1".utf8)
    private static let gzipDecodedSHA = "7764f43c21e37e7407e3c88e92a76428533454f34a4ce265628a1df974366633"

    private var server = FakeServer()
    private var settings: ZGMicroSettings!
    private var cacheRoot: URL!

    override func setUpWithError() throws {
        try super.setUpWithError()
        server = FakeServer()
        settings = ZGMicroSettings(host: "127.0.0.1", port: 18_008, token: "zgphs00",
                                   connectTimeoutMs: 2_000, readTimeoutMs: 2_000,
                                   maxAttempts: 1, cacheBudgetMb: 64)
        cacheRoot = FileManager.default.temporaryDirectory
            .appendingPathComponent("zgmicro-tests-" + UUID().uuidString, isDirectory: true)
        ZGMicroFetcher.shared.reset()
        let server = self.server
        try ZGMicroFetcher.shared.configure(settings: settings, cacheRoot: cacheRoot,
                                            factory: { _, _ in FakeChannel(server: server) })
    }

    override func tearDownWithError() throws {
        ZGMicroFetcher.shared.reset()
        if let cacheRoot = cacheRoot { try? FileManager.default.removeItem(at: cacheRoot) }
        try super.tearDownWithError()
    }

    private func connect() throws -> ZGMicroConnection {
        let server = self.server
        return try ZGMicroConnection(endpoint: settings.endpoint, token: "zgphs00",
                                     connectTimeout: 2, readTimeout: 2,
                                     factory: { _, _ in FakeChannel(server: server) })
    }

    // MARK: 用例

    func testHandshakeReadsBlockSizeAndManifest() throws {
        let connection = try connect()
        XCTAssertEqual(connection.blockSize, 65_536)
        XCTAssertEqual(connection.manifestSize, 659_000)
        XCTAssertEqual(connection.manifestSHA.count, 32)
        XCTAssertEqual(connection.manifestSHA.first, 0x11)
        connection.close()
    }

    func testWrongTokenStopsAtHello() {
        let server = self.server
        server.token = "别的口令"
        XCTAssertThrowsError(try ZGMicroConnection(endpoint: settings.endpoint, token: "zgphs00",
                                                   connectTimeout: 2, readTimeout: 2,
                                                   factory: { _, _ in FakeChannel(server: server) })) { error in
            guard case ZGMicroFailure.server(let code, _, _) = error else {
                return XCTFail("期望 server(.auth)，实际 \(error)")
            }
            XCTAssertEqual(code, .auth)
        }
    }

    func testGetCarriesOffsetAndLengthAtExpectedOffsets() throws {
        server.resources["path:bridge/a.json"] = Self.plainFixture
        let connection = try connect()
        var collected = Data()
        try connection.fetch(id: "path:bridge/a.json", offset: 0, length: 0x7FFF_FF00) { _, chunk in
            collected.append(chunk)
        }
        XCTAssertEqual(collected, Self.plainFixture)
        XCTAssertEqual(server.requests.count, 1)
        XCTAssertEqual(server.requests[0].id, "path:bridge/a.json")
        XCTAssertEqual(server.requests[0].offset, 0)
        XCTAssertEqual(server.requests[0].length, 0x7FFF_FF00)
        connection.close()
    }

    func testMultiFrameDataIsReassembledAndEndFlagHonoured() throws {
        server.resources["path:native107/2047.glb"] = Self.plainFixture
        server.splitChunk = 7
        let connection = try connect()
        var collected = Data()
        let received = try connection.fetch(id: "path:native107/2047.glb", offset: 0, length: 0x7FFF_FF00) { _, chunk in
            collected.append(chunk)
        }
        XCTAssertEqual(received, Int64(Self.plainFixture.count))
        XCTAssertEqual(collected, Self.plainFixture)
        connection.close()
    }

    func testServerErrorBecomesTypedFailure() throws {
        server.failWith = (code: 0x0010, message: "no such pack")
        let connection = try connect()
        XCTAssertThrowsError(try connection.fetch(id: "path:scenes/99.json", offset: 0, length: 1_024) { _, _ in }) { error in
            guard case ZGMicroFailure.server(let code, let raw, let message) = error else {
                return XCTFail("期望 server 失败，实际 \(error)")
            }
            XCTAssertEqual(code, .noPack)
            XCTAssertEqual(raw, 0x0010)
            XCTAssertEqual(message, "no such pack")
        }
        XCTAssertFalse(connection.isUsable)
    }

    func testFetcherLandsDecodedGzipAndServesFromCache() throws {
        let remoteId = "path:ui110/labels.json"
        server.resources[remoteId] = Self.gzipFixture
        let first = try ZGMicroFetcher.shared.resolve(
            remoteId: remoteId, storedSize: Int64(Self.gzipFixture.count),
            decodedSize: Int64(Self.gzipDecoded.count), codec: 1,
            sha256: Self.gzipDecodedSHA, cancelled: { false })
        XCTAssertEqual(try Data(contentsOf: first), Self.gzipDecoded)
        XCTAssertEqual(ZGMicroFetcher.fileSize(first), Int64(Self.gzipDecoded.count))
        // 第二次必须完全不连网：把资源清空后仍能命中。
        server.resources.removeAll()
        let second = try ZGMicroFetcher.shared.resolve(
            remoteId: remoteId, storedSize: Int64(Self.gzipFixture.count),
            decodedSize: Int64(Self.gzipDecoded.count), codec: 1,
            sha256: Self.gzipDecodedSHA, cancelled: { false })
        XCTAssertEqual(second, first)
        let leftover = try FileManager.default.contentsOfDirectory(atPath: cacheRoot.path)
        XCTAssertFalse(leftover.contains { $0.hasSuffix(".part") }, "解码成功后不该留下 .part")
    }

    func testFetcherKeepsIdentityBytesAsIs() throws {
        let remoteId = "path:bridge/plain.bin"
        server.resources[remoteId] = Self.plainFixture
        let url = try ZGMicroFetcher.shared.resolve(
            remoteId: remoteId, storedSize: Int64(Self.plainFixture.count),
            decodedSize: Int64(Self.plainFixture.count), codec: 0,
            sha256: Self.plainFixtureSHA, cancelled: { false })
        XCTAssertEqual(try Data(contentsOf: url), Self.plainFixture)
    }

    func testChecksumMismatchDropsCacheAndThrows() throws {
        let remoteId = "path:bridge/tampered.bin"
        server.resources[remoteId] = Self.plainFixture
        XCTAssertThrowsError(try ZGMicroFetcher.shared.resolve(
            remoteId: remoteId, storedSize: Int64(Self.plainFixture.count),
            decodedSize: Int64(Self.plainFixture.count), codec: 0,
            sha256: String(repeating: "0", count: 64), cancelled: { false })) { error in
            guard case ZGMicroFailure.checksum(_, _) = error else {
                return XCTFail("期望 checksum，实际 \(error)")
            }
        }
        let key = ZGMicroFetcher.cacheKey(remoteId)
        let landed = cacheRoot.appendingPathComponent(key + ".bin")
        XCTAssertFalse(FileManager.default.fileExists(atPath: landed.path))
    }

    func testFetcherRefusesExecutablePayloads() {
        XCTAssertThrowsError(try ZGMicroFetcher.shared.resolve(
            remoteId: "path:client.js", storedSize: 10, decodedSize: 10, codec: 0,
            sha256: String(repeating: "a", count: 64), cancelled: { false })) { error in
            guard case ZGMicroFailure.refused(_) = error else {
                return XCTFail("期望 refused，实际 \(error)")
            }
        }
    }

    func testPartialFileResumesFromItsLength() throws {
        let remoteId = "path:characters84/body.glb"
        server.resources[remoteId] = Self.plainFixture
        let key = ZGMicroFetcher.cacheKey(remoteId)
        try FileManager.default.createDirectory(at: cacheRoot, withIntermediateDirectories: true)
        let part = cacheRoot.appendingPathComponent(key + ".part")
        try Self.plainFixture.prefix(12).write(to: part)
        _ = try ZGMicroFetcher.shared.resolve(
            remoteId: remoteId, storedSize: Int64(Self.plainFixture.count),
            decodedSize: Int64(Self.plainFixture.count), codec: 0,
            sha256: Self.plainFixtureSHA, cancelled: { false })
        XCTAssertEqual(server.requests.count, 1)
        XCTAssertEqual(server.requests[0].offset, 12, "续传要从已有长度开始，不能从 0 重来")
    }

    func testCancellationDuringFetchSurfacesAsAssetCancelled() {
        server.resources["path:scenes/big.glb"] = Self.plainFixture
        XCTAssertThrowsError(try ZGMicroFetcher.shared.resolve(
            remoteId: "path:scenes/big.glb", storedSize: Int64(Self.plainFixture.count),
            decodedSize: Int64(Self.plainFixture.count), codec: 0,
            sha256: Self.plainFixtureSHA, cancelled: { true })) { error in
            guard case ZGMicroFailure.cancelled = error else {
                return XCTFail("期望 cancelled，实际 \(error)")
            }
        }
    }
}
