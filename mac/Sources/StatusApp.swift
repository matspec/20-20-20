import AppKit

// Glues everything together: engine (logic), menu bar icon + menu, desktop pet, break overlays.
final class StatusApp: NSObject, NSMenuDelegate {
    private let settings: Settings
    private let engine: ReminderEngine
    private let statusItem: NSStatusItem
    private let menu = NSMenu()
    private let skinMenu = NSMenu(title: "Skin")
    private let statusLine = NSMenuItem(title: "", action: nil, keyEquivalent: "")
    private var pauseItem: NSMenuItem!
    private var petItem: NSMenuItem!
    private var renameItem: NSMenuItem!
    private var iconActive: NSImage!
    private var iconPaused: NSImage!
    private var skin: Skin
    private var pet: PetPanel?
    private var breakSession: BreakSession?

    override init() {
        let firstRun = !Settings.exists
        settings = Settings.load()
        engine = ReminderEngine(settings: settings)
        skin = Skin.loadByName(settings.skinName)
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        super.init()

        rebuildIcons()
        buildMenu()
        statusItem.menu = menu
        statusItem.button?.imageScaling = .scaleNone

        engine.onChanged = { [weak self] in
            guard let self = self else { return }
            self.updateUi()
            self.pet?.engineChanged()
        }
        engine.onBreakRequested = { [weak self] in self?.startBreak() }
        updateUi()

        if firstRun {
            promptName(welcome: true)
            settings.save()
            refreshNames()
        }
        recreatePet()
    }

    // ---------- menu ----------

    private func buildMenu() {
        menu.delegate = self
        menu.autoenablesItems = false
        statusLine.isEnabled = false
        pauseItem = NSMenuItem.make("Pause") { [weak self] in self?.engine.togglePause() }
        let breakNow = NSMenuItem.make("Take a break now") { [weak self] in self?.engine.requestBreak() }
        let reset = NSMenuItem.make("Reset timer") { [weak self] in self?.engine.reset() }

        let interval = choiceMenu("Work interval", [10, 15, 20, 25, 30, 45, 60].map { ("\($0) min", $0) }, current: settings.workMinutes) { [weak self] m in
            guard let self = self else { return }
            self.settings.workMinutes = m; self.settings.save()
            self.engine.intervalChanged()
        }
        let breakLen = choiceMenu("Break length", [20, 30, 45, 60].map { ("\($0) sec", $0) }, current: settings.breakSeconds) { [weak self] s in
            guard let self = self else { return }
            self.settings.breakSeconds = s; self.settings.save()
        }
        let petSize = choiceMenu("Pet size", [("Small", 3), ("Medium", 4), ("Large", 6), ("Huge", 8)], current: settings.petScale) { [weak self] sc in
            guard let self = self else { return }
            self.settings.petScale = sc; self.settings.petPlaced = false; self.settings.save()
            self.recreatePet()
        }

        petItem = NSMenuItem.make("") { [weak self] in
            guard let self = self else { return }
            self.settings.showPet.toggle(); self.settings.save()
            self.petItem.isChecked = self.settings.showPet
            self.recreatePet()
        }
        petItem.isChecked = settings.showPet

        renameItem = NSMenuItem.make("") { [weak self] in
            guard let self = self else { return }
            self.promptName(welcome: false)
            self.settings.save()
            self.refreshNames()
        }

        let skinItem = NSMenuItem(title: "Skin", action: nil, keyEquivalent: "")
        skinMenu.delegate = self
        skinItem.submenu = skinMenu
        populateSkinMenu()

        let fullScreenItem = NSMenuItem.make("Full-screen break (all displays)") { [weak self] in
            guard let self = self else { return }
            self.settings.fullScreen.toggle(); self.settings.save()
        }
        fullScreenItem.isChecked = settings.fullScreen
        bindToggle(fullScreenItem) { [weak self] in self?.settings.fullScreen ?? false }

        let soundItem = NSMenuItem.make("Play sound") { [weak self] in
            guard let self = self else { return }
            self.settings.playSound.toggle(); self.settings.save()
        }
        soundItem.isChecked = settings.playSound
        bindToggle(soundItem) { [weak self] in self?.settings.playSound ?? false }

        let exit = NSMenuItem.make("Quit \(App.title)") { NSApp.terminate(nil) }

        menu.addItem(statusLine)
        menu.addItem(.separator())
        menu.addItem(pauseItem); menu.addItem(breakNow); menu.addItem(reset)
        menu.addItem(.separator())
        menu.addItem(interval); menu.addItem(breakLen)
        menu.addItem(.separator())
        menu.addItem(petItem); menu.addItem(petSize); menu.addItem(skinItem); menu.addItem(renameItem)
        menu.addItem(fullScreenItem); menu.addItem(soundItem)
        if Settings.startupSupported {
            let startupItem = NSMenuItem.make("Open at login") {
                Settings.setStartup(!Settings.isStartupEnabled())
            }
            startupItem.isChecked = Settings.isStartupEnabled()
            bindToggle(startupItem) { Settings.isStartupEnabled() }
            menu.addItem(startupItem)
        }
        menu.addItem(.separator())
        menu.addItem(exit)
        refreshNames()
    }

    // Menu items whose checkmark mirrors a setting: re-read it every time the menu opens.
    private var toggles: [(NSMenuItem, () -> Bool)] = []
    private func bindToggle(_ item: NSMenuItem, _ read: @escaping () -> Bool) { toggles.append((item, read)) }

    private func choiceMenu(_ title: String, _ options: [(String, Int)], current: Int, apply: @escaping (Int) -> Void) -> NSMenuItem {
        let sub = NSMenu(title: title)
        for (label, value) in options {
            let it = NSMenuItem.make(label) { [weak sub] in
                apply(value)
                sub?.items.forEach { $0.isChecked = $0.tag == value }
            }
            it.tag = value
            it.isChecked = value == current
            sub.addItem(it)
        }
        let parent = NSMenuItem(title: title, action: nil, keyEquivalent: "")
        parent.submenu = sub
        return parent
    }

    func menuNeedsUpdate(_ m: NSMenu) {
        if m === skinMenu { populateSkinMenu() }
        else if m === menu { for (item, read) in toggles { item.isChecked = read() } }
    }

    private func refreshNames() {
        petItem.title = "Show \(settings.catName) on desktop"
        renameItem.title = "Rename \(settings.catName)..."
        updateUi()
        pet?.petView.needsDisplay = true
    }

    private func promptName(welcome: Bool) {
        NSApp.activate(ignoringOtherApps: true)
        let a = NSAlert()
        a.alertStyle = .informational
        a.icon = PixelRenderer.image(skin, skin.frame("happy", alt: false), size: 64)
        if welcome {
            a.messageText = "Meet your eye-care buddy"
            a.informativeText = "A pixel cat will live in your menu bar (and on your desktop) and remind you to look 20 feet away for 20 seconds, every 20 minutes.\n\nWhat would you like to call it? Everything else can be changed from the menu bar icon."
        } else {
            a.messageText = "Rename your cat"
            a.informativeText = "What should the cat be called?"
        }
        let field = NSTextField(frame: NSRect(x: 0, y: 0, width: 220, height: 24))
        field.stringValue = settings.catName
        field.placeholderString = "Mochi"
        a.accessoryView = field
        a.addButton(withTitle: "OK")
        if !welcome { a.addButton(withTitle: "Cancel") }
        a.window.initialFirstResponder = field
        if a.runModal() == .alertFirstButtonReturn {
            let n = field.stringValue.trimmed
            if !n.isEmpty { settings.catName = String(n.prefix(16)) }
        }
    }

    // ---------- skins ----------

    private func populateSkinMenu() {
        skinMenu.removeAllItems()
        for s in BuiltIn.all { addSkinItem(s.name) }
        let custom = Skin.customSkinNames().filter { !$0.hasPrefix("_template-") }
        if !custom.isEmpty {
            skinMenu.addItem(.separator())
            for name in custom { addSkinItem(name) }
        }
        skinMenu.addItem(.separator())
        skinMenu.addItem(NSMenuItem.make("Open skins folder (make your own)...") {
            Skin.ensureTemplates()
            NSWorkspace.shared.open(URL(fileURLWithPath: Skin.dir, isDirectory: true))
        })
    }

    private func addSkinItem(_ name: String) {
        let it = NSMenuItem.make(name) { [weak self] in self?.applySkin(name) }
        it.isChecked = settings.skinName.caseInsensitiveCompare(name) == .orderedSame
        skinMenu.addItem(it)
    }

    private func applySkin(_ name: String) {
        let path = Skin.dir + "/" + name + Skin.ext
        let loaded: Skin
        do {
            if FileManager.default.fileExists(atPath: path) {
                let text = try String(contentsOfFile: path, encoding: .utf8)
                loaded = try Skin.parse(text, source: path)
            } else {
                loaded = Skin.loadByName(name)
            }
        } catch {
            Native.alert("Couldn't load skin '\(name)':\n\n\(error.localizedDescription)")
            return
        }
        skin = loaded
        settings.skinName = name; settings.save()
        rebuildIcons()
        updateUi()
        recreatePet()
    }

    private func rebuildIcons() {
        iconActive = PixelRenderer.image(skin, skin.frame("idle", alt: false), size: 20)
        iconPaused = PixelRenderer.image(skin, skin.frame("sleep", alt: false), size: 20)
    }

    // ---------- pet / break ----------

    private func recreatePet() {
        pet?.dispose()
        pet = nil
        guard settings.showPet else { return }
        let p = PetPanel(settings: settings, engine: engine, skin: skin, menu: menu)
        p.petView.onPauseToggled = { [weak self] in self?.engine.togglePause() }
        p.orderFrontRegardless()
        pet = p
    }

    private func startBreak() {
        if breakSession != nil { return }
        pet?.orderOut(nil)
        breakSession = BreakSession(settings: settings, skin: skin) { [weak self] completed in
            guard let self = self else { return }
            self.breakSession = nil
            self.engine.completeBreak(completed)
            self.pet?.orderFrontRegardless()
        }
    }

    private func updateUi() {
        let mood = engine.mood
        statusItem.button?.image = (mood == .sleeping || mood == .away) ? iconPaused : iconActive
        pauseItem.title = settings.paused ? "Resume" : "Pause"
        let status = engine.statusText()
        statusLine.title = "\(settings.catName): \(status)"
        statusItem.button?.toolTip = "\(App.title) - \(status)"
    }

    func dispose() {
        breakSession?.dispose()
        pet?.dispose()
        engine.dispose()
        NSStatusBar.system.removeStatusItem(statusItem)
    }
}
