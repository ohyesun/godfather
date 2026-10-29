import UIKit
import WebKit

@MainActor
final class GameViewController: UIViewController, WKNavigationDelegate, WKUIDelegate {
    private var webView: WKWebView?
    private var bridge: ZGScriptBridge?
    private var assets: ZGAssetSchemeHandler?
    private var networking: ZGNativeNetworking?
    private let storage = ZGSecureStorage()
    private var statusPanel: UIView?
    private var hasAllowedMainNavigation = false
    private var pendingDialogCancel: (() -> Void)?
    private let loading = UIActivityIndicatorView(style: .large)

    override var prefersStatusBarHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
    override var shouldAutorotate: Bool { true }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .black
        loading.color = .systemYellow
        loading.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(loading)
        NSLayoutConstraint.activate([loading.centerXAnchor.constraint(equalTo: view.centerXAnchor),
                                     loading.centerYAnchor.constraint(equalTo: view.centerYAnchor)])
        openGame()
    }

    private func prepareServices() throws {
        if networking == nil {
            let configuration = try ZGConfiguration.load()
            networking = try ZGNativeNetworking(profile: configuration.profile,
                                                launcherAuthentication: configuration.launcherAuthentication)
        }
        if assets == nil {
            guard let root = Bundle.main.resourceURL else { throw ZGConfigurationError.missingAssets }
            assets = try ZGAssetSchemeHandler(rootURL: root.appendingPathComponent("GameAssets", isDirectory: true),
                overlayURL: root.appendingPathComponent("GameOverlay", isDirectory: true))
        }
    }

    private func openGame() {
        shutdown()
        statusPanel?.removeFromSuperview(); statusPanel = nil
        loading.startAnimating()
        do {
            try prepareServices()
            guard let networking, let assets, let resourceRoot = Bundle.main.resourceURL else {
                throw ZGConfigurationError.missingAssets
            }
            let source = try String(contentsOf: resourceRoot.appendingPathComponent("GameOverlay/native-ios.js"), encoding: .utf8)
            let bridge = ZGScriptBridge(networking: networking, storage: storage)
            let controller = WKUserContentController()
            controller.add(ZGWeakScriptHandler(bridge), name: ZGScriptBridge.channel)
            controller.addUserScript(try bridge.userScript(source: source))
            let configuration = WKWebViewConfiguration()
            configuration.userContentController = controller
            configuration.websiteDataStore = .default()
            configuration.setURLSchemeHandler(assets, forURLScheme: "zgasset")
            configuration.defaultWebpagePreferences.allowsContentJavaScript = true
            configuration.defaultWebpagePreferences.preferredContentMode = .mobile
            configuration.allowsInlineMediaPlayback = true
            configuration.mediaTypesRequiringUserActionForPlayback = []
            configuration.suppressesIncrementalRendering = false
            let webView = WKWebView(frame: .zero, configuration: configuration)
            webView.isOpaque = false; webView.backgroundColor = .black
            webView.scrollView.backgroundColor = .black
            webView.scrollView.bounces = false
            webView.scrollView.contentInsetAdjustmentBehavior = .never
            webView.scrollView.keyboardDismissMode = .interactive
            webView.allowsBackForwardNavigationGestures = false
            webView.navigationDelegate = self; webView.uiDelegate = self
            #if DEBUG
            if #available(iOS 16.4, *) { webView.isInspectable = true }
            #endif
            webView.translatesAutoresizingMaskIntoConstraints = false
            view.insertSubview(webView, belowSubview: loading)
            let safe = view.safeAreaLayoutGuide
            NSLayoutConstraint.activate([webView.leadingAnchor.constraint(equalTo: safe.leadingAnchor),
                webView.trailingAnchor.constraint(equalTo: safe.trailingAnchor),
                webView.topAnchor.constraint(equalTo: safe.topAnchor), webView.bottomAnchor.constraint(equalTo: safe.bottomAnchor)])
            self.bridge = bridge; self.webView = webView
            hasAllowedMainNavigation = false
            bridge.attach(webView)
            webView.load(URLRequest(url: ZGAssetSchemeHandler.entryURL))
        } catch {
            loading.stopAnimating()
            showStatus("游戏文件未能载入，请使用完整安装包。", button: "重新载入")
        }
    }

    func shutdown() {
        pendingDialogCancel?(); pendingDialogCancel = nil
        presentedViewController?.dismiss(animated: false)
        bridge?.invalidate(); bridge = nil
        assets?.cancelAll()
        if let webView {
            webView.stopLoading()
            webView.configuration.userContentController.removeScriptMessageHandler(forName: ZGScriptBridge.channel)
            webView.configuration.userContentController.removeAllUserScripts()
            webView.navigationDelegate = nil; webView.uiDelegate = nil
            webView.removeFromSuperview()
        }
        webView = nil
    }

    func enteredBackground() {
        guard webView != nil else { return }
        // No background networking entitlement or false offline hosting.
        // Replace the document on the next explicit entry; do not replay a
        // possible in-flight cast, purchase, login, or connection write.
        shutdown()
        loading.stopAnimating()
        showStatus("连接已暂停，请重新进入游戏。", button: "重新进入")
    }

    private func showStatus(_ text: String, button: String) {
        statusPanel?.removeFromSuperview()
        let label = UILabel()
        label.text = text; label.textColor = UIColor(red: 0.85, green: 0.76, blue: 0.53, alpha: 1)
        label.numberOfLines = 0; label.textAlignment = .center
        let action = UIButton(type: .system)
        action.setTitle(button, for: .normal); action.tintColor = .systemYellow
        action.addTarget(self, action: #selector(reenter), for: .touchUpInside)
        let stack = UIStackView(arrangedSubviews: [label, action])
        stack.axis = .vertical; stack.spacing = 24
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([stack.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            stack.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stack.widthAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.widthAnchor, multiplier: 0.85)])
        statusPanel = stack
    }

    @objc private func reenter() { openGame() }

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                 decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        guard webView === self.webView, navigationAction.targetFrame?.isMainFrame == true,
              ZGScriptBridge.permittedDocument(navigationAction.request.url) else {
            decisionHandler(.cancel); return
        }
        // Every replacement document, including location.assign and an
        // ordinary link to index.html, needs a new nonce. A fragment-only
        // navigation retains the same document and is safe to keep.
        let requested = navigationAction.request.url
        var currentParts = webView.url.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false) }
        var requestedParts = requested.flatMap { URLComponents(url: $0, resolvingAgainstBaseURL: false) }
        let changedFragment = currentParts?.fragment != requestedParts?.fragment
        currentParts?.fragment = nil; requestedParts?.fragment = nil
        let fragmentOnly = changedFragment && currentParts?.url == requestedParts?.url
        if hasAllowedMainNavigation && !fragmentOnly {
            decisionHandler(.cancel)
            DispatchQueue.main.async { [weak self, weak webView] in
                guard let self, let webView, self.webView === webView,
                      UIApplication.shared.applicationState != .background else { return }
                self.openGame()
            }
        } else { hasAllowedMainNavigation = true; decisionHandler(.allow) }
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        guard webView === self.webView else { return }
        loading.stopAnimating()
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        navigationFailed(webView, error: error)
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        navigationFailed(webView, error: error)
    }

    private func navigationFailed(_ webView: WKWebView, error: Error) {
        guard webView === self.webView, (error as NSError).code != NSURLErrorCancelled else { return }
        shutdown(); loading.stopAnimating()
        showStatus("游戏画面未能载入。", button: "重新载入")
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        guard webView === self.webView else { return }
        shutdown(); loading.stopAnimating()
        showStatus("游戏画面已关闭，请重新进入。", button: "重新进入")
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
        guard webView === self.webView, frame.isMainFrame, ZGScriptBridge.permittedDocument(frame.request.url),
              view.window != nil, presentedViewController == nil else { completionHandler(); return }
        let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
        var finished = false
        let finish = { [weak self] in
            guard !finished else { return }; finished = true
            self?.pendingDialogCancel = nil; completionHandler()
        }
        pendingDialogCancel = finish
        alert.addAction(UIAlertAction(title: "确定", style: .default) { _ in finish() })
        present(alert, animated: true)
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
                 initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
        guard webView === self.webView, frame.isMainFrame, ZGScriptBridge.permittedDocument(frame.request.url),
              view.window != nil, presentedViewController == nil else { completionHandler(false); return }
        let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
        var finished = false
        let finish: (Bool) -> Void = { [weak self] value in
            guard !finished else { return }; finished = true
            self?.pendingDialogCancel = nil; completionHandler(value)
        }
        pendingDialogCancel = { finish(false) }
        alert.addAction(UIAlertAction(title: "取消", style: .cancel) { _ in finish(false) })
        alert.addAction(UIAlertAction(title: "确定", style: .default) { _ in finish(true) })
        present(alert, animated: true)
    }

    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String,
                 defaultText: String?, initiatedByFrame frame: WKFrameInfo,
                 completionHandler: @escaping (String?) -> Void) {
        guard webView === self.webView, frame.isMainFrame, ZGScriptBridge.permittedDocument(frame.request.url),
              view.window != nil, presentedViewController == nil else { completionHandler(nil); return }
        let alert = UIAlertController(title: nil, message: prompt, preferredStyle: .alert)
        alert.addTextField { $0.text = defaultText }
        var finished = false
        let finish: (String?) -> Void = { [weak self] value in
            guard !finished else { return }; finished = true
            self?.pendingDialogCancel = nil; completionHandler(value)
        }
        pendingDialogCancel = { finish(nil) }
        alert.addAction(UIAlertAction(title: "取消", style: .cancel) { _ in finish(nil) })
        alert.addAction(UIAlertAction(title: "确定", style: .default) { [weak alert] _ in finish(alert?.textFields?.first?.text) })
        present(alert, animated: true)
    }
}
