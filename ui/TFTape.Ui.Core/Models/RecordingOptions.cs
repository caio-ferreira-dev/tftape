namespace TFTape.Ui.Models;

public sealed class RecordingOptions
{
    public string OutputDirectory { get; set; } = string.Empty;
    public string Resolution { get; set; } = "1080p"; // 720p | 1080p
    public string Fps { get; set; } = "30";          // 30 | 60
    public bool GameAudio { get; set; } = true;      // padrão
    public bool SystemAudio { get; set; } = false;   // outras janelas/sistema
    public bool Microphone { get; set; } = false;
}
