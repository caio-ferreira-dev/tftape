namespace TFTape.Ui.Services;

/// <summary>Persiste a última pasta usada para salvar VODs.</summary>
public interface IOutputDirectoryStore
{
    string Load();
    void Save(string path);
}
