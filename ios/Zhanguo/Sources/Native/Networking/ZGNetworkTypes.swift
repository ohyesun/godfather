import Foundation

public struct ZGNetworkEndpoint: Codable, Hashable {
    public let host: String
    public let port: Int
    public init(host: String, port: Int) { self.host = host.lowercased(); self.port = port }
}

/// This is the bundled, native-validated profile, never a page-supplied profile.
public struct ZGConnectionProfile: Decodable {
    public let role: String
    public let host: String
    public let port: Int
    public let transport: String
    public let cert: String?
    public let launcherHost: String?
    public let launcherPort: Int?
    public let allowedGameEndpoints: [ZGNetworkEndpoint]?
    public var isDirect: Bool { role == "direct-game" && transport == "direct-tcp" }
    public init(role: String, host: String, port: Int, transport: String, cert: String? = nil,
                launcherHost: String? = nil, launcherPort: Int? = nil,
                allowedGameEndpoints: [ZGNetworkEndpoint]? = nil) {
        self.role = role; self.host = host; self.port = port; self.transport = transport
        self.cert = cert; self.launcherHost = launcherHost; self.launcherPort = launcherPort
        self.allowedGameEndpoints = allowedGameEndpoints
    }
    public func validate() throws {
        guard Self.validHost(host), Self.validPort(port),
              isDirect || (role == "server" && transport == "zg-tls1" && !(cert ?? "").isEmpty),
              launcherHost.map(Self.validHost) ?? true,
              launcherPort.map(Self.validPort) ?? true,
              (allowedGameEndpoints ?? []).allSatisfy({ Self.validHost($0.host) && Self.validPort($0.port) })
        else { throw ZGNativeNetworkFailure.invalid("连接配置无效") }
    }
    static func validPort(_ value: Int) -> Bool { (1...65535).contains(value) }
    static func validHost(_ value: String) -> Bool {
        !value.isEmpty && value.utf8.count <= 255 &&
        value.rangeOfCharacter(from: .whitespacesAndNewlines) == nil &&
        !value.contains("/") && !value.contains("\\") && !value.contains("@") &&
        !value.contains("?") && !value.contains("#") && !value.contains(":")
    }
    func validateEndpoint(_ text: String) throws {
        guard let url = URLComponents(string: text),
              url.scheme?.lowercased() == (isDirect ? "zg+tcp" : "zg+tls"),
              url.host?.lowercased() == host.lowercased(), url.port == port,
              url.user == nil, url.password == nil, url.query == nil, url.fragment == nil,
              url.path.isEmpty || url.path == "/" else {
            throw ZGNativeNetworkFailure.invalid("连接地址与本地配置不一致")
        }
    }
}

public struct ZGNativeTransportReply: Encodable {
    public let status: Int
    /// Kept as JSON text because the frozen r119 nativeReply parses this field.
    public let body: String
    public let encoding = "identity"
    public let wireBytes: Int
    public let decodedBytes: Int
    public var jsonObject: [String: Any] {
        ["status": status, "body": body, "encoding": encoding,
         "wireBytes": wireBytes, "decodedBytes": decodedBytes]
    }
    public var dictionary: [String: Any] { jsonObject }
    public func toDictionary() -> [String: Any] { jsonObject }
    static func make(status: Int = 200, body: Any, extraWireBytes: Int = 0) throws -> Self {
        let data = try JSONSerialization.data(withJSONObject: body, options: [.fragmentsAllowed, .sortedKeys])
        guard let text = String(data: data, encoding: .utf8) else { throw ZGNativeNetworkFailure.invalid("响应编码无效") }
        return Self(status: status, body: text, wireBytes: data.count + extraWireBytes, decodedBytes: data.count)
    }
    static func relay(status: Int, body: Any) throws -> Self {
        if let text = body as? String {
            return Self(status: status, body: text, wireBytes: text.utf8.count + 18, decodedBytes: text.utf8.count)
        }
        return try make(status: status, body: body, extraWireBytes: 18)
    }
}

/// Messages deliberately exclude credentials, body bytes and socket endpoints.
public struct ZGNativeNetworkFailure: Error, LocalizedError {
    public enum Delivery: String { case notSent = "not_sent", unknown }
    public let code: String
    public let message: String
    public let delivery: Delivery
    public var errorDescription: String? { message }
    public init(code: String, message: String, delivery: Delivery = .notSent) {
        self.code = code; self.message = message; self.delivery = delivery
    }
    public var reply: ZGNativeTransportReply {
        // All values are controlled native strings, never page-provided text.
        try! .make(status: 503, body: ["error": message, "code": code, "delivery": delivery.rawValue])
    }
    static func invalid(_ text: String) -> Self { .init(code: "invalid_native_request", message: text) }
    static var staleOwner: Self { .init(code: "stale_owner", message: "页面或登录会话已变化") }
    static var staleConnection: Self { .init(code: "stale_connection", message: "连接已变化，旧请求未发送") }
    static var busy: Self { .init(code: "native_network_busy", message: "网络请求队列已满，请稍后重试") }
    static func network(_ sent: Bool) -> Self {
        .init(code: sent ? "native_write_unknown" : "native_connection_failed",
              message: sent ? "网络写入结果未确认，请重新连接后核对" : "无法连接区服，请稍后重试",
              delivery: sent ? .unknown : .notSent)
    }
}
public typealias ZGNativeTransportError = ZGNativeNetworkFailure

enum ZGNetworkLimits {
    static let rawBytes = 1_048_576
    static let queuedBytes = 2_097_152
    static let launcherBytes = 2_097_152
    static let relayRequestBytes = 65_536
    static let relayResponseBytes = 4_194_304
    static let directConnectSeconds: TimeInterval = 5
    static let launcherSeconds: TimeInterval = 6
    static let relaySeconds: TimeInterval = 50
    static let pollSeconds: TimeInterval = 0.080
}
