import AppKit

// UI palette, a tiny 3x5 pixel font and a nearest-neighbour sprite renderer.
// Character-to-colour mapping for sprites lives in the Skin.
//
// All drawing code assumes a y-down coordinate system (the views set isFlipped = true),
// so it reads the same as the Windows GDI+ version.

enum Palette {
    static func rgb(_ r: Int, _ g: Int, _ b: Int) -> NSColor {
        NSColor(srgbRed: CGFloat(r) / 255, green: CGFloat(g) / 255, blue: CGFloat(b) / 255, alpha: 1)
    }
    static let bg = rgb(11, 15, 20)
    static let grid = rgb(22, 30, 40)
    static let accent = rgb(94, 240, 160)
    static let accentDim = rgb(40, 120, 85)
    static let text = rgb(225, 232, 240)
    static let textDim = rgb(120, 135, 150)
    static let bubble = rgb(28, 36, 48)
    static let bubbleBorder = rgb(94, 240, 160)
}

@inline(__always) func px(_ x: Int, _ y: Int, _ w: Int, _ h: Int) -> CGRect {
    CGRect(x: x, y: y, width: w, height: h)
}

enum PixelFont {
    static let w = 3, h = 5
    private static let glyphs: [Character: [String]] = [
        "A": ["010","101","111","101","101"], "B": ["110","101","110","101","110"],
        "C": ["011","100","100","100","011"], "D": ["110","101","101","101","110"],
        "E": ["111","100","110","100","111"], "F": ["111","100","110","100","100"],
        "G": ["011","100","101","101","011"], "H": ["101","101","111","101","101"],
        "I": ["111","010","010","010","111"], "J": ["001","001","001","101","010"],
        "K": ["101","101","110","101","101"], "L": ["100","100","100","100","111"],
        "M": ["101","111","111","101","101"], "N": ["110","101","101","101","101"],
        "O": ["010","101","101","101","010"], "P": ["110","101","110","100","100"],
        "Q": ["010","101","101","011","001"], "R": ["110","101","110","101","101"],
        "S": ["011","100","010","001","110"], "T": ["111","010","010","010","010"],
        "U": ["101","101","101","101","111"], "V": ["101","101","101","101","010"],
        "W": ["101","101","111","111","101"], "X": ["101","101","010","101","101"],
        "Y": ["101","101","010","010","010"], "Z": ["111","001","010","100","111"],
        "0": ["111","101","101","101","111"], "1": ["010","110","010","010","111"],
        "2": ["111","001","111","100","111"], "3": ["111","001","111","001","111"],
        "4": ["101","101","111","001","001"], "5": ["111","100","111","001","111"],
        "6": ["111","100","111","101","111"], "7": ["111","001","001","001","001"],
        "8": ["111","101","111","101","111"], "9": ["111","101","111","001","111"],
        ":": ["000","010","000","010","000"], "!": ["010","010","010","000","010"],
        ".": ["000","000","000","000","010"], "-": ["000","000","111","000","000"],
        "'": ["010","010","000","000","000"], ",": ["000","000","000","010","100"],
        "?": ["111","001","011","000","010"], "/": ["001","001","010","100","100"],
        "(": ["010","100","100","100","010"], ")": ["010","001","001","001","010"],
        "%": ["101","001","010","100","101"], "~": ["000","010","101","000","000"],
        " ": ["000","000","000","000","000"],
    ]

    static func measure(_ text: String, _ scale: Int) -> Int {
        text.isEmpty ? 0 : (text.count * (w + 1) - 1) * scale
    }

    static func draw(_ g: CGContext, _ text: String, _ x: Int, _ y: Int, _ scale: Int, _ color: NSColor) {
        g.setFillColor(color.cgColor)
        var x = x
        for ch in text.uppercased() {
            if let rows = glyphs[ch] {
                for (r, row) in rows.enumerated() {
                    for (c, bit) in row.enumerated() where bit == "1" {
                        g.fill(px(x + c * scale, y + r * scale, scale, scale))
                    }
                }
            }
            x += (w + 1) * scale
        }
    }
}

enum PixelRenderer {
    static func crisp(_ g: CGContext) {
        g.setShouldAntialias(false)
        g.interpolationQuality = .none
    }

    // Draws one frame directly as filled rectangles, merging horizontal runs of the same colour.
    // Sprites are at most 64x64 so this is cheap enough to do every animation tick.
    static func drawSprite(_ g: CGContext, _ skin: Skin, _ rows: [[Character]], _ x: Int, _ y: Int, _ scale: Int) {
        for (r, row) in rows.enumerated() {
            var c = 0
            while c < row.count {
                guard let col = skin.color(row[c]) else { c += 1; continue }
                var run = 1
                while c + run < row.count && row[c + run] == row[c] { run += 1 }
                g.setFillColor(col.cgColor)
                g.fill(px(x + c * scale, y + r * scale, run * scale, scale))
                c += run
            }
        }
    }

    // Speech bubble with a little pixel tail pointing down to (tailX, bottom).
    static func drawBubble(_ g: CGContext, _ r: CGRect, _ p: Int, _ tailX: Int) {
        let x = Int(r.minX), y = Int(r.minY), w = Int(r.width), h = Int(r.height), bottom = y + h
        g.setFillColor(Palette.bubbleBorder.cgColor)
        g.fill(px(x, y + p, w, h - 2 * p))
        g.fill(px(x + p, y, w - 2 * p, h))
        g.setFillColor(Palette.bubble.cgColor)
        g.fill(px(x + p, y + p, w - 2 * p, h - 2 * p))
        g.setFillColor(Palette.bubbleBorder.cgColor)
        g.fill(px(tailX - p, bottom, 3 * p, p))
        g.fill(px(tailX, bottom + p, p, p))
        g.setFillColor(Palette.bubble.cgColor)
        g.fill(px(tailX, bottom - p, p, p))
    }

    static func drawScanlines(_ g: CGContext, _ r: CGRect, _ alpha: Int) {
        g.setFillColor(NSColor(srgbRed: 0, green: 0, blue: 0, alpha: CGFloat(alpha) / 255).cgColor)
        var y = Int(r.minY)
        while y < Int(r.maxY) { g.fill(px(Int(r.minX), y, Int(r.width), 1)); y += 3 }
    }

    static func drawGrid(_ g: CGContext, _ r: CGRect, _ step: Int) {
        g.setFillColor(Palette.grid.cgColor)
        var x = Int(r.minX)
        while x < Int(r.maxX) { g.fill(px(x, Int(r.minY), 1, Int(r.height))); x += step }
        var y = Int(r.minY)
        while y < Int(r.maxY) { g.fill(px(Int(r.minX), y, Int(r.width), 1)); y += step }
    }

    // Fits a sprite into a square image of the given point size (crisp on Retina because the
    // drawing handler is re-run at the backing scale).
    static func image(_ skin: Skin, _ rows: [[Character]], size: Int) -> NSImage {
        let scale = max(1, size / max(skin.w, skin.h))
        return NSImage(size: NSSize(width: size, height: size), flipped: true) { _ in
            guard let g = NSGraphicsContext.current?.cgContext else { return false }
            crisp(g)
            let sw = skin.w * scale, sh = skin.h * scale
            drawSprite(g, skin, rows, (size - sw) / 2, (size - sh) / 2, scale)
            return true
        }
    }
}
