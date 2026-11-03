import Foundation
import ServiceManagement

final class Settings {
    var workMinutes = 20
    var breakSeconds = 20
    var fullScreen = true
    var playSound = true
    var paused = false
    var showPet = true
    var catName = "Mochi"
    var skinName = "Tabby"
    var petScale = 4
    // AppKit window origin (bottom-left, screen coordinates). Can legitimately be negative on
    // multi-monitor setups, so a separate flag records whether the user has placed the pet.
    var petX = 0, petY = 0
    var petPlaced = false

    // ~/Library/Application Support/TwentyTwentyTwenty
    static let dir: String = {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? URL(fileURLWithPath: NSHomeDirectory() + "/Library/Application Support")
        return base.appendingPathComponent(App.name).path
    }()
    static let filePath = dir + "/settings.ini"
    static var exists: Bool { FileManager.default.fileExists(atPath: filePath) }

    static func load() -> Settings {
        let s = Settings()
        guard let text = try? String(contentsOfFile: filePath, encoding: .utf8) else { return s }
        for raw in text.components(separatedBy: .newlines) {
            let line = raw.trimmed
            guard let eq = line.firstIndex(of: "="), eq != line.startIndex,
                  !line.hasPrefix(";"), !line.hasPrefix("[") else { continue }
            let k = String(line[..<eq]).trimmed
            let v = String(line[line.index(after: eq)...]).trimmed
            switch k {
            case "WorkMinutes": if let n = Int(v), n > 0 { s.workMinutes = n }
            case "BreakSeconds": if let n = Int(v), n > 0 { s.breakSeconds = n }
            case "FullScreen": if let b = parseBool(v) { s.fullScreen = b }
            case "PlaySound": if let b = parseBool(v) { s.playSound = b }
            case "Paused": if let b = parseBool(v) { s.paused = b }
            case "ShowPet": if let b = parseBool(v) { s.showPet = b }
            case "CatName": if !v.isEmpty { s.catName = String(v.prefix(16)) }
            case "SkinName": if !v.isEmpty { s.skinName = v }
            case "PetScale": if let n = Int(v) { s.petScale = max(2, min(8, n)) }
            case "PetX": if let n = Int(v) { s.petX = n }
            case "PetY": if let n = Int(v) { s.petY = n }
            case "PetPlaced": if let b = parseBool(v) { s.petPlaced = b }
            default: break
            }
        }
        return s
    }

    // Accepts true/false and 1/0.
    static func parseBool(_ v: String) -> Bool? {
        switch v.lowercased() {
        case "true", "1": return true
        case "false", "0": return false
        default: return nil
        }
    }

    func save() {
        try? FileManager.default.createDirectory(atPath: Settings.dir, withIntermediateDirectories: true)
        let lines = [
            "WorkMinutes=\(workMinutes)",
            "BreakSeconds=\(breakSeconds)",
            "FullScreen=\(fullScreen)",
            "PlaySound=\(playSound)",
            "Paused=\(paused)",
            "ShowPet=\(showPet)",
            "CatName=\(catName)",
            "SkinName=\(skinName)",
            "PetScale=\(petScale)",
            "PetX=\(petX)",
            "PetY=\(petY)",
            "PetPlaced=\(petPlaced)",
        ]
        try? (lines.joined(separator: "\n") + "\n").write(toFile: Settings.filePath, atomically: true, encoding: .utf8)
    }

    // ---- Open at login (SMAppService, macOS 13+; no admin) ----

    static var startupSupported: Bool {
        if #available(macOS 13, *) { return true }
        return false
    }

    static func isStartupEnabled() -> Bool {
        if #available(macOS 13, *) { return SMAppService.mainApp.status == .enabled }
        return false
    }

    static func setStartup(_ enabled: Bool) {
        guard #available(macOS 13, *) else { return }
        do {
            if enabled { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }
        } catch {
            NSLog("Login item change failed: \(error)")
        }
    }
}
