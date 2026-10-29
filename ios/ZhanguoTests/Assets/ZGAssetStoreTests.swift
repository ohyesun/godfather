import CryptoKit
import Foundation
import WebKit
import XCTest
import zlib
@testable import Zhanguo

/// Requires the Apple SDK/XCTest runner. These are not marked as Windows
/// passes: they exercise the actual Swift resolver, inflater and WK task API.
final class ZGAssetStoreTests: XCTestCase {
    private struct Fixture {
        let root: URL
        let model: Data
        let client: Data
        func cleanup() { try? FileManager.default.removeItem(at: root) }
    }
    private func sha(_ bytes: Data) -> String { SHA256.hash(data: bytes).map { String(format: "%02x", $0) }.joined() }
    private func gzip(_ input: Data) throws -> Data {
        var state = z_stream()
        guard deflateInit2_(&state, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY,
            ZLIB_VERSION, Int32(MemoryLayout<z_stream>.size)) == Z_OK else { throw ZGAssetError.invalidManifest }
        defer { deflateEnd(&state) }
        var output = [UInt8](repeating: 0, count: Int(compressBound(uLong(input.count))) + 32)
        var status: Int32 = Z_OK
        input.withUnsafeBytes { source in
            output.withUnsafeMutableBytes { target in
                state.next_in = UnsafeMutablePointer(mutating: source.bindMemory(to: Bytef.self).baseAddress!)
                state.avail_in = uInt(input.count)
                state.next_out = target.bindMemory(to: Bytef.self).baseAddress!
                state.avail_out = uInt(target.count)
                status = deflate(&state, Z_FINISH)
            }
        }
        guard status == Z_STREAM_END else { throw ZGAssetError.corruptCompressedData(status) }
        return Data(output.prefix(Int(state.total_out)))
    }
    private func fixture() throws -> Fixture {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("zg-ios-asset-test-" + UUID().uuidString, isDirectory: true)
        let files = root.appendingPathComponent("files", isDirectory: true)
        try FileManager.default.createDirectory(at: files, withIntermediateDirectories: true)
        var model = Data(repeating: 65, count: 2 * 1024 * 1024)
        model.replaceSubrange(0..<4, with: Data("glTF".utf8))
        let oldClient = Data("old packed client".utf8), oldStored = try gzip(oldClient), stored = try gzip(model)
        var pack = oldStored; pack.append(stored)
        let client = Data("export const source = 'direct-r119';".utf8), html = Data("<!doctype html>".utf8)
        try pack.write(to: files.appendingPathComponent("resources107.pak"))
        try client.write(to: files.appendingPathComponent("client.js"))
        try html.write(to: files.appendingPathComponent("index.html"))
        let manifest: [String: Any] = ["schemaVersion": 1,
            "source": ["apkSHA256": ZGAssetStore.sourceAPKHash],
            "direct": ["resources107.pak": [pack.count, sha(pack)], "client.js": [client.count, sha(client)], "index.html": [html.count, sha(html)]],
            "packed": ["client.js": [0, oldStored.count, oldClient.count, 1, sha(oldClient)],
                       "native107/fixture.glb": [oldStored.count, stored.count, model.count, 1, sha(model)]]]
        try JSONSerialization.data(withJSONObject: manifest, options: [.sortedKeys]).write(to: root.appendingPathComponent("assets-manifest.json"))
        return Fixture(root: root, model: model, client: client)
    }
    private func request(_ path: String, method: String = "GET", range: String? = nil) -> URLRequest {
        var request = URLRequest(url: URL(string: "zgasset://bundle/" + path)!)
        request.httpMethod = method
        if let range { request.setValue(range, forHTTPHeaderField: "Range") }
        return request
    }
    private func bytes(_ plan: ZGAssetResponsePlan) throws -> Data {
        if plan.head { return Data() }
        if let body = plan.smallBody { return body }
        guard let source = plan.source else { return Data() }
        var result = Data()
        try ZGAssetReader.stream(source, range: plan.range, cancelled: { false }, emit: { result.append($0); return true })
        return result
    }

    func testOriginalDirectPrecedenceAndDecodedModelMIME() throws {
        let f = try fixture(); defer { f.cleanup() }
        let store = try ZGAssetStore(rootURL: f.root)
        XCTAssertEqual(try bytes(store.response(for: request("client.js?v=r119"))), f.client)
        let model = try store.response(for: request("native107/fixture.glb?cache=anything"))
        XCTAssertEqual(model.status, 200)
        XCTAssertEqual(model.headers["Content-Type"], "model/gltf-binary")
        XCTAssertNil(model.headers["Content-Encoding"])
        XCTAssertEqual(model.headers["Content-Length"], String(f.model.count))
        XCTAssertEqual(try bytes(model), f.model)
        XCTAssertEqual(ZGAssetStore.mimeType(path: "module.wasm"), "application/wasm")
        XCTAssertEqual(ZGAssetStore.mimeType(path: "terrain.webp"), "image/webp")
        XCTAssertEqual(ZGAssetStore.mimeType(path: "music.mp3"), "audio/mpeg")
    }

    func testRangesAddressDecodedBytesAndHEADDoesNotRead() throws {
        let f = try fixture(); defer { f.cleanup() }
        let store = try ZGAssetStore(rootURL: f.root), size = f.model.count
        for (range, expected) in [("bytes=0-3", Data("glTF".utf8)), ("bytes=-3", Data("AAA".utf8)),
                                   ("bytes=\(size - 2)-", Data("AA".utf8))] {
            let plan = try store.response(for: request("native107/fixture.glb", range: range))
            XCTAssertEqual(plan.status, 206); XCTAssertEqual(try bytes(plan), expected)
        }
        let head = try store.response(for: request("native107/fixture.glb", method: "HEAD", range: "bytes=0-3"))
        XCTAssertEqual(head.status, 200); XCTAssertEqual(head.headers["Content-Length"], String(size)); XCTAssertTrue(try bytes(head).isEmpty)
        for range in ["bytes=0-1,4-8", "bytes=-0", "bytes=9-2", "bytes=\(size)-", "bytes=999999999999999999999999-"] {
            let plan = try store.response(for: request("native107/fixture.glb", range: range))
            XCTAssertEqual(plan.status, 416); XCTAssertEqual(plan.headers["Content-Range"], "bytes */\(size)")
        }
        let missing = try store.response(for: request("unknown.png")); XCTAssertEqual(missing.status, 404)
        XCTAssertEqual(try store.response(for: request("client.js", method: "POST")).status, 405)
        XCTAssertEqual(try store.response(for: request("client.js", method: "OPTIONS")).status, 204)
    }

    func testStreamingBoundAndCancellation() throws {
        let f = try fixture(); defer { f.cleanup() }
        let source = try XCTUnwrap(ZGAssetStore(rootURL: f.root).response(for: request("native107/fixture.glb")).source)
        var count = 0, maximum = 0, callbacks = 0, hash = SHA256()
        try ZGAssetReader.stream(source, range: 0..<source.size, cancelled: { false }, emit: {
            count += $0.count; maximum = max(maximum, $0.count); callbacks += 1; hash.update(data: $0); return true
        })
        XCTAssertEqual(count, f.model.count); XCTAssertLessThanOrEqual(maximum, 64 * 1024); XCTAssertGreaterThan(callbacks, 1)
        XCTAssertEqual(hash.finalize().map { String(format: "%02x", $0) }.joined(), sha(f.model))
        var delivered = 0
        XCTAssertThrowsError(try ZGAssetReader.stream(source, range: 0..<source.size, cancelled: { false }, emit: { _ in delivered += 1; return false }))
        XCTAssertEqual(delivered, 1, "cancellation does not inflate and buffer the rest of the model")
    }

    func testOnlyCanonicalLocalAuthorityAndManifestPaths() throws {
        let f = try fixture(); defer { f.cleanup() }
        let store = try ZGAssetStore(rootURL: f.root)
        for url in ["https://bundle/client.js", "zgasset://other/client.js", "zgasset://user@bundle/client.js",
                    "zgasset://bundle:99/client.js", "zgasset://bundle/%2e%2e/secret", "zgasset://bundle/a%5cb.js",
                    "zgasset://bundle/a//b.js", "zgasset://bundle/%00client.js"] {
            XCTAssertThrowsError(try store.response(for: URLRequest(url: URL(string: url)!)), url)
        }
        let outside = f.root.deletingLastPathComponent().appendingPathComponent("outside-" + UUID().uuidString)
        try Data("outside".utf8).write(to: outside); defer { try? FileManager.default.removeItem(at: outside) }
        try FileManager.default.removeItem(at: f.root.appendingPathComponent("files/client.js"))
        try FileManager.default.createSymbolicLink(at: f.root.appendingPathComponent("files/client.js"), withDestinationURL: outside)
        XCTAssertThrowsError(try store.response(for: request("client.js")))
    }

    func testSeparateIOSOverlayWithoutChangingOriginalFiles() throws {
        let f = try fixture(); defer { f.cleanup() }
        let overlay = f.root.appendingPathComponent("platform", isDirectory: true)
        try FileManager.default.createDirectory(at: overlay, withIntermediateDirectories: true)
        let patched = Data("export const platform = 'ios';".utf8)
        try patched.write(to: overlay.appendingPathComponent("client.js"))
        let manifest: [String: Any] = ["schemaVersion": 1, "sourceAPK_SHA256": ZGAssetStore.sourceAPKHash,
                                      "files": ["client.js": [patched.count, sha(patched)]]]
        try JSONSerialization.data(withJSONObject: manifest).write(to: overlay.appendingPathComponent("overlay-manifest.json"))
        let store = try ZGAssetStore(rootURL: f.root, overlayURL: overlay)
        XCTAssertEqual(try bytes(store.response(for: request("client.js"))), patched)
        XCTAssertEqual(try Data(contentsOf: f.root.appendingPathComponent("files/client.js")), f.client)
    }

    @MainActor
    func testStopOnResponseNeverCallsDataFinishOrFailureLater() async throws {
        let f = try fixture(); defer { f.cleanup() }
        let handler = try ZGAssetSchemeHandler(rootURL: f.root), webView = WKWebView()
        let task = RecordingSchemeTask(request: request("native107/fixture.glb"))
        let stopped = expectation(description: "response then native stop")
        task.onResponse = { handler.webView(webView, stop: task); stopped.fulfill() }
        handler.webView(webView, start: task)
        await fulfillment(of: [stopped], timeout: 3)
        let drained = expectation(description: "worker/callback cancellation drained")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) { drained.fulfill() }
        await fulfillment(of: [drained], timeout: 1)
        XCTAssertEqual(task.responses, 1); XCTAssertEqual(task.dataCalls, 0); XCTAssertEqual(task.finishes, 0); XCTAssertEqual(task.failures, 0)
        handler.cancelAll()
    }

    @MainActor
    func testRetiredViewCannotCancelOrDeliverIntoReplacementView() async throws {
        let f = try fixture(); defer { f.cleanup() }
        let handler = try ZGAssetSchemeHandler(rootURL: f.root), oldView = WKWebView(), newView = WKWebView()
        let oldTask = RecordingSchemeTask(request: request("native107/fixture.glb"))
        let newTask = RecordingSchemeTask(request: request("native107/fixture.glb"))
        let finished = expectation(description: "replacement view still receives its complete model")
        oldTask.onResponse = {
            handler.cancelAll()
            handler.webView(newView, start: newTask)
            // A late stop for a retired task must never remove the new task.
            handler.webView(oldView, stop: oldTask)
        }
        newTask.onFinish = { finished.fulfill() }
        handler.webView(oldView, start: oldTask)
        await fulfillment(of: [finished], timeout: 5)
        XCTAssertEqual(oldTask.responses, 1); XCTAssertEqual(oldTask.dataCalls, 0)
        XCTAssertEqual(oldTask.finishes, 0); XCTAssertEqual(oldTask.failures, 0)
        XCTAssertEqual(newTask.responses, 1); XCTAssertGreaterThan(newTask.dataCalls, 1)
        XCTAssertEqual(newTask.finishes, 1); XCTAssertEqual(newTask.failures, 0)
        handler.cancelAll()
    }
}

@MainActor
private final class RecordingSchemeTask: NSObject, WKURLSchemeTask {
    let request: URLRequest
    var responses = 0, dataCalls = 0, finishes = 0, failures = 0
    var onResponse: (() -> Void)?
    var onFinish: (() -> Void)?
    init(request: URLRequest) { self.request = request }
    func didReceive(_ response: URLResponse) { responses += 1; onResponse?() }
    func didReceive(_ data: Data) { dataCalls += 1 }
    func didFinish() { finishes += 1; onFinish?() }
    func didFailWithError(_ error: Error) { failures += 1 }
}
