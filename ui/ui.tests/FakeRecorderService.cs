using TFTape.Ui.Models;
using TFTape.Ui.Services;

namespace TFTape.Ui.Tests;

internal sealed class FakeRecorderService : IRecorderService
{
    private readonly List<RecorderEvent> _raised = new();

    public event Action<RecorderEvent>? OnEvent;

    public List<RecordingOptions> StartCalls { get; } = new();
    public List<RecordingOptions> StopCalls { get; } = new();
    public int ShutdownCalls { get; private set; }
    public Exception? StartException { get; set; }
    public Exception? StopException { get; set; }
    public IReadOnlyList<RecorderEvent> RaisedEvents => _raised;

    public Task StartAsync(RecordingOptions options, CancellationToken ct = default)
    {
        StartCalls.Add(Clone(options));
        if (StartException != null) throw StartException;
        return Task.CompletedTask;
    }

    public Task StopAsync(CancellationToken ct = default)
    {
        StopCalls.Add(new RecordingOptions());
        if (StopException != null) throw StopException;
        return Task.CompletedTask;
    }

    public Task ShutdownAsync(CancellationToken ct = default)
    {
        ShutdownCalls++;
        return Task.CompletedTask;
    }

    public void Raise(bool ok, string name, string message = "", string? path = null,
                      string? sidecarPath = null, long? durationMs = null)
    {
        var e = new RecorderEvent(ok, name, message, path, sidecarPath, durationMs);
        _raised.Add(e);
        OnEvent?.Invoke(e);
    }

    private static RecordingOptions Clone(RecordingOptions o) => new()
    {
        OutputDirectory = o.OutputDirectory,
        Resolution = o.Resolution,
        Fps = o.Fps,
        GameAudio = o.GameAudio,
        SystemAudio = o.SystemAudio,
        Microphone = o.Microphone
    };
}
