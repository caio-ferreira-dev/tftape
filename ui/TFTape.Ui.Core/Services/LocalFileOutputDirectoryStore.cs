using System.Text;

namespace TFTape.Ui.Services;

/// <summary>
/// Persiste a última pasta em %LOCALAPPDATA%\TFTape\output_directory.txt.
/// Funciona em app WinUI 3 unpackaged, onde ApplicationData exige identidade de pacote.
/// </summary>
public sealed class LocalFileOutputDirectoryStore : IOutputDirectoryStore
{
    private readonly string _path;

    public LocalFileOutputDirectoryStore(string? path = null)
    {
        _path = path ?? Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "TFTape",
            "output_directory.txt");
    }

    public string Load()
    {
        try
        {
            return File.Exists(_path) ? File.ReadAllText(_path, Encoding.UTF8).Trim() : string.Empty;
        }
        catch
        {
            // Preferência corrompida ou ilegível: começa sem pasta lembrada.
            return string.Empty;
        }
    }

    public void Save(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return;
        try
        {
            var directory = Path.GetDirectoryName(_path);
            if (!string.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
            File.WriteAllText(_path, path, Encoding.UTF8);
        }
        catch
        {
            // Persistência é conveniência; não deve impedir a gravação.
        }
    }
}
