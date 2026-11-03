import AppKit

// Renders the default cat into an .iconset folder (fed to iconutil by build.sh).
// Compiled together with the shared sources so the icon can never drift from the sprite.
@main
struct MakeIcon {
    static func main() {
        let args = CommandLine.arguments
        guard args.count > 1 else {
            FileHandle.standardError.write("usage: MakeIcon <out.iconset>\n".data(using: .utf8)!)
            exit(1)
        }
        let out = args[1]
        try? FileManager.default.createDirectory(atPath: out, withIntermediateDirectories: true)

        let skin = BuiltIn.all[0]
        let rows = skin.frame("idle", alt: false)

        for pt in [16, 32, 128, 256, 512] {
            for mult in [1, 2] {
                let size = pt * mult
                let name = mult == 1 ? "icon_\(pt)x\(pt).png" : "icon_\(pt)x\(pt)@2x.png"
                guard let png = render(skin, rows, size) else { exit(1) }
                do { try png.write(to: URL(fileURLWithPath: out + "/" + name)) } catch { exit(1) }
            }
        }
    }

    static func render(_ skin: Skin, _ rows: [[Character]], _ size: Int) -> Data? {
        guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size, bitsPerSample: 8,
                                         samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                                         colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
              let ctx = NSGraphicsContext(bitmapImageRep: rep) else { return nil }
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = ctx
        let g = ctx.cgContext
        PixelRenderer.crisp(g)
        g.clear(CGRect(x: 0, y: 0, width: size, height: size))
        // y-down like the rest of the renderer
        g.translateBy(x: 0, y: CGFloat(size))
        g.scaleBy(x: 1, y: -1)

        // ~10% padding like other macOS icons; integer scale when there is room, fractional below 32px.
        let cell = max(skin.w, skin.h)
        let avail = Double(size) * 0.86
        let s = size >= 32 ? floor(avail / Double(cell)) : avail / Double(cell)
        let x0 = (Double(size) - s * Double(skin.w)) / 2, y0 = (Double(size) - s * Double(skin.h)) / 2
        for (r, row) in rows.enumerated() {
            for (c, ch) in row.enumerated() {
                guard let col = skin.color(ch) else { continue }
                g.setFillColor(col.cgColor)
                let x = x0 + Double(c) * s, y = y0 + Double(r) * s
                g.fill(CGRect(x: x.rounded(.down), y: y.rounded(.down),
                              width: (x + s).rounded(.down) - x.rounded(.down),
                              height: (y + s).rounded(.down) - y.rounded(.down)))
            }
        }
        NSGraphicsContext.restoreGraphicsState()
        return rep.representation(using: .png, properties: [:])
    }
}
