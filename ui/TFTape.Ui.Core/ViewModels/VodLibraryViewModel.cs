using System.ComponentModel;
using System.Runtime.CompilerServices;
using TFTape.Ui.Models;
using TFTape.Ui.Services;

namespace TFTape.Ui.ViewModels;

/// <summary>
/// Estado da aba VODs. A reprodução em si fica na janela WinUI; o ViewModel apenas
/// publica eventos com o caminho do vídeo e o instante do round, o que mantém a lógica
/// testável sem MediaPlayerElement.
/// </summary>
public sealed class VodLibraryViewModel : INotifyPropertyChanged
{
    private const string VodFolderSetting = "vod_directory.txt";

    private readonly IVodLibraryService _library;
    private readonly IOutputDirectoryStore _folderStore;
    private IReadOnlyList<VodEntry> _vods = Array.Empty<VodEntry>();
    private VodEntry? _selectedVod;
    private string _folder = string.Empty;
    private string _status = "Escolha uma pasta de VODs.";
    private string _error = string.Empty;
    private bool _isBusy;

    public VodLibraryViewModel(
        IVodLibraryService library,
        IOutputDirectoryStore? folderStore = null,
        IOutputDirectoryStore? recordingFolderStore = null,
        string? localAppDataPath = null)
    {
        _library = library;
        _folderStore = folderStore ?? BuildDefaultStore(localAppDataPath);

        var saved = _folderStore.Load();
        // Na primeira execução, a biblioteca começa na pasta usada pela gravação.
        _folder = string.IsNullOrWhiteSpace(saved)
            ? recordingFolderStore?.Load() ?? string.Empty
            : saved;

        RefreshCommand = new RelayCommand(RefreshAsync, () => !IsBusy);
    }

    private static LocalFileOutputDirectoryStore BuildDefaultStore(string? localAppDataPath)
    {
        var basePath = localAppDataPath
                       ?? Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return new LocalFileOutputDirectoryStore(
            Path.Combine(basePath, "TFTape", VodFolderSetting));
    }

    public RelayCommand RefreshCommand { get; }

    /// <summary>Pede à janela para abrir este MP4 no player.</summary>
    public event Action<string>? PlayRequested;

    /// <summary>Pede à janela para posicionar o player neste instante (ms).</summary>
    public event Action<long>? SeekRequested;

    public string Folder
    {
        get => _folder;
        set
        {
            var next = value ?? string.Empty;
            if (_folder == next) return;
            _folder = next;
            OnPropertyChanged();
            _folderStore.Save(next);
            RefreshCommand.Raise();
        }
    }

    public IReadOnlyList<VodEntry> Vods
    {
        get => _vods;
        private set { if (ReferenceEquals(_vods, value)) return; _vods = value; OnPropertyChanged(); }
    }

    public VodEntry? SelectedVod
    {
        get => _selectedVod;
        set
        {
            if (ReferenceEquals(_selectedVod, value)) return;
            _selectedVod = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(SelectedFileName));
            OnPropertyChanged(nameof(SelectedMetaLine));
            OnPropertyChanged(nameof(SelectedRounds));
            OnPropertyChanged(nameof(RoundsHint));
            if (value != null) PlayRequested?.Invoke(value.FilePath);
        }
    }

    public string SelectedFileName => SelectedVod?.FileName ?? "Nenhum VOD selecionado";
    public string SelectedMetaLine => SelectedVod?.MetaLine ?? string.Empty;
    public IReadOnlyList<RoundMarker> SelectedRounds => SelectedVod?.Rounds ?? Array.Empty<RoundMarker>();

    public string RoundsHint => SelectedVod == null
        ? "Selecione um VOD para começar."
        : SelectedRounds.Count == 0
            ? "Nenhum round detectado nesta gravação."
            : $"{SelectedRounds.Count} round(s) — clique em um para ir até o instante.";

    public bool IsBusy
    {
        get => _isBusy;
        private set
        {
            if (_isBusy == value) return;
            _isBusy = value;
            OnPropertyChanged();
            RefreshCommand.Raise();
        }
    }

    public string Status { get => _status; private set { if (_status == value) return; _status = value; OnPropertyChanged(); } }
    public string Error { get => _error; private set { if (_error == value) return; _error = value; OnPropertyChanged(); } }

    /// <summary>Usado pelo code-behind quando o MediaPlayer recusa um arquivo.</summary>
    public void ReportError(string message) => Error = message;

    public async Task RefreshAsync()
    {
        if (IsBusy) return;
        IsBusy = true;
        Error = string.Empty;
        var folderSnapshot = Folder;

        try
        {
            var found = await _library.ScanAsync(folderSnapshot);
            var previousPath = SelectedVod?.FilePath;

            Vods = found;
            if (string.IsNullOrWhiteSpace(folderSnapshot))
                Status = "Escolha uma pasta de VODs.";
            else if (found.Count == 0)
                Status = "Nenhum VOD encontrado nesta pasta.";
            else
                Status = $"{found.Count} VOD(s) encontrado(s).";

            var previous = previousPath == null
                ? null
                : found.FirstOrDefault(v => v.FilePath == previousPath);
            SelectedVod = previous ?? (found.Count > 0 ? found[0] : null);
        }
        catch (Exception ex)
        {
            Error = ex.Message;
            Status = "Falha ao ler a pasta de VODs.";
        }
        finally
        {
            IsBusy = false;
        }
    }

    /// <summary>Valida o intervalo do marcador antes de pedir o seek ao player.</summary>
    public void JumpToRound(RoundMarker? round)
    {
        if (round == null || SelectedVod == null) return;
        if (round.StartMs < 0) return;
        if (SelectedVod.DurationMs > 0 && round.StartMs > SelectedVod.DurationMs) return;
        SeekRequested?.Invoke(round.StartMs);
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null) =>
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
