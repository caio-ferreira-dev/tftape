using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using TftVodRecorder.Ui.Models;
using TftVodRecorder.Ui.Services;

namespace TftVodRecorder.Ui.ViewModels;

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
    private bool _isRecording;
    private string _status = "Pronto";
    private string _outputDirectory = string.Empty;
    private string _resolution = "720p";
    private string _fps = "30";
    private bool _gameAudio = true;
    private bool _systemAudio;
    private bool _microphone;
    private string _lastFile = string.Empty;
    private string _error = string.Empty;

    public RecorderViewModel(IRecorderService recorder)
    {
        _recorder = recorder;
        _recorder.OnEvent += OnRecorderEvent;
        StartCommand = new RelayCommand(StartAsync, () => !IsRecording && !string.IsNullOrWhiteSpace(OutputDirectory));
        StopCommand = new RelayCommand(StopAsync, () => IsRecording);
    }

    public RelayCommand StartCommand { get; }
    public RelayCommand StopCommand { get; }

    public string[] Resolutions { get; } = { "720p", "1080p" };
    public string[] FpsOptions { get; } = { "30", "60" };

    public bool IsRecording { get => _isRecording; private set { _isRecording = value; OnPropertyChanged(); StartCommand.Raise(); StopCommand.Raise(); } }
    public string Status { get => _status; private set { _status = value; OnPropertyChanged(); } }
    public string OutputDirectory { get => _outputDirectory; set { _outputDirectory = value; OnPropertyChanged(); StartCommand.Raise(); } }
    public string Resolution { get => _resolution; set { _resolution = value; OnPropertyChanged(); } }
    public string Fps { get => _fps; set { _fps = value; OnPropertyChanged(); } }
    public bool GameAudio { get => _gameAudio; set { _gameAudio = value; OnPropertyChanged(); } }
    public bool SystemAudio { get => _systemAudio; set { _systemAudio = value; OnPropertyChanged(); } }
    public bool Microphone { get => _microphone; set { _microphone = value; OnPropertyChanged(); } }
    public string LastFile { get => _lastFile; private set { _lastFile = value; OnPropertyChanged(); } }
    public string Error { get => _error; private set { _error = value; OnPropertyChanged(); } }

    public async Task StartAsync()
    {
        Error = string.Empty;
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
        await _recorder.StartAsync(options);
    }

    public async Task StopAsync()
    {
        Status = "Parando...";
        await _recorder.StopAsync();
    }

    private void OnRecorderEvent(RecorderEvent e)
    {
        if (e.Name == "recording_started" && e.Ok) { IsRecording = true; Status = "Gravando (TFT)"; LastFile = e.Path ?? ""; }
        else if (e.Name == "recording_stopped") { IsRecording = false; Status = "Pronto"; if (!string.IsNullOrEmpty(e.Path)) LastFile = e.Path; }
        else if (!e.Ok) { IsRecording = false; Status = "Erro"; Error = e.Message; }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
