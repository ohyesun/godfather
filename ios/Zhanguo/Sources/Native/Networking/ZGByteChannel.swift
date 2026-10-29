import Foundation
import Network
import Security

/// Test injection point. Implementations must call each completion once;
/// service state is always serialized again on the service's private queue.
protocol ZGByteChannel: AnyObject {
    func start(_ completion: @escaping (Error?) -> Void)
    func send(_ data: Data, completion: @escaping (Error?) -> Void)
    func receive(maximum: Int, completion: @escaping (Data, Bool, Error?) -> Void)
    func cancel()
}
typealias ZGChannelFactory = (ZGNetworkEndpoint, Data?) throws -> ZGByteChannel

final class ZGNWByteChannel: ZGByteChannel {
    private let connection: NWConnection
    private let queue: DispatchQueue
    init(endpoint: ZGNetworkEndpoint, pinnedCertificate: Data?) throws {
        let channelQueue = DispatchQueue(label: "zg.native.tcp.channel")
        self.queue = channelQueue
        guard let port = NWEndpoint.Port(rawValue: UInt16(endpoint.port)) else {
            throw ZGNativeNetworkFailure.invalid("端口无效")
        }
        let tcp = NWProtocolTCP.Options(); tcp.noDelay = true
        let parameters: NWParameters
        if let der = pinnedCertificate {
            guard let pinned = SecCertificateCreateWithData(nil, der as CFData) else {
                throw ZGNativeNetworkFailure.invalid("固定网关证书无效")
            }
            let tls = NWProtocolTLS.Options()
            sec_protocol_options_set_min_tls_protocol_version(tls.securityProtocolOptions, .TLSv12)
            sec_protocol_options_set_verify_block(tls.securityProtocolOptions, { _, trust, finish in
                let value = sec_trust_copy_ref(trust).takeRetainedValue()
                guard let leaf = SecTrustGetCertificateAtIndex(value, 0),
                      SecCertificateCopyData(leaf) as Data == der,
                      ZGCertificateValidity.containsNow(der) else { finish(false); return }
                // Exact leaf pin + native validity verification, including a
                // locally pinned self-signed gateway. Never accept any leaf.
                SecTrustSetPolicies(value, SecPolicyCreateBasicX509())
                SecTrustSetAnchorCertificates(value, [pinned] as CFArray)
                SecTrustSetAnchorCertificatesOnly(value, true)
                finish(SecTrustEvaluateWithError(value, nil))
            }, channelQueue)
            parameters = NWParameters(tls: tls, tcp: tcp)
        } else { parameters = NWParameters(tls: nil, tcp: tcp) }
        connection = NWConnection(host: NWEndpoint.Host(endpoint.host), port: port, using: parameters)
    }
    func start(_ completion: @escaping (Error?) -> Void) {
        var completed = false
        connection.stateUpdateHandler = { state in
            guard !completed else { return }
            switch state {
            case .ready: completed = true; completion(nil)
            case .failed(let error): completed = true; completion(error)
            case .cancelled: completed = true; completion(ZGNativeNetworkFailure.staleConnection)
            default: break
            }
        }
        connection.start(queue: queue)
    }
    func send(_ data: Data, completion: @escaping (Error?) -> Void) {
        connection.send(content: data, completion: .contentProcessed { completion($0) })
    }
    func receive(maximum: Int, completion: @escaping (Data, Bool, Error?) -> Void) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: maximum) { data, _, complete, error in
            completion(data ?? Data(), complete, error)
        }
    }
    func cancel() { connection.cancel() }
}

/// Explicit leaf time validation, since an exact leaf used as an anchor can
/// otherwise receive trust-anchor validity treatment. Only DER X.509 UTC or
/// GeneralizedTime in their canonical Zulu forms is accepted.
enum ZGCertificateValidity {
    private struct Element { let tag: UInt8; let content: Range<Int>; let end: Int }
    private static func element(_ bytes: [UInt8], at offset: Int, end: Int) -> Element? {
        guard offset >= 0, offset + 2 <= end, end <= bytes.count else { return nil }
        let marker = Int(bytes[offset + 1]); var cursor = offset + 2, length = marker
        if marker & 0x80 != 0 {
            let count = marker & 0x7f; guard count > 0, count <= 4, cursor + count <= end else { return nil }
            length = 0; for index in 0..<count { length = (length << 8) | Int(bytes[cursor + index]) }; cursor += count
        }
        guard length >= 0, length <= end - cursor else { return nil }
        return Element(tag: bytes[offset], content: cursor..<(cursor + length), end: cursor + length)
    }
    private static func date(_ bytes: [UInt8], _ node: Element) -> Date? {
        let data = Array(bytes[node.content]), yearCount = node.tag == 0x17 ? 2 : node.tag == 0x18 ? 4 : 0
        guard yearCount > 0, data.count == yearCount + 11, data.last == 90,
              data.dropLast().allSatisfy({ (48...57).contains($0) }) else { return nil }
        func number(_ start: Int, _ count: Int = 2) -> Int { data[start..<(start + count)].reduce(0) { $0 * 10 + Int($1 - 48) } }
        var year = number(0, yearCount); if yearCount == 2 { year += year >= 50 ? 1900 : 2000 }
        var calendar = Calendar(identifier: .gregorian); calendar.timeZone = TimeZone(secondsFromGMT: 0)!
        let components = DateComponents(timeZone: calendar.timeZone, year: year, month: number(yearCount),
            day: number(yearCount + 2), hour: number(yearCount + 4), minute: number(yearCount + 6), second: number(yearCount + 8))
        guard let result = calendar.date(from: components),
              calendar.dateComponents([.year, .month, .day, .hour, .minute, .second], from: result).year == components.year,
              calendar.component(.month, from: result) == components.month,
              calendar.component(.day, from: result) == components.day,
              calendar.component(.hour, from: result) == components.hour,
              calendar.component(.minute, from: result) == components.minute,
              calendar.component(.second, from: result) == components.second else { return nil }
        return result
    }
    static func containsNow(_ der: Data, now: Date = Date()) -> Bool {
        let bytes = Array(der); guard bytes.count <= 1_048_576,
              let certificate = element(bytes, at: 0, end: bytes.count), certificate.tag == 0x30,
              let tbs = element(bytes, at: certificate.content.lowerBound, end: certificate.end), tbs.tag == 0x30 else { return false }
        var cursor = tbs.content.lowerBound
        if let version = element(bytes, at: cursor, end: tbs.end), version.tag == 0xa0 { cursor = version.end }
        for tag in [UInt8(0x02), UInt8(0x30), UInt8(0x30)] {
            guard let node = element(bytes, at: cursor, end: tbs.end), node.tag == tag else { return false }; cursor = node.end
        }
        guard let validity = element(bytes, at: cursor, end: tbs.end), validity.tag == 0x30,
              let start = element(bytes, at: validity.content.lowerBound, end: validity.end),
              let finish = element(bytes, at: start.end, end: validity.end), finish.end == validity.end,
              let from = date(bytes, start), let to = date(bytes, finish), from <= to else { return false }
        return now >= from && now <= to
    }
}

/// A bounded exact-read stream used for launcher headers/files and the optional
/// legacy TLS envelope. One read and one send at a time; fragmented and joined
/// TCP deliveries are retained without confusing them with game frames.
final class ZGExactBytePipe {
    let channel: ZGByteChannel
    private let queue: DispatchQueue
    private let stillCurrent: () -> Bool
    private var buffer = Data()
    private var closed = false
    private var receiving = false
    private var readLength: Int?
    private var readCompletion: ((Result<Data, Error>) -> Void)?
    private var timer: DispatchWorkItem?
    private var timerID: UUID?
    private var timedCompletion: ((Error) -> Void)?
    private(set) var didWrite = false
    init(channel: ZGByteChannel, queue: DispatchQueue, stillCurrent: @escaping () -> Bool) {
        self.channel = channel; self.queue = queue; self.stillCurrent = stillCurrent
    }
    private func arm(_ seconds: TimeInterval, failure: @escaping (Error) -> Void) {
        timer?.cancel(); timedCompletion = failure
        let id = UUID(); timerID = id
        let item = DispatchWorkItem { [weak self] in
            guard let self = self, self.timerID == id else { return }
            self.fail(ZGNativeNetworkFailure.network(self.didWrite))
        }
        timer = item; queue.asyncAfter(deadline: .now() + seconds, execute: item)
    }
    private func disarm() { timer?.cancel(); timer = nil; timerID = nil; timedCompletion = nil }
    func start(timeout: TimeInterval, completion: @escaping (Error?) -> Void) {
        guard !closed, stillCurrent() else { completion(ZGNativeNetworkFailure.staleOwner); return }
        arm(timeout) { completion($0) }
        channel.start { [weak self] error in self?.queue.async {
            guard let self = self, !self.closed else { return }
            guard self.stillCurrent() else { self.fail(ZGNativeNetworkFailure.staleOwner); return }
            if let error = error { self.fail(error); return }
            self.disarm(); completion(nil)
        } }
    }
    func send(_ data: Data, timeout: TimeInterval, completion: @escaping (Error?) -> Void) {
        guard !closed, stillCurrent() else { completion(ZGNativeNetworkFailure.staleOwner); return }
        arm(timeout) { completion($0) }; didWrite = true
        channel.send(data) { [weak self] error in self?.queue.async {
            guard let self = self, !self.closed else { return }
            guard self.stillCurrent() else { self.fail(ZGNativeNetworkFailure.staleOwner); return }
            if let error = error { self.fail(error); return }
            self.disarm(); completion(nil)
        } }
    }
    func read(_ count: Int, timeout: TimeInterval, completion: @escaping (Result<Data, Error>) -> Void) {
        guard !closed, stillCurrent(), count >= 0, count <= ZGNetworkLimits.relayResponseBytes, readCompletion == nil else {
            completion(.failure(ZGNativeNetworkFailure.staleConnection)); return
        }
        readLength = count; readCompletion = completion
        arm(timeout) { completion(.failure($0)) }; pump()
    }
    private func pump() {
        guard !closed, let wanted = readLength, let completion = readCompletion else { return }
        guard stillCurrent() else { fail(ZGNativeNetworkFailure.staleOwner); return }
        if buffer.count >= wanted {
            let data = Data(buffer.prefix(wanted)); buffer.removeFirst(wanted)
            readLength = nil; readCompletion = nil; disarm(); completion(.success(data)); return
        }
        guard !receiving else { return }; receiving = true
        channel.receive(maximum: min(65_536, wanted - buffer.count)) { [weak self] data, eof, error in self?.queue.async {
            guard let self = self, !self.closed else { return }; self.receiving = false
            guard self.stillCurrent() else { self.fail(ZGNativeNetworkFailure.staleOwner); return }
            if let error = error { self.fail(error); return }
            guard !data.isEmpty, self.buffer.count <= ZGNetworkLimits.relayResponseBytes - data.count else {
                self.fail(ZGNativeNetworkFailure.network(self.didWrite)); return
            }
            self.buffer.append(data)
            if eof && self.buffer.count < (self.readLength ?? 0) { self.fail(ZGNativeNetworkFailure.network(self.didWrite)); return }
            self.pump()
        } }
    }
    private func fail(_ error: Error) {
        guard !closed else { return }
        let completion = timedCompletion; timedCompletion = nil; timer?.cancel(); timer = nil; timerID = nil
        readCompletion = nil; readLength = nil; closed = true; buffer.removeAll(); channel.cancel()
        completion?(error is ZGNativeNetworkFailure ? error : ZGNativeNetworkFailure.network(didWrite))
    }
    func close() { fail(ZGNativeNetworkFailure.staleOwner) }
}
