import Foundation
import WebKit

private final class ZGAssetCancellation: @unchecked Sendable {
    private let lock = NSLock()
    private var value = false
    var cancelled: Bool { lock.lock(); defer { lock.unlock() }; return value }
    func cancel() { lock.lock(); value = true; lock.unlock() }
}

/// Register before constructing WKWebView:
/// configuration.setURLSchemeHandler(handler, forURLScheme: "zgasset")
/// webView.load(URLRequest(url: ZGAssetSchemeHandler.entryURL))
///
/// WebKit start/stop and every task callback share the main actor. A worker
/// never calls WKURLSchemeTask directly. Queued delivery checks identity again
/// on the main actor, so stop() cannot race a later didReceive/didFinish.
@MainActor
final class ZGAssetSchemeHandler: NSObject, WKURLSchemeHandler {
    static let entryURL = ZGAssetStore.entryURL
    private struct Active { let task: WKURLSchemeTask; let cancellation: ZGAssetCancellation }
    private enum Delivery { case response(HTTPURLResponse), data(Data), finished, failed(Error) }
    private let store: ZGAssetStore
    private let workers: OperationQueue
    private var active: [ObjectIdentifier: Active] = [:]

    init(rootURL: URL, overlayURL: URL? = nil) throws {
        let assetStore = try ZGAssetStore(rootURL: rootURL, overlayURL: overlayURL)
        let workerQueue = OperationQueue()
        workerQueue.name = "cn.zhanguo.assets"
        workerQueue.qualityOfService = .userInitiated
        workerQueue.maxConcurrentOperationCount = 3
        store = assetStore
        workers = workerQueue
        super.init()
    }

    func webView(_ webView: WKWebView, start urlSchemeTask: WKURLSchemeTask) {
        let id = ObjectIdentifier(urlSchemeTask), cancellation = ZGAssetCancellation(), request = urlSchemeTask.request
        active[id]?.cancellation.cancel()
        active[id] = Active(task: urlSchemeTask, cancellation: cancellation)
        let assetStore = store
        // Acknowledged main-actor delivery provides backpressure: even a
        // request for the full 1.64 GB PAK queues at most one 64 KB chunk.
        let deliver: (Delivery) -> Bool = { [weak self] event in
            if cancellation.cancelled { return false }
            let acknowledged = DispatchSemaphore(value: 0)
            DispatchQueue.main.async { [weak self] in
                defer { acknowledged.signal() }
                guard let self, let entry = self.active[id], entry.cancellation === cancellation,
                      !cancellation.cancelled else { cancellation.cancel(); return }
                switch event {
                case .response(let response): entry.task.didReceive(response)
                case .data(let bytes): entry.task.didReceive(bytes)
                case .finished:
                    self.active.removeValue(forKey: id)
                    entry.task.didFinish()
                case .failed(let error):
                    self.active.removeValue(forKey: id)
                    entry.task.didFailWithError(error)
                }
            }
            while acknowledged.wait(timeout: .now() + .milliseconds(100)) == .timedOut {
                if cancellation.cancelled { return false }
            }
            return !cancellation.cancelled
        }
        workers.addOperation {
            do {
                if cancellation.cancelled { return }
                let plan = try assetStore.response(for: request)
                guard let response = HTTPURLResponse(url: plan.url, statusCode: plan.status,
                    httpVersion: "HTTP/1.1", headerFields: plan.headers) else { throw ZGAssetError.invalidPath }
                guard deliver(.response(response)) else { return }
                if !plan.head {
                    if let bytes = plan.smallBody, !deliver(.data(bytes)) { return }
                    if let source = plan.source {
                        try ZGAssetReader.stream(source, range: plan.range,
                            cancelled: { cancellation.cancelled }, emit: { deliver(.data($0)) })
                    }
                }
                if !cancellation.cancelled { _ = deliver(.finished) }
            } catch ZGAssetError.cancelled {
                // WebKit has already stopped the task. All callbacks,
                // including didFailWithError, are forbidden after stop().
            } catch {
                if !cancellation.cancelled { _ = deliver(.failed(error)) }
            }
        }
    }

    func webView(_ webView: WKWebView, stop urlSchemeTask: WKURLSchemeTask) {
        let id = ObjectIdentifier(urlSchemeTask)
        active.removeValue(forKey: id)?.cancellation.cancel()
    }

    func cancelAll() {
        for entry in active.values { entry.cancellation.cancel() }
        active.removeAll()
        workers.cancelAllOperations()
    }
}
