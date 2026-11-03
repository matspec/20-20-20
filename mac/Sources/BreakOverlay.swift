import AppKit

// One break = one BreakSession, which opens an overlay on every screen (or a single window
// in non-full-screen mode) and closes them all together.
final class BreakSession {
    private let settings: Settings
    private let onDone: (Bool) -> Void
    private var windows: [BreakWindow] = []
    private var second: Timer?
    private var anim: Timer?
    private var closed = false

    private(set) var secondsLeft: Int
    var finished: Bool { secondsLeft <= 0 }
    private(set) var frame = 0
    private(set) var alt = false
    let skin: Skin
    var catName: String { settings.catName }
    var breakSeconds: Int { settings.breakSeconds }

    init(settings s: Settings, skin: Skin, done: @escaping (Bool) -> Void) {
        settings = s; self.skin = skin; onDone = done
        secondsLeft = s.breakSeconds

        let screens = NSScreen.screens
        let primary = screens.first
        if s.fullScreen {
            for sc in screens {
                windows.append(BreakWindow(session: self, frame: sc.frame, primary: sc == primary, windowed: false))
            }
        } else if let sc = primary {
            let wa = sc.visibleFrame
            let size = NSSize(width: min(560, wa.width - 40), height: min(380, wa.height - 40))
            let b = NSRect(x: wa.minX + (wa.width - size.width) / 2, y: wa.minY + (wa.height - size.height) / 2,
                           width: size.width, height: size.height)
            windows.append(BreakWindow(session: self, frame: b, primary: true, windowed: true))
        }

        let t1 = Timer(timeInterval: 1, repeats: true) { [weak self] _ in self?.tick() }
        let t2 = Timer(timeInterval: 0.14, repeats: true) { [weak self] _ in
            guard let self = self else { return }
            self.frame += 1
            if self.frame % 3 == 0 { self.alt.toggle() }
            self.redraw()
        }
        RunLoop.main.add(t1, forMode: .common)
        RunLoop.main.add(t2, forMode: .common)
        second = t1; anim = t2

        // Show secondaries first so the primary ends up on top with focus.
        for w in windows where !w.isPrimary { w.orderFrontRegardless() }
        for w in windows where w.isPrimary {
            w.makeKeyAndOrderFront(nil)
            w.makeFirstResponder(w.contentView)
        }
        NSApp.activate(ignoringOtherApps: true)

        if s.playSound { Sounds.play("Glass") }
    }

    private func tick() {
        if secondsLeft <= 0 { return }
        secondsLeft -= 1
        if secondsLeft == 0 {
            second?.invalidate(); second = nil
            if settings.playSound { Sounds.play("Purr") }
        }
        redraw()
    }

    private func redraw() {
        for w in windows { w.contentView?.needsDisplay = true }
    }

    func end(_ completed: Bool) {
        if closed { return }
        closed = true
        second?.invalidate(); anim?.invalidate()
        second = nil; anim = nil
        for w in windows { w.orderOut(nil); w.close() }
        windows.removeAll()
        onDone(completed)
    }

    func dispose() { end(false) }
}

enum Sounds {
    static func play(_ name: String) {
        if let s = NSSound(named: name) { s.play() } else { NSSound.beep() }
    }
}

final class BreakWindow: NSWindow {
    let isPrimary: Bool

    init(session: BreakSession, frame: NSRect, primary: Bool, windowed: Bool) {
        isPrimary = primary
        super.init(contentRect: frame, styleMask: .borderless, backing: .buffered, defer: false)
        isReleasedWhenClosed = false
        level = .screenSaver
        collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .ignoresCycle]
        backgroundColor = Palette.bg
        isOpaque = primary
        hasShadow = windowed
        hidesOnDeactivate = false
        isExcludedFromWindowsMenu = true
        acceptsMouseMovedEvents = true
        if !primary { alphaValue = 0.92 }
        contentView = BreakView(frame: NSRect(origin: .zero, size: frame.size), session: session, primary: primary, windowed: windowed)
    }

    override var canBecomeKey: Bool { isPrimary }
    override var canBecomeMain: Bool { isPrimary }
}

final class BreakView: NSView {
    private weak var session: BreakSession?
    private let isPrimary: Bool
    private let windowed: Bool
    private var doneRect = CGRect.zero, skipRect = CGRect.zero

    init(frame: NSRect, session: BreakSession, primary: Bool, windowed: Bool) {
        self.session = session; isPrimary = primary; self.windowed = windowed
        super.init(frame: frame)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) is not supported") }

    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        for a in trackingAreas { removeTrackingArea(a) }
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .activeAlways, .inVisibleRect], owner: self, userInfo: nil))
    }

    // ---------- input ----------

    override func keyDown(with event: NSEvent) {
        guard let session = session else { return }
        switch event.keyCode {
        case 53: session.end(false)                                   // Esc
        case 36, 76, 49: if session.finished { session.end(true) }    // Return, keypad Enter, Space
        default: super.keyDown(with: event)
        }
    }

    override func mouseUp(with event: NSEvent) {
        guard isPrimary, let session = session else { return }
        let p = convert(event.locationInWindow, from: nil)
        if session.finished && doneRect.contains(p) { session.end(true) }
        else if skipRect.contains(p) { session.end(false) }
    }

    override func mouseMoved(with event: NSEvent) {
        guard let session = session else { return }
        let p = convert(event.locationInWindow, from: nil)
        let hot = isPrimary && (skipRect.contains(p) || (session.finished && doneRect.contains(p)))
        (hot ? NSCursor.pointingHand : NSCursor.arrow).set()
    }

    // ---------- painting ----------

    override func draw(_ dirtyRect: NSRect) {
        guard let g = NSGraphicsContext.current?.cgContext, let session = session else { return }
        PixelRenderer.crisp(g)
        let r = bounds
        g.setFillColor(Palette.bg.cgColor)
        g.fill(r)
        PixelRenderer.drawGrid(g, r, max(24, Int(r.height) / 24))

        if isPrimary { paintPrimary(g, r, session) } else { paintSecondary(g, r, session) }

        PixelRenderer.drawScanlines(g, r, 60)
        if windowed {
            g.setStrokeColor(Palette.accentDim.cgColor)
            g.setLineWidth(2)
            g.stroke(r.insetBy(dx: 2, dy: 2))
        }
    }

    private func paintPrimary(_ g: CGContext, _ rect: CGRect, _ session: BreakSession) {
        let left = Int(rect.minX), top = Int(rect.minY), width = Int(rect.width), height = Int(rect.height)
        let bottom = top + height

        // Scale everything off the shorter edge so it looks the same on a 13" or a 5K panel.
        let unit = max(2, min(width, height) / 160)   // base pixel size
        var catScale = unit * 3
        let fsBig = unit * 4, fsMid = unit, fsSmall = max(1, unit * 3 / 4)
        let done = session.finished

        // Cat, gazing into the distance (or beaming when done).
        let skin = session.skin
        catScale = max(2, catScale * 16 / max(skin.w, skin.h))
        let key = done ? "happy" : (session.frame / 20 % 5 == 0 ? "blink" : "far")
        let catW = skin.w * catScale, catH = skin.h * catScale
        let cx = left + (width - catW) / 2
        let cy = top + height / 2 - catH - unit * 6 + (done ? (session.frame % 2) * unit : 0)
        PixelRenderer.drawSprite(g, skin, skin.frame(key, alt: session.alt), cx, cy, catScale)

        // Speech bubble.
        let say = done ? "that's it! back to work?" : "look at something far away..."
        let p = unit
        let tw = PixelFont.measure(say, fsMid)
        let bw = tw + 6 * p, bh = PixelFont.h * fsMid + 6 * p
        let bx = left + (width - bw) / 2, by = cy - bh - 4 * p
        PixelRenderer.drawBubble(g, px(bx, by, bw, bh), p, cx + catW / 2 - p)
        PixelFont.draw(g, say, bx + 3 * p, by + 3 * p, fsMid, Palette.text)

        // Big countdown.
        let big = done ? "DONE" : String(format: "%02d", session.secondsLeft)
        let bigW = PixelFont.measure(big, fsBig)
        let bigY = cy + catH + unit * 6
        PixelFont.draw(g, big, left + (width - bigW) / 2, bigY, fsBig, Palette.accent)

        // Progress bar (pixel segments).
        let segs = 20, segW = unit * 3, gap = unit
        let barW = segs * segW + (segs - 1) * gap
        let barX = left + (width - barW) / 2, barY = bigY + PixelFont.h * fsBig + unit * 4
        let lit = done ? segs : Int((Double(segs) * (1.0 - Double(session.secondsLeft) / Double(session.breakSeconds))).rounded())
        for i in 0..<segs {
            g.setFillColor((i < lit ? Palette.accent : Palette.grid).cgColor)
            g.fill(px(barX + i * (segW + gap), barY, segW, unit * 2))
        }

        // Caption.
        let cap = done ? session.catName.uppercased() + " APPROVES" : "20 FEET  /  20 SECONDS  /  EVERY 20 MINUTES"
        let cw = PixelFont.measure(cap, fsSmall)
        PixelFont.draw(g, cap, left + (width - cw) / 2, barY + unit * 6, fsSmall, Palette.textDim)

        // Buttons.
        let btnH = PixelFont.h * fsMid + 6 * unit, btnY = bottom - btnH - unit * 8
        let doneW = PixelFont.measure("DONE", fsMid) + 10 * unit, skipW = PixelFont.measure("SKIP", fsMid) + 10 * unit
        let totalW = doneW + skipW + unit * 4
        doneRect = px(left + (width - totalW) / 2, btnY, doneW, btnH)
        skipRect = px(Int(doneRect.maxX) + unit * 4, btnY, skipW, btnH)
        BreakView.drawButton(g, doneRect, "DONE", fsMid, unit, done ? Palette.accent : Palette.grid, done ? Palette.bg : Palette.textDim)
        BreakView.drawButton(g, skipRect, "SKIP", fsMid, unit, Palette.bubble, Palette.textDim)

        let hint = done ? "ENTER TO CONTINUE" : "ESC TO SKIP"
        let hw = PixelFont.measure(hint, fsSmall)
        PixelFont.draw(g, hint, left + (width - hw) / 2, bottom - unit * 5, fsSmall, Palette.textDim)
    }

    private func paintSecondary(_ g: CGContext, _ rect: CGRect, _ session: BreakSession) {
        let left = Int(rect.minX), top = Int(rect.minY), width = Int(rect.width), height = Int(rect.height)
        let unit = max(2, min(width, height) / 160)
        let fsBig = unit * 3, fsSmall = unit
        let big = session.finished ? "DONE" : String(format: "%02d", session.secondsLeft)
        let bw = PixelFont.measure(big, fsBig)
        PixelFont.draw(g, big, left + (width - bw) / 2, top + height / 2 - PixelFont.h * fsBig, fsBig, Palette.accent)
        let s = session.finished ? "NICE" : "LOOK AWAY FROM THE SCREEN"
        let sw = PixelFont.measure(s, fsSmall)
        PixelFont.draw(g, s, left + (width - sw) / 2, top + height / 2 + unit * 4, fsSmall, Palette.textDim)
    }

    private static func drawButton(_ g: CGContext, _ rect: CGRect, _ text: String, _ fs: Int, _ p: Int, _ fill: NSColor, _ fg: NSColor) {
        let x = Int(rect.minX), y = Int(rect.minY), w = Int(rect.width), h = Int(rect.height)
        g.setFillColor(fill.cgColor)
        g.fill(px(x + p, y, w - 2 * p, h))
        g.fill(px(x, y + p, w, h - 2 * p))
        let tw = PixelFont.measure(text, fs)
        PixelFont.draw(g, text, x + (w - tw) / 2, y + (h - PixelFont.h * fs) / 2, fs, fg)
    }
}
