using System.Text.Json;
using TFTape.Ui.Models;

namespace TFTape.Ui.Services;

/// <summary>
/// Lê os MP4 de uma pasta e associa cada um ao sidecar &lt;arquivo&gt;.mp4.tftape.json.
/// Um sidecar ausente ou corrompido nunca impede o VOD de aparecer: o erro é informado
/// no próprio objeto para a UI mostrar um aviso.
/// </summary>
public sealed class VodLibraryService : IVodLibraryService
{
    public Task<IReadOnlyList<VodEntry>> ScanAsync(string folder, CancellationToken ct = default)
    {
        if (string.IsNullOrWhiteSpace(folder) || !Directory.Exists(folder))
        {
            return Task.FromResult<IReadOnlyList<VodEntry>>(Array.Empty<VodEntry>());
        }

        var entries = new List<VodEntry>();
        try
        {
            foreach (var mp4 in Directory.EnumerateFiles(folder, "*.mp4", SearchOption.TopDirectoryOnly))
            {
                ct.ThrowIfCancellationRequested();
                entries.Add(BuildEntry(mp4));
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or OperationCanceledException)
        {
            // Pasta removida/sem permissão: devolve o que já foi lido em vez de quebrar a UI.
        }

        entries.Sort((a, b) => string.CompareOrdinal(SortKey(b), SortKey(a)));
        return Task.FromResult<IReadOnlyList<VodEntry>>(entries);
    }

    private static string SortKey(VodEntry entry) =>
        string.IsNullOrEmpty(entry.CreatedAtUtc)
            ? entry.FileDateUtc.ToString("o")
            : entry.CreatedAtUtc;

    private static VodEntry BuildEntry(string mp4Path)
    {
        var entry = new VodEntry
        {
            FilePath = mp4Path,
            FileDateUtc = GetFileDateUtc(mp4Path)
        };

        var sidecarPath = mp4Path + ".tftape.json";
        if (!File.Exists(sidecarPath)) return entry;

        entry.SidecarPath = sidecarPath;
        try
        {
            var sidecar = RecordingSidecar.Parse(File.ReadAllText(sidecarPath));
            entry.SchemaVersion = sidecar.SchemaVersion;
            entry.VideoFile = sidecar.VideoFile;
            entry.CreatedAtUtc = sidecar.CreatedAtUtc;
            entry.DurationMs = sidecar.DurationMs;
            entry.Resolution = sidecar.Resolution;
            entry.Fps = sidecar.Fps;
            entry.Rounds = sidecar.Rounds;
            entry.HasSidecar = true;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException
                                      or JsonException or ArgumentException)
        {
            entry.LoadError = "Metadados inválidos: " + ex.Message;
        }
        return entry;
    }

    private static DateTime GetFileDateUtc(string path)
    {
        try { return File.GetLastWriteTimeUtc(path); }
        catch { return DateTime.MinValue; }
    }
}
