using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using TFTape.Ui.Models;
using TFTape.Ui.Services;

namespace TFTape.Ui.ViewModels;

public sealed class RelayCommand : ICommand
{
    private readonly Func<Task> _execute;
    private readonly Func<bool>? _can;
    public RelayCommand(Func<Task> execute, Func<bool>? can = null) { _execute = execute; _can = can; }
    public bool CanExecute(object? parameter) => _can?.Invoke() ?? true;
    public async void Execute(object? parameter) => await _execute();
    public event EventHandler? CanExecuteChanged;
    public void Raise() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}

public sealed class RecorderViewModel : INotifyPropertyChanged
{
    private readonly IRecorderService _recorder;
    private readonly IUiDispatcher _dispatcher;
    private readonly IOutputDirectoryStore _outputDirectoryStore;
    private bool _isRecording;
    private string _status = "Pronto";
    private string _outputDirectory;
    private string _resolution = "1080p";
    private string _fps = "30";
    private bool _gameAudio = true;
    private bool _systemAudio;
    private bool _microphone;
    private string _lastFile = string.Empty;
    private string _lastSidecar = string.Empty;
    private long? _lastDurationMs;
    private string _error = string.Empty;
    private string _warning = string.Empty;

    public RecorderViewModel(IRecorderService recorder, IUiDispatcher? dispatcher = null, IOutputDirectoryStore? outputDirectoryStore = null)
    {
        _recorder = recorder;
        _dispatcher = dispatcher ?? new InlineUiDispatcher();
        _outputDirectoryStore = outputDirectoryStore ?? new InMemoryOutputDirectoryStore();
        _outputDirectory = _outputDirectoryStore.Load() ?? string.Empty;
        _recorder.OnEvent += OnRecorderEvent;
        StartCommand = new RelayCommand(StartAsync, () => !IsRecording && !string.IsNullOrWhiteSpace(OutputDirectory));
        StopCommand = new RelayCommand(StopAsync, () => IsRecording);
    }

    public RelayCommand StartCommand { get; }
    public RelayCommand StopCommand { get; }

    public string[] Resolutions { get; } = { "720p", "1080p" };
    public string[] FpsOptions { get; } = { "30", "60" };

    public bool IsRecording { get => _isRecording; private set { if (_isRecording == value) return; _isRecording = value; OnPropertyChanged(); StartCommand.Raise(); StopCommand.Raise(); } }
    public string Status { get => _status; private set { if (_status == value) return; _status = value; OnPropertyChanged(); } }
    public string OutputDirectory
    {
        get => _outputDirectory;
        set
        {
            var next = value ?? string.Empty;
            if (_outputDirectory == next) return;
            _outputDirectory = next;
            OnPropertyChanged();
            StartCommand.Raise();
            _outputDirectoryStore.Save(next);
        }
    }
    public string Resolution { get => _resolution; set { if (_resolution == value) return; _resolution = value; OnPropertyChanged(); } }
    public string Fps { get => _fps; set { if (_fps == value) return; _fps = value; OnPropertyChanged(); } }
    public bool GameAudio { get => _gameAudio; set { if (_gameAudio == value) return; _gameAudio = value; OnPropertyChanged(); } }
    public bool SystemAudio { get => _systemAudio; set { if (_systemAudio == value) return; _systemAudio = value; OnPropertyChanged(); } }
    public bool Microphone { get => _microphone; set { if (_microphone == value) return; _microphone = value; OnPropertyChanged(); } }
    public string LastFile { get => _lastFile; private set { if (_lastFile == value) return; _lastFile = value; OnPropertyChanged(); } }
    public string LastSidecar { get => _lastSidecar; private set { if (_lastSidecar == value) return; _lastSidecar = value; OnPropertyChanged(); } }
    public long? LastDurationMs { get => _lastDurationMs; private set { if (_lastDurationMs == value) return; _lastDurationMs = value; OnPropertyChanged(); } }
    public string Error { get => _error; private set { if (_error == value) return; _error = value; OnPropertyChanged(); } }
    public string Warning { get => _warning; private set { if (_warning == value) return; _warning = value; OnPropertyChanged(); } }

    /// <summary>Deve ser chamado ao fechar a janela para finalizar o MP4.</summary>
    public Task ShutdownAsync() => _recorder.ShutdownAsync();

    public async Task StartAsync()
    {
        Error = string.Empty;
        Warning = string.Empty;
        Status = "Detectando janela do TFT...";
        var options = new RecordingOptions
        {
            OutputDirectory = OutputDirectory,
            Resolution = Resolution,
            Fps = Fps,
            GameAudio = GameAudio,
            SystemAudio = SystemAudio,
            Microphone = Microphone
        };
        try { await _recorder.StartAsync(options); }
        catch (Exception ex) { IsRecording = false; Status = "Erro"; Error = ex.Message; }
    }

    public async Task StopAsync()
    {
        Status = "Parando...";
        try { await _recorder.StopAsync(); }
        catch (Exception ex) { Status = "Erro"; Error = ex.Message; }
    }

    private void OnRecorderEvent(RecorderEvent e) => _dispatcher.Run(() => ApplyEvent(e));

    private void ApplyEvent(RecorderEvent e)
    {
        if (e.Name == "recording_started" && e.Ok)
        {
            IsRecording = true;
            Status = "Gravando (TFT)";
            LastFile = e.Path ?? "";
            LastSidecar = string.Empty;
            LastDurationMs = null;
            Error = string.Empty;
        }
        else if (e.Name == "warning" && e.Ok)
        {
            // Fontes opcionais indisponíveis: a gravação segue, mas o usuário é avisado.
            Warning = e.Message;
        }
        else if (e.Name == "recording_stopped")
        {
            IsRecording = false;
            Status = "Pronto - gravação finalizada";
            Warning = string.Empty;
            Error = string.Empty;
            if (!string.IsNullOrEmpty(e.Path)) LastFile = e.Path;
            if (!string.IsNullOrEmpty(e.SidecarPath)) LastSidecar = e.SidecarPath;
            if (e.DurationMs.HasValue) LastDurationMs = e.DurationMs;
        }
        else if (!e.Ok)
        {
            IsRecording = false;
            Status = "Erro";
            Error = e.Message;
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
