import AppKit

// The always-on desktop buddy. Borderless, non-activating (never steals focus), floats above
// normal windows on every Space, draggable, transparent background.
final class PetPanel: NSPanel {
    let petView: PetView

    init(settings: Settings, engine: ReminderEngine, skin: Skin, menu: NSMenu) {
        let scale = max(2, min(8, settings.petScale))
        let size = NSSize(width: max(60, skin.w + 44) * scale, height: (skin.h + 16) * scale)
        petView = PetView(frame: NSRect(origin: .zero, size: size), settings: settings, engine: engine, skin: skin, menu: menu, scale: scale)

        super.init(contentRect: NSRect(origin: .zero, size: size),
                   styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
        isReleasedWhenClosed = false
        level = .floating
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .ignoresCycle]
        isOpaque = false
        backgroundColor = .clear
        hasShadow = false
        hidesOnDeactivate = false
        isFloatingPanel = true
        becomesKeyOnlyIfNeeded = true
        isMovableByWindowBackground = false
        isExcludedFromWindowsMenu = true
        contentView = petView
        setFrameOrigin(PetPanel.initialOrigin(settings, size))
    }

    override var canBecomeKey: Bool { false }
    override var canBecomeMain: Bool { false }

    static func initialOrigin(_ settings: Settings, _ size: NSSize) -> NSPoint {
        if settings.petPlaced {
            let r = NSRect(x: CGFloat(settings.petX), y: CGFloat(settings.petY), width: size.width, height: size.height)
            for sc in NSScreen.screens where sc.visibleFrame.contains(r) { return r.origin }
        }
        guard let screen = NSScreen.screens.first else { return .zero }
        let vf = screen.visibleFrame
        return NSPoint(x: vf.maxX - size.width - 24, y: vf.minY)
    }

    func engineChanged() { petView.engineChanged() }

    func dispose() {
        petView.dispose()
        orderOut(nil)
        close()
    }
}

final class PetView: NSView {
    private let settings: Settings
    private let engine: ReminderEngine
    private let contextMenu: NSMenu
    private let skin: Skin
    private let scale: Int
    private var anim: Timer?

    private var frameNo = 0
    private var blinkFor = 0, farFor = 0, bubbleFor = 0
    private var alt = false
    private var hover = false, dragging = false, moved = false
    private var dragStart = NSPoint.zero, dragOrigin = NSPoint.zero
    private var lastMood: Mood

    var onPauseToggled: (() -> Void)?

    init(frame frameRect: NSRect, settings: Settings, engine: ReminderEngine, skin: Skin, menu: NSMenu, scale: Int) {
        self.settings = settings; self.engine = engine; self.skin = skin; self.contextMenu = menu; self.scale = scale
        lastMood = engine.mood
        super.init(frame: frameRect)
        bubbleFor = 30 // say hi on launch
        let t = Timer(timeInterval: 0.14, repeats: true) { [weak self] _ in self?.step() }
        RunLoop.main.add(t, forMode: .common)
        anim = t
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) is not supported") }

    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { false }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    func dispose() { anim?.invalidate(); anim = nil }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        for a in trackingAreas { removeTrackingArea(a) }
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseEnteredAndExited, .activeAlways, .inVisibleRect], owner: self, userInfo: nil))
    }

    override func mouseEntered(with event: NSEvent) { hover = true; needsDisplay = true }
    override func mouseExited(with event: NSEvent) { hover = false; needsDisplay = true }

    // ---------- animation ----------

    private func step() {
        frameNo += 1
        let mood = engine.mood
        if blinkFor > 0 { blinkFor -= 1 }
        if farFor > 0 { farFor -= 1 }
        if bubbleFor > 0 { bubbleFor -= 1 }

        let sleepy = mood == .sleeping || mood == .away
        if !sleepy && blinkFor == 0 && Int.random(in: 0..<100) < 4 { blinkFor = 2 }
        // The cat models good behaviour: it periodically gazes into the distance.
        if mood == .idle && farFor == 0 && Int.random(in: 0..<100) < 2 { farFor = 12 }

        let tailEvery = (mood == .restless || mood == .happy) ? 2 : sleepy ? 12 : 5
        if frameNo % tailEvery == 0 { alt.toggle() }
        needsDisplay = true
    }

    func engineChanged() {
        let m = engine.mood
        if m != lastMood { lastMood = m; bubbleFor = 25 }
        else if m == .idle && engine.remaining % 600 == 0 { bubbleFor = 20 } // every 10 min check-in
    }

    private func currentFrameKey() -> String {
        switch engine.mood {
        case .sleeping, .away: return "sleep"
        case .happy: return "happy"
        case .onBreak: return "wide"
        case .restless: return blinkFor > 0 ? "blink" : (frameNo / 4 % 2 == 0 ? "idle" : "wide")
        default: return blinkFor > 0 ? "blink" : farFor > 0 ? "far" : "idle"
        }
    }

    private func bubbleText() -> String {
        switch engine.mood {
        case .sleeping: return "zzz... (paused)"
        case .away: return "zzz..."
        case .onBreak: return "look far away!"
        case .happy: return "good job!"
        case .restless: return "break soon... " + ReminderEngine.fmt(engine.remaining)
        default:
            if bubbleFor > 20 && frameNo < 60 { return "hi, i'm \(settings.catName)!" }
            return "next break: " + ReminderEngine.fmt(engine.remaining)
        }
    }

    override func draw(_ dirtyRect: NSRect) {
        guard let g = NSGraphicsContext.current?.cgContext else { return }
        PixelRenderer.crisp(g)
        g.clear(bounds)

        let width = Int(bounds.width), height = Int(bounds.height)
        let catW = skin.w * scale, catH = skin.h * scale
        let cx = (width - catW) / 2, cy = height - catH
        let mood = engine.mood
        let bob = mood == .happy ? (frameNo % 2) * scale : 0
        PixelRenderer.drawSprite(g, skin, skin.frame(currentFrameKey(), alt: alt), cx, cy - bob, scale)

        if mood == .sleeping || mood == .away {
            let fs = max(1, scale / 2)
            let phase = frameNo / 3 % 4
            for i in 0..<3 {
                var a = 255 - i * 70 - phase * 20
                if a < 40 { a = 40 }
                PixelFont.draw(g, "z", cx + catW - 3 * scale + i * 3 * fs, cy - (2 + i * 4 + phase) * fs, fs,
                               Palette.accent.withAlphaComponent(CGFloat(a) / 255))
            }
        }

        if hover || bubbleFor > 0 || mood == .onBreak {
            let fs = max(1, scale / 2), p = max(1, scale / 2)
            let text = bubbleText()
            let tw = PixelFont.measure(text, fs)
            let bw = tw + 6 * p, bh = PixelFont.h * fs + 6 * p
            let bx = max(0, min(width - bw, cx + catW / 2 - bw / 2))
            var by = cy - bh - 3 * p - bob
            if by < 0 { by = 0 }
            PixelRenderer.drawBubble(g, px(bx, by, bw, bh), p, cx + catW / 2 - p)
            PixelFont.draw(g, text, bx + 3 * p, by + 3 * p, fs, Palette.text)
        }
    }

    // ---------- mouse ----------

    override func mouseDown(with event: NSEvent) {
        if event.modifierFlags.contains(.control) { showMenu(event); return }
        if event.clickCount == 2 { dragging = false; onPauseToggled?(); return }
        dragging = true; moved = false
        dragStart = NSEvent.mouseLocation
        dragOrigin = window?.frame.origin ?? .zero
    }

    override func mouseDragged(with event: NSEvent) {
        guard dragging, let win = window else { return }
        let now = NSEvent.mouseLocation
        let dx = now.x - dragStart.x, dy = now.y - dragStart.y
        if abs(dx) > 3 || abs(dy) > 3 { moved = true }
        if moved { win.setFrameOrigin(NSPoint(x: dragOrigin.x + dx, y: dragOrigin.y + dy)) }
    }

    override func mouseUp(with event: NSEvent) {
        guard dragging else { return }
        dragging = false
        if moved, let win = window {
            settings.petX = Int(win.frame.origin.x.rounded())
            settings.petY = Int(win.frame.origin.y.rounded())
            settings.petPlaced = true
            settings.save()
        } else {
            bubbleFor = bubbleFor > 0 ? 0 : 25
        }
    }

    override func rightMouseDown(with event: NSEvent) { showMenu(event) }

    private func showMenu(_ event: NSEvent) {
        NSMenu.popUpContextMenu(contextMenu, with: event, for: self)
    }
}
