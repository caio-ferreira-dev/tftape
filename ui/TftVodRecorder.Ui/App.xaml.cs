using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;

namespace TftVodRecorder.Ui;

public partial class App : Application
{
    private Window? _window;

    public App()
    {
        InitializeComponent();
        RegisterCrashLogging();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        _window.Activate();
    }

    private void RegisterCrashLogging()
    {
        UnhandledException += (_, e) =>
        {
            LogException("UnhandledException", e.Exception);
            e.Handled = true; // tenta evitar fechamento abrupto quando possível
        };
        AppDomain.CurrentDomain.UnhandledException += (_, e) =>
        {
            if (e.ExceptionObject is Exception ex) LogException("AppDomain.UnhandledException", ex);
            else LogException("AppDomain.UnhandledException", new Exception(e.ExceptionObject?.ToString() ?? "unknown"));
        };
        TaskScheduler.UnobservedTaskException += (_, e) =>
        {
            LogException("UnobservedTaskException", e.Exception);
            e.SetObserved();
        };
    }

    private static void LogException(string source, Exception ex)
    {
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "ui_crash.log");
            File.AppendAllText(path,
                $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}] {source}\n{ex}\n\n");
        }
        catch { }
    }
}
