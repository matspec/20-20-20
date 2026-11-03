using System;
using System.Drawing;
using System.Windows.Forms;

namespace TwentyTwentyTwenty
{
    // Glues everything together: engine (logic), tray icon + menu, desktop pet, break overlays.
    sealed class TrayApp : IDisposable
    {
        readonly Settings settings;
        readonly ReminderEngine engine;
        readonly NotifyIcon tray;
        readonly ContextMenuStrip menu = new ContextMenuStrip();
        readonly ToolStripMenuItem statusItem, pauseItem, petItem, fullScreenItem, soundItem, startupItem;
        readonly ToolStripMenuItem skinMenu;
        Icon iconActive, iconPaused;
        Skin skin;
        PetForm pet;
        BreakSession breakSession;

        public TrayApp(bool justInstalled)
        {
            settings = Settings.Load();
            engine = new ReminderEngine(settings);
            skin = Skin.LoadByName(settings.SkinName);
            RebuildIcons();

            skinMenu = new ToolStripMenuItem("Skin");
            skinMenu.DropDownOpening += (s, e) => PopulateSkinMenu();
            PopulateSkinMenu();

            statusItem = new ToolStripMenuItem { Enabled = false };
            pauseItem = new ToolStripMenuItem("Pause", null, (s, e) => engine.TogglePause());
            var breakNow = new ToolStripMenuItem("Take a break now", null, (s, e) => engine.RequestBreak());
            var reset = new ToolStripMenuItem("Reset timer", null, (s, e) => engine.Reset());

            var interval = new ToolStripMenuItem("Work interval");
            foreach (int m in new[] { 10, 15, 20, 25, 30, 45, 60 })
            {
                int mm = m;
                var it = new ToolStripMenuItem(m + " min") { Checked = settings.WorkMinutes == m };
                it.Click += (s, e) =>
                {
                    settings.WorkMinutes = mm; settings.Save();
                    foreach (ToolStripMenuItem x in interval.DropDownItems) x.Checked = x == it;
                    engine.IntervalChanged();
                };
                interval.DropDownItems.Add(it);
            }

            var breakLen = new ToolStripMenuItem("Break length");
            foreach (int sec in new[] { 20, 30, 45, 60 })
            {
                int ss = sec;
                var it = new ToolStripMenuItem(sec + " sec") { Checked = settings.BreakSeconds == sec };
                it.Click += (s, e) =>
                {
                    settings.BreakSeconds = ss; settings.Save();
                    foreach (ToolStripMenuItem x in breakLen.DropDownItems) x.Checked = x == it;
                };
                breakLen.DropDownItems.Add(it);
            }

            var petSize = new ToolStripMenuItem("Pet size");
            foreach (var kv in new[] { new { L = "Small", S = 3 }, new { L = "Medium", S = 4 }, new { L = "Large", S = 6 }, new { L = "Huge", S = 8 } })
            {
                int sc = kv.S;
                var it = new ToolStripMenuItem(kv.L) { Checked = settings.PetScale == sc };
                it.Click += (s, e) =>
                {
                    settings.PetScale = sc; settings.PetX = settings.PetY = -1; settings.Save();
                    foreach (ToolStripMenuItem x in petSize.DropDownItems) x.Checked = x == it;
                    RecreatePet();
                };
                petSize.DropDownItems.Add(it);
            }

            petItem = new ToolStripMenuItem("Show " + settings.CatName + " on desktop", null, (s, e) =>
            { settings.ShowPet = !settings.ShowPet; settings.Save(); petItem.Checked = settings.ShowPet; RecreatePet(); })
            { Checked = settings.ShowPet };
            fullScreenItem = new ToolStripMenuItem("Full-screen break (all monitors)", null, (s, e) =>
            { settings.FullScreen = !settings.FullScreen; fullScreenItem.Checked = settings.FullScreen; settings.Save(); })
            { Checked = settings.FullScreen };
            soundItem = new ToolStripMenuItem("Play sound", null, (s, e) =>
            { settings.PlaySound = !settings.PlaySound; soundItem.Checked = settings.PlaySound; settings.Save(); })
            { Checked = settings.PlaySound };
            startupItem = new ToolStripMenuItem("Run at startup", null, (s, e) =>
            { Settings.SetStartup(!Settings.IsStartupEnabled()); startupItem.Checked = Settings.IsStartupEnabled(); })
            { Checked = Settings.IsStartupEnabled() };

            var exit = new ToolStripMenuItem("Exit", null, (s, e) => Application.Exit());

            menu.Items.AddRange(new ToolStripItem[] {
                statusItem, new ToolStripSeparator(),
                pauseItem, breakNow, reset, new ToolStripSeparator(),
                interval, breakLen, new ToolStripSeparator(),
                petItem, petSize, skinMenu, fullScreenItem, soundItem, startupItem, new ToolStripSeparator(),
                exit });

            tray = new NotifyIcon { ContextMenuStrip = menu, Visible = true, Icon = iconActive };
            tray.DoubleClick += (s, e) => engine.TogglePause();

            engine.Changed += UpdateUi;
            engine.BreakRequested += StartBreak;
            UpdateUi();
            RecreatePet();

            if (justInstalled)
                Balloon(settings.CatName + " is on duty. Every " + settings.WorkMinutes + " minutes: look 20 feet away for " + settings.BreakSeconds + " seconds.");
        }

        // ---------- skins ----------

        void PopulateSkinMenu()
        {
            skinMenu.DropDownItems.Clear();
            foreach (var s in BuiltIn.All) AddSkinItem(s.Name, s.Name);
            var custom = Skin.CustomSkinNames();
            if (custom.Count > 0)
            {
                skinMenu.DropDownItems.Add(new ToolStripSeparator());
                foreach (var name in custom) if (!name.StartsWith("_template-")) AddSkinItem(name, name);
            }
            skinMenu.DropDownItems.Add(new ToolStripSeparator());
            skinMenu.DropDownItems.Add(new ToolStripMenuItem("Open skins folder (make your own)...", null, (s, e) =>
            {
                Skin.EnsureTemplates();
                System.Diagnostics.Process.Start("explorer.exe", "\"" + Skin.Dir + "\"");
            }));
        }

        void AddSkinItem(string label, string name)
        {
            var it = new ToolStripMenuItem(label) { Checked = string.Equals(settings.SkinName, name, StringComparison.OrdinalIgnoreCase) };
            it.Click += (s, e) => ApplySkin(name);
            skinMenu.DropDownItems.Add(it);
        }

        void ApplySkin(string name)
        {
            string path = System.IO.Path.Combine(Skin.Dir, name + Skin.Extension);
            Skin loaded;
            try
            {
                loaded = System.IO.File.Exists(path) ? Skin.Parse(System.IO.File.ReadAllText(path), path) : Skin.LoadByName(name);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Couldn't load skin '" + name + "':\n\n" + ex.Message, App.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            skin = loaded;
            settings.SkinName = name; settings.Save();
            PixelRenderer.ClearCache();
            RebuildIcons();
            UpdateUi();
            RecreatePet();
        }

        void RebuildIcons()
        {
            if (iconActive != null) iconActive.Dispose();
            if (iconPaused != null) iconPaused.Dispose();
            iconActive = PixelRenderer.ToIcon(skin, skin.Frame("idle", false), 32);
            iconPaused = PixelRenderer.ToIcon(skin, skin.Frame("sleep", false), 32);
            if (tray != null) tray.Icon = iconActive;
        }

        // ---------- pet / break ----------

        void RecreatePet()
        {
            if (pet != null) { pet.Close(); pet.Dispose(); pet = null; }
            if (!settings.ShowPet) return;
            pet = new PetForm(settings, engine, skin, menu);
            pet.PauseToggled += engine.TogglePause;
            pet.Show();
        }

        void StartBreak()
        {
            if (breakSession != null) return;
            if (pet != null) pet.Hide();
            breakSession = new BreakSession(settings, skin, completed =>
            {
                breakSession = null;
                engine.CompleteBreak(completed);
                if (pet != null) pet.Show();
            });
        }

        void UpdateUi()
        {
            var mood = engine.Mood;
            tray.Icon = mood == Mood.Sleeping || mood == Mood.Away ? iconPaused : iconActive;
            pauseItem.Text = settings.Paused ? "Resume" : "Pause";
            string status = engine.StatusText();
            statusItem.Text = settings.CatName + ": " + status;
            string tip = App.Title + " - " + status;
            tray.Text = tip.Length > 63 ? tip.Substring(0, 63) : tip;
        }

        void Balloon(string text)
        {
            tray.BalloonTipTitle = App.Title;
            tray.BalloonTipText = text;
            tray.ShowBalloonTip(4000);
        }

        public void Dispose()
        {
            if (breakSession != null) breakSession.Dispose();
            if (pet != null) pet.Dispose();
            engine.Dispose();
            tray.Visible = false;
            tray.Dispose();
            menu.Dispose();
            iconActive.Dispose();
            iconPaused.Dispose();
        }
    }
}
