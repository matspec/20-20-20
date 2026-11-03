using System;
using System.Windows.Forms;
using Microsoft.Win32;

namespace TwentyTwentyTwenty
{
    enum Mood { Idle, Restless, Break, Happy, Sleeping, Away }

    // Owns the countdown and the "what is the cat feeling" state. UI layers subscribe to Changed.
    sealed class ReminderEngine : IDisposable
    {
        const int IdleResetSeconds = 5 * 60;
        const int RestlessSeconds = 2 * 60;
        const int HappySeconds = 8;

        readonly Settings settings;
        readonly Timer tick = new Timer { Interval = 1000 };
        int happyLeft;
        bool away;

        public int Remaining { get; private set; }
        public bool BreakActive { get; private set; }
        public Mood Mood
        {
            get
            {
                if (BreakActive) return Mood.Break;
                if (settings.Paused) return Mood.Sleeping;
                if (away) return Mood.Away;
                if (happyLeft > 0) return Mood.Happy;
                return Remaining <= RestlessSeconds ? Mood.Restless : Mood.Idle;
            }
        }

        public event Action Changed;
        public event Action BreakRequested;

        public ReminderEngine(Settings s)
        {
            settings = s;
            Remaining = s.WorkMinutes * 60;
            SystemEvents.PowerModeChanged += OnPower;
            SystemEvents.SessionSwitch += OnSession;
            tick.Tick += (o, e) => OnTick();
            tick.Start();
        }

        void OnTick()
        {
            if (happyLeft > 0) happyLeft--;
            if (settings.Paused || away || BreakActive) { Raise(); return; }

            if (Native.IdleSeconds() >= IdleResetSeconds) { Remaining = settings.WorkMinutes * 60; Raise(); return; }

            if (--Remaining <= 0) { Remaining = 0; RequestBreak(); return; }
            Raise();
        }

        public void RequestBreak()
        {
            if (BreakActive) return;
            BreakActive = true;
            Raise();
            var h = BreakRequested; if (h != null) h();
        }

        public void CompleteBreak(bool finished)
        {
            BreakActive = false;
            Remaining = settings.WorkMinutes * 60;
            happyLeft = finished ? HappySeconds : 0;
            Raise();
        }

        public void Reset() { Remaining = settings.WorkMinutes * 60; Raise(); }
        public void TogglePause() { settings.Paused = !settings.Paused; settings.Save(); Raise(); }
        public void IntervalChanged() { Remaining = Math.Min(Remaining, settings.WorkMinutes * 60); Raise(); }

        void OnPower(object s, PowerModeChangedEventArgs e)
        {
            if (e.Mode == PowerModes.Suspend) { away = true; Raise(); }
            else if (e.Mode == PowerModes.Resume) CameBack();
        }

        void OnSession(object s, SessionSwitchEventArgs e)
        {
            switch (e.Reason)
            {
                case SessionSwitchReason.SessionLock:
                case SessionSwitchReason.ConsoleDisconnect:
                case SessionSwitchReason.RemoteDisconnect: away = true; Raise(); break;
                case SessionSwitchReason.SessionUnlock:
                case SessionSwitchReason.ConsoleConnect:
                case SessionSwitchReason.RemoteConnect: CameBack(); break;
            }
        }

        void CameBack() { away = false; Remaining = settings.WorkMinutes * 60; Raise(); }

        void Raise() { var h = Changed; if (h != null) h(); }

        public string StatusText()
        {
            switch (Mood)
            {
                case Mood.Sleeping: return "Paused";
                case Mood.Away: return "Away";
                case Mood.Break: return "Break time";
                default: return "Next break in " + Fmt(Remaining);
            }
        }

        public static string Fmt(int secs)
        {
            return secs >= 60 ? (secs + 59) / 60 + " min" : secs + " sec";
        }

        public void Dispose()
        {
            SystemEvents.PowerModeChanged -= OnPower;
            SystemEvents.SessionSwitch -= OnSession;
            tick.Dispose();
        }
    }
}
