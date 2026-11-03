using System;
using System.IO;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;

namespace TwentyTwentyTwenty
{
    public sealed class Settings
    {
        public int WorkMinutes = 20;
        public int BreakSeconds = 20;
        public bool FullScreen = true;
        public bool PlaySound = true;
        public bool Paused = false;
        public bool ShowPet = true;
        public string CatName = "Mochi";
        public string SkinName = "Tabby";
        public int PetScale = 4;
        public int PetX = -1, PetY = -1;

        public static readonly string Dir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), App.Name);
        static readonly string FilePath = Path.Combine(Dir, "settings.ini");

        public static Settings Load()
        {
            var s = new Settings();
            if (!File.Exists(FilePath)) return s;
            foreach (var raw in File.ReadAllLines(FilePath))
            {
                var line = raw.Trim();
                int eq = line.IndexOf('=');
                if (eq < 1 || line.StartsWith(";") || line.StartsWith("[")) continue;
                string k = line.Substring(0, eq).Trim(), v = line.Substring(eq + 1).Trim();
                int n; bool b;
                switch (k)
                {
                    case "WorkMinutes": if (int.TryParse(v, out n) && n > 0) s.WorkMinutes = n; break;
                    case "BreakSeconds": if (int.TryParse(v, out n) && n > 0) s.BreakSeconds = n; break;
                    case "FullScreen": if (ParseBool(v, out b)) s.FullScreen = b; break;
                    case "PlaySound": if (ParseBool(v, out b)) s.PlaySound = b; break;
                    case "Paused": if (ParseBool(v, out b)) s.Paused = b; break;
                    case "ShowPet": if (ParseBool(v, out b)) s.ShowPet = b; break;
                    case "CatName": if (v.Length > 0) s.CatName = v.Length > 16 ? v.Substring(0, 16) : v; break;
                    case "SkinName": if (v.Length > 0) s.SkinName = v; break;
                    case "PetScale": if (int.TryParse(v, out n)) s.PetScale = Math.Max(2, Math.Min(8, n)); break;
                    case "PetX": if (int.TryParse(v, out n)) s.PetX = n; break;
                    case "PetY": if (int.TryParse(v, out n)) s.PetY = n; break;
                }
            }
            return s;
        }

        // Accepts true/false and 1/0 so the installer can write either.
        static bool ParseBool(string v, out bool b)
        {
            if (bool.TryParse(v, out b)) return true;
            if (v == "1") { b = true; return true; }
            if (v == "0") { b = false; return true; }
            return false;
        }

        public void Save()
        {
            Directory.CreateDirectory(Dir);
            var sb = new StringBuilder();
            sb.AppendLine("WorkMinutes=" + WorkMinutes);
            sb.AppendLine("BreakSeconds=" + BreakSeconds);
            sb.AppendLine("FullScreen=" + FullScreen);
            sb.AppendLine("PlaySound=" + PlaySound);
            sb.AppendLine("Paused=" + Paused);
            sb.AppendLine("ShowPet=" + ShowPet);
            sb.AppendLine("CatName=" + CatName);
            sb.AppendLine("SkinName=" + SkinName);
            sb.AppendLine("PetScale=" + PetScale);
            sb.AppendLine("PetX=" + PetX);
            sb.AppendLine("PetY=" + PetY);
            File.WriteAllText(FilePath, sb.ToString());
        }

        // ---- Run at startup (HKCU, no admin) ----
        const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";

        public static bool IsStartupEnabled()
        {
            using (var k = Registry.CurrentUser.OpenSubKey(RunKey))
                return k != null && k.GetValue(App.Name) != null;
        }

        public static void SetStartup(bool enabled)
        {
            using (var k = Registry.CurrentUser.CreateSubKey(RunKey))
            {
                if (enabled) k.SetValue(App.Name, "\"" + Application.ExecutablePath + "\" --minimized");
                else k.DeleteValue(App.Name, false);
            }
        }
    }
}
