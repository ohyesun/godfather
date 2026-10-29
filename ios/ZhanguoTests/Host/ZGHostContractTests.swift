import Foundation
import Security
import WebKit
import XCTest
@testable import Zhanguo

/// Actual Apple Keychain/WKWebView validation. These tests never call a game
/// endpoint and use an isolated, disposable Keychain service for test records.
@MainActor
final class ZGHostContractTests: XCTestCase, WKScriptMessageHandler {
    private var completed: XCTestExpectation?
    private var report: [String: Any]?
    private func call(_ storage: ZGSecureStorage, _ operation: String, _ value: String? = nil) async throws -> Any {
        try await withCheckedThrowingContinuation { continuation in
            storage.request(operation: operation, value: value) { result in continuation.resume(with: result) }
        }
    }
    private func removeTestService(_ service: String) {
        SecItemDelete([kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: service] as CFDictionary)
    }
    func testDocumentURLAllowlist() {
        for allowed in ["zgasset://bundle/index.html", "zgasset://bundle/index.html#scene"] {
            XCTAssertTrue(ZGScriptBridge.permittedDocument(URL(string: allowed)))
        }
        for rejected in ["https://bundle/index.html", "zgasset://other/index.html", "zgasset://bundle/other.html",
                         "zgasset://bundle/index.html?x=1", "zgasset://user@bundle/index.html", "zgasset://bundle:99/index.html"] {
            XCTAssertFalse(ZGScriptBridge.permittedDocument(URL(string: rejected)))
        }
    }
    func testLeaseFilteringAndKeychainPersistence() async throws {
        let service = "cn.zhanguo.test." + UUID().uuidString
        defer { removeTestService(service) }
        let first = ZGSecureStorage(service: service)
        let device = try await call(first, "device") as! String
        let input: [String: Any] = ["account": "fixture-user", "startKey": "fixture-start", "zoneId": 4,
            "characterId": 123, "password": "fixture-never-save", "token": "fixture-never-save", "deviceSecret": "fixture-never-save",
            "activity": ["name": "fixture", "mapId": 59, "password": "fixture-never-save"]]
        let text = String(data: try JSONSerialization.data(withJSONObject: input), encoding: .utf8)!
        let recorded = try await call(first, "record", text) as? Bool
        XCTAssertEqual(recorded, true)
        let second = ZGSecureStorage(service: service)
        let saved = try await call(second, "read") as! String
        let object = try JSONSerialization.jsonObject(with: Data(saved.utf8)) as! [String: Any]
        XCTAssertEqual(object["account"] as? String, "fixture-user")
        for key in ["password", "token", "deviceSecret"] { XCTAssertNil(object[key]) }
        XCTAssertNil((object["activity"] as? [String: Any])?["password"])
        let deviceAgain = try await call(second, "device") as! String
        XCTAssertTrue(device == deviceAgain, "Device identity must persist, without printing capability values")
        let forgotten = try await call(second, "forget") as? Bool
        let afterForget = try await call(first, "read") as? String
        let preservedDevice = try await call(first, "device") as! String
        XCTAssertEqual(forgotten, true)
        XCTAssertEqual(afterForget, "")
        XCTAssertTrue(device == preservedDevice, "Forgetting a lease must preserve device identity")
    }
    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.name == "hostSmoke", message.frameInfo.isMainFrame, report == nil else { return }
        report = message.body as? [String: Any]; completed?.fulfill()
    }
    private final class Page: NSObject, WKURLSchemeHandler {
        let html: Data
        init(_ html: String) { self.html = Data(html.utf8) }
        func webView(_ webView: WKWebView, start urlSchemeTask: WKURLSchemeTask) {
            guard let url = urlSchemeTask.request.url else { return }
            urlSchemeTask.didReceive(URLResponse(url: url, mimeType: "text/html", expectedContentLength: html.count, textEncodingName: "utf-8"))
            urlSchemeTask.didReceive(html); urlSchemeTask.didFinish()
        }
        func webView(_ webView: WKWebView, stop urlSchemeTask: WKURLSchemeTask) {}
    }
    func testActualJavaScriptStorageWireReachesKeychainAndReportBridge() async throws {
        let service = "cn.zhanguo.test.wk." + UUID().uuidString
        defer { removeTestService(service) }
        let storage = ZGSecureStorage(service: service)
        let profile = ZGConnectionProfile(role: "direct-game", host: "127.0.0.1", port: 6020, transport: "direct-tcp")
        let network = try ZGNativeNetworking(profile: profile, launcherAuthentication: Data("fixture-auth".utf8))
        let bridge = ZGScriptBridge(networking: network, storage: storage)
        let source = try String(contentsOf: Bundle.main.resourceURL!.appendingPathComponent("GameOverlay/native-ios.js"), encoding: .utf8)
        let html = #"""
        <!doctype html><meta name=viewport content="width=device-width"><script>
        (async()=>{const r={};try{
          const first=await ZgNativeIOS.storage.device();r.device=typeof JSON.parse(first).deviceId==='string';
          r.record=await ZgNativeIOS.storage.record(JSON.stringify({account:'fixture-user',startKey:'fixture-start',password:'fixture-never-save'}));
          const saved=JSON.parse(await ZgNativeIOS.storage.read());r.read=saved.account==='fixture-user';r.filtered=!('password' in saved);
          r.forget=await ZgNativeIOS.storage.forget();r.empty=(await ZgNativeIOS.storage.read())==='';
          Lab.report(JSON.stringify({fps:61,password:'fixture-do-not-record'}));
        }catch(e){r.error=String(e&&e.message||e);}
          webkit.messageHandlers.hostSmoke.postMessage(r);
        })();</script>
        """#
        let page = Page(html), config = WKWebViewConfiguration(), content = config.userContentController
        content.add(ZGWeakScriptHandler(bridge), name: ZGScriptBridge.channel)
        content.add(self, name: "hostSmoke"); content.addUserScript(try bridge.userScript(source: source))
        config.setURLSchemeHandler(page, forURLScheme: "zgasset")
        let view = WKWebView(frame: CGRect(x: 0, y: 0, width: 640, height: 360), configuration: config)
        bridge.attach(view); completed = expectation(description: "Actual shim body fields survive native Keychain bridge")
        view.load(URLRequest(url: URL(string: "zgasset://bundle/index.html")!))
        await fulfillment(of: [completed!], timeout: 15)
        XCTAssertNil(report?["error"])
        for field in ["device", "record", "read", "filtered", "forget", "empty"] { XCTAssertEqual(report?[field] as? Bool, true, field) }
        XCTAssertEqual(bridge.performanceValues["fps"], 61)
        XCTAssertNil(bridge.performanceValues["password"])
        bridge.invalidate(); view.stopLoading()
        content.removeScriptMessageHandler(forName: ZGScriptBridge.channel); content.removeScriptMessageHandler(forName: "hostSmoke")
    }
}
