import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?

    func application(_ application: UIApplication,
                     didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.backgroundColor = .black
        // Hosted unit tests use fake channels and local fixtures. They must
        // never cold-start the real launcher or contact a game account.
        if ProcessInfo.processInfo.environment["XCTestConfigurationFilePath"] != nil ||
            ProcessInfo.processInfo.arguments.contains("--zg-tests") {
            let empty = UIViewController(); empty.view.backgroundColor = .black
            window.rootViewController = empty
        } else { window.rootViewController = GameViewController() }
        self.window = window
        window.makeKeyAndVisible()
        application.isIdleTimerDisabled = true
        return true
    }

    func applicationDidEnterBackground(_ application: UIApplication) {
        application.isIdleTimerDisabled = false
        (window?.rootViewController as? GameViewController)?.enteredBackground()
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        application.isIdleTimerDisabled = true
    }

    func applicationWillTerminate(_ application: UIApplication) {
        (window?.rootViewController as? GameViewController)?.shutdown()
    }
}
