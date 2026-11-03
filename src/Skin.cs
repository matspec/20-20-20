using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;

namespace TwentyTwentyTwenty
{
    // A skin is a plain text file: a palette (one character = one colour) and a set of named
    // frames where every character is a single pixel. Users drop *.skin files into
    // %LOCALAPPDATA%\TwentyTwentyTwenty\skins and pick them from the tray menu.
    //
    //   name: Tabby
    //   palette:
    //   K #1b1b2f
    //   O #f0a04b
    //   frame idle:
    //   ....KK....
    //   (rows...)
    //
    // Frame keys the app looks for (all optional except idle):
    //   idle, blink, far, happy, wide, sleep   and an optional "<key>2" alternate for animation
    //   (e.g. idle2 with the tail in a different position). Missing frames fall back to idle.
    sealed class Skin
    {
        public string Name = "Unnamed";
        public string Source;                 // file path, or null for built-in
        public int W, H;
        public readonly Dictionary<char, Color> Palette = new Dictionary<char, Color>();
        public readonly Dictionary<string, string[]> Frames = new Dictionary<string, string[]>(StringComparer.OrdinalIgnoreCase);

        public static readonly string Dir = Path.Combine(Settings.Dir, "skins");
        public const string Extension = ".skin";

        public string[] Frame(string key, bool alt)
        {
            string[] rows;
            if (alt && Frames.TryGetValue(key + "2", out rows)) return rows;
            if (Frames.TryGetValue(key, out rows)) return rows;
            if (key == "sleep" && Frames.TryGetValue("blink", out rows)) return rows;
            return Frames["idle"];
        }

        public bool TryColor(char c, out Color color) { return Palette.TryGetValue(c, out color); }

        // ---------- parsing ----------

        public static Skin Parse(string text, string source)
        {
            var skin = new Skin { Source = source };
            string section = null; List<string> rows = null;
            Action flush = () =>
            {
                if (section != null && section.StartsWith("frame ") && rows != null && rows.Count > 0)
                    skin.Frames[section.Substring(6).Trim()] = rows.ToArray();
                rows = null;
            };

            foreach (var raw in text.Replace("\r", "").Split('\n'))
            {
                string line = raw.TrimEnd();
                if (line.Length == 0 || line.StartsWith("#")) continue;
                string lower = line.ToLowerInvariant();

                if (lower.StartsWith("name:")) { skin.Name = line.Substring(5).Trim(); continue; }
                if (lower == "palette:") { flush(); section = "palette"; continue; }
                if (lower.StartsWith("frame ") && lower.EndsWith(":")) { flush(); section = line.Substring(0, line.Length - 1).Trim(); rows = new List<string>(); continue; }

                if (section == "palette")
                {
                    var parts = line.Trim().Split(new[] { ' ', '\t', '=' }, StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length >= 2 && parts[0].Length == 1) skin.Palette[parts[0][0]] = ParseColor(parts[1]);
                }
                else if (rows != null) rows.Add(line.Trim());
            }
            flush();

            if (!skin.Frames.ContainsKey("idle")) throw new FormatException("Skin needs at least a 'frame idle:' section.");
            skin.W = skin.Frames["idle"][0].Length; skin.H = skin.Frames["idle"].Length;
            if (skin.W < 4 || skin.H < 4 || skin.W > 64 || skin.H > 64) throw new FormatException("Frames must be between 4x4 and 64x64 pixels.");
            foreach (var kv in skin.Frames)
            {
                if (kv.Value.Length != skin.H) throw new FormatException("Frame '" + kv.Key + "' has " + kv.Value.Length + " rows, expected " + skin.H + ".");
                foreach (var r in kv.Value)
                    if (r.Length != skin.W) throw new FormatException("Frame '" + kv.Key + "' has a row of width " + r.Length + ", expected " + skin.W + ".");
            }
            return skin;
        }

        static Color ParseColor(string s)
        {
            s = s.TrimStart('#');
            if (s.Length == 3) s = "" + s[0] + s[0] + s[1] + s[1] + s[2] + s[2];
            if (s.Length == 6) s = "FF" + s;
            if (s.Length != 8) throw new FormatException("Bad colour '" + s + "' (use #RRGGBB or #AARRGGBB).");
            return Color.FromArgb(unchecked((int)Convert.ToUInt32(s, 16)));
        }

        public string Serialize()
        {
            var sb = new StringBuilder();
            sb.AppendLine("# 20-20-20 skin. One character = one pixel. '.' (or any unlisted character) is transparent.");
            sb.AppendLine("# Frames: idle (required), blink, far, happy, wide, sleep. Add '<name>2' for a second animation frame.");
            sb.AppendLine("# All frames must be the same size. Any size from 4x4 to 64x64 works.");
            sb.AppendLine("name: " + Name);
            sb.AppendLine();
            sb.AppendLine("palette:");
            foreach (var kv in Palette)
                sb.AppendLine(kv.Key + " #" + kv.Value.R.ToString("x2") + kv.Value.G.ToString("x2") + kv.Value.B.ToString("x2"));
            foreach (var kv in Frames)
            {
                sb.AppendLine();
                sb.AppendLine("frame " + kv.Key + ":");
                foreach (var r in kv.Value) sb.AppendLine(r);
            }
            return sb.ToString();
        }

        // ---------- discovery ----------

        public static Skin LoadByName(string name)
        {
            foreach (var s in BuiltIn.All) if (s.Name.Equals(name, StringComparison.OrdinalIgnoreCase)) return s;
            string path = Path.Combine(Dir, name + Extension);
            if (File.Exists(path))
                try { return Parse(File.ReadAllText(path), path); } catch { }
            return BuiltIn.All[0];
        }

        public static List<string> CustomSkinNames()
        {
            var list = new List<string>();
            if (Directory.Exists(Dir))
                foreach (var f in Directory.GetFiles(Dir, "*" + Extension)) list.Add(Path.GetFileNameWithoutExtension(f));
            list.Sort(StringComparer.OrdinalIgnoreCase);
            return list;
        }

        // Creates the skins folder with every built-in exported as an editable template.
        public static void EnsureTemplates()
        {
            Directory.CreateDirectory(Dir);
            foreach (var s in BuiltIn.All)
            {
                string p = Path.Combine(Dir, "_template-" + s.Name + Extension);
                if (!File.Exists(p)) File.WriteAllText(p, s.Serialize());
            }
            string readme = Path.Combine(Dir, "README.txt");
            if (!File.Exists(readme))
                File.WriteAllText(readme,
                    "Custom skins for 20-20-20\r\n\r\n" +
                    "1. Copy one of the _template-*.skin files and rename it, e.g. MyCat.skin\r\n" +
                    "2. Open it in any text editor. Each character is one pixel.\r\n" +
                    "3. Change colours in the palette section (#RRGGBB) or add new letters.\r\n" +
                    "4. Edit the frames. Every frame must have the same width and height.\r\n" +
                    "5. Right-click the tray icon > Skin > pick your file.\r\n\r\n" +
                    "Files starting with _template- are regenerated if deleted; make a copy instead of editing them.\r\n");
        }
    }

    // Built-in skins, defined with the same text format so templates and code never drift.
    static class BuiltIn
    {
        // Palettes: K outline, O fur, L light fur (muzzle/belly), P pink, E eye, W eye highlight
        const string Tabby    = "K #1b1b2f\nO #f0a04b\nL #fad38a\nP #f28ca8\nE #2a2a40\nW #ffffff";
        const string Midnight = "K #0d0d18\nO #2e2e48\nL #4a4a6a\nP #f28ca8\nE #5ef0a0\nW #ffffff";
        const string Snow     = "K #3a3a4a\nO #f4f4f8\nL #e2e2ea\nP #f2a0b8\nE #4a90e2\nW #ffffff";
        const string Matcha   = "K #1b2f24\nO #7fcf9a\nL #c6f0d2\nP #f2a0b8\nE #2a2a40\nW #ffffff";

        static readonly string[] Base =
        {
            "...KK..........KK...",
            "..KOOK........KOOK..",
            "..KOPOK......KOPOK..",
            "..KOPPOKKKKKKOPPOK..",
            "..KOOOOOOOOOOOOOOK..",
            ".KOOOOOOOOOOOOOOOOK.",
            ".KOOOOOOOOOOOOOOOOK.",   // 6
            ".KOOEEOOOOOOOOEEOOK.",   // 7  eyes
            ".KOOEWOOOOOOOOWEOOK.",   // 8  eyes
            ".KOOEEOOOLLOOOEEOOK.",   // 9  eyes + muzzle
            ".KOOOOOOLPLOOOOOOOK.",   // 10 nose
            ".KOOOOOOKLLKOOOOOOK.",   // 11 mouth
            "..KOOOOOOOOOOOOOOK..",
            "..KKOOOOOOOOOOOOKK..",   // 13
            ".KOOOLLLLLLLLLLOOOK.",   // 14
            ".KOOOLLLLLLLLLLOOOKK",   // 15 tail
            ".KOOOLLLLLLLLLLOOOOK",   // 16 tail
            ".KOOOLLLLLLLLLLOOOOK",   // 17 tail
            ".KOOKKOOOOOOKKOOOKK.",   // 18
            ".KKK..KKKKKK..KKKK..",
        };

        // Declared after the data it depends on: static fields initialise in textual order.
        public static readonly Skin[] All = { Cat("Tabby", Tabby), Cat("Midnight", Midnight), Cat("Snow", Snow), Cat("Matcha", Matcha) };

        static string[] Patch(string[] src, int firstRow, params string[] rows)
        {
            var r = (string[])src.Clone();
            for (int i = 0; i < rows.Length; i++) r[firstRow + i] = rows[i];
            return r;
        }

        static Skin Cat(string name, string palette)
        {
            var idle = Base;
            // Tail swings up on the alternate frame.
            var idleAlt = Patch(Base, 12,
                "..KOOOOOOOOOOOOOOK.K",
                "..KKOOOOOOOOOOOOKKOK",
                ".KOOOLLLLLLLLLLOOOKK",
                ".KOOOLLLLLLLLLLOOOK.",
                ".KOOOLLLLLLLLLLOOOK.",
                ".KOOOLLLLLLLLLLOOOK.");
            var blink = Patch(Base, 7,
                ".KOOOOOOOOOOOOOOOOK.",
                ".KOOKKOOOOOOOOKKOOK.",
                ".KOOOOOOOLLOOOOOOOK.");
            // Gazing up into the distance: eyes shift one row up.
            var far = Patch(Base, 6,
                ".KOOEEOOOOOOOOEEOOK.",
                ".KOOEWOOOOOOOOWEOOK.",
                ".KOOEEOOOOOOOOEEOOK.",
                ".KOOOOOOOLLOOOOOOOK.");
            var far2 = Patch(far, 12, idleAlt[12], idleAlt[13], idleAlt[14], idleAlt[15], idleAlt[16], idleAlt[17]);
            // ^ ^ eyes
            var happy = Patch(Base, 7,
                ".KOOKKOOOOOOOOKKOOK.",
                ".KOKOOKOOOOOOKOOKOK.",
                ".KOOOOOOOLLOOOOOOOK.");
            var happy2 = Patch(happy, 12, idleAlt[12], idleAlt[13], idleAlt[14], idleAlt[15], idleAlt[16], idleAlt[17]);
            // Wide awake: taller eyes.
            var wide = Patch(Base, 6,
                ".KOOEEOOOOOOOOEEOOK.",
                ".KOOEWOOOOOOOOWEOOK.",
                ".KOOEEOOOOOOOOEEOOK.",
                ".KOOEEOOOLLOOOEEOOK.");

            var text = new StringBuilder();
            text.AppendLine("name: " + name).AppendLine("palette:").AppendLine(palette);
            Add(text, "idle", idle); Add(text, "idle2", idleAlt); Add(text, "blink", blink);
            Add(text, "far", far); Add(text, "far2", far2); Add(text, "happy", happy); Add(text, "happy2", happy2);
            Add(text, "wide", wide); Add(text, "sleep", blink);
            return Skin.Parse(text.ToString(), null);
        }

        static void Add(StringBuilder sb, string key, string[] rows)
        {
            sb.AppendLine("frame " + key + ":");
            foreach (var r in rows) sb.AppendLine(r);
        }
    }
}
