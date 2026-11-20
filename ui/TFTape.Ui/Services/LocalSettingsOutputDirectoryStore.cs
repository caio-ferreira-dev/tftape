using Windows.Storage;

namespace TFTape.Ui.Services;

/// <summary>Guarda a última pasta de VODs nas configurações locais do WinUI 3.</summary>
public sealed class LocalSettingsOutputDirectoryStore : IOutputDirectoryStore
{
    private const string Key = "LastOutputDirectory";

    public string Load()
    {
        try
        {
            var values = ApplicationData.Current.LocalSettings.Values;
            return values.TryGetValue(Key, out var value) ? value as string ?? string.Empty : string.Empty;
        }
        catch
        {
            // Configurações corrompidas ou indisponíveis: começa sem pasta lembrada.
            return string.Empty;
        }
    }

    public void Save(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return;
        try
        {
            ApplicationData.Current.LocalSettings.Values[Key] = path;
        }
        catch
        {
            // Persistência é conveniência; não deve impedir a gravação.
        }
    }
}
