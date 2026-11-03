using System;
using System.Collections.Generic;
using System.Drawing;
using System.Media;
using System.Windows.Forms;

namespace TwentyTwentyTwenty
{
    // One break = one BreakSession, which opens an overlay on every monitor (or a single window
    // in non-full-screen mode) and closes them all together.
    sealed class BreakSession : IDisposable
    {
        readonly Settings settings;
        readonly Action<bool> onDone;
        readonly List<BreakOverlayForm> forms = new List<BreakOverlayForm>();
        readonly Timer second = new Timer { Interval = 1000 };
        readonly Timer anim = new Timer { Interval = 140 };
        bool closed;

        public int SecondsLeft { get; private set; }
        public bool Finished { get { return SecondsLeft <= 0; } }
        public int Frame { get; private set; }
        public bool Alt { get; private set; }
        public Skin Skin { get; private set; }
        public string CatName { get { return settings.CatName; } }
        public int BreakSeconds { get { return settings.BreakSeconds; } }

        public BreakSession(Settings s, Skin skin, Action<bool> done)
        {
            settings = s; Skin = skin; onDone = done;
            SecondsLeft = s.BreakSeconds;

            if (s.FullScreen)
            {
                foreach (var sc in Screen.AllScreens)
                    forms.Add(new BreakOverlayForm(this, sc.Bounds, sc.Primary, false));
            }
            else
            {
                var wa = Screen.PrimaryScreen.WorkingArea;
                var size = new Size(Math.Min(560, wa.Width - 40), Math.Min(380, wa.Height - 40));
                var b = new Rectangle(wa.Left + (wa.Width - size.Width) / 2, wa.Top + (wa.Height - size.Height) / 2, size.Width, size.Height);
                forms.Add(new BreakOverlayForm(this, b, true, true));
            }

            second.Tick += (o, e) => Tick();
            anim.Tick += (o, e) => { Frame++; if (Frame % 3 == 0) Alt = !Alt; Redraw(); };

            // Show secondaries first so the primary ends up on top with focus.
            foreach (var f in forms) if (!f.IsPrimary) f.Show();
            foreach (var f in forms) if (f.IsPrimary) { f.Show(); f.Activate(); }

            if (s.PlaySound) SystemSounds.Asterisk.Play();
            second.Start(); anim.Start();
        }

        void Tick()
        {
            if (SecondsLeft <= 0) return;
            SecondsLeft--;
            if (SecondsLeft == 0)
            {
                second.Stop();
                if (settings.PlaySound) SystemSounds.Beep.Play();
            }
            Redraw();
        }

        void Redraw()
        {
            foreach (var f in forms)
            {
                if (Frame % 7 == 0) Native.KeepOnTop(f.Handle);
                f.Invalidate();
            }
        }

        public void End(bool completed)
        {
            if (closed) return;
            closed = true;
            second.Stop(); anim.Stop();
            foreach (var f in forms) { f.Close(); f.Dispose(); }
            forms.Clear();
            onDone(completed);
        }

        public void Dispose() { End(false); second.Dispose(); anim.Dispose(); }
    }

    sealed class BreakOverlayForm : Form
    {
        readonly BreakSession session;
        readonly bool windowed;
        Rectangle doneRect, skipRect;
        public bool IsPrimary { get; private set; }

        public BreakOverlayForm(BreakSession s, Rectangle bounds, bool primary, bool isWindowed)
        {
            session = s; IsPrimary = primary; windowed = isWindowed;
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.Manual;
            Bounds = bounds;
            TopMost = true;
            ShowInTaskbar = false;
            DoubleBuffered = true;
            BackColor = Palette.Bg;
            KeyPreview = true;
            Cursor = Cursors.Default;
            if (!primary) Opacity = 0.92;

            KeyDown += (o, e) =>
            {
                if (e.KeyCode == Keys.Escape) session.End(false);
                else if ((e.KeyCode == Keys.Enter || e.KeyCode == Keys.Space) && session.Finished) session.End(true);
            };
            MouseClick += (o, e) =>
            {
                if (!IsPrimary) return;
                if (session.Finished && doneRect.Contains(e.Location)) session.End(true);
                else if (skipRect.Contains(e.Location)) session.End(false);
            };
            MouseMove += (o, e) =>
            {
                Cursor = IsPrimary && (skipRect.Contains(e.Location) || (session.Finished && doneRect.Contains(e.Location)))
                    ? Cursors.Hand : Cursors.Default;
            };
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            Native.KeepOnTop(Handle);
            if (IsPrimary) { BringToFront(); Activate(); }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics;
            PixelRenderer.Crisp(g);
            var r = ClientRectangle;
            g.Clear(Palette.Bg);
            PixelRenderer.DrawGrid(g, r, Math.Max(24, r.Height / 24));

            if (IsPrimary) PaintPrimary(g, r); else PaintSecondary(g, r);

            PixelRenderer.DrawScanlines(g, r, 60);
            if (windowed)
                using (var p = new Pen(Palette.AccentDim, 2)) g.DrawRectangle(p, 1, 1, r.Width - 3, r.Height - 3);
        }

        void PaintPrimary(Graphics g, Rectangle r)
        {
            // Scale everything off the shorter edge so it looks the same on a 1080p or a 4K panel.
            int unit = Math.Max(2, Math.Min(r.Width, r.Height) / 160);   // base pixel size
            int catScale = unit * 3;
            int fsBig = unit * 4, fsMid = unit, fsSmall = Math.Max(1, unit * 3 / 4);
            bool done = session.Finished;

            // Cat, gazing into the distance (or beaming when done).
            var skin = session.Skin;
            catScale = Math.Max(2, catScale * 16 / Math.Max(skin.W, skin.H));
            string key = done ? "happy" : (session.Frame / 20 % 5 == 0 ? "blink" : "far");
            int catW = skin.W * catScale, catH = skin.H * catScale;
            int cx = r.Left + (r.Width - catW) / 2;
            int cy = r.Top + r.Height / 2 - catH - unit * 6 + (done ? (session.Frame % 2) * unit : 0);
            PixelRenderer.DrawSprite(g, skin, skin.Frame(key, session.Alt), cx, cy, catScale);

            // Speech bubble.
            string say = done ? "that's it! back to work?" : "look at something far away...";
            int px = unit;
            int tw = PixelFont.Measure(say, fsMid);
            int bw = tw + 6 * px, bh = PixelFont.H * fsMid + 6 * px;
            int bx = r.Left + (r.Width - bw) / 2, by = cy - bh - 4 * px;
            PixelRenderer.DrawBubble(g, new Rectangle(bx, by, bw, bh), px, cx + catW / 2 - px);
            PixelFont.Draw(g, say, bx + 3 * px, by + 3 * px, fsMid, Palette.Text);

            // Big countdown.
            string big = done ? "DONE" : session.SecondsLeft.ToString("00");
            int bigW = PixelFont.Measure(big, fsBig);
            int bigY = cy + catH + unit * 6;
            PixelFont.Draw(g, big, r.Left + (r.Width - bigW) / 2, bigY, fsBig, Palette.Accent);

            // Progress bar (pixel segments).
            int segs = 20, segW = unit * 3, gap = unit;
            int barW = segs * segW + (segs - 1) * gap;
            int barX = r.Left + (r.Width - barW) / 2, barY = bigY + PixelFont.H * fsBig + unit * 4;
            int lit = done ? segs : (int)Math.Round(segs * (1.0 - (double)session.SecondsLeft / session.BreakSeconds));
            using (var on = new SolidBrush(Palette.Accent))
            using (var off = new SolidBrush(Palette.Grid))
                for (int i = 0; i < segs; i++)
                    g.FillRectangle(i < lit ? on : off, barX + i * (segW + gap), barY, segW, unit * 2);

            // Caption.
            string cap = done ? session.CatName.ToUpperInvariant() + " APPROVES" : "20 FEET  /  20 SECONDS  /  EVERY 20 MINUTES";
            int cw = PixelFont.Measure(cap, fsSmall);
            PixelFont.Draw(g, cap, r.Left + (r.Width - cw) / 2, barY + unit * 6, fsSmall, Palette.TextDim);

            // Buttons.
            int btnH = PixelFont.H * fsMid + 6 * unit, btnY = r.Bottom - btnH - unit * 8;
            int doneW = PixelFont.Measure("DONE", fsMid) + 10 * unit, skipW = PixelFont.Measure("SKIP", fsMid) + 10 * unit;
            int totalW = doneW + skipW + unit * 4;
            doneRect = new Rectangle(r.Left + (r.Width - totalW) / 2, btnY, doneW, btnH);
            skipRect = new Rectangle(doneRect.Right + unit * 4, btnY, skipW, btnH);
            DrawButton(g, doneRect, "DONE", fsMid, unit, done ? Palette.Accent : Palette.Grid, done ? Palette.Bg : Palette.TextDim);
            DrawButton(g, skipRect, "SKIP", fsMid, unit, Palette.Bubble, Palette.TextDim);

            string hint = done ? "ENTER TO CONTINUE" : "ESC TO SKIP";
            int hw = PixelFont.Measure(hint, fsSmall);
            PixelFont.Draw(g, hint, r.Left + (r.Width - hw) / 2, r.Bottom - unit * 5, fsSmall, Palette.TextDim);
        }

        void PaintSecondary(Graphics g, Rectangle r)
        {
            int unit = Math.Max(2, Math.Min(r.Width, r.Height) / 160);
            int fsBig = unit * 3, fsSmall = unit;
            string big = session.Finished ? "DONE" : session.SecondsLeft.ToString("00");
            int bw = PixelFont.Measure(big, fsBig);
            PixelFont.Draw(g, big, r.Left + (r.Width - bw) / 2, r.Top + r.Height / 2 - PixelFont.H * fsBig, fsBig, Palette.Accent);
            string s = session.Finished ? "NICE" : "LOOK AWAY FROM THE SCREEN";
            int sw = PixelFont.Measure(s, fsSmall);
            PixelFont.Draw(g, s, r.Left + (r.Width - sw) / 2, r.Top + r.Height / 2 + unit * 4, fsSmall, Palette.TextDim);
        }

        static void DrawButton(Graphics g, Rectangle rect, string text, int fs, int px, Color fill, Color fg)
        {
            using (var b = new SolidBrush(fill))
            {
                g.FillRectangle(b, rect.X + px, rect.Y, rect.Width - 2 * px, rect.Height);
                g.FillRectangle(b, rect.X, rect.Y + px, rect.Width, rect.Height - 2 * px);
            }
            int tw = PixelFont.Measure(text, fs);
            PixelFont.Draw(g, text, rect.X + (rect.Width - tw) / 2, rect.Y + (rect.Height - PixelFont.H * fs) / 2, fs, fg);
        }
    }
}
