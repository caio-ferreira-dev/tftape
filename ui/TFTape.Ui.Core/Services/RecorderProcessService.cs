using System.Diagnostics;
using System.Text;
using System.Text.Json;
using TFTape.Ui.Models;

namespace TFTape.Ui.Services;

/// <summary>
/// Controla o processo recorder.exe via stdin/stdout (JSON-lines).
/// O executável deve estar disponível no caminho configurado (ex.: ao lado da UI).
/// </summary>
public sealed class RecorderProcessService : IRecorderService, IDisposable
{
    private const int MaxStderrLines = 20;
    private readonly string _recorderPath;
    private readonly object _stderrLock = new();
    private readonly Queue<string> _stderrLines = new();
    private Process? _process;

    public RecorderProcessService(string? recorderPath = null)
    {
        _recorderPath = recorderPath ?? Path.Combine(AppContext.BaseDirectory, "recorder.exe");
    }

    public event Action<RecorderEvent>? OnEvent;

    public async Task StartAsync(RecordingOptions options, CancellationToken ct = default)
    {
        if (!EnsureProcess()) return;
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
        await SendAsync(new { cmd = "stop" }, ct);
    }

    /// <summary>
    /// Envia shutdown, aguarda o core finalizar o MP4 e, se não responder, mata a árvore
    /// de processos. Chamado no fechamento da janela.
    /// </summary>
    public async Task ShutdownAsync(CancellationToken ct = default)
    {
        var process = _process;
        if (process == null) return;
        try
        {
            if (!process.HasExited)
            {
                try
                {
                    if (process.StandardInput != null)
                    {
                        await process.StandardInput.WriteLineAsync("{\"cmd\":\"shutdown\"}").ConfigureAwait(false);
                        await process.StandardInput.FlushAsync().ConfigureAwait(false);
                    }
                }
                catch { /* pipe já fechado: segue para o timeout/kill */ }

                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(TimeSpan.FromSeconds(5));
                try
                {
                    await process.WaitForExitAsync(cts.Token).ConfigureAwait(false);
                }
                catch (OperationCanceledException)
                {
                    TryKill(process);
                }
            }
        }
        catch
        {
            TryKill(process);
        }
        finally
        {
            if (ReferenceEquals(_process, process)) _process = null;
            process.Dispose();
        }
    }

    private bool EnsureProcess()
    {
        if (_process is { HasExited: false }) return true;

        if (_process != null)
        {
            _process.Dispose();
            _process = null;
        }
        if (!File.Exists(_recorderPath))
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", $"recorder.exe não encontrado em: {_recorderPath}"));
            return false;
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
            // O core emite JSON com mensagens em português em UTF-8; sem isso o .NET
            // usa a code page do console e gera mojibake (ex.: "indisponÃvel").
            StandardInputEncoding = Encoding.UTF8,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
            CreateNoWindow = true
        };

        Process? process;
        try
        {
            process = Process.Start(psi);
        }
        catch
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", "Falha ao iniciar recorder.exe"));
            return false;
        }
        if (process == null)
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", "Falha ao iniciar recorder.exe"));
            return false;
        }
        _process = process;

        // O core escreve diagnóstico no stderr. Sem drenagem o pipe enche (~4 KB) e
        // bloqueia o processo, travando também stdin/stdout.
        try
        {
            process.ErrorDataReceived += OnErrorDataReceived;
            process.BeginErrorReadLine();
        }
        catch { /* processo já encerrado: o loop de stdout cuida do erro */ }

        _ = Task.Run(() => ReadLoopAsync(process));
        return true;
    }

    private void OnErrorDataReceived(object sender, DataReceivedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(e.Data)) return;
        lock (_stderrLock)
        {
            _stderrLines.Enqueue(e.Data);
            while (_stderrLines.Count > MaxStderrLines) _stderrLines.Dequeue();
        }
    }

    private string RecentStderr()
    {
        lock (_stderrLock)
        {
            return string.Join(" | ", _stderrLines);
        }
    }

    private async Task SendAsync(object payload, CancellationToken ct)
    {
        var process = _process;
        if (process is null || process.HasExited || process.StandardInput == null)
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", "Processo do gravador não está ativo"));
            return;
        }
        var json = JsonSerializer.Serialize(payload);
        try
        {
            await process.StandardInput.WriteLineAsync(json.AsMemory(), ct).ConfigureAwait(false);
            await process.StandardInput.FlushAsync(ct).ConfigureAwait(false);
        }
        catch
        {
            OnEvent?.Invoke(new RecorderEvent(false, "error", "Falha ao comunicar com o gravador"));
        }
    }

    private async Task ReadLoopAsync(Process process)
    {
        if (process.StandardOutput == null) return;
        try
        {
            while (!process.HasExited)
            {
                var line = await process.StandardOutput.ReadLineAsync().ConfigureAwait(false);
                if (line == null) break;
                try
                {
                    using var doc = JsonDocument.Parse(line);
                    var root = doc.RootElement;
                    var ok = root.TryGetProperty("ok", out var okp) && okp.GetBoolean();
                    var name = root.TryGetProperty("event", out var np) ? np.GetString() ?? "event" : "event";
                    var msg = root.TryGetProperty("message", out var mp) ? mp.GetString() ?? "" : "";
                    var path = root.TryGetProperty("path", out var pp) ? pp.GetString() : null;
                    var sidecarPath = root.TryGetProperty("sidecar_path", out var sp) ? sp.GetString() : null;
                    long? durationMs = null;
                    if (root.TryGetProperty("duration_ms", out var dp) && dp.TryGetInt64(out var parsedDuration))
                    {
                        durationMs = parsedDuration;
                    }
                    OnEvent?.Invoke(new RecorderEvent(ok, name, msg, path, sidecarPath, durationMs));
                }
                catch
                {
                    // ignora linhas não-JSON
                }
            }
        }
        catch
        {
            // pipe fechado durante o shutdown
        }

        try
        {
            process.WaitForExit(0);
            if (process.ExitCode != 0)
            {
                var details = RecentStderr();
                var message = details.Length > 0
                    ? $"Gravador encerrou com código {process.ExitCode}: {details}"
                    : $"Gravador encerrou inesperadamente (código {process.ExitCode})";
                OnEvent?.Invoke(new RecorderEvent(false, "error", message));
            }
        }
        catch
        {
            // processo já disposed
        }
    }

    private static void TryKill(Process process)
    {
        try { if (!process.HasExited) process.Kill(entireProcessTree: true); }
        catch { }
    }

    public void Dispose()
    {
        var process = _process;
        _process = null;
        if (process == null) return;
        try
        {
            if (!process.HasExited)
            {
                try
                {
                    process.StandardInput?.WriteLine("{\"cmd\":\"shutdown\"}");
                    process.StandardInput?.Flush();
                }
                catch { /* segue para o timeout/kill */ }
                if (!process.WaitForExit(3000)) TryKill(process);
            }
        }
        catch
        {
            TryKill(process);
        }
        finally
        {
            process.Dispose();
        }
    }
}
