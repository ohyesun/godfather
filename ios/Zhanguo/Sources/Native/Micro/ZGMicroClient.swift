import Foundation

// MARK: - 线格式

/// ZGMU/1 线格式常量。与服务器 `ZgmuServerCore.Proto` 和安卓 `Zgmu.java`
/// 是同一份定义，任何一边改动都必须同步另外两边。
///
/// 这里刻意只放常量与字节读写，不碰网络和文件系统：帧边界、END 位、
/// 错误码这三件事写错了在真机上只表现为"某个模型没加载出来"，
/// 在桌面上却能断言到具体字节。
enum ZGMicroWire {
    static let magic: UInt16 = 0x475A
    static let headerBytes = 8
    static let maxBody = (1 << 20) + 64
    static let protoVersion: UInt16 = 1

    static let typeHello: UInt8 = 0x01
    static let typeHelloAck: UInt8 = 0x81
    static let typeGet: UInt8 = 0x10
    static let typeData: UInt8 = 0x90
    static let typeError: UInt8 = 0x93
    static let typePing: UInt8 = 0x18
    static let typePong: UInt8 = 0x99
    static let typeClose: UInt8 = 0x1A

    static let flagEnd: UInt8 = 0x02

    /// HELLO_ACK 的最小长度：status(1) + blockSize(4) + maxInflight(2)
    /// + serverTime(8) + manifestSize(8) + manifestSha256(32)。
    static let helloAckBytes = 55

    enum Code: UInt16 {
        case notHello = 0x0001
        case protoViolation = 0x0002
        case inflight = 0x0003
        case noPack = 0x0010
        case offset = 0x0011
        case length = 0x0012
        case auth = 0x0020
        case io = 0x0030

        var text: String {
            switch self {
            case .notHello: return "未 HELLO"
            case .protoViolation: return "协议错误"
            case .inflight: return "在途请求超限"
            case .noPack: return "资源不存在"
            case .offset: return "offset 越界"
            case .length: return "length 非法"
            case .auth: return "口令错误"
            case .io: return "服务端读取失败"
            }
        }

        /// 重试没有意义的那几个：同一个请求再发一次还是同一个结果。
        var isFatal: Bool {
            switch self {
            case .auth, .noPack, .offset, .length, .protoViolation, .notHello: return true
            case .inflight, .io: return false
            }
        }
    }
}

enum ZGMicroBytes {
    static func u16(_ b: [UInt8], _ o: Int) -> UInt16 {
        guard o >= 0, o + 2 <= b.count else { return 0 }
        return UInt16(b[o]) | (UInt16(b[o + 1]) << 8)
    }
    static func u32(_ b: [UInt8], _ o: Int) -> UInt32 {
        guard o >= 0, o + 4 <= b.count else { return 0 }
        return UInt32(b[o]) | (UInt32(b[o + 1]) << 8) | (UInt32(b[o + 2]) << 16) | (UInt32(b[o + 3]) << 24)
    }
    static func u64(_ b: [UInt8], _ o: Int) -> UInt64 {
        guard o >= 0, o + 8 <= b.count else { return 0 }
        var v: UInt64 = 0
        for i in stride(from: 7, through: 0, by: -1) { v = (v << 8) | UInt64(b[o + i]) }
        return v
    }
    static func put16(_ b: inout [UInt8], _ o: Int, _ v: UInt16) {
        guard o >= 0, o + 2 <= b.count else { return }
        b[o] = UInt8(v & 0xFF); b[o + 1] = UInt8((v >> 8) & 0xFF)
    }
    static func put32(_ b: inout [UInt8], _ o: Int, _ v: UInt32) {
        guard o >= 0, o + 4 <= b.count else { return }
        for i in 0..<4 { b[o + i] = UInt8((v >> (8 * UInt32(i))) & 0xFF) }
    }
    static func put64(_ b: inout [UInt8], _ o: Int, _ v: UInt64) {
        guard o >= 0, o + 8 <= b.count else { return }
        for i in 0..<8 { b[o + i] = UInt8((v >> (8 * UInt64(i))) & 0xFF) }
    }
}

// MARK: - 失败

enum ZGMicroFailure: Error, LocalizedError {
    case notConfigured
    case refused(String)
    case protocolViolation(String)
    case server(code: ZGMicroWire.Code?, raw: UInt16, message: String)
    case connection(String)
    case timeout
    case cancelled
    case sizeMismatch(expected: Int64, actual: Int64)
    case checksum(expected: String, actual: String)
    case disk(String)

    var errorDescription: String? {
        switch self {
        case .notConfigured: return "微端服务器未配置"
        case .refused(let why): return why
        case .protocolViolation(let why): return "微端协议异常：" + why
        case .server(let code, let raw, let message):
            let name = code?.text ?? "未知错误"
            return "微端返回 0x" + String(raw, radix: 16) + "（" + name + "）：" + message
        case .connection(let why): return "微端连接失败：" + why
        case .timeout: return "微端读取超时"
        case .cancelled: return "请求已取消"
        case .sizeMismatch(let expected, let actual):
            return "资源长度不符：" + String(actual) + " != " + String(expected)
        case .checksum(let expected, let actual):
            return "资源校验失败：" + String(actual.prefix(16)) + " != " + String(expected.prefix(16))
        case .disk(let why): return "资源缓存失败：" + why
        }
    }

    /// 内容级失败重试无意义，网络级失败可以退避重试。
    var isRetryable: Bool {
        switch self {
        case .server(let code, _, _): return !(code?.isFatal ?? false)
        case .connection, .timeout, .disk: return true
        case .protocolViolation, .sizeMismatch, .checksum, .refused, .notConfigured, .cancelled:
            return false
        }
    }
}

// MARK: - 异步通道的同步读

/// 把 `ZGByteChannel` 的回调式收发封成同步阻塞读。
///
/// 调用方是 `ZGAssetSchemeHandler` 的 worker 线程（不是主线程），阻塞是安全的。
/// 回调可能在同一把锁内被同步触发（测试用的假通道就是这样），所以发起
/// `receive` 一律在解锁之后，绝不持锁回调。
final class ZGMicroChannelReader {
    private let channel: ZGByteChannel
    private let timeout: TimeInterval
    private let condition = NSCondition()
    private var buffer = Data()
    private var failure: Error?
    private var atEnd = false
    private var receiving = false
    private var closed = false

    init(channel: ZGByteChannel, timeout: TimeInterval) {
        self.channel = channel
        self.timeout = max(1, timeout)
    }

    func open() throws {
        let ready = DispatchSemaphore(value: 0)
        let box = ZGErrorBox()
        channel.start { error in box.value = error; ready.signal() }
        if ready.wait(timeout: .now() + timeout) == .timedOut {
            channel.cancel()
            throw ZGMicroFailure.timeout
        }
        if let error = box.value {
            channel.cancel()
            throw ZGMicroFailure.connection(error.localizedDescription)
        }
    }

    func write(_ data: Data) throws {
        let done = DispatchSemaphore(value: 0)
        let box = ZGErrorBox()
        channel.send(data) { error in box.value = error; done.signal() }
        if done.wait(timeout: .now() + timeout) == .timedOut {
            channel.cancel()
            throw ZGMicroFailure.timeout
        }
        if let error = box.value { throw ZGMicroFailure.connection(error.localizedDescription) }
    }

    func read(_ count: Int) throws -> Data {
        guard count >= 0 else { throw ZGMicroFailure.protocolViolation("读取长度为负") }
        if count == 0 { return Data() }
        while true {
            var shouldArm = false
            condition.lock()
            if buffer.count >= count {
                let out = Data(buffer.prefix(count))
                buffer.removeFirst(count)
                condition.unlock()
                return out
            }
            if let failure = failure { condition.unlock(); throw failure }
            if closed { condition.unlock(); throw ZGMicroFailure.cancelled }
            if atEnd { condition.unlock(); throw ZGMicroFailure.protocolViolation("连接提前结束") }
            if receiving {
                if !condition.wait(until: Date().addingTimeInterval(timeout)) {
                    condition.unlock()
                    throw ZGMicroFailure.timeout
                }
            } else {
                receiving = true
                shouldArm = true
            }
            condition.unlock()
            if shouldArm { arm() }
        }
    }

    func close() {
        condition.lock()
        let wasClosed = closed
        closed = true
        condition.broadcast()
        condition.unlock()
        if !wasClosed { channel.cancel() }
    }

    private func arm() {
        channel.receive(maximum: 65_536) { [weak self] data, complete, error in
            guard let self = self else { return }
            self.condition.lock()
            self.receiving = false
            if let error = error {
                if self.failure == nil { self.failure = ZGMicroFailure.connection(error.localizedDescription) }
            } else {
                if !data.isEmpty { self.buffer.append(data) }
                if complete { self.atEnd = true }
            }
            self.condition.broadcast()
            self.condition.unlock()
        }
    }
}

/// 同步回调里回填结果用的极简盒子。回调与等待方在一个信号量上配对，
/// 因此不需要额外加锁。
final class ZGErrorBox: @unchecked Sendable {
    var value: Error?
}

// MARK: - 单连接

/// 一条 ZGMU/1 连接。同一时刻只允许一个在途请求（服务端按 reqId 串行回帧），
/// 并发靠 `ZGMicroPool` 的多条连接解决。
final class ZGMicroConnection: @unchecked Sendable {
    private let reader: ZGMicroChannelReader
    private var seq: UInt32 = 0
    private var broken = false
    private(set) var blockSize = 65_536
    private(set) var manifestSize: Int64 = 0
    private(set) var manifestSHA = Data()

    init(endpoint: ZGNetworkEndpoint, token: String, pinned: Data? = nil,
         connectTimeout: TimeInterval, readTimeout: TimeInterval,
         factory: ZGChannelFactory) throws {
        let channel = try factory(endpoint, pinned)
        reader = ZGMicroChannelReader(channel: channel, timeout: connectTimeout)
        do {
            try reader.open()
            try hello(token: token)
        } catch {
            reader.close()
            throw error
        }
    }

    var isUsable: Bool { !broken }

    /// 取 `[offset, offset+length)`。服务端允许 length 越过结尾并自动裁剪，
    /// 所以传 0x7FFF_FF00 等于"取到底"。
    @discardableResult
    func fetch(id: String, offset: Int64, length: Int,
               sink: (Int64, Data) throws -> Void) throws -> Int64 {
        guard !broken else { throw ZGMicroFailure.protocolViolation("连接已失效") }
        let idBytes = Array(id.utf8)
        guard !idBytes.isEmpty, idBytes.count <= 255 else {
            throw ZGMicroFailure.protocolViolation("资源 id 长度非法")
        }
        seq &+= 1
        let rid = seq
        var body = [UInt8](repeating: 0, count: 6 + idBytes.count + 12)
        ZGMicroBytes.put32(&body, 0, rid)
        ZGMicroBytes.put16(&body, 4, UInt16(idBytes.count))
        body.replaceSubrange(6..<(6 + idBytes.count), with: idBytes)
        ZGMicroBytes.put64(&body, 6 + idBytes.count, UInt64(bitPattern: offset))
        ZGMicroBytes.put32(&body, 14 + idBytes.count, UInt32(truncatingIfNeeded: length))

        do {
            try reader.write(Self.frame(ZGMicroWire.typeGet, flags: 0, body: body))
            var expected = offset
            var received: Int64 = 0
            while true {
                let (type, flags, payload) = try readFrame()
                if type == ZGMicroWire.typeError { throw Self.parseError(payload) }
                guard type == ZGMicroWire.typeData else {
                    throw ZGMicroFailure.protocolViolation("期望 DATA，收到 0x" + String(type, radix: 16))
                }
                guard payload.count >= 16 else {
                    throw ZGMicroFailure.protocolViolation("DATA 帧太短")
                }
                let got = ZGMicroBytes.u32(payload, 0)
                guard got == rid else {
                    throw ZGMicroFailure.protocolViolation("reqId 不匹配")
                }
                let chunkOffset = Int64(bitPattern: ZGMicroBytes.u64(payload, 4))
                guard chunkOffset == expected else {
                    throw ZGMicroFailure.protocolViolation("DATA 不连续")
                }
                let dataLength = Int(ZGMicroBytes.u32(payload, 12))
                guard dataLength >= 0, 16 + dataLength <= payload.count else {
                    throw ZGMicroFailure.protocolViolation("DATA 长度越界")
                }
                if dataLength > 0 {
                    try sink(chunkOffset, Data(payload[16..<(16 + dataLength)]))
                }
                received += Int64(dataLength)
                expected += Int64(dataLength)
                if flags & ZGMicroWire.flagEnd != 0 { return received }
            }
        } catch {
            // 帧边界已经不可信，这条连接不能再复用。
            broken = true
            throw error
        }
    }

    func close() {
        broken = true
        try? reader.write(Self.frame(ZGMicroWire.typeClose, flags: 0, body: []))
        reader.close()
    }

    /// 只关闭底层通道，不礼貌地发 CLOSE（超时/出错后用）。
    func abort() {
        broken = true
        reader.close()
    }

    // MARK: 内部

    private func hello(token: String) throws {
        let tokenBytes = Array(token.utf8)
        guard tokenBytes.count <= 255 else {
            throw ZGMicroFailure.protocolViolation("口令过长")
        }
        var body = [UInt8](repeating: 0, count: 3 + tokenBytes.count + 8)
        ZGMicroBytes.put16(&body, 0, ZGMicroWire.protoVersion)
        body[2] = UInt8(tokenBytes.count)
        if !tokenBytes.isEmpty {
            body.replaceSubrange(3..<(3 + tokenBytes.count), with: tokenBytes)
        }
        ZGMicroBytes.put64(&body, 3 + tokenBytes.count,
                           UInt64(Date().timeIntervalSince1970 * 1000))
        try reader.write(Self.frame(ZGMicroWire.typeHello, flags: 0, body: body))

        let (type, _, payload) = try readFrame()
        if type == ZGMicroWire.typeError { throw Self.parseError(payload) }
        guard type == ZGMicroWire.typeHelloAck else {
            throw ZGMicroFailure.protocolViolation("首帧不是 HELLO_ACK")
        }
        guard payload.count >= ZGMicroWire.helloAckBytes else {
            throw ZGMicroFailure.protocolViolation("HELLO_ACK 太短")
        }
        guard payload[0] == 0 else {
            throw ZGMicroFailure.server(code: .auth, raw: UInt16(payload[0]), message: "hello status")
        }
        let block = Int(ZGMicroBytes.u32(payload, 1))
        if block >= 4_096, block <= ZGMicroWire.maxBody - 64 { blockSize = block }
        manifestSize = Int64(ZGMicroBytes.u64(payload, 7))
        manifestSHA = Data(payload[23..<ZGMicroWire.helloAckBytes])
    }

    private func readFrame() throws -> (UInt8, UInt8, [UInt8]) {
        let header = try reader.read(ZGMicroWire.headerBytes)
        let bytes = [UInt8](header)
        guard ZGMicroBytes.u16(bytes, 0) == ZGMicroWire.magic else {
            broken = true
            throw ZGMicroFailure.protocolViolation("帧头 magic 不符")
        }
        let type = bytes[2]
        let flags = bytes[3]
        let length = Int(ZGMicroBytes.u32(bytes, 4))
        guard length >= 0, length <= ZGMicroWire.maxBody else {
            broken = true
            throw ZGMicroFailure.protocolViolation("帧体超长")
        }
        let body = length == 0 ? [UInt8]() : [UInt8](try reader.read(length))
        return (type, flags, body)
    }

    private static func frame(_ type: UInt8, flags: UInt8, body: [UInt8]) -> Data {
        var out = [UInt8](repeating: 0, count: ZGMicroWire.headerBytes + body.count)
        ZGMicroBytes.put16(&out, 0, ZGMicroWire.magic)
        out[2] = type
        out[3] = flags
        ZGMicroBytes.put32(&out, 4, UInt32(body.count))
        if !body.isEmpty {
            out.replaceSubrange(ZGMicroWire.headerBytes..<(ZGMicroWire.headerBytes + body.count), with: body)
        }
        return Data(out)
    }

    private static func parseError(_ body: [UInt8]) -> ZGMicroFailure {
        guard body.count >= 7 else {
            return .protocolViolation("错误帧太短")
        }
        let raw = ZGMicroBytes.u16(body, 4)
        let declared = Int(body[6])
        let upper = min(body.count, 7 + declared)
        let message = upper > 7 ? String(decoding: body[7..<upper], as: UTF8.self) : ""
        return .server(code: ZGMicroWire.Code(rawValue: raw), raw: raw, message: message)
    }
}

// MARK: - 连接池

/// 最多 4 条连接，与安卓 `PakFetcher.MAX_CONNECTIONS` 一致。
/// 借不到就等，等不到就报"忙"——不无限建连，避免把小机房打出连接风暴。
final class ZGMicroPool: @unchecked Sendable {
    static let maxConnections = 4

    private let condition = NSCondition()
    private var idle: [ZGMicroConnection] = []
    private var live = 0
    private var closed = false
    private let endpoint: ZGNetworkEndpoint
    private let token: String
    private let connectTimeout: TimeInterval
    private let readTimeout: TimeInterval
    private let factory: ZGChannelFactory

    init(endpoint: ZGNetworkEndpoint, token: String,
         connectTimeout: TimeInterval, readTimeout: TimeInterval,
         factory: @escaping ZGChannelFactory) {
        self.endpoint = endpoint
        self.token = token
        self.connectTimeout = connectTimeout
        self.readTimeout = readTimeout
        self.factory = factory
    }

    func borrow() throws -> ZGMicroConnection {
        let deadline = Date().addingTimeInterval(max(5, connectTimeout))
        condition.lock()
        while true {
            if closed { condition.unlock(); throw ZGMicroFailure.cancelled }
            while !idle.isEmpty {
                let reused = idle.removeFirst()
                if reused.isUsable {
                    condition.unlock()
                    return reused
                }
                live -= 1
                reused.abort()
            }
            if live < ZGMicroPool.maxConnections {
                live += 1
                condition.unlock()
                do {
                    return try ZGMicroConnection(endpoint: endpoint, token: token,
                        connectTimeout: connectTimeout, readTimeout: readTimeout,
                        factory: factory)
                } catch {
                    condition.lock()
                    live -= 1
                    condition.broadcast()
                    condition.unlock()
                    throw error
                }
            }
            if !condition.wait(until: deadline) {
                condition.unlock()
                throw ZGMicroFailure.connection("连接池忙")
            }
        }
    }

    func give(_ connection: ZGMicroConnection) {
        condition.lock()
        if !closed, connection.isUsable, idle.count < ZGMicroPool.maxConnections {
            idle.append(connection)
            condition.broadcast()
            condition.unlock()
            return
        }
        live -= 1
        condition.broadcast()
        condition.unlock()
        connection.abort()
    }

    func drop(_ connection: ZGMicroConnection) {
        condition.lock()
        live -= 1
        condition.broadcast()
        condition.unlock()
        connection.abort()
    }

    func closeAll() {
        condition.lock()
        closed = true
        let pending = idle
        idle.removeAll()
        live -= pending.count
        condition.broadcast()
        condition.unlock()
        for connection in pending { connection.close() }
    }
}
