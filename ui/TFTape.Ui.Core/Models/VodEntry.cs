using System.Globalization;

namespace TFTape.Ui.Models;

/// <summary>Um VOD descoberto na pasta: o MP4 e, se existir, os metadados do sidecar.</summary>
public sealed class VodEntry
{
    public string FilePath { get; set; } = string.Empty;
    public string SidecarPath { get; set; } = string.Empty;
    public bool HasSidecar { get; set; }
    public string LoadError { get; set; } = string.Empty;

    public int SchemaVersion { get; set; }
    public string VideoFile { get; set; } = string.Empty;
    public string CreatedAtUtc { get; set; } = string.Empty;
    public long DurationMs { get; set; }
    public string Resolution { get; set; } = string.Empty;
    public int Fps { get; set; }
    public IReadOnlyList<RoundMarker> Rounds { get; set; } = Array.Empty<RoundMarker>();

    public DateTime FileDateUtc { get; set; }

    public string FileName => string.IsNullOrEmpty(FilePath) ? string.Empty : Path.GetFileName(FilePath);

    public string MetaLine
    {
        get
        {
            var profile = string.IsNullOrEmpty(Resolution) || Fps <= 0
                ? string.Empty
                : $" · {Resolution} {Fps} fps";
            var duration = DurationMs > 0
                ? $" · {TimeSpan.FromMilliseconds(DurationMs):hh\\:mm\\:ss}"
                : string.Empty;
            var rounds = Rounds.Count > 0 ? $" · {Rounds.Count} round(s)" : string.Empty;
            return $"{CreatedDisplay}{profile}{duration}{rounds}";
        }
    }

    private string CreatedDisplay
    {
        get
        {
            if (DateTimeOffset.TryParse(CreatedAtUtc, CultureInfo.InvariantCulture,
                    DateTimeStyles.AssumeUniversal, out var parsed))
            {
                return parsed.LocalDateTime.ToString("dd/MM/yyyy HH:mm", CultureInfo.CurrentCulture);
            }
            return FileDateUtc.ToLocalTime().ToString("dd/MM/yyyy HH:mm", CultureInfo.CurrentCulture);
        }
    }
}
