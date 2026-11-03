import AppKit

// A skin is a plain text file: a palette (one character = one colour) and a set of named
// frames where every character is a single pixel. Users drop *.skin files into
// ~/Library/Application Support/TwentyTwentyTwenty/skins and pick them from the menu bar.
//
//   name: Tabby
//   palette:
//   K #1b1b2f
//   O #f0a04b
//   frame idle:
//   ....KK....
//   (rows...)
//
// Frame keys the app looks for (all optional except idle):
//   idle, blink, far, happy, wide, sleep   and an optional "<key>2" alternate for animation
//   (e.g. idle2 with the tail in a different position). Missing frames fall back to idle.
//
// The format is identical to the Windows build, so skin files can be shared between the two.

struct SkinError: LocalizedError {
    let message: String
    init(_ m: String) { message = m }
    var errorDescription: String? { message }
}

final class Skin {
    var name = "Unnamed"
    var source: String?                     // file path, or nil for built-in
    var w = 0, h = 0
    private(set) var palette: [Character: NSColor] = [:]
    private(set) var paletteOrder: [Character] = []
    private(set) var frames: [String: [[Character]]] = [:]   // keys lowercased
    private(set) var frameOrder: [String] = []

    static let dir = Settings.dir + "/skins"
    static let ext = ".skin"

    func frame(_ key: String, alt: Bool) -> [[Character]] {
        let k = key.lowercased()
        if alt, let r = frames[k + "2"] { return r }
        if let r = frames[k] { return r }
        if k == "sleep", let r = frames["blink"] { return r }
        return frames["idle"]!
    }

    func color(_ c: Character) -> NSColor? { palette[c] }

    private func setPalette(_ c: Character, _ color: NSColor) {
        if palette[c] == nil { paletteOrder.append(c) }
        palette[c] = color
    }

    private func setFrame(_ key: String, _ rows: [[Character]]) {
        let k = key.lowercased()
        if frames[k] == nil { frameOrder.append(k) }
        frames[k] = rows
    }

    // ---------- parsing ----------

    static func parse(_ text: String, source: String?) throws -> Skin {
        let skin = Skin()
        skin.source = source
        var section: String? = nil
        var rows: [[Character]]? = nil

        func flush() {
            if let s = section, s.hasPrefix("frame "), let r = rows, !r.isEmpty {
                skin.setFrame(String(s.dropFirst(6)).trimmed, r)
            }
            rows = nil
        }

        for raw in text.replacingOccurrences(of: "\r", with: "").components(separatedBy: "\n") {
            let line = raw.trimmedEnd
            if line.isEmpty || line.hasPrefix("#") { continue }
            let lower = line.lowercased()

            if lower.hasPrefix("name:") { skin.name = String(line.dropFirst(5)).trimmed; continue }
            if lower == "palette:" { flush(); section = "palette"; continue }
            if lower.hasPrefix("frame ") && lower.hasSuffix(":") {
                flush()
                section = String(line.dropLast()).trimmed
                rows = []
                continue
            }

            if section == "palette" {
                let parts = line.trimmed.split(whereSeparator: { $0 == " " || $0 == "\t" || $0 == "=" })
                if parts.count >= 2, parts[0].count == 1 {
                    let color = try parseColor(String(parts[1]))
                    skin.setPalette(parts[0].first!, color)
                }
            } else if rows != nil {
                rows!.append(Array(line.trimmed))
            }
        }
        flush()

        guard let idle = skin.frames["idle"] else { throw SkinError("Skin needs at least a 'frame idle:' section.") }
        skin.w = idle[0].count
        skin.h = idle.count
        if skin.w < 4 || skin.h < 4 || skin.w > 64 || skin.h > 64 {
            throw SkinError("Frames must be between 4x4 and 64x64 pixels.")
        }
        for (key, f) in skin.frames {
            if f.count != skin.h { throw SkinError("Frame '\(key)' has \(f.count) rows, expected \(skin.h).") }
            for r in f where r.count != skin.w {
                throw SkinError("Frame '\(key)' has a row of width \(r.count), expected \(skin.w).")
            }
        }
        return skin
    }

    static func parseColor(_ raw: String) throws -> NSColor {
        var s = raw.hasPrefix("#") ? String(raw.dropFirst()) : raw
        if s.count == 3 {
            let c = Array(s)
            s = "\(c[0])\(c[0])\(c[1])\(c[1])\(c[2])\(c[2])"
        }
        if s.count == 6 { s = "FF" + s }
        guard s.count == 8, let v = UInt32(s, radix: 16) else {
            throw SkinError("Bad colour '\(raw)' (use #RRGGBB or #AARRGGBB).")
        }
        let a = CGFloat((v >> 24) & 0xFF) / 255, r = CGFloat((v >> 16) & 0xFF) / 255
        let g = CGFloat((v >> 8) & 0xFF) / 255, b = CGFloat(v & 0xFF) / 255
        return NSColor(srgbRed: r, green: g, blue: b, alpha: a)
    }

    func serialize() -> String {
        var out: [String] = []
        out.append("# 20-20-20 skin. One character = one pixel. '.' (or any unlisted character) is transparent.")
        out.append("# Frames: idle (required), blink, far, happy, wide, sleep. Add '<name>2' for a second animation frame.")
        out.append("# All frames must be the same size. Any size from 4x4 to 64x64 works.")
        out.append("name: " + name)
        out.append("")
        out.append("palette:")
        for c in paletteOrder {
            let col = (palette[c] ?? .black).usingColorSpace(.sRGB) ?? .black
            let r = Int(round(col.redComponent * 255)), g = Int(round(col.greenComponent * 255)), b = Int(round(col.blueComponent * 255))
            out.append("\(c) #" + String(format: "%02x%02x%02x", r, g, b))
        }
        for key in frameOrder {
            out.append("")
            out.append("frame \(key):")
            for r in frames[key] ?? [] { out.append(String(r)) }
        }
        return out.joined(separator: "\n") + "\n"
    }

    // ---------- discovery ----------

    static func loadByName(_ name: String) -> Skin {
        for s in BuiltIn.all where s.name.caseInsensitiveCompare(name) == .orderedSame { return s }
        let path = dir + "/" + name + ext
        if FileManager.default.fileExists(atPath: path),
           let text = try? String(contentsOfFile: path, encoding: .utf8),
           let skin = try? parse(text, source: path) {
            return skin
        }
        return BuiltIn.all[0]
    }

    static func customSkinNames() -> [String] {
        let files = (try? FileManager.default.contentsOfDirectory(atPath: dir)) ?? []
        return files.filter { $0.lowercased().hasSuffix(ext) }
            .map { String($0.dropLast(ext.count)) }
            .sorted { $0.caseInsensitiveCompare($1) == .orderedAscending }
    }

    // Creates the skins folder with every built-in exported as an editable template.
    static func ensureTemplates() {
        let fm = FileManager.default
        try? fm.createDirectory(atPath: dir, withIntermediateDirectories: true)
        for s in BuiltIn.all {
            let p = dir + "/_template-" + s.name + ext
            if !fm.fileExists(atPath: p) { try? s.serialize().write(toFile: p, atomically: true, encoding: .utf8) }
        }
        let readme = dir + "/README.txt"
        if !fm.fileExists(atPath: readme) {
            let text =
                "Custom skins for 20-20-20\n\n" +
                "1. Copy one of the _template-*.skin files and rename it, e.g. MyCat.skin\n" +
                "2. Open it in any text editor. Each character is one pixel.\n" +
                "3. Change colours in the palette section (#RRGGBB) or add new letters.\n" +
                "4. Edit the frames. Every frame must have the same width and height.\n" +
                "5. Click the cat in the menu bar > Skin > pick your file.\n\n" +
                "Files starting with _template- are regenerated if deleted; make a copy instead of editing them.\n" +
                "Skin files are compatible with the Windows version of 20-20-20.\n"
            try? text.write(toFile: readme, atomically: true, encoding: .utf8)
        }
    }
}

// Built-in skins, defined with the same text format so templates and code never drift.
enum BuiltIn {
    // Palettes: K outline, O fur, L light fur (muzzle/belly), P pink, E eye, W eye highlight
    private static let tabby    = "K #1b1b2f\nO #f0a04b\nL #fad38a\nP #f28ca8\nE #2a2a40\nW #ffffff"
    private static let midnight = "K #0d0d18\nO #2e2e48\nL #4a4a6a\nP #f28ca8\nE #5ef0a0\nW #ffffff"
    private static let snow     = "K #3a3a4a\nO #f4f4f8\nL #e2e2ea\nP #f2a0b8\nE #4a90e2\nW #ffffff"
    private static let matcha   = "K #1b2f24\nO #7fcf9a\nL #c6f0d2\nP #f2a0b8\nE #2a2a40\nW #ffffff"

    private static let base: [String] = [
        "...KK..........KK...",
        "..KOOK........KOOK..",
        "..KOPOK......KOPOK..",
        "..KOPPOKKKKKKOPPOK..",
        "..KOOOOOOOOOOOOOOK..",
        ".KOOOOOOOOOOOOOOOOK.",
        ".KOOOOOOOOOOOOOOOOK.",   // 6
        ".KOOEEOOOOOOOOEEOOK.",   // 7  eyes
        ".KOOEWOOOOOOOOWEOOK.",   // 8  eyes
        ".KOOEEOOOLLOOOEEOOK.",   // 9  eyes + muzzle
        ".KOOOOOOLPLOOOOOOOK.",   // 10 nose
        ".KOOOOOOKLLKOOOOOOK.",   // 11 mouth
        "..KOOOOOOOOOOOOOOK..",
        "..KKOOOOOOOOOOOOKK..",   // 13
        ".KOOOLLLLLLLLLLOOOK.",   // 14
        ".KOOOLLLLLLLLLLOOOKK",   // 15 tail
        ".KOOOLLLLLLLLLLOOOOK",   // 16 tail
        ".KOOOLLLLLLLLLLOOOOK",   // 17 tail
        ".KOOKKOOOOOOKKOOOKK.",   // 18
        ".KKK..KKKKKK..KKKK..",
    ]

    static let all: [Skin] = [cat("Tabby", tabby), cat("Midnight", midnight), cat("Snow", snow), cat("Matcha", matcha)]

    private static func patch(_ src: [String], _ firstRow: Int, _ rows: String...) -> [String] {
        var r = src
        for (i, row) in rows.enumerated() { r[firstRow + i] = row }
        return r
    }

    private static func cat(_ name: String, _ palette: String) -> Skin {
        let idle = base
        // Tail swings up on the alternate frame.
        let idleAlt = patch(base, 12,
            "..KOOOOOOOOOOOOOOK.K",
            "..KKOOOOOOOOOOOOKKOK",
            ".KOOOLLLLLLLLLLOOOKK",
            ".KOOOLLLLLLLLLLOOOK.",
            ".KOOOLLLLLLLLLLOOOK.",
            ".KOOOLLLLLLLLLLOOOK.")
        let blink = patch(base, 7,
            ".KOOOOOOOOOOOOOOOOK.",
            ".KOOKKOOOOOOOOKKOOK.",
            ".KOOOOOOOLLOOOOOOOK.")
        // Gazing up into the distance: eyes shift one row up.
        let far = patch(base, 6,
            ".KOOEEOOOOOOOOEEOOK.",
            ".KOOEWOOOOOOOOWEOOK.",
            ".KOOEEOOOOOOOOEEOOK.",
            ".KOOOOOOOLLOOOOOOOK.")
        let far2 = patch(far, 12, idleAlt[12], idleAlt[13], idleAlt[14], idleAlt[15], idleAlt[16], idleAlt[17])
        // ^ ^ eyes
        let happy = patch(base, 7,
            ".KOOKKOOOOOOOOKKOOK.",
            ".KOKOOKOOOOOOKOOKOK.",
            ".KOOOOOOOLLOOOOOOOK.")
        let happy2 = patch(happy, 12, idleAlt[12], idleAlt[13], idleAlt[14], idleAlt[15], idleAlt[16], idleAlt[17])
        // Wide awake: taller eyes.
        let wide = patch(base, 6,
            ".KOOEEOOOOOOOOEEOOK.",
            ".KOOEWOOOOOOOOWEOOK.",
            ".KOOEEOOOOOOOOEEOOK.",
            ".KOOEEOOOLLOOOEEOOK.")

        var text = "name: \(name)\npalette:\n\(palette)\n"
        add(&text, "idle", idle); add(&text, "idle2", idleAlt); add(&text, "blink", blink)
        add(&text, "far", far); add(&text, "far2", far2); add(&text, "happy", happy); add(&text, "happy2", happy2)
        add(&text, "wide", wide); add(&text, "sleep", blink)
        return try! Skin.parse(text, source: nil)
    }

    private static func add(_ sb: inout String, _ key: String, _ rows: [String]) {
        sb += "frame \(key):\n"
        for r in rows { sb += r + "\n" }
    }
}
