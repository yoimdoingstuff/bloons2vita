// Generates the Bloons 2 trophy pack: original icon art, TROPCONF.SFM,
// TROP.SFM and an (unsigned, for use with NoTrpDrm) TROPHY.TRP.
//
// The achievement list is the game's own Game Center set, inherited from the
// iOS version (com.digitalgoldfish.bloons2.*). Trophy IDs must stay in sync
// with source/game/achievements.c.
//
// Run through make_trophies.ps1 (Windows PowerShell 5.1, uses System.Drawing).

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Security;
using System.Security.Cryptography;
using System.Text;

public static class TrophyPack {
    class Trophy {
        public string Type, Name, Detail, Label; public Color Balloon;
        public Trophy(string t, string n, string d, string l, Color c) { Type = t; Name = n; Detail = d; Label = l; Balloon = c; }
    }

    static readonly Trophy[] Trophies = {
        new Trophy("P", "Monkey Legend", "Unlock all other trophies.", "ALL", Color.FromArgb(150, 80, 220)),
        new Trophy("G", "Million Pops", "Pop 100k Bloons.", "100K", Color.FromArgb(230, 40, 40)),
        new Trophy("S", "Bloon Baron", "Launch 100 monkey aces.", "x100", Color.FromArgb(40, 120, 230)),
        new Trophy("B", "100%", "Pop every Bloon in a level.", "100%", Color.FromArgb(40, 180, 60)),
        new Trophy("B", "Swing and a Miss", "Hit nothing.", "MISS", Color.FromArgb(150, 150, 150)),
        new Trophy("G", "Ace Monkey", "Complete the game.", "ACE", Color.FromArgb(250, 200, 30)),
        new Trophy("B", "Bloons of Steel", "Use a Super Monkey.", "SUPER", Color.FromArgb(220, 50, 50)),
        new Trophy("B", "Bloon Academy", "Use a solution.", "?!", Color.FromArgb(60, 160, 220)),
        new Trophy("B", "Easter Egg", "Pop 25 Bloons in the main menu.", "x25", Color.FromArgb(240, 120, 200)),
        new Trophy("B", "Starfleet Bloon", "Spin the planet 50 times.", "x50", Color.FromArgb(70, 70, 180)),
        new Trophy("B", "Daisy Cutter", "You completed Popping Fields!", "1", Color.FromArgb(80, 190, 60)),
        new Trophy("B", "Just Deserts", "You completed Bloon Dunes!", "2", Color.FromArgb(225, 185, 100)),
        new Trophy("B", "Ice Demon", "You completed Frosty Fun!", "3", Color.FromArgb(120, 210, 240)),
        new Trophy("B", "My Everest", "You completed Sneak Peaks!", "4", Color.FromArgb(140, 140, 160)),
        new Trophy("S", "Jungle Fever", "You completed Rubber Jungle!", "5", Color.FromArgb(30, 130, 50)),
        new Trophy("S", "Temple of Bloon", "You completed Temple of Bloon!", "6", Color.FromArgb(170, 140, 90)),
        new Trophy("S", "Bloon BBQ", "You completed Crazy Coast!", "7", Color.FromArgb(250, 220, 60)),
        new Trophy("S", "Hot Stuff", "You completed Mount Magma!", "8", Color.FromArgb(240, 100, 20)),
    };

    const string Header = "<!--Sce-Np-Trophy-Signature: ";

    public static void Generate(string outDir, string npCommId) {
        Directory.CreateDirectory(outDir);
        var files = new List<KeyValuePair<string, byte[]>>();

        files.Add(new KeyValuePair<string, byte[]>("TROPCONF.SFM", Encoding.UTF8.GetBytes(Sfm(npCommId, false))));
        files.Add(new KeyValuePair<string, byte[]>("TROP.SFM", Encoding.UTF8.GetBytes(Sfm(npCommId, true))));
        files.Add(new KeyValuePair<string, byte[]>("ICON0.PNG", Png(DrawIcon0())));
        for (int i = 0; i < Trophies.Length; i++)
            files.Add(new KeyValuePair<string, byte[]>(string.Format("TROP{0:000}.PNG", i), Png(DrawTrophy(Trophies[i]))));

        foreach (var f in files)
            File.WriteAllBytes(Path.Combine(outDir, f.Key), f.Value);

        File.WriteAllBytes(Path.Combine(outDir, "TROPHY.TRP"), BuildTrp(files));
    }

    static string Sfm(string npCommId, bool full) {
        var sb = new StringBuilder();
        sb.Append(Header).Append(new string('x', 320)).Append("-->\n");
        sb.Append("<trophyconf version=\"1.1\" platform=\"psp2\" policy=\"large\">\n");
        sb.AppendFormat(" <npcommid>{0}</npcommid>\n", npCommId);
        sb.Append(" <trophyset-version>01.00</trophyset-version>\n");
        sb.Append(" <parental-level license-area=\"default\">0</parental-level>\n");
        if (full) {
            sb.Append(" <title-name>Bloons 2</title-name>\n");
            sb.Append(" <title-detail>Throw darts, pop the bloons and clear every level with as few darts as you can.</title-detail>\n");
        }
        for (int i = 0; i < Trophies.Length; i++) {
            var t = Trophies[i];
            string attrs = string.Format("id=\"{0:000}\" hidden=\"no\" ttype=\"{1}\" pid=\"{2}\"", i, t.Type, i == 0 ? "-1" : "000");
            if (!full) {
                sb.AppendFormat(" <trophy {0}/>\n", attrs);
            } else {
                sb.AppendFormat(" <trophy {0}>\n", attrs);
                sb.AppendFormat("  <name>{0}</name>\n", SecurityElement.Escape(t.Name));
                sb.AppendFormat("  <detail>{0}</detail>\n", SecurityElement.Escape(t.Detail));
                sb.Append(" </trophy>\n");
            }
        }
        sb.Append("</trophyconf>\n");
        return sb.ToString();
    }

    // TRP v2: 0x40 byte header, 0x40 byte entries, data aligned to 16 bytes,
    // SHA-1 of the whole file (with the hash field zeroed) at 0x1C.
    static byte[] BuildTrp(List<KeyValuePair<string, byte[]>> files) {
        int count = files.Count;
        long offset = 0x40 + count * 0x40;
        var offsets = new long[count];
        for (int i = 0; i < count; i++) {
            offsets[i] = offset;
            offset = (offset + files[i].Value.Length + 15) & ~15L;
        }

        var trp = new byte[offset];
        PutBE32(trp, 0x00, 0xDCA24D00);
        PutBE32(trp, 0x04, 2);
        PutBE64(trp, 0x08, (ulong)trp.Length);
        PutBE32(trp, 0x10, (uint)count);
        PutBE32(trp, 0x14, 0x40);
        PutBE32(trp, 0x18, 0);

        for (int i = 0; i < count; i++) {
            int e = 0x40 + i * 0x40;
            byte[] name = Encoding.ASCII.GetBytes(files[i].Key);
            Buffer.BlockCopy(name, 0, trp, e, Math.Min(name.Length, 31));
            PutBE64(trp, e + 0x20, (ulong)offsets[i]);
            PutBE64(trp, e + 0x28, (ulong)files[i].Value.Length);
            Buffer.BlockCopy(files[i].Value, 0, trp, (int)offsets[i], files[i].Value.Length);
        }

        byte[] hash;
        using (var sha = SHA1.Create()) hash = sha.ComputeHash(trp);
        Buffer.BlockCopy(hash, 0, trp, 0x1C, 20);
        return trp;
    }

    static void PutBE32(byte[] b, int o, uint v) { b[o] = (byte)(v >> 24); b[o + 1] = (byte)(v >> 16); b[o + 2] = (byte)(v >> 8); b[o + 3] = (byte)v; }
    static void PutBE64(byte[] b, int o, ulong v) { PutBE32(b, o, (uint)(v >> 32)); PutBE32(b, o + 4, (uint)v); }

    static byte[] Png(Bitmap bmp) {
        using (var ms = new MemoryStream()) { bmp.Save(ms, ImageFormat.Png); bmp.Dispose(); return ms.ToArray(); }
    }

    // ---- Art ----

    static Color TierColor(string type) {
        switch (type) {
            case "P": return Color.FromArgb(120, 170, 220);
            case "G": return Color.FromArgb(230, 180, 40);
            case "S": return Color.FromArgb(175, 180, 190);
            default: return Color.FromArgb(190, 120, 60);
        }
    }

    static Graphics Canvas(Bitmap bmp) {
        var g = Graphics.FromImage(bmp);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = TextRenderingHint.AntiAlias;
        g.Clear(Color.Transparent);
        return g;
    }

    static Color Shade(Color c, float f) {
        return Color.FromArgb(Math.Min(255, (int)(c.R * f)), Math.Min(255, (int)(c.G * f)), Math.Min(255, (int)(c.B * f)));
    }

    static void Balloon(Graphics g, float cx, float cy, float r, Color c) {
        using (var pen = new Pen(Color.FromArgb(60, 60, 60), Math.Max(1.5f, r * 0.06f)))
            g.DrawBezier(pen, cx, cy + r * 1.15f, cx - r * 0.3f, cy + r * 1.5f, cx + r * 0.3f, cy + r * 1.8f, cx, cy + r * 2.1f);
        var knot = new[] { new PointF(cx, cy + r * 1.08f), new PointF(cx - r * 0.15f, cy + r * 1.27f), new PointF(cx + r * 0.15f, cy + r * 1.27f) };
        using (var b = new SolidBrush(Shade(c, 0.7f))) g.FillPolygon(b, knot);
        var body = new RectangleF(cx - r * 0.9f, cy - r, r * 1.8f, r * 2.15f);
        using (var path = new GraphicsPath()) {
            path.AddEllipse(body);
            using (var b = new PathGradientBrush(path) { CenterColor = Shade(c, 1.25f), SurroundColors = new[] { Shade(c, 0.8f) }, CenterPoint = new PointF(cx - r * 0.3f, cy - r * 0.4f) })
                g.FillPath(b, path);
        }
        using (var b = new SolidBrush(Color.FromArgb(160, 255, 255, 255)))
            g.FillEllipse(b, cx - r * 0.52f, cy - r * 0.68f, r * 0.42f, r * 0.58f);
    }

    static void Dart(Graphics g, float x1, float y1, float x2, float y2, float w) {
        using (var pen = new Pen(Color.FromArgb(80, 60, 40), w) { EndCap = LineCap.Triangle, StartCap = LineCap.Round })
            g.DrawLine(pen, x1, y1, x2, y2);
        float dx = x2 - x1, dy = y2 - y1, len = (float)Math.Sqrt(dx * dx + dy * dy);
        float ux = dx / len, uy = dy / len;
        var tip = new[] { new PointF(x2 + ux * w * 3, y2 + uy * w * 3), new PointF(x2 - uy * w * 1.2f, y2 + ux * w * 1.2f), new PointF(x2 + uy * w * 1.2f, y2 - ux * w * 1.2f) };
        using (var b = new SolidBrush(Color.FromArgb(200, 200, 210))) g.FillPolygon(b, tip);
        var fl = new[] { new PointF(x1, y1), new PointF(x1 - ux * w * 3 - uy * w * 2, y1 - uy * w * 3 + ux * w * 2), new PointF(x1 - ux * w * 3 + uy * w * 2, y1 - uy * w * 3 - ux * w * 2) };
        using (var b = new SolidBrush(Color.FromArgb(220, 40, 40))) g.FillPolygon(b, fl);
    }

    static void OutlinedText(Graphics g, string text, float cx, float cy, float size, Color fill) {
        using (var path = new GraphicsPath())
        using (var family = new FontFamily("Arial Black")) {
            var fmt = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
            path.AddString(text, family, (int)FontStyle.Regular, size, new PointF(cx, cy), fmt);
            using (var pen = new Pen(Color.FromArgb(30, 35, 70), size * 0.16f) { LineJoin = LineJoin.Round }) g.DrawPath(pen, path);
            using (var b = new SolidBrush(fill)) g.FillPath(b, path);
        }
    }

    static Bitmap DrawTrophy(Trophy t) {
        var bmp = new Bitmap(240, 240, PixelFormat.Format32bppArgb);
        using (var g = Canvas(bmp)) {
            Color tier = TierColor(t.Type);
            // medallion
            using (var b = new LinearGradientBrush(new Rectangle(8, 8, 224, 224), Shade(tier, 1.3f), Shade(tier, 0.7f), 60f))
                g.FillEllipse(b, 8, 8, 224, 224);
            using (var b = new LinearGradientBrush(new Rectangle(24, 24, 192, 192), Color.FromArgb(120, 195, 250), Color.FromArgb(210, 238, 255), 90f))
                g.FillEllipse(b, 24, 24, 192, 192);
            using (var pen = new Pen(Shade(tier, 0.55f), 4)) { g.DrawEllipse(pen, 8, 8, 224, 224); g.DrawEllipse(pen, 24, 24, 192, 192); }
            Balloon(g, 120, 86, 44, t.Balloon);
            Dart(g, 46, 150, 82, 112, 6);
            OutlinedText(g, t.Label, 120, 180, t.Label.Length > 4 ? 30 : 40, Color.White);
        }
        return bmp;
    }

    static Bitmap DrawIcon0() {
        var bmp = new Bitmap(320, 176, PixelFormat.Format32bppArgb);
        using (var g = Canvas(bmp)) {
            using (var b = new LinearGradientBrush(new Rectangle(0, 0, 320, 176), Color.FromArgb(100, 185, 250), Color.FromArgb(205, 236, 255), 90f))
                g.FillRectangle(b, 0, 0, 320, 176);
            using (var b = new SolidBrush(Color.FromArgb(110, 190, 80))) g.FillEllipse(b, -60, 140, 440, 120);
            Balloon(g, 50, 44, 26, Color.FromArgb(230, 40, 40));
            Balloon(g, 270, 40, 24, Color.FromArgb(40, 120, 230));
            Balloon(g, 236, 70, 18, Color.FromArgb(250, 200, 30));
            Dart(g, 70, 140, 110, 110, 5);
            OutlinedText(g, "Bloons 2", 160, 96, 52, Color.White);
        }
        return bmp;
    }
}
