# 20-20-20

A pixel cat that lives in your tray and reminds you to look 20 feet away for 20 seconds, every 20 minutes.

- Desktop pet (draggable, always on top, optional) plus a tray icon with the full menu
- Full-screen break overlay on every monitor, or a small window
- Resets itself when you are idle, locked or asleep
- Custom skins: a plain text file, one character per pixel, shared across all platforms
- No account, no network access, no runtime to install

Website and downloads: see the `website` repo. Windows is available now; macOS and Linux are in progress.

## Building

| Platform | Requirements | Command | Output |
|---|---|---|---|
| Windows | Windows 10/11 (uses the bundled .NET Framework compiler), optional Inno Setup 6 | `build.cmd` | `dist\TwentyTwentyTwenty.exe`, `dist\TwentyTwentyTwenty-Setup.exe` |
| macOS | Xcode Command Line Tools | `mac/build.sh` | `mac/dist/20-20-20.app`, `.dmg` |
| Linux | gcc, pkg-config, GTK 3 dev headers (see `linux/Makefile` for distro package names) | `make -C linux` | `linux/build/twentytwentytwenty` |

GitHub Actions builds the macOS and Linux versions on every push to `mac/` or `linux/`.

## Layout

```
src/        Windows (C#, WinForms)
installer/  Inno Setup script with the first-run wizard
mac/        macOS (Swift, AppKit)
linux/      Linux (C, GTK 3)
tools/      icon and preview generators for Windows
```

The three ports mirror each other file for file; see `AGENTS.md` for the mapping and conventions.

## Skins

Right-click the tray icon > Skin > Open skins folder. Copy a `_template-*.skin`, edit it in any text editor, pick it from the menu.

## License

MIT
