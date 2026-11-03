using System;
using System.Drawing;
using System.Windows.Forms;

namespace TwentyTwentyTwenty
{
    // The always-on desktop buddy. Frameless, never steals focus, draggable, magenta-keyed transparency
    // (pixel art has no anti-aliasing so colour keying gives clean edges).
    sealed class PetForm : Form
    {
        readonly Settings settings;
        readonly ReminderEngine engine;
        readonly Timer anim = new Timer { Interval = 140 };
        readonly Random rng = new Random();

        Skin skin;
        int scale;
        int frame;
        int blinkFor, farFor, bubbleFor;
        bool alt;
        bool hover, dragging, moved;
        Point dragStart, dragOrigin;
        Mood lastMood;

        public event Action PauseToggled;

        public PetForm(Settings s, ReminderEngine e, Skin sk, ContextMenuStrip menu)
        {
            settings = s; engine = e; skin = sk;
            scale = Math.Max(2, Math.Min(8, s.PetScale));

            FormBorderStyle = FormBorderStyle.None;
            ShowInTaskbar = false;
            TopMost = true;
            StartPosition = FormStartPosition.Manual;
            BackColor = Palette.Transparent;
            TransparencyKey = Palette.Transparent;
            DoubleBuffered = true;
            ContextMenuStrip = menu;
            Size = new Size(Math.Max(60, skin.W + 44) * scale, (skin.H + 16) * scale);
            Location = InitialLocation();

            MouseDown += OnDown; MouseMove += OnMove; MouseUp += OnUp;
            MouseEnter += (o, a) => { hover = true; Invalidate(); };
            MouseLeave += (o, a) => { hover = false; Invalidate(); };
            MouseDoubleClick += (o, a) => { if (a.Button == MouseButtons.Left) { var h = PauseToggled; if (h != null) h(); } };

            engine.Changed += OnEngineChanged;
            lastMood = engine.Mood;
            anim.Tick += (o, a) => Step();
            anim.Start();
            bubbleFor = 30; // say hi on launch
        }

        public void SetSkin(Skin sk) { skin = sk; Invalidate(); }

        protected override bool ShowWithoutActivation { get { return true; } }
        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.ExStyle |= Native.WS_EX_TOOLWINDOW | Native.WS_EX_NOACTIVATE | Native.WS_EX_TOPMOST;
                return cp;
            }
        }

        Point InitialLocation()
        {
            if (settings.PetX >= 0 && settings.PetY >= 0)
            {
                var p = new Point(settings.PetX, settings.PetY);
                foreach (var sc in Screen.AllScreens)
                    if (sc.WorkingArea.Contains(new Rectangle(p, Size))) return p;
            }
            var wa = Screen.PrimaryScreen.WorkingArea;
            return new Point(wa.Right - Width - 24, wa.Bottom - Height + 4 * scale);
        }

        // ---------- animation ----------

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            Native.KeepOnTop(Handle);
        }

        void Step()
        {
            frame++;
            if (frame % 7 == 0 && !dragging) Native.KeepOnTop(Handle); // ~once a second
            var mood = engine.Mood;
            if (blinkFor > 0) blinkFor--;
            if (farFor > 0) farFor--;
            if (bubbleFor > 0) bubbleFor--;

            bool sleepy = mood == Mood.Sleeping || mood == Mood.Away;
            if (!sleepy && blinkFor == 0 && rng.Next(100) < 4) blinkFor = 2;
            // The cat models good behaviour: it periodically gazes into the distance.
            if (mood == Mood.Idle && farFor == 0 && rng.Next(100) < 2) farFor = 12;

            int tailEvery = mood == Mood.Restless || mood == Mood.Happy ? 2 : sleepy ? 12 : 5;
            if (frame % tailEvery == 0) alt = !alt;
            Invalidate();
        }

        void OnEngineChanged()
        {
            var m = engine.Mood;
            if (m != lastMood) { lastMood = m; bubbleFor = 25; }
            else if (m == Mood.Idle && engine.Remaining % 600 == 0) bubbleFor = 20; // every 10 min check-in
        }

        string CurrentFrameKey()
        {
            switch (engine.Mood)
            {
                case Mood.Sleeping: case Mood.Away: return "sleep";
                case Mood.Happy: return "happy";
                case Mood.Break: return "wide";
                case Mood.Restless: return blinkFor > 0 ? "blink" : (frame / 4 % 2 == 0 ? "idle" : "wide");
                default: return blinkFor > 0 ? "blink" : farFor > 0 ? "far" : "idle";
            }
        }

        string BubbleText()
        {
            string n = settings.CatName;
            switch (engine.Mood)
            {
                case Mood.Sleeping: return "zzz... (paused)";
                case Mood.Away: return "zzz...";
                case Mood.Break: return "look far away!";
                case Mood.Happy: return "good job!";
                case Mood.Restless: return "break soon... " + ReminderEngine.Fmt(engine.Remaining);
                default:
                    if (bubbleFor > 20 && frame < 60) return "hi, i'm " + n + "!";
                    return "next break: " + ReminderEngine.Fmt(engine.Remaining);
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics;
            PixelRenderer.Crisp(g);
            g.Clear(Palette.Transparent);

            int catW = skin.W * scale, catH = skin.H * scale;
            int cx = (Width - catW) / 2, cy = Height - catH;
            int bob = engine.Mood == Mood.Happy ? (frame % 2) * scale : 0;
            PixelRenderer.DrawSprite(g, skin, skin.Frame(CurrentFrameKey(), alt), cx, cy - bob, scale);

            var mood = engine.Mood;
            if (mood == Mood.Sleeping || mood == Mood.Away)
            {
                int fs = Math.Max(1, scale / 2);
                int phase = frame / 3 % 4;
                for (int i = 0; i < 3; i++)
                {
                    int a = 255 - i * 70 - phase * 20; if (a < 40) a = 40;
                    PixelFont.Draw(g, "z", cx + catW - 3 * scale + i * 3 * fs, cy - (2 + i * 4 + phase) * fs, fs, Color.FromArgb(a, Palette.Accent));
                }
            }

            if (hover || bubbleFor > 0 || mood == Mood.Break)
            {
                int fs = Math.Max(1, scale / 2), px = Math.Max(1, scale / 2);
                string text = BubbleText();
                int tw = PixelFont.Measure(text, fs);
                int bw = tw + 6 * px, bh = PixelFont.H * fs + 6 * px;
                int bx = Math.Max(0, Math.Min(Width - bw, cx + catW / 2 - bw / 2));
                int by = cy - bh - 3 * px - bob;
                if (by < 0) by = 0;
                PixelRenderer.DrawBubble(g, new Rectangle(bx, by, bw, bh), px, cx + catW / 2 - px);
                PixelFont.Draw(g, text, bx + 3 * px, by + 3 * px, fs, Palette.Text);
            }
        }

        // ---------- dragging ----------

        void OnDown(object s, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;
            dragging = true; moved = false;
            dragStart = Cursor.Position; dragOrigin = Location;
        }

        void OnMove(object s, MouseEventArgs e)
        {
            if (!dragging) return;
            var d = new Size(Cursor.Position.X - dragStart.X, Cursor.Position.Y - dragStart.Y);
            if (Math.Abs(d.Width) > 3 || Math.Abs(d.Height) > 3) moved = true;
            if (moved) Location = dragOrigin + d;
        }

        void OnUp(object s, MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;
            dragging = false;
            if (moved) { settings.PetX = Left; settings.PetY = Top; settings.Save(); }
            else bubbleFor = bubbleFor > 0 ? 0 : 25;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) { engine.Changed -= OnEngineChanged; anim.Dispose(); }
            base.Dispose(disposing);
        }
    }
}
