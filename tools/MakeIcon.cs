// Generates app.ico (16/32/48/256 px, PNG-compressed) from the cat sprite in src/Pixel.cs.
// Compiled together with src/Pixel.cs and src/Native.cs by build.cmd.
// Usage: MakeIcon.exe <output.ico>
using System;
using System.Drawing.Imaging;
using System.IO;
using TwentyTwentyTwenty;

static class MakeIcon
{
    static void Main(string[] args)
    {
        string outPath = args.Length > 0 ? args[0] : "app.ico";
        int[] sizes = { 16, 32, 48, 256 };
        var images = new byte[sizes.Length][];
        var skin = BuiltIn.All[0];
        var rows = skin.Frame("idle", false);
        for (int i = 0; i < sizes.Length; i++)
            using (var icon = PixelRenderer.ToIcon(skin, rows, sizes[i]))
            using (var bmp = icon.ToBitmap())
            using (var ms = new MemoryStream()) { bmp.Save(ms, ImageFormat.Png); images[i] = ms.ToArray(); }

        using (var fs = File.Create(outPath))
        using (var w = new BinaryWriter(fs))
        {
            w.Write((short)0); w.Write((short)1); w.Write((short)sizes.Length);
            int offset = 6 + 16 * sizes.Length;
            for (int i = 0; i < sizes.Length; i++)
            {
                byte dim = (byte)(sizes[i] == 256 ? 0 : sizes[i]);
                w.Write(dim); w.Write(dim); w.Write((byte)0); w.Write((byte)0);
                w.Write((short)1); w.Write((short)32);
                w.Write(images[i].Length); w.Write(offset);
                offset += images[i].Length;
            }
            foreach (var img in images) w.Write(img);
        }
        Console.WriteLine("Wrote " + outPath);

        // Installer wizard artwork (Inno Setup wants BMP).
        string dir = Path.GetDirectoryName(Path.GetFullPath(outPath));
        WizardImage(Path.Combine(dir, "wizard.bmp"), 164, 314, 8, true);
        WizardImage(Path.Combine(dir, "wizard-small.bmp"), 55, 58, 3, false);
    }

    static void WizardImage(string path, int w, int h, int scale, bool title)
    {
        using (var bmp = new System.Drawing.Bitmap(w, h))
        using (var g = System.Drawing.Graphics.FromImage(bmp))
        {
            PixelRenderer.Crisp(g);
            g.Clear(Palette.Bg);
            PixelRenderer.DrawGrid(g, new System.Drawing.Rectangle(0, 0, w, h), 16);
            var skin = BuiltIn.All[0];
            scale = Math.Max(1, scale * 16 / skin.W);
            int cw = skin.W * scale, ch = skin.H * scale;
            int cy = title ? h / 2 - ch / 2 - 20 : (h - ch) / 2;
            PixelRenderer.DrawSprite(g, skin, skin.Frame("far", true), (w - cw) / 2, cy, scale);
            if (title)
            {
                int fs = 3; string t = "20-20-20";
                PixelFont.Draw(g, t, (w - PixelFont.Measure(t, fs)) / 2, cy + ch + 24, fs, Palette.Accent);
                fs = 1; t = "REST YOUR EYES";
                PixelFont.Draw(g, t, (w - PixelFont.Measure(t, fs)) / 2, cy + ch + 48, fs, Palette.TextDim);
            }
            PixelRenderer.DrawScanlines(g, new System.Drawing.Rectangle(0, 0, w, h), 50);
            bmp.Save(path, ImageFormat.Bmp);
        }
        Console.WriteLine("Wrote " + path);
    }
}
