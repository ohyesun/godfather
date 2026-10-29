import Foundation
import CoreFoundation

/// Network-only implementation of the frozen r119 Lab.request95 contract.
/// WKWebView ownership/nonce validation and delivery of nativeReply stay in the
/// host. This class never evaluates JavaScript, logs payloads or retries writes.
public final class ZGNativeNetworking {
    public typealias Completion = (Result<ZGNativeTransportReply, Error>) -> Void
    private final class Ticket {
        let id = UUID(); let owner: String; let epoch: UInt64; let completion: Completion
        var done = false
        init(owner: String, epoch: UInt64, completion: @escaping Completion) {
            self.owner = owner; self.epoch = epoch; self.completion = completion
        }
    }
    private final class RawWrite {
        let data: Data; let ticket: Ticket; var started = false
        var timer: DispatchWorkItem?
        init(_ data: Data, _ ticket: Ticket) { self.data = data; self.ticket = ticket }
    }
    private final class RawSession {
        let id = UUID().uuidString; let channel: ZGByteChannel; let connectTicket: Ticket
        var connected = false, reading = false
        var buffer = Data(); var terminal: Error?
        var connectTimer: DispatchWorkItem?, pollTimer: DispatchWorkItem?
        var poll: Ticket?; var writes = [RawWrite](); var activeWrite: RawWrite?; var queuedBytes = 0
        init(channel: ZGByteChannel, ticket: Ticket) { self.channel = channel; self.connectTicket = ticket }
    }
    private struct RelayJob { let ticket: Ticket; let route: String; let payload: [String: Any] }
    private let profile: ZGConnectionProfile
    private let launcherAuthentication: Data
    private let factory: ZGChannelFactory
    private let callbackQueue: DispatchQueue
    private let queue = DispatchQueue(label: "zg.native.network.owner")
    private var owner = "", ownerEpoch: UInt64 = 0
    private var tickets = [UUID: Ticket]()
    private var direct: RawSession?
    private var trustedEndpoints: Set<ZGNetworkEndpoint>
    private var launcherPipes = [UUID: ZGExactBytePipe]()
    private var relayPipe: ZGExactBytePipe?, relayID = UUID(), relayReady = false
    private var relayJobs = [RelayJob](), relayActive: RelayJob?
    private var relaySequence: UInt64 = 0

    public convenience init(profile: ZGConnectionProfile, launcherAuthentication: Data,
                            callbackQueue: DispatchQueue = .main) throws {
        try self.init(profile: profile, launcherAuthentication: launcherAuthentication, callbackQueue: callbackQueue,
                      channelFactory: { try ZGNWByteChannel(endpoint: $0, pinnedCertificate: $1) })
    }
    init(profile: ZGConnectionProfile, launcherAuthentication: Data, callbackQueue: DispatchQueue,
         channelFactory: @escaping ZGChannelFactory) throws {
        try profile.validate()
        if profile.isDirect { _ = try ZGWireCodec.versionPayload(authentication: launcherAuthentication) }
        self.profile = profile; self.launcherAuthentication = launcherAuthentication
        self.callbackQueue = callbackQueue; self.factory = channelFactory
        self.trustedEndpoints = Set((profile.allowedGameEndpoints ?? []) + [ZGNetworkEndpoint(host: profile.host, port: profile.port)])
    }
    /// A document reload gets a fresh opaque owner from the host. This closes
    /// old channels and cancels all operations before accepting the new owner.
    public func replaceOwner(_ value: String) { queue.async { self.replaceOwnerOnQueue(value) } }
    public func cancelAll() { replaceOwner("") }
    private func replaceOwnerOnQueue(_ value: String) {
        ownerEpoch &+= 1; owner = value
        let old = Array(tickets.values)
        closeDirect(ZGNativeNetworkFailure.staleOwner)
        for pipe in Array(launcherPipes.values) { pipe.close() }; launcherPipes.removeAll()
        closeRelay(ZGNativeNetworkFailure.staleOwner)
        trustedEndpoints = Set((profile.allowedGameEndpoints ?? []) + [ZGNetworkEndpoint(host: profile.host, port: profile.port)])
        for ticket in old { finish(ticket, .failure(ZGNativeNetworkFailure.staleOwner)) }
    }
    public func request(owner requestedOwner: String, endpoint: String, route: String,
                        credential: String, account: String, body: String,
                        completion: @escaping Completion) {
        queue.async {
            guard !requestedOwner.isEmpty, requestedOwner == self.owner else {
                self.callbackQueue.async { completion(.failure(ZGNativeNetworkFailure.staleOwner)) }; return
            }
            guard self.tickets.count < 64 else {
                self.callbackQueue.async { completion(.failure(ZGNativeNetworkFailure.busy)) }; return
            }
            let ticket = Ticket(owner: requestedOwner, epoch: self.ownerEpoch, completion: completion)
            self.tickets[ticket.id] = ticket
            do {
                try self.profile.validateEndpoint(endpoint)
                guard body.utf8.count <= ZGNetworkLimits.rawBytes * 2,
                      credential.range(of: "^([a-fA-F0-9]{64})?$", options: .regularExpression) != nil,
                      account.range(of: "^[A-Za-z0-9_]{0,31}$", options: .regularExpression) != nil,
                      let bytes = body.data(using: .utf8),
                      let object = try JSONSerialization.jsonObject(with: bytes) as? [String: Any] else {
                    throw ZGNativeNetworkFailure.invalid("原生请求格式无效")
                }
                if self.profile.isDirect { try self.directRequest(route, input: object, ticket: ticket) }
                else {
                    guard body.utf8.count <= 16_384, Self.relayRoutes.contains(route) else {
                        throw ZGNativeNetworkFailure.invalid("网关请求路径或长度无效")
                    }
                    guard self.relayJobs.count < 8 else { throw ZGNativeNetworkFailure.busy }
                    self.relayJobs.append(RelayJob(ticket: ticket, route: route,
                        payload: ["v": 1, "route": route, "credential": credential, "account": account, "body": object]))
                    self.pumpRelay()
                }
            } catch { self.finish(ticket, .failure(error)) }
        }
    }
    private func current(_ ticket: Ticket) -> Bool {
        !ticket.done && !owner.isEmpty && ticket.owner == owner && ticket.epoch == ownerEpoch && tickets[ticket.id] === ticket
    }
    private func finish(_ ticket: Ticket, _ result: Result<ZGNativeTransportReply, Error>) {
        guard !ticket.done else { return }; ticket.done = true; tickets.removeValue(forKey: ticket.id)
        callbackQueue.async { ticket.completion(result) }
    }
    private func success(_ ticket: Ticket, _ body: [String: Any]) {
        do { finish(ticket, .success(try .make(body: body))) } catch { finish(ticket, .failure(error)) }
    }
    private func unsent(_ error: Error) -> Error {
        if let native = error as? ZGNativeNetworkFailure, native.delivery == .unknown {
            return ZGNativeNetworkFailure(code: "native_connection_failed", message: "连接已中断，排队请求未发送")
        }
        return error
    }
    private func directRequest(_ route: String, input: [String: Any], ticket: Ticket) throws {
        switch route {
        case "/direct/zones": try launcher(input, register: false, ticket: ticket)
        case "/direct/register": try launcher(input, register: true, ticket: ticket)
        case "/direct/open":
            let host = input["host"] as? String ?? profile.host
            let port = try Self.jsonPort(input["port"] ?? profile.port)
            guard let ip = Self.ipv4(host), trustedEndpoints.contains(ZGNetworkEndpoint(host: ip, port: port)) else {
                throw ZGNativeNetworkFailure.invalid("区服地址不在本地配置或原生区服列表中")
            }
            closeDirect(ZGNativeNetworkFailure.staleConnection)
            let channel = try factory(ZGNetworkEndpoint(host: ip, port: port), nil)
            let session = RawSession(channel: channel, ticket: ticket); direct = session
            let timer = DispatchWorkItem { [weak self, weak session] in
                guard let self = self, let session = session, self.direct === session, !session.connected else { return }
                self.closeDirect(ZGNativeNetworkFailure.network(false))
            }
            session.connectTimer = timer; queue.asyncAfter(deadline: .now() + ZGNetworkLimits.directConnectSeconds, execute: timer)
            channel.start { [weak self, weak session] error in self?.queue.async {
                guard let self = self, let session = session, self.direct === session else { return }
                guard self.current(ticket) else { self.closeDirect(ZGNativeNetworkFailure.staleOwner); return }
                if error != nil { self.closeDirect(ZGNativeNetworkFailure.network(false)); return }
                session.connectTimer?.cancel(); session.connectTimer = nil; session.connected = true
                self.success(ticket, ["connected": true, "connectionId": session.id, "host": ip, "port": port])
                self.receiveRaw(session)
            } }
        case "/direct/send", "/direct/receive", "/direct/close":
            guard let id = input["connectionId"] as? String, !id.isEmpty,
                  let session = direct, session.id == id, session.connected,
                  session.connectTicket.owner == ticket.owner, session.connectTicket.epoch == ticket.epoch else {
                throw ZGNativeNetworkFailure.staleConnection
            }
            if route == "/direct/close" {
                closeDirect(ZGNativeNetworkFailure.staleConnection)
                success(ticket, ["closed": true, "connectionId": id])
            } else if route == "/direct/send" {
                guard session.terminal == nil else { throw ZGNativeNetworkFailure.network(false) }
                guard let hex = input["packetHex"] as? String else { throw ZGNativeNetworkFailure.invalid("发送数据缺失") }
                let data = try ZGWireCodec.decodeHex(hex)
                guard session.writes.count < 64, session.queuedBytes <= ZGNetworkLimits.queuedBytes - data.count else {
                    throw ZGNativeNetworkFailure.busy
                }
                session.writes.append(RawWrite(data, ticket)); session.queuedBytes += data.count; pumpWrites(session)
            } else {
                guard session.poll == nil else { throw ZGNativeNetworkFailure.busy }
                session.poll = ticket
                if !session.buffer.isEmpty || session.terminal != nil { drainRaw(session) }
                else {
                    let timer = DispatchWorkItem { [weak self, weak session, weak ticket] in
                        guard let self = self, let session = session, let ticket = ticket,
                              self.direct === session, session.poll === ticket else { return }
                        self.drainRaw(session)
                    }
                    session.pollTimer = timer; queue.asyncAfter(deadline: .now() + ZGNetworkLimits.pollSeconds, execute: timer)
                    receiveRaw(session)
                }
            }
        default: throw ZGNativeNetworkFailure.invalid("原生请求路径无效")
        }
    }
    private func closeDirect(_ error: Error) {
        guard let session = direct else { return }; direct = nil
        session.connectTimer?.cancel(); session.pollTimer?.cancel(); session.channel.cancel()
        finish(session.connectTicket, .failure(error))
        if let poll = session.poll { finish(poll, .failure(error)) }; session.poll = nil
        if let active = session.activeWrite {
            active.timer?.cancel()
            finish(active.ticket, .failure(active.started ? ZGNativeNetworkFailure.network(true) : error))
        }
        for write in session.writes { finish(write.ticket, .failure(unsent(error))) }
        session.writes.removeAll(); session.buffer.removeAll()
    }
    private func pumpWrites(_ session: RawSession) {
        guard direct === session, session.activeWrite == nil, !session.writes.isEmpty else { return }
        let write = session.writes.removeFirst()
        guard current(write.ticket), session.terminal == nil else {
            session.queuedBytes -= write.data.count; finish(write.ticket, .failure(ZGNativeNetworkFailure.staleConnection)); pumpWrites(session); return
        }
        session.activeWrite = write; write.started = true
        let timeout = DispatchWorkItem { [weak self, weak session] in
            guard let self = self, let session = session, self.direct === session,
                  session.activeWrite === write else { return }
            self.closeDirect(ZGNativeNetworkFailure.network(true))
        }
        write.timer = timeout; queue.asyncAfter(deadline: .now() + ZGNetworkLimits.relaySeconds, execute: timeout)
        // Last owner/connection check occurs immediately before native send.
        session.channel.send(write.data) { [weak self, weak session] error in self?.queue.async {
            guard let self = self, let session = session, self.direct === session, session.activeWrite === write else { return }
            write.timer?.cancel(); write.timer = nil
            session.activeWrite = nil; session.queuedBytes -= write.data.count
            guard self.current(write.ticket) else { self.finish(write.ticket, .failure(ZGNativeNetworkFailure.staleOwner)); self.pumpWrites(session); return }
            if error != nil { self.finish(write.ticket, .failure(ZGNativeNetworkFailure.network(true))); self.closeDirect(ZGNativeNetworkFailure.network(true)); return }
            self.success(write.ticket, ["dataHex": "", "connectionId": session.id]); self.pumpWrites(session)
        } }
    }
    private func receiveRaw(_ session: RawSession) {
        guard direct === session, session.connected, !session.reading, session.terminal == nil,
              session.buffer.count < ZGNetworkLimits.rawBytes else { return }
        session.reading = true
        session.channel.receive(maximum: min(65_536, ZGNetworkLimits.rawBytes - session.buffer.count)) { [weak self, weak session] data, eof, error in self?.queue.async {
            guard let self = self, let session = session, self.direct === session else { return }; session.reading = false
            guard session.buffer.count <= ZGNetworkLimits.rawBytes - data.count else { self.closeDirect(ZGNativeNetworkFailure.network(false)); return }
            session.buffer.append(data)
            if error != nil || eof || data.isEmpty { session.terminal = ZGNativeNetworkFailure.network(false) }
            if session.poll != nil && (!session.buffer.isEmpty || session.terminal != nil) { self.drainRaw(session) }
            self.receiveRaw(session)
        } }
    }
    private func drainRaw(_ session: RawSession) {
        guard direct === session, let poll = session.poll else { return }
        session.poll = nil; session.pollTimer?.cancel(); session.pollTimer = nil
        guard current(poll) else { finish(poll, .failure(ZGNativeNetworkFailure.staleOwner)); return }
        if session.buffer.isEmpty, let error = session.terminal { finish(poll, .failure(error)); closeDirect(error); return }
        let bytes = session.buffer; session.buffer.removeAll(keepingCapacity: true)
        success(poll, ["dataHex": ZGWireCodec.hex(bytes), "connectionId": session.id]); receiveRaw(session)
    }
    private func launcher(_ input: [String: Any], register: Bool, ticket: Ticket) throws {
        guard launcherPipes.count < 4 else { throw ZGNativeNetworkFailure.busy }
        let registration: Data
        if register { registration = try ZGWireCodec.registerPayload(input) } else { registration = Data() }
        let version = try ZGWireCodec.launcherRequest(command: 9, payload: ZGWireCodec.versionPayload(authentication: launcherAuthentication))
        let channel = try factory(ZGNetworkEndpoint(host: profile.launcherHost ?? profile.host, port: profile.launcherPort ?? 9999), nil)
        let pipe = ZGExactBytePipe(channel: channel, queue: queue, stillCurrent: { [weak self, weak ticket] in
            guard let self = self, let ticket = ticket else { return false }; return self.current(ticket)
        }); launcherPipes[ticket.id] = pipe
        func fail(_ error: Error) { self.finish(ticket, .failure(error)); self.launcherPipes.removeValue(forKey: ticket.id); pipe.close() }
        func readReply(_ completion: @escaping (ZGWireCodec.LauncherHeader, Data) -> Void) {
            pipe.read(20, timeout: ZGNetworkLimits.launcherSeconds) { result in
                do {
                    let header = try ZGWireCodec.launcherHeader(result.get())
                    let count = register ? (header.payloadBytes > 0 ? header.payloadBytes : header.fileBytes) : header.fileBytes
                    pipe.read(count, timeout: ZGNetworkLimits.launcherSeconds) { result in
                        do { completion(header, try result.get()) } catch { fail(error) }
                    }
                } catch { fail(error) }
            }
        }
        pipe.start(timeout: ZGNetworkLimits.launcherSeconds) { error in
            if let error = error { fail(error); return }
            pipe.send(version, timeout: ZGNetworkLimits.launcherSeconds) { error in
                if let error = error { fail(error); return }
                readReply { versionHeader, _ in
                    guard versionHeader.status == 0 else { fail(ZGNativeNetworkFailure.invalid("登录器版本校验失败")); return }
                    do {
                        let request = try ZGWireCodec.launcherRequest(command: register ? 8 : 2,
                            payload: register ? registration : ZGWireCodec.filePayload())
                        pipe.send(request, timeout: ZGNetworkLimits.launcherSeconds) { error in
                            if let error = error { fail(error); return }
                            readReply { header, data in
                                do {
                                    let text = try ZGWireCodec.decodeLauncherText(data)
                                    if register { self.success(ticket, ["ok": header.status == 0, "status": header.status,
                                        "message": text.trimmingCharacters(in: .whitespacesAndNewlines)]) }
                                    else {
                                        guard header.status == 0 else { throw ZGNativeNetworkFailure.invalid("区服列表请求被拒绝") }
                                        self.trustedEndpoints.formUnion(Self.catalogueEndpoints(text)); self.success(ticket, ["configText": text])
                                    }
                                    self.launcherPipes.removeValue(forKey: ticket.id); pipe.close()
                                } catch { fail(error) }
                            }
                        }
                    } catch { fail(error) }
                }
            }
        }
    }
    private static let relayRoutes: Set<String> = Set(["/player/login", "/player/register"] +
        ["open", "state", "action", "step", "logout", "catalogue", "systems", "party", "guild", "auction", "social", "ladder", "stalls", "chat", "presence", "offline"].map { "/mobile/" + $0 })
    private func pumpRelay() {
        guard relayActive == nil, !relayJobs.isEmpty else { return }
        let job = relayJobs.removeFirst()
        guard current(job.ticket) else { finish(job.ticket, .failure(ZGNativeNetworkFailure.staleOwner)); pumpRelay(); return }
        relayActive = job
        do {
            if relayPipe == nil {
                guard let pem = profile.cert else { throw ZGNativeNetworkFailure.invalid("固定网关证书缺失") }
                let encoded = pem.components(separatedBy: .newlines).filter { !$0.hasPrefix("-----") }.joined()
                guard let der = Data(base64Encoded: encoded, options: .ignoreUnknownCharacters), !der.isEmpty else { throw ZGNativeNetworkFailure.invalid("固定网关证书无效") }
                let id = UUID(); relayID = id
                let relayOwner = job.ticket.owner, relayEpoch = job.ticket.epoch
                let channel = try factory(ZGNetworkEndpoint(host: profile.host, port: profile.port), der)
                relayPipe = ZGExactBytePipe(channel: channel, queue: queue, stillCurrent: { [weak self] in
                    guard let self = self else { return false }; return self.relayID == id && self.owner == relayOwner && self.ownerEpoch == relayEpoch
                }); relayReady = false
            }
            guard let pipe = relayPipe else { throw ZGNativeNetworkFailure.network(false) }
            if relayReady { sendRelay(job, pipe: pipe) }
            else { pipe.start(timeout: ZGNetworkLimits.directConnectSeconds) { error in
                if let error = error { self.closeRelay(error); return }; self.relayReady = true; self.sendRelay(job, pipe: pipe)
            } }
        } catch { closeRelay(error) }
    }
    private func sendRelay(_ job: RelayJob, pipe: ZGExactBytePipe) {
        guard current(job.ticket), relayPipe === pipe, relayActive?.ticket === job.ticket else { closeRelay(ZGNativeNetworkFailure.staleOwner); return }
        do {
            relaySequence = relaySequence == UInt64.max ? 1 : relaySequence + 1
            let id = relaySequence, payload = try JSONSerialization.data(withJSONObject: job.payload, options: [.sortedKeys])
            let frame = try ZGWireCodec.relayFrame(id: id, payload: payload)
            pipe.send(frame, timeout: ZGNetworkLimits.relaySeconds) { error in
                if let error = error { self.closeRelay(error); return }
                pipe.read(18, timeout: ZGNetworkLimits.relaySeconds) { result in
                    do {
                        let size = try ZGWireCodec.relayLength(result.get(), id: id)
                        pipe.read(size, timeout: ZGNetworkLimits.relaySeconds) { result in
                            do {
                                let bytes = try result.get()
                                guard let text = String(data: bytes, encoding: .utf8), let json = text.data(using: .utf8),
                                      let object = try JSONSerialization.jsonObject(with: json) as? [String: Any],
                                      ((object["v"] as? NSNumber)?.intValue ?? 1) == 1,
                                      ((object["id"] as? NSNumber)?.uint64Value ?? id) == id else { throw ZGNativeNetworkFailure.invalid("网关响应格式或编号无效") }
                                let reply = try ZGNativeTransportReply.relay(status: (object["status"] as? NSNumber)?.intValue ?? 500, body: object["body"] ?? [:])
                                self.relayActive = nil; self.finish(job.ticket, .success(reply))
                                if job.route == "/mobile/logout" { self.closeRelay(ZGNativeNetworkFailure.staleOwner) }
                                else { self.pumpRelay() }
                            } catch { self.closeRelay(error) }
                        }
                    } catch { self.closeRelay(error) }
                }
            }
        } catch { closeRelay(error) }
    }
    private func closeRelay(_ error: Error) {
        let pipe = relayPipe; relayPipe = nil; relayReady = false; relayID = UUID()
        let active = relayActive; relayActive = nil; let queued = relayJobs; relayJobs.removeAll()
        if let active = active { finish(active.ticket, .failure(error)) }
        for job in queued { finish(job.ticket, .failure(unsent(error))) }; pipe?.close()
    }
    private static func jsonPort(_ value: Any) throws -> Int {
        guard let number = value as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
              number.doubleValue.rounded() == number.doubleValue, ZGConnectionProfile.validPort(number.intValue) else {
            throw ZGNativeNetworkFailure.invalid("区服端口无效")
        }
        return number.intValue
    }
    private static func ipv4(_ host: String) -> String? {
        let components = host.split(separator: ".", omittingEmptySubsequences: false)
        guard components.count == 4 else { return nil }; var output = [String]()
        for part in components {
            guard !part.isEmpty, part.count <= 3, part.allSatisfy({ $0 >= "0" && $0 <= "9" }),
                  let value = Int(part), value <= 255 else { return nil }; output.append(String(value))
        }
        return output.joined(separator: ".")
    }
    private static func catalogueEndpoints(_ text: String) -> Set<ZGNetworkEndpoint> {
        var server = false, output = Set<ZGNetworkEndpoint>()
        for raw in text.components(separatedBy: .newlines) {
            let line = raw.trimmingCharacters(in: .whitespacesAndNewlines)
            if line.isEmpty || line.hasPrefix(";") { continue }
            if line.hasPrefix("[") && line.hasSuffix("]") { server = line.dropFirst().dropLast().trimmingCharacters(in: .whitespaces).lowercased() == "server"; continue }
            guard server, let equal = line.firstIndex(of: "=") else { continue }
            let value = line[line.index(after: equal)...].trimmingCharacters(in: CharacterSet(charactersIn: " \t\"'`"))
            let fields = value.components(separatedBy: "|").map { $0.trimmingCharacters(in: .whitespaces) }
            guard fields.count >= 7, !fields[1].isEmpty, !fields[2].isEmpty, let host = ipv4(fields[3]),
                  let port = Int(fields[4]), ZGConnectionProfile.validPort(port), Int(fields[5]) != nil else { continue }
            output.insert(ZGNetworkEndpoint(host: host, port: port))
            if fields.count > 7, let port = Int(fields[7]), ZGConnectionProfile.validPort(port) { output.insert(ZGNetworkEndpoint(host: host, port: port)) }
            if fields.count > 10, let host = ipv4(fields[9]), let port = Int(fields[10]), ZGConnectionProfile.validPort(port) { output.insert(ZGNetworkEndpoint(host: host, port: port)) }
        }
        return output
    }
}
