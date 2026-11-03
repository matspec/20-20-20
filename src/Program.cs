using System;
using System.Threading;
using System.Windows.Forms;

namespace TwentyTwentyTwenty
{
    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            bool created;
            using (var mutex = new Mutex(true, "Local\\" + App.Name + "_SingleInstance", out created))
            {
                if (!created)
                {
                    MessageBox.Show(App.Title + " is already running. Look for the cat in the system tray.",
                        App.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }
                using (var tray = new TrayApp(Array.IndexOf(args, "--installed") >= 0))
                    Application.Run();
            }
        }
    }
}
