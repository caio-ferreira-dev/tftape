using System.Diagnostics;
using System.Text.Json;
using TftVodRecorder.Ui.Models;

namespace TftVodRecorder.Ui.Services;

/// <summary>
/// Controla o processo recorder.exe via stdin/stdout (JSON-lines).
/// O executável deve estar disponível no caminho configurado (ex.: ao lado da UI).
/// </summary>
public sealed class RecorderProcessService : IRecorderService, IDisposable
{
    private readonly string _recorderPath;
    private Process? _process;

    public RecorderProcessService(string? recorderPath = null)
    {
        _recorderPath = recorderPath ?? Path.Combine(AppContext.BaseDirectory, "recorder.exe");
    }

    public event Action<RecorderEvent>? OnEvent;

    public async Task StartAsync(RecordingOptions options, CancellationToken ct = default)
    {
        EnsureProcess();
        var cmd = new
        {
            cmd = "start",
            output_dir = options.OutputDirectory,
            resolution = options.Resolution,
            fps = options.Fps,
            game_audio = options.GameAudio,
            system_audio = options.SystemAudio,
            microphone = options.Microphone
        };
        await SendAsync(cmd, ct);
    }

    public async Task StopAsync(CancellationToken ct = default)
    {
        if (_process == null || _process.HasExited) return;
        await SendAsync(new { cmd = "stop" }, ct);
    }

    private void EnsureProcess()
    {
        if (_process != null && !_process.HasExited) return;
        if (!File.Exists(_recorderPath))
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", $"recorder.exe não encontrado em: {_recorderPath}"));
            return;
        }
        var psi = new ProcessStartInfo
        {
            FileName = _recorderPath,
            // Garante que o core encontre tft_window_rules.json ao lado do próprio executável,
            // independentemente do diretório de trabalho herdado da UI.
            WorkingDirectory = Path.GetDirectoryName(_recorderPath) ?? AppContext.BaseDirectory,
            UseShellExecute = false,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };
        _process = Process.Start(psi);
        if (_process == null) { OnEvent?.Invoke(new RecorderEvent(false, "error", "Falha ao iniciar recorder.exe")); return; }
        _ = Task.Run(ReadLoopAsync);
    }

    private async Task SendAsync(object payload, CancellationToken ct)
    {
        if (_process?.HasExited != false || _process.StandardInput == null)
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", "Processo do gravador não está ativo"));
            return;
        }
        var json = JsonSerializer.Serialize(payload);
        await _process.StandardInput.WriteLineAsync(json.AsMemory(), ct);
        await _process.StandardInput.FlushAsync();
    }

    private async Task ReadLoopAsync()
    {
        if (_process?.StandardOutput == null) return;
        while (!_process.HasExited)
        {
            var line = await _process.StandardOutput.ReadLineAsync();
            if (line == null) break;
            try
            {
                using var doc = JsonDocument.Parse(line);
                var root = doc.RootElement;
                var ok = root.TryGetProperty("ok", out var okp) && okp.GetBoolean();
                var name = root.TryGetProperty("event", out var np) ? np.GetString() ?? "event" : "event";
                var msg = root.TryGetProperty("message", out var mp) ? mp.GetString() ?? "" : "";
                var path = root.TryGetProperty("path", out var pp) ? pp.GetString() : null;
                OnEvent?.Invoke(new RecorderEvent(ok, name, msg, path));
            }
            catch
            {
                // ignora linhas não-JSON
            }
        }
    }

    public void Dispose()
    {
        try { if (_process?.HasExited == false) { _process.StandardInput.WriteLine("{\"cmd\":\"shutdown\"}"); _process.WaitForExit(1500); } }
        catch { }
        _process?.Dispose();
    }
}
