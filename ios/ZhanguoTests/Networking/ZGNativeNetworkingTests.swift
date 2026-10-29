import XCTest
@testable import Zhanguo

/// All channels are in-memory. No test connects to an account or live host.
final class ZGNativeNetworkingTests: XCTestCase {
    private final class FakeChannel: ZGByteChannel {
        let lock = NSLock()
        var cancelled = false, writes = [Data](), incoming = [Data]()
        var receiver: ((Data, Bool, Error?) -> Void)?, maximum = 0
        var holdWrites = false, writeCallbacks = [(Error?) -> Void]()
        var respond: ((Data) -> Data?)?
        func start(_ completion: @escaping (Error?) -> Void) { completion(nil) }
        func send(_ data: Data, completion: @escaping (Error?) -> Void) {
            lock.lock(); writes.append(data); let response = respond?(data)
            let hold = holdWrites; if hold { writeCallbacks.append(completion) }; lock.unlock()
            if !hold { completion(nil) }; if let response = response { emit(response) }
        }
        func receive(maximum: Int, completion: @escaping (Data, Bool, Error?) -> Void) {
            lock.lock(); self.maximum = maximum; receiver = completion; lock.unlock(); drain()
        }
        func cancel() { lock.lock(); cancelled = true; lock.unlock() }
        func emit(_ data: Data) { lock.lock(); incoming.append(data); lock.unlock(); drain() }
        private func drain() {
            lock.lock()
            guard let callback = receiver, !incoming.isEmpty else { lock.unlock(); return }
            var data = incoming.removeFirst(); let size = min(maximum, data.count)
            let delivered = Data(data.prefix(size)); data.removeFirst(size)
            if !data.isEmpty { incoming.insert(data, at: 0) }; receiver = nil; lock.unlock()
            callback(delivered, false, nil)
        }
        func failFirstWrite() {
            lock.lock(); let callback = writeCallbacks.removeFirst(); lock.unlock()
            callback(NSError(domain: "offline-test", code: 1))
        }
        var writeCount: Int { lock.lock(); defer { lock.unlock() }; return writes.count }
    }
    private let profile = ZGConnectionProfile(role: "direct-game", host: "127.0.0.1", port: 6020,
        transport: "direct-tcp", launcherHost: "127.0.0.1", launcherPort: 9999)
    private func service(_ channels: [FakeChannel], profile: ZGConnectionProfile? = nil) throws -> ZGNativeNetworking {
        var index = 0
        let service = try ZGNativeNetworking(profile: profile ?? self.profile,
            launcherAuthentication: Data("fixture-auth".utf8), callbackQueue: .global(), channelFactory: { _, _ in
                guard index < channels.count else { throw ZGNativeNetworkFailure.network(false) }
                defer { index += 1 }; return channels[index]
            })
        service.replaceOwner("document-A"); return service
    }
    private func request(_ service: ZGNativeNetworking, _ route: String, _ body: [String: Any] = [:],
                         owner: String = "document-A", endpoint: String = "zg+tcp://127.0.0.1:6020") -> Result<ZGNativeTransportReply, Error> {
        let done = expectation(description: route); var result: Result<ZGNativeTransportReply, Error>!
        let text = String(data: try! JSONSerialization.data(withJSONObject: body), encoding: .utf8)!
        service.request(owner: owner, endpoint: endpoint, route: route, credential: "", account: "", body: text) {
            result = $0; done.fulfill()
        }
        wait(for: [done], timeout: 2); return result
    }
    private func body(_ result: Result<ZGNativeTransportReply, Error>) throws -> [String: Any] {
        let reply = try result.get(); XCTAssertEqual(reply.status, 200)
        return try JSONSerialization.jsonObject(with: Data(reply.body.utf8)) as! [String: Any]
    }
    private func open(_ service: ZGNativeNetworking, port: Int = 6020) throws -> String {
        try body(request(service, "/direct/open", ["host": "127.0.0.1", "port": port]))["connectionId"] as! String
    }
    private func failure(_ result: Result<ZGNativeTransportReply, Error>) -> ZGNativeNetworkFailure? {
        do { _ = try result.get(); XCTFail("Expected rejection"); return nil }
        catch { return error as? ZGNativeNetworkFailure }
    }
    func testGoldenWireAndBounds() throws {
        XCTAssertEqual(try ZGWireCodec.decodeHex("00AaFF"), Data([0, 170, 255]))
        XCTAssertThrowsError(try ZGWireCodec.decodeHex("x0")); XCTAssertThrowsError(try ZGWireCodec.decodeHex("0"))
        XCTAssertEqual(try ZGWireCodec.launcherRequest(command: 9, payload: Data([1, 2, 3])),
            Data([1, 9, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3]))
        XCTAssertEqual(ZGWireCodec.filePayload().count, 38)
        XCTAssertEqual(ZGWireCodec.filePayload().suffix(4), Data(repeating: 0, count: 4))
        XCTAssertEqual(ZGWireCodec.hex(try ZGWireCodec.relayFrame(id: 0x0102030405060708, payload: Data([123, 125]))),
            "5a47545001000102030405060708000000027b7d")
        var invalid = Data(repeating: 0, count: 20); invalid[0] = 1; invalid[7] = 1
        XCTAssertThrowsError(try ZGWireCodec.launcherHeader(invalid))
        XCTAssertThrowsError(try ZGWireCodec.versionPayload(authentication: Data()))
    }
    func testGBKChineseAndRegistration() throws {
        XCTAssertEqual(try ZGWireCodec.decodeLauncherText(Data([0xb2, 0xe2, 0xca, 0xd4])), "测试")
        XCTAssertEqual(try ZGWireCodec.encodeGBK("测试"), Data([0xb2, 0xe2, 0xca, 0xd4]))
        XCTAssertThrowsError(try ZGWireCodec.encodeGBK("🙂"))
        XCTAssertThrowsError(try ZGWireCodec.decodeLauncherText(Data([0x81])))
        let bytes = try ZGWireCodec.registerPayload(["account": "tester1", "password": "secret1", "qq": "", "zone": "一区"])
        XCTAssertEqual(try ZGWireCodec.integer(bytes, 0, 4), 7)
        XCTAssertEqual(try ZGWireCodec.integer(bytes, 4, 4), 7)
        XCTAssertEqual(try ZGWireCodec.integer(bytes, 8, 4), 10)
        XCTAssertEqual(bytes.count, 36)
        XCTAssertThrowsError(try ZGWireCodec.registerPayload(["account": "bad", "password": "x"]))
    }
    func testPinnedLeafValidityIsNotExemptAsAnchor() {
        func element(_ tag: UInt8, _ bytes: Data) -> Data { Data([tag, UInt8(bytes.count)]) + bytes }
        let validity = element(0x30, element(0x17, Data("250101000000Z".utf8)) + element(0x17, Data("270101000000Z".utf8)))
        let tbs = element(0x30, element(0x02, Data([1])) + element(0x30, Data()) + element(0x30, Data()) + validity)
        let certificate = element(0x30, tbs)
        let format = ISO8601DateFormatter()
        XCTAssertTrue(ZGCertificateValidity.containsNow(certificate, now: format.date(from: "2026-10-16T00:00:00Z")!))
        XCTAssertFalse(ZGCertificateValidity.containsNow(certificate, now: format.date(from: "2024-10-16T00:00:00Z")!))
        XCTAssertFalse(ZGCertificateValidity.containsNow(certificate, now: format.date(from: "2028-10-16T00:00:00Z")!))
        XCTAssertFalse(ZGCertificateValidity.containsNow(Data([0x30, 0x80])))
    }
    func testEndpointAndPerConnectionIsolation() throws {
        let old = FakeChannel(), new = FakeChannel(), native = try service([old, new])
        XCTAssertEqual(failure(request(native, "/direct/open", ["host": "192.0.2.2", "port": 6020]))?.code, "invalid_native_request")
        XCTAssertEqual(failure(request(native, "/direct/open", [:], endpoint: "zg+tcp://127.0.0.1:9999"))?.code, "invalid_native_request")
        let oldID = try open(native), newID = try open(native); XCTAssertNotEqual(oldID, newID)
        XCTAssertEqual(failure(request(native, "/direct/close", ["connectionId": oldID]))?.code, "stale_connection")
        XCTAssertFalse(new.cancelled)
        _ = try body(request(native, "/direct/send", ["connectionId": newID, "packetHex": "010203"]))
        XCTAssertEqual(new.writeCount, 1)
        XCTAssertEqual(failure(request(native, "/direct/send", ["packetHex": "00"]))?.code, "stale_connection")
        XCTAssertEqual(failure(request(native, "/direct/send", ["connectionId": oldID, "packetHex": "00"]))?.code, "stale_connection")
        native.cancelAll()
    }
    func testRawFragmentedBytesAndEmptyPoll() throws {
        let socket = FakeChannel(), native = try service([socket]), id = try open(native)
        socket.emit(Data([1, 2, 3])); socket.emit(Data([4, 5, 6, 7]))
        var collected = ""
        for _ in 0..<2 {
            collected += try body(request(native, "/direct/receive", ["connectionId": id]))["dataHex"] as! String
            if collected.count == 14 { break }
        }
        XCTAssertEqual(collected, "01020304050607", "TCP chunks are not crypto or game-frame boundaries")
        XCTAssertEqual(try body(request(native, "/direct/receive", ["connectionId": id]))["dataHex"] as? String, "")
        native.cancelAll()
    }
    func testOwnerCancelsQueuedWriteAndNeverReplaysUnknown() throws {
        let socket = FakeChannel(), native = try service([socket]), id = try open(native)
        socket.holdWrites = true
        let first = expectation(description: "unknown old write"), queued = expectation(description: "queued old write")
        let text = String(data: try JSONSerialization.data(withJSONObject: ["connectionId": id, "packetHex": "0102"]), encoding: .utf8)!
        native.request(owner: "document-A", endpoint: "zg+tcp://127.0.0.1:6020", route: "/direct/send", credential: "", account: "", body: text) {
            XCTAssertEqual(self.failure($0)?.delivery, .unknown); first.fulfill()
        }
        native.request(owner: "document-A", endpoint: "zg+tcp://127.0.0.1:6020", route: "/direct/send", credential: "", account: "", body: text) {
            XCTAssertEqual(self.failure($0)?.delivery, .notSent); queued.fulfill()
        }
        native.replaceOwner("document-B"); wait(for: [first, queued], timeout: 2)
        XCTAssertEqual(socket.writeCount, 1, "Queued old-owner data was never written or replayed")
        XCTAssertEqual(failure(request(native, "/direct/close", ["connectionId": id]))?.code, "stale_owner")
        native.cancelAll()
    }
    func testTransportWriteFailureIsUnknownAndNotRetried() throws {
        let socket = FakeChannel(), native = try service([socket]), id = try open(native); socket.holdWrites = true
        let done = expectation(description: "write error")
        let text = String(data: try JSONSerialization.data(withJSONObject: ["connectionId": id, "packetHex": "00"]), encoding: .utf8)!
        native.request(owner: "document-A", endpoint: "zg+tcp://127.0.0.1:6020", route: "/direct/send", credential: "", account: "", body: text) {
            XCTAssertEqual(self.failure($0)?.delivery, .unknown); done.fulfill()
        }
        for _ in 0..<100 where socket.writeCount == 0 { Thread.sleep(forTimeInterval: 0.001) }
        XCTAssertEqual(socket.writeCount, 1); socket.failFirstWrite(); wait(for: [done], timeout: 2)
        XCTAssertEqual(socket.writeCount, 1); native.cancelAll()
    }
    func testLauncherCatalogueEnablesOnlyReturnedEndpoints() throws {
        let launcher = FakeChannel(), game = FakeChannel(), native = try service([launcher, game])
        let config = Data("[Server]\n1=普通|一区|one|127.0.0.1|7000|1|0|6021\n".utf8)
        launcher.respond = { packet in
            var header = Data([1, 0, 0, 0]); ZGWireCodec.appendLE(0, bytes: 4, to: &header)
            ZGWireCodec.appendLE(packet[1] == 2 ? UInt64(config.count) : 0, bytes: 8, to: &header)
            ZGWireCodec.appendLE(0, bytes: 4, to: &header)
            if packet[1] == 2 { header.append(config) }; return header
        }
        XCTAssertEqual(try body(request(native, "/direct/zones"))["configText"] as? String, String(data: config, encoding: .utf8))
        XCTAssertEqual(launcher.writeCount, 2)
        XCTAssertFalse(try open(native, port: 7000).isEmpty)
        XCTAssertEqual(failure(request(native, "/direct/open", ["host": "127.0.0.1", "port": 9998]))?.code, "invalid_native_request")
        native.cancelAll()
    }
    func testLegacyTLSFramingThroughInjectedChannel() throws {
        let channel = FakeChannel()
        let relay = ZGConnectionProfile(role: "server", host: "relay.invalid", port: 9443, transport: "zg-tls1", cert: "AQ==")
        let native = try service([channel], profile: relay)
        channel.respond = { data in
            let id = try! ZGWireCodec.integer(data, 6, 8, bigEndian: true)
            let payload = try! JSONSerialization.data(withJSONObject: ["v": 1, "id": id, "status": 200, "body": ["ok": true]])
            return try! ZGWireCodec.relayFrame(id: id, payload: payload)
        }
        XCTAssertEqual(try body(request(native, "/mobile/state", endpoint: "zg+tls://relay.invalid:9443"))["ok"] as? Bool, true)
        XCTAssertEqual(try body(request(native, "/mobile/state", endpoint: "zg+tls://relay.invalid:9443"))["ok"] as? Bool, true)
        XCTAssertEqual(channel.writeCount, 2, "The pinned TLS stream is reused, not reconnected for every poll")
        native.cancelAll()
    }
}
