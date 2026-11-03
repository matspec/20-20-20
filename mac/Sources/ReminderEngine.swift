import AppKit

enum Mood { case idle, restless, onBreak, happy, sleeping, away }

// Owns the countdown and the "what is the cat feeling" state. UI layers subscribe via onChanged.
final class ReminderEngine {
    private static let idleResetSeconds = 5 * 60
    private static let restlessSeconds = 2 * 60
    private static let happySeconds = 8

    let settings: Settings
    private var tick: Timer?
    private var happyLeft = 0
    private var away = false
    private var observers: [(NotificationCenter, NSObjectProtocol)] = []

    private(set) var remaining: Int
    private(set) var breakActive = false

    var mood: Mood {
        if breakActive { return .onBreak }
        if settings.paused { return .sleeping }
        if away { return .away }
        if happyLeft > 0 { return .happy }
        return remaining <= ReminderEngine.restlessSeconds ? .restless : .idle
    }

    var onChanged: (() -> Void)?
    var onBreakRequested: (() -> Void)?

    init(settings s: Settings) {
        settings = s
        remaining = s.workMinutes * 60

        let ws = NSWorkspace.shared.notificationCenter
        let dnc = DistributedNotificationCenter.default()
        func on(_ center: NotificationCenter, _ name: Notification.Name, _ f: @escaping () -> Void) {
            observers.append((center, center.addObserver(forName: name, object: nil, queue: .main) { _ in f() }))
        }
        // Sleep / wake
        on(ws, NSWorkspace.willSleepNotification) { [weak self] in self?.goAway() }
        on(ws, NSWorkspace.didWakeNotification) { [weak self] in self?.cameBack() }
        // Fast user switching
        on(ws, NSWorkspace.sessionDidResignActiveNotification) { [weak self] in self?.goAway() }
        on(ws, NSWorkspace.sessionDidBecomeActiveNotification) { [weak self] in self?.cameBack() }
        // Screen lock / screensaver (not in the public SDK as constants, but stable for 15+ years)
        on(dnc, Notification.Name("com.apple.screenIsLocked")) { [weak self] in self?.goAway() }
        on(dnc, Notification.Name("com.apple.screenIsUnlocked")) { [weak self] in self?.cameBack() }
        on(dnc, Notification.Name("com.apple.screensaver.didstart")) { [weak self] in self?.goAway() }
        on(dnc, Notification.Name("com.apple.screensaver.didstop")) { [weak self] in self?.cameBack() }

        let t = Timer(timeInterval: 1, repeats: true) { [weak self] _ in self?.onTick() }
        RunLoop.main.add(t, forMode: .common)   // keeps ticking while a menu is open
        tick = t
    }

    private func onTick() {
        if happyLeft > 0 { happyLeft -= 1 }
        if settings.paused || away || breakActive { raise(); return }

        if Native.idleSeconds() >= ReminderEngine.idleResetSeconds {
            remaining = settings.workMinutes * 60
            raise()
            return
        }

        remaining -= 1
        if remaining <= 0 { remaining = 0; requestBreak(); return }
        raise()
    }

    func requestBreak() {
        if breakActive { return }
        breakActive = true
        raise()
        onBreakRequested?()
    }

    func completeBreak(_ finished: Bool) {
        breakActive = false
        remaining = settings.workMinutes * 60
        happyLeft = finished ? ReminderEngine.happySeconds : 0
        raise()
    }

    func reset() { remaining = settings.workMinutes * 60; raise() }
    func togglePause() { settings.paused.toggle(); settings.save(); raise() }
    func intervalChanged() { remaining = min(remaining, settings.workMinutes * 60); raise() }

    private func goAway() { away = true; raise() }
    private func cameBack() { away = false; remaining = settings.workMinutes * 60; raise() }

    private func raise() { onChanged?() }

    func statusText() -> String {
        switch mood {
        case .sleeping: return "Paused"
        case .away: return "Away"
        case .onBreak: return "Break time"
        default: return "Next break in " + ReminderEngine.fmt(remaining)
        }
    }

    static func fmt(_ secs: Int) -> String {
        secs >= 60 ? "\((secs + 59) / 60) min" : "\(secs) sec"
    }

    func dispose() {
        tick?.invalidate()
        tick = nil
        for (center, token) in observers { center.removeObserver(token) }
        observers.removeAll()
    }

    deinit { dispose() }
}
