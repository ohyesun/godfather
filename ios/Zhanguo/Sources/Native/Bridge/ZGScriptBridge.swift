import Foundation
import WebKit
import CoreFoundation

/// The page gets the Android-compatible Lab API, while all native work stays
/// asynchronous. A document nonce and a main-frame origin check isolate reloads.
@MainActor
final class ZGScriptBridge: NSObject, WKScriptMessageHandler {
    static let channel = "zgNative"
    let nonce = UUID().uuidString
    private let networking: ZGNativeNetworking
    private let storage: ZGSecureStorage
    private weak var webView: WKWebView?
    private var active = true
    private var pending = Set<String>()
    private(set) var performanceValues: [String: Double] = [:]

    init(networking: ZGNativeNetworking, storage: ZGSecureStorage) {
        self.networking = networking; self.storage = storage
        super.init()
        networking.replaceOwner(nonce)
    }

    func attach(_ view: WKWebView) { webView = view }

    func userScript(source: String) throws -> WKUserScript {
        let marker = "__ZG_IOS_BOOTSTRAP_JSON__"
        guard source.components(separatedBy: marker).count == 2 else { throw ZGConfigurationError.missingBootstrap }
        let bytes = try JSONSerialization.data(withJSONObject: ["v": 1, "nonce": nonce], options: .sortedKeys)
        guard let json = String(data: bytes, encoding: .utf8) else { throw ZGConfigurationError.missingBootstrap }
        return WKUserScript(source: source.replacingOccurrences(of: marker, with: json),
                            injectionTime: .atDocumentStart, forMainFrameOnly: true)
    }

    static func permittedDocument(_ url: URL?) -> Bool {
        guard let url else { return false }
        return url.scheme == "zgasset" && url.host == "bundle" && url.path == "/index.html" &&
            url.user == nil && url.password == nil && url.port == nil && url.query == nil
    }

    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage) {
        guard active, message.name == Self.channel, message.frameInfo.isMainFrame,
              Self.permittedDocument(message.frameInfo.request.url),
              let body = message.body as? [String: Any], body["v"] as? Int == 1,
              body["nonce"] as? String == nonce, let kind = body["kind"] as? String else { return }
        if kind == "report" {
            collectPerformance(body["body"] as? String)
            return
        }
        guard let id = body["id"], let key = requestKey(id: id, kind: kind),
              !pending.contains(key) else { return }
        guard pending.count < 128 else {
            emit(id: id, kind: kind, failure: .init(code: "native_bridge_busy", message: "请求过多，请稍后重试"))
            return
        }
        pending.insert(key)
        if kind == "request95" {
            guard let endpoint = body["endpoint"] as? String, endpoint.utf8.count <= 512,
                  let route = body["route"] as? String, route.utf8.count <= 128,
                  let credential = body["credential"] as? String, credential.utf8.count <= 4096,
                  let account = body["account"] as? String, account.utf8.count <= 1024,
                  let text = body["body"] as? String, text.utf8.count <= 4_194_304 else {
                complete(key, id: id, kind: kind, result: .failure(ZGNativeNetworkFailure.invalid("请求格式无效")))
                return
            }
            networking.request(owner: nonce, endpoint: endpoint, route: route, credential: credential,
                               account: account, body: text) { [weak self] result in
                guard let self else { return }
                self.complete(key, id: id, kind: kind, result: result.map { $0.dictionary as Any })
            }
        } else if kind == "storage", let operation = body["operation"] as? String {
            storage.request(operation: operation, value: body["body"] as? String) { [weak self] result in
                self?.complete(key, id: id, kind: kind, result: result)
            }
        } else {
            complete(key, id: id, kind: kind, result: .failure(ZGNativeNetworkFailure.invalid("未知请求类型")))
        }
    }

    private func requestKey(id: Any, kind: String) -> String? {
        if kind == "request95", let number = id as? NSNumber, CFGetTypeID(number) != CFBooleanGetTypeID(),
           number.doubleValue.rounded() == number.doubleValue, (0...9007199254740991).contains(number.doubleValue) {
            return "request95:" + number.stringValue
        }
        if kind == "storage", let text = id as? String, text.utf8.count <= 64,
           text.range(of: "^storage:[0-9]+$", options: .regularExpression) != nil { return text }
        return nil
    }

    private func complete(_ key: String, id: Any, kind: String, result: Result<Any, Error>) {
        guard active, pending.remove(key) != nil else { return }
        switch result {
        case .success(let value): emit(id: id, kind: kind, value: value)
        case .failure(let error):
            emit(id: id, kind: kind, failure: error as? ZGNativeNetworkFailure ??
                .init(code: "native_operation_failed", message: error.localizedDescription, delivery: .unknown))
        }
    }

    private func emit(id: Any, kind: String, value: Any? = nil, failure: ZGNativeNetworkFailure? = nil) {
        guard active, let webView, Self.permittedDocument(webView.url) else { return }
        var reply: [String: Any] = ["v": 1, "nonce": nonce, "id": id, "kind": kind, "ok": failure == nil]
        if let failure {
            reply["error"] = ["code": failure.code, "message": failure.message, "delivery": failure.delivery.rawValue]
        } else { reply["result"] = value ?? NSNull() }
        guard let data = try? JSONSerialization.data(withJSONObject: reply, options: [.fragmentsAllowed]),
              var json = String(data: data, encoding: .utf8) else { return }
        json = json.replacingOccurrences(of: "\u{2028}", with: "\\u2028").replacingOccurrences(of: "\u{2029}", with: "\\u2029")
        webView.evaluateJavaScript("window.ZgNativeIOS && window.ZgNativeIOS.receive(\(json));", completionHandler: nil)
    }

    private func collectPerformance(_ text: String?) {
        guard let text, text.utf8.count <= 65_536, let data = text.data(using: .utf8),
              let values = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        for key in ["fps", "frames", "drawCalls", "triangles", "objects"] {
            if let number = values[key] as? NSNumber, number.doubleValue.isFinite { performanceValues[key] = number.doubleValue }
        }
    }

    func invalidate() {
        guard active else { return }
        active = false; pending.removeAll(); performanceValues.removeAll()
        networking.cancelAll()
        webView = nil
    }
}

@MainActor
final class ZGWeakScriptHandler: NSObject, WKScriptMessageHandler {
    weak var target: ZGScriptBridge?
    init(_ target: ZGScriptBridge) { self.target = target }
    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage) {
        target?.userContentController(controller, didReceive: message)
    }
}
