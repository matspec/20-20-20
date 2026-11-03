import AppKit
import CoreGraphics

enum Native {
    // Seconds since the last keyboard/mouse/trackpad event in this login session.
    static func idleSeconds() -> Int {
        // ~0 is kCGAnyInputEventType.
        guard let any = CGEventType(rawValue: ~0) else { return 0 }
        let t = CGEventSource.secondsSinceLastEventType(.combinedSessionState, eventType: any)
        return t.isFinite ? Int(t) : 0
    }

    static func alert(_ text: String, style: NSAlert.Style = .warning) {
        NSApp.activate(ignoringOtherApps: true)
        let a = NSAlert()
        a.alertStyle = style
        a.messageText = App.title
        a.informativeText = text
        a.runModal()
    }
}

// Lets menu items run closures (NSMenuItem.target is weak, so the helper is parked in
// representedObject to keep it alive for as long as the item exists).
final class MenuAction: NSObject {
    let run: () -> Void
    init(_ run: @escaping () -> Void) { self.run = run }
    @objc func fire(_ sender: Any?) { run() }
}

extension NSMenuItem {
    static func make(_ title: String, _ run: @escaping () -> Void) -> NSMenuItem {
        let t = MenuAction(run)
        let it = NSMenuItem(title: title, action: #selector(MenuAction.fire(_:)), keyEquivalent: "")
        it.target = t
        it.representedObject = t
        return it
    }

    var isChecked: Bool {
        get { state == .on }
        set { state = newValue ? .on : .off }
    }
}
