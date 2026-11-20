namespace TFTape.Ui.Services;

/// <summary>Store simples para testes e para o caso sem persistência configurada.</summary>
public sealed class InMemoryOutputDirectoryStore : IOutputDirectoryStore
{
    private string _value;

    public InMemoryOutputDirectoryStore(string initial = "") => _value = initial;

    public int SaveCount { get; private set; }

    public string Load() => _value;

    public void Save(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return;
        _value = path;
        SaveCount++;
    }
}
