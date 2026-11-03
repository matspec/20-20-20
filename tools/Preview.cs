// Dev helper: renders every built-in skin x expression to a PNG contact sheet.
// Usage: Preview.exe <out.png>
using System.Drawing;
using TwentyTwentyTwenty;

static class Preview
{
    static void Main(string[] args)
    {
        string[] keys = { "idle", "idle2", "blink", "far", "happy", "wide" };
        int scale = 6, pad = 16;
        var skins = BuiltIn.All;
        int cellW = skins[0].W * scale + pad, cellH = skins[0].H * scale + pad + 12;
        using (var bmp = new Bitmap(keys.Length * cellW + pad, skins.Length * cellH + pad))
        using (var g = Graphics.FromImage(bmp))
        {
            PixelRenderer.Crisp(g);
            g.Clear(Palette.Bg);
            for (int r = 0; r < skins.Length; r++)
                for (int c = 0; c < keys.Length; c++)
                {
                    int x = pad + c * cellW, y = pad + r * cellH;
                    PixelRenderer.DrawSprite(g, skins[r], skins[r].Frame(keys[c], false), x, y, scale);
                    PixelFont.Draw(g, keys[c], x, y + skins[r].H * scale + 4, 1, Palette.TextDim);
                }
            bmp.Save(args.Length > 0 ? args[0] : "preview.png");
        }
    }
}
