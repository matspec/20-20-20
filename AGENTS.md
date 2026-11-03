# 20-20-20

Pixel-cat eye-break reminder. Two native, dependency-free builds that share the same
settings.ini keys and the same `.skin` text format.

## Windows (src/, build.cmd)
- C# / WinForms on .NET Framework 4.x, compiled with the csc.exe that ships with Windows.
- `build.cmd` -> `dist\TwentyTwentyTwenty.exe` (portable) and, if Inno Setup 6 is installed, `dist\TwentyTwentyTwenty-Setup.exe`.
- Settings: `%LOCALAPPDATA%\TwentyTwentyTwenty\settings.ini`, skins in `...\skins`.

## macOS (mac/, mac/build.sh)
- Swift / AppKit, compiled with plain `swiftc` from the Xcode Command Line Tools. No Xcode project, no SwiftPM.
- `mac/build.sh` -> universal `mac/dist/20-20-20.app` + `mac/dist/TwentyTwentyTwenty-mac.dmg`.
  `--native` builds only the host arch, `--run` builds native and launches it.
- `.github/workflows/mac.yml` builds the same thing on a macOS runner and uploads the artifacts.
- Settings: `~/Library/Application Support/TwentyTwentyTwenty/settings.ini`, skins in `.../skins`.
- File-to-file mapping with the Windows source: TrayApp.cs = StatusApp.swift, PetForm.cs = PetPanel.swift,
  BreakOverlay.cs = BreakOverlay.swift, the rest share names. Keep the two in sync when changing behaviour.
- All custom views use `isFlipped = true` so drawing code is y-down and identical to the GDI+ version.
- "Open at login" uses SMAppService (macOS 13+); the menu item is hidden on older systems.
- Minimum macOS: 11.0.
