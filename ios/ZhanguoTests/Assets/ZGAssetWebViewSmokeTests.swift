import CryptoKit
import Foundation
import WebKit
import XCTest
@testable import Zhanguo

/// Real Apple-WebKit acceptance, intentionally not replaced by a Chrome/node
/// claim on Windows. No account, server, game data or network API is involved.
@MainActor
final class ZGAssetWebViewSmokeTests: XCTestCase, WKScriptMessageHandler, WKNavigationDelegate {
    private var completed: XCTestExpectation?
    private var report: [String: Any]?
    private var lastStage = "NAVIGATION"
    private var rejectedMessage = false
    private let stages: Set<String> = ["NAVIGATION", "BOOTSTRAP", "MODULE", "ESM", "FETCH", "FETCH_BODY",
        "HEAD", "HEAD_BODY", "RANGE", "RANGE_BODY", "IMAGE", "WEBGL", "AUDIO_FETCH", "AUDIO_BODY",
        "AUDIO_DECODE", "AUDIO_CLOSE", "WASM", "DONE"]
    private let failures: Set<String> = ["NONE", "DEADLINE", "MODULE_LOAD", "SCRIPT", "REJECTION", "JAVASCRIPT",
        "PROVISIONAL_NAVIGATION", "NAVIGATION", "WEB_PROCESS"]

    private func finish(_ value: [String: Any]) {
        guard report == nil else { return }
        report = value
        completed?.fulfill()
    }

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.name == "assetSmoke", report == nil else { return }
        guard message.frameInfo.isMainFrame, message.frameInfo.request.url?.scheme == "zgasset",
              let value = message.body as? [String: Any], let stage = value["stage"] as? String,
              stages.contains(stage) else { rejectedMessage = true; return }
        lastStage = stage
        if value["kind"] as? String == "progress" { return }
        guard value["kind"] as? String == "complete", let code = value["code"] as? String,
              failures.contains(code) else { rejectedMessage = true; return }
        finish(value)
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        finish(["code": "PROVISIONAL_NAVIGATION", "stage": lastStage])
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        finish(["code": "NAVIGATION", "stage": lastStage])
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        finish(["code": "WEB_PROCESS", "stage": lastStage])
    }

    func testSameOriginESMFetchRangesImageWebGLAudioAndWasm() async throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("zg-wk-assets-" + UUID().uuidString, isDirectory: true)
        defer { try? FileManager.default.removeItem(at: root) }
        let files = root.appendingPathComponent("files", isDirectory: true)
        try FileManager.default.createDirectory(at: files, withIntermediateDirectories: true)
        let html = #"""
        <!doctype html><meta name=viewport content="width=device-width"><canvas id=c width=2 height=2></canvas>
        <script>
        (()=>{
          const stages=new Set(['BOOTSTRAP','MODULE','ESM','FETCH','FETCH_BODY','HEAD','HEAD_BODY','RANGE','RANGE_BODY',
            'IMAGE','WEBGL','AUDIO_FETCH','AUDIO_BODY','AUDIO_DECODE','AUDIO_CLOSE','WASM','DONE']);
          const codes=new Set(['NONE','DEADLINE','MODULE_LOAD','SCRIPT','REJECTION','JAVASCRIPT']);
          let stage='BOOTSTRAP',finished=false;
          const post=value=>window.webkit.messageHandlers.assetSmoke.postMessage(value);
          const step=value=>{if(!finished&&stages.has(value)){stage=value;post({kind:'progress',stage});}};
          const finish=(code,values={})=>{if(finished||!codes.has(code))return;finished=true;
            post({...values,kind:'complete',stage,code});};
          window.__zgAssetSmoke={step,complete:values=>{step('DONE');finish('NONE',values);},fail:code=>finish(code)};
          addEventListener('error',event=>finish(event.target&&event.target.id==='assetSmokeModule'?'MODULE_LOAD':'SCRIPT'),true);
          addEventListener('unhandledrejection',()=>finish('REJECTION'));
          // A pending browser API must produce a fixed failing stage, never a
          // false success or an opaque outer XCTest timeout. No raw error text.
          setTimeout(()=>finish('DEADLINE'),12000);step('BOOTSTRAP');
        })();
        </script>
        <script type=importmap>{"imports":{"probe":"./module.js"}}</script>
        <script id=assetSmokeModule type=module>
        const r={},probe=window.__zgAssetSmoke;probe.step('MODULE');try{
          probe.step('ESM');
          const module=await import('probe');r.esm=module.answer===42;
          probe.step('FETCH');const q=await fetch('data.bin?v=119');probe.step('FETCH_BODY');r.fetch=(await q.text())==='abcdef';
          probe.step('HEAD');const h=await fetch('data.bin',{method:'HEAD'});probe.step('HEAD_BODY');
          r.head=h.status===200&&h.headers.get('Content-Length')==='6'&&(await h.arrayBuffer()).byteLength===0;
          probe.step('RANGE');const p=await fetch('data.bin',{headers:{Range:'bytes=1-3'}});probe.step('RANGE_BODY');r.range=p.status===206&&(await p.text())==='bcd';
          probe.step('IMAGE');
          const image=await new Promise((resolve,reject)=>{const i=new Image();i.onload=()=>resolve(i);i.onerror=reject;i.src='pixel.png';});
          r.image=image.width===1&&image.height===1;
          probe.step('WEBGL');
          const gl=document.querySelector('#c').getContext('webgl2');r.webgl=!!gl;
          if(gl){gl.bindTexture(gl.TEXTURE_2D,gl.createTexture());gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,gl.RGBA,gl.UNSIGNED_BYTE,image);r.webglTexture=gl.getError()===gl.NO_ERROR;}
          const audio=new AudioContext();probe.step('AUDIO_FETCH');const sound=await fetch('tone.wav');
          probe.step('AUDIO_BODY');const bytes=await sound.arrayBuffer();probe.step('AUDIO_DECODE');const buffer=await audio.decodeAudioData(bytes);
          r.audio=buffer.length>0;probe.step('AUDIO_CLOSE');await audio.close();
          probe.step('WASM');
          await WebAssembly.instantiateStreaming(fetch('probe.wasm'));r.wasm=true;
          probe.complete(r);
        }catch{probe.fail('JAVASCRIPT');}
        </script>
        """#
        var wave = Data(count: 44 + 8820)
        func ascii(_ value: String, at offset: Int) { wave.replaceSubrange(offset..<(offset + value.utf8.count), with: Data(value.utf8)) }
        func number<T: FixedWidthInteger>(_ value: T, at offset: Int) {
            var n = value.littleEndian
            withUnsafeBytes(of: &n) { wave.replaceSubrange(offset..<(offset + $0.count), with: $0) }
        }
        ascii("RIFF", at: 0); number(UInt32(wave.count - 8), at: 4); ascii("WAVEfmt ", at: 8)
        number(UInt32(16), at: 16); number(UInt16(1), at: 20); number(UInt16(1), at: 22)
        number(UInt32(44100), at: 24); number(UInt32(88200), at: 28); number(UInt16(2), at: 32)
        number(UInt16(16), at: 34); ascii("data", at: 36); number(UInt32(8820), at: 40)
        let payloads: [String: Data] = [
            "index.html": Data(html.utf8), "module.js": Data("export const answer=42;".utf8), "data.bin": Data("abcdef".utf8),
            "pixel.png": Data(base64Encoded: "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=")!,
            "probe.wasm": Data([0,97,115,109,1,0,0,0]), "tone.wav": wave, "resources107.pak": Data()
        ]
        var direct: [String: Any] = [:]
        for (path, data) in payloads {
            try data.write(to: files.appendingPathComponent(path))
            direct[path] = [data.count, SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()]
        }
        let manifest: [String: Any] = ["schemaVersion": 1, "source": ["apkSHA256": ZGAssetStore.sourceAPKHash],
                                      "direct": direct, "packed": [String: Any]()]
        try JSONSerialization.data(withJSONObject: manifest).write(to: root.appendingPathComponent("assets-manifest.json"))
        let scheme = try ZGAssetSchemeHandler(rootURL: root), configuration = WKWebViewConfiguration()
        configuration.setURLSchemeHandler(scheme, forURLScheme: "zgasset")
        configuration.userContentController.add(self, name: "assetSmoke")
        let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 300), configuration: configuration)
        webView.navigationDelegate = self
        defer {
            webView.navigationDelegate = nil
            webView.stopLoading(); scheme.cancelAll()
            configuration.userContentController.removeScriptMessageHandler(forName: "assetSmoke")
        }
        completed = expectation(description: "native custom-scheme resources execute in actual WKWebView")
        webView.load(URLRequest(url: ZGAssetSchemeHandler.entryURL))
        await fulfillment(of: [completed!], timeout: 15)
        guard let report else {
            XCTFail("ZGSMOKE_NO_REPORT_" + lastStage + (rejectedMessage ? "_REJECTED_MESSAGE" : ""))
            return
        }
        let code = report["code"] as? String ?? "SCRIPT"
        if code != "NONE" {
            XCTFail("ZGSMOKE_" + code + "_" + lastStage)
            return
        }
        XCTAssertNil(report["error"])
        let capabilities = ["esm", "fetch", "head", "range", "image", "webgl", "webglTexture", "audio", "wasm"]
        for key in capabilities {
            XCTAssertEqual(report[key] as? Bool, true, "ZGSMOKE_CAPABILITY_" + key.uppercased())
        }
        let passed = capabilities.filter { report[$0] as? Bool == true }.count
        print("ZGSMOKE_ASSET_COUNTS PASS=\(passed) FAIL=\(capabilities.count - passed)")
        if report["error"] == nil && passed == capabilities.count {
            print("ZGSMOKE_ASSET_PASS")
        }
    }
}
