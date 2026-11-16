using TftVodRecorder.Ui.Models;

namespace TftVodRecorder.Ui.Services;

public sealed record RecorderEvent(bool Ok, string Name, string Message, string? Path = null);

public interface IRecorderService
{
    event Action<RecorderEvent>? OnEvent;
    Task StartAsync(RecordingOptions options, CancellationToken ct = default);
    Task StopAsync(CancellationToken ct = default);
}
