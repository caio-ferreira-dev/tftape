using TFTape.Ui.Models;

namespace TFTape.Ui.Services;

/// <summary>Varredura de uma pasta em busca de VODs (.mp4) e seus sidecars.</summary>
public interface IVodLibraryService
{
    Task<IReadOnlyList<VodEntry>> ScanAsync(string folder, CancellationToken ct = default);
}
