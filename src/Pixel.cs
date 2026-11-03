using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

namespace TwentyTwentyTwenty
{
    // UI palette, a tiny 3x5 pixel font and a nearest-neighbour sprite renderer.
    // Character-to-colour mapping for sprites lives in the Skin.
    static class Palette
    {
        public static readonly Color Bg = Color.FromArgb(11, 15, 20);
        public static readonly Color Grid = Color.FromArgb(22, 30, 40);
        public static readonly Color Accent = Color.FromArgb(94, 240, 160);
        public static readonly Color AccentDim = Color.FromArgb(40, 120, 85);
        public static readonly Color Text = Color.FromArgb(225, 232, 240);
        public static readonly Color TextDim = Color.FromArgb(120, 135, 150);
        public static readonly Color Bubble = Color.FromArgb(28, 36, 48);
        public static readonly Color BubbleBorder = Color.FromArgb(94, 240, 160);
        public static readonly Color Transparent = Color.FromArgb(255, 0, 255); // TransparencyKey
    }

    static class PixelFont
    {
        public const int W = 3, H = 5;
        static readonly Dictionary<char, string[]> G = new Dictionary<char, string[]>
        {
            {'A', new[]{"010","101","111","101","101"}}, {'B', new[]{"110","101","110","101","110"}},
            {'C', new[]{"011","100","100","100","011"}}, {'D', new[]{"110","101","101","101","110"}},
            {'E', new[]{"111","100","110","100","111"}}, {'F', new[]{"111","100","110","100","100"}},
            {'G', new[]{"011","100","101","101","011"}}, {'H', new[]{"101","101","111","101","101"}},
            {'I', new[]{"111","010","010","010","111"}}, {'J', new[]{"001","001","001","101","010"}},
            {'K', new[]{"101","101","110","101","101"}}, {'L', new[]{"100","100","100","100","111"}},
            {'M', new[]{"101","111","111","101","101"}}, {'N', new[]{"110","101","101","101","101"}},
            {'O', new[]{"010","101","101","101","010"}}, {'P', new[]{"110","101","110","100","100"}},
            {'Q', new[]{"010","101","101","011","001"}}, {'R', new[]{"110","101","110","101","101"}},
            {'S', new[]{"011","100","010","001","110"}}, {'T', new[]{"111","010","010","010","010"}},
            {'U', new[]{"101","101","101","101","111"}}, {'V', new[]{"101","101","101","101","010"}},
            {'W', new[]{"101","101","111","111","101"}}, {'X', new[]{"101","101","010","101","101"}},
            {'Y', new[]{"101","101","010","010","010"}}, {'Z', new[]{"111","001","010","100","111"}},
            {'0', new[]{"111","101","101","101","111"}}, {'1', new[]{"010","110","010","010","111"}},
            {'2', new[]{"111","001","111","100","111"}}, {'3', new[]{"111","001","111","001","111"}},
            {'4', new[]{"101","101","111","001","001"}}, {'5', new[]{"111","100","111","001","111"}},
            {'6', new[]{"111","100","111","101","111"}}, {'7', new[]{"111","001","001","001","001"}},
            {'8', new[]{"111","101","111","101","111"}}, {'9', new[]{"111","101","111","001","111"}},
            {':', new[]{"000","010","000","010","000"}}, {'!', new[]{"010","010","010","000","010"}},
            {'.', new[]{"000","000","000","000","010"}}, {'-', new[]{"000","000","111","000","000"}},
            {'\'',new[]{"010","010","000","000","000"}}, {',', new[]{"000","000","000","010","100"}},
            {'?', new[]{"111","001","011","000","010"}}, {'/', new[]{"001","001","010","100","100"}},
            {'(', new[]{"010","100","100","100","010"}}, {')', new[]{"010","001","001","001","010"}},
            {'%', new[]{"101","001","010","100","101"}}, {'~', new[]{"000","010","101","000","000"}},
            {' ', new[]{"000","000","000","000","000"}},
        };

        public static int Measure(string text, int scale)
        {
            return text.Length == 0 ? 0 : (text.Length * (W + 1) - 1) * scale;
        }

        public static void Draw(Graphics g, string text, int x, int y, int scale, Color color)
        {
            using (var b = new SolidBrush(color))
            {
                foreach (char raw in text.ToUpperInvariant())
                {
                    string[] rows;
                    if (G.TryGetValue(raw, out rows))
                        for (int r = 0; r < H; r++)
                            for (int c = 0; c < W; c++)
                                if (rows[r][c] == '1') g.FillRectangle(b, x + c * scale, y + r * scale, scale, scale);
                    x += (W + 1) * scale;
                }
            }
        }
    }

    static class PixelRenderer
    {
        static readonly Dictionary<string, Bitmap> cache = new Dictionary<string, Bitmap>();

        public static void Crisp(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.None;
            g.InterpolationMode = InterpolationMode.NearestNeighbor;
            g.PixelOffsetMode = PixelOffsetMode.Half;
        }

        public static void DrawSprite(Graphics g, Skin skin, string[] rows, int x, int y, int scale)
        {
            string key = skin.Name + "/" + (skin.Source ?? "") + "|" + string.Join("|", rows) + "@" + scale;
            Bitmap bmp;
            if (!cache.TryGetValue(key, out bmp))
            {
                bmp = Rasterize(skin, rows, scale);
                if (cache.Count > 96) ClearCache();
                cache[key] = bmp;
            }
            g.DrawImageUnscaled(bmp, x, y);
        }

        public static void ClearCache()
        {
            foreach (var b in cache.Values) b.Dispose();
            cache.Clear();
        }

        public static Bitmap Rasterize(Skin skin, string[] rows, int scale)
        {
            int h = rows.Length, w = rows[0].Length;
            var bmp = new Bitmap(Math.Max(1, w * scale), Math.Max(1, h * scale), PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp))
            {
                g.Clear(Color.Transparent);
                for (int r = 0; r < h; r++)
                    for (int c = 0; c < w; c++)
                    {
                        Color col;
                        if (!skin.TryColor(rows[r][c], out col)) continue;
                        using (var b = new SolidBrush(col)) g.FillRectangle(b, c * scale, r * scale, scale, scale);
                    }
            }
            return bmp;
        }

        // Speech bubble with a little pixel tail pointing down to (tailX, bottom).
        public static void DrawBubble(Graphics g, Rectangle r, int px, int tailX)
        {
            using (var fill = new SolidBrush(Palette.Bubble))
            using (var border = new SolidBrush(Palette.BubbleBorder))
            {
                g.FillRectangle(border, r.X, r.Y + px, r.Width, r.Height - 2 * px);
                g.FillRectangle(border, r.X + px, r.Y, r.Width - 2 * px, r.Height);
                g.FillRectangle(fill, r.X + px, r.Y + px, r.Width - 2 * px, r.Height - 2 * px);
                g.FillRectangle(border, tailX - px, r.Bottom, 3 * px, px);
                g.FillRectangle(border, tailX, r.Bottom + px, px, px);
                g.FillRectangle(fill, tailX, r.Bottom - px, px, px);
            }
        }

        public static void DrawScanlines(Graphics g, Rectangle r, int alpha)
        {
            using (var b = new SolidBrush(Color.FromArgb(alpha, 0, 0, 0)))
                for (int y = r.Top; y < r.Bottom; y += 3) g.FillRectangle(b, r.X, y, r.Width, 1);
        }

        public static void DrawGrid(Graphics g, Rectangle r, int step)
        {
            using (var p = new Pen(Palette.Grid))
            {
                for (int x = r.Left; x < r.Right; x += step) g.DrawLine(p, x, r.Top, x, r.Bottom);
                for (int y = r.Top; y < r.Bottom; y += step) g.DrawLine(p, r.Left, y, r.Right, y);
            }
        }

        // Fits a sprite into a square icon of the given pixel size.
        public static Icon ToIcon(Skin skin, string[] rows, int size)
        {
            int scale = Math.Max(1, size / Math.Max(skin.W, skin.H));
            using (var sprite = Rasterize(skin, rows, scale))
            using (var canvas = new Bitmap(size, size, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(canvas))
                {
                    g.Clear(Color.Transparent);
                    g.DrawImageUnscaled(sprite, (size - sprite.Width) / 2, (size - sprite.Height) / 2);
                }
                IntPtr h = canvas.GetHicon();
                try { return (Icon)Icon.FromHandle(h).Clone(); }
                finally { Native.DestroyIcon(h); }
            }
        }
    }
}
