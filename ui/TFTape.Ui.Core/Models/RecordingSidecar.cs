using System.Text.Json;

namespace TFTape.Ui.Models;

/// <summary>Marcador de round gravado pelo core no sidecar (fase 2 preencherá a lista).</summary>
public sealed class RoundMarker
{
    public string Label { get; set; } = string.Empty;
    public long StartMs { get; set; }
    public string Source { get; set; } = string.Empty;
    public double Confidence { get; set; }
}

/// <summary>
/// Espelho do schema escrito pelo core C++ em &lt;video&gt;.mp4.tftape.json.
/// Campos desconhecidos são ignorados para manter compatibilidade com versões futuras.
/// </summary>
public sealed class RecordingSidecar
{
    public int SchemaVersion { get; set; }
    public string VideoFile { get; set; } = string.Empty;
    public string CreatedAtUtc { get; set; } = string.Empty;
    public long DurationMs { get; set; }
    public string Resolution { get; set; } = string.Empty;
    public int Fps { get; set; }
    public List<RoundMarker> Rounds { get; set; } = new();

    public static RecordingSidecar Parse(string json)
    {
        if (string.IsNullOrWhiteSpace(json)) throw new JsonException("Sidecar vazio");

        var options = new JsonSerializerOptions
        {
            PropertyNameCaseInsensitive = true,
            AllowTrailingCommas = true
        };
        var parsed = JsonSerializer.Deserialize<RecordingSidecar>(json, options)
                     ?? throw new JsonException("Sidecar inválido");

        parsed.Rounds ??= new List<RoundMarker>();
        if (parsed.SchemaVersion <= 0) throw new JsonException("schemaVersion inválido no sidecar");
        return parsed;
    }
}
