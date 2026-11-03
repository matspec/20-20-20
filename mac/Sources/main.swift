import AppKit

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusApp: StatusApp?

    func applicationDidFinishLaunching(_ notification: Notification) {
        statusApp = StatusApp()
    }

    func applicationWillTerminate(_ notification: Notification) {
        statusApp?.dispose()
        statusApp = nil
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool { true }
}

let app = NSApplication.shared
app.setActivationPolicy(.accessory)   // menu bar only, no Dock icon

// Single instance (only meaningful when running from a .app bundle).
if let id = Bundle.main.bundleIdentifier,
   NSRunningApplication.runningApplications(withBundleIdentifier: id).count > 1 {
    Native.alert("\(App.title) is already running. Look for the cat in the menu bar.", style: .informational)
    exit(0)
}

let delegate = AppDelegate()
app.delegate = delegate
app.run()
