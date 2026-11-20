using TFTape.Ui.Services;
using TFTape.Ui.ViewModels;
using Xunit;

namespace TFTape.Ui.Tests;

public sealed class RecordingUiTests
{
    [Fact]
    public void StartCommandRequiresOutputDirectory()
    {
        var vm = new RecorderViewModel(new FakeRecorderService());

        Assert.False(vm.StartCommand.CanExecute(null));
        vm.OutputDirectory = @"C:\Vods";
        Assert.True(vm.StartCommand.CanExecute(null));
        Assert.False(vm.StopCommand.CanExecute(null));
    }

    [Fact]
    public async Task StartSendsCurrentOptionsAndShowsDetectingStatus()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service)
        {
            OutputDirectory = @"C:\Vods",
            Resolution = "1080p",
            Fps = "60",
            GameAudio = true,
            SystemAudio = true,
            Microphone = true
        };

        await vm.StartAsync();

        var sent = Assert.Single(service.StartCalls);
        Assert.Equal(@"C:\Vods", sent.OutputDirectory);
        Assert.Equal("1080p", sent.Resolution);
        Assert.Equal("60", sent.Fps);
        Assert.True(sent.GameAudio);
        Assert.True(sent.SystemAudio);
        Assert.True(sent.Microphone);
        Assert.Equal("Detectando janela do TFT...", vm.Status);
        Assert.False(vm.IsRecording);
    }

    [Fact]
    public void RecordingStartedEventUpdatesStateAndCommands()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service) { OutputDirectory = @"C:\Vods" };

        service.Raise(true, "recording_started", "Gravação iniciada", @"C:\Vods\jogo.mp4");

        Assert.True(vm.IsRecording);
        Assert.Equal("Gravando (TFT)", vm.Status);
        Assert.Equal(@"C:\Vods\jogo.mp4", vm.LastFile);
        Assert.False(vm.StartCommand.CanExecute(null));
        Assert.True(vm.StopCommand.CanExecute(null));
    }

    [Fact]
    public void WarningEventDoesNotStopRecording()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);

        service.Raise(true, "recording_started", "", @"C:\Vods\jogo.mp4");
        service.Raise(true, "warning", "Microfone indisponível; gravando sem essa fonte.");

        Assert.True(vm.IsRecording);
        Assert.Equal("Microfone indisponível; gravando sem essa fonte.", vm.Warning);
        Assert.Equal(string.Empty, vm.Error);
    }

    [Fact]
    public void StoppedEventClearsWarningAndUpdatesFile()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);

        service.Raise(true, "warning", "Aviso temporário");
        service.Raise(true, "recording_stopped", "Gravação finalizada", @"C:\Vods\final.mp4");

        Assert.False(vm.IsRecording);
        Assert.Equal("Pronto - gravação finalizada", vm.Status);
        Assert.Equal(string.Empty, vm.Warning);
        Assert.Equal(string.Empty, vm.Error);
        Assert.Equal(@"C:\Vods\final.mp4", vm.LastFile);
    }

    [Fact]
    public void FailedEventSetsErrorState()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);

        service.Raise(false, "error", "Janela do TFT não encontrada");

        Assert.False(vm.IsRecording);
        Assert.Equal("Erro", vm.Status);
        Assert.Equal("Janela do TFT não encontrada", vm.Error);
    }

    [Fact]
    public async Task StartExceptionIsShownAsError()
    {
        var service = new FakeRecorderService { StartException = new InvalidOperationException("processo ausente") };
        var vm = new RecorderViewModel(service) { OutputDirectory = @"C:\Vods" };

        await vm.StartAsync();

        Assert.False(vm.IsRecording);
        Assert.Equal("Erro", vm.Status);
        Assert.Equal("processo ausente", vm.Error);
    }

    [Fact]
    public async Task StopSendsCommandAndHandlesException()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);

        await vm.StopAsync();

        Assert.Single(service.StopCalls);
        Assert.Equal("Parando...", vm.Status);

        service.StopException = new InvalidOperationException("pipe fechado");
        await vm.StopAsync();

        Assert.Equal("Erro", vm.Status);
        Assert.Equal("pipe fechado", vm.Error);
    }

    [Fact]
    public async Task ShutdownDelegatesToService()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);

        await vm.ShutdownAsync();

        Assert.Equal(1, service.ShutdownCalls);
    }

    [Fact]
    public void EventsAreMarshalledThroughDispatcher()
    {
        var service = new FakeRecorderService();
        var dispatcher = new RecordingUiTests_Dispatcher();
        var vm = new RecorderViewModel(service, dispatcher);

        service.Raise(true, "recording_started", "", @"C:\Vods\jogo.mp4");

        Assert.Equal(1, dispatcher.RunCount);
        Assert.False(vm.IsRecording); // ainda não executado no "thread da UI"

        dispatcher.Flush();

        Assert.True(vm.IsRecording);
        Assert.Equal(@"C:\Vods\jogo.mp4", vm.LastFile);
    }

    [Fact]
    public void PropertyChangedIsRaisedForBoundProperties()
    {
        var service = new FakeRecorderService();
        var vm = new RecorderViewModel(service);
        var changed = new List<string?>();
        vm.PropertyChanged += (_, e) => changed.Add(e.PropertyName);

        vm.OutputDirectory = @"C:\Vods";
        vm.Resolution = "1080p";
        vm.Microphone = true;

        Assert.Contains(nameof(RecorderViewModel.OutputDirectory), changed);
        Assert.Contains(nameof(RecorderViewModel.Resolution), changed);
        Assert.Contains(nameof(RecorderViewModel.Microphone), changed);
    }

    [Fact]
    public void LoadsLastOutputDirectoryOnStartup()
    {
        var store = new InMemoryOutputDirectoryStore(@"C:\UltimaPasta");
        var vm = new RecorderViewModel(new FakeRecorderService(), outputDirectoryStore: store);

        Assert.Equal(@"C:\UltimaPasta", vm.OutputDirectory);
        Assert.True(vm.StartCommand.CanExecute(null));
    }

    [Fact]
    public void SavesOutputDirectoryWhenItChanges()
    {
        var store = new InMemoryOutputDirectoryStore();
        var vm = new RecorderViewModel(new FakeRecorderService(), outputDirectoryStore: store);

        vm.OutputDirectory = @"C:\Vods";
        vm.OutputDirectory = @"C:\Vods";
        vm.OutputDirectory = "   ";

        Assert.Equal(@"C:\Vods", store.Load());
        Assert.Equal(1, store.SaveCount);
    }

    private sealed class RecordingUiTests_Dispatcher : IUiDispatcher
    {
        private readonly List<Action> _actions = new();

        public int RunCount { get; private set; }

        public void Run(Action action)
        {
            RunCount++;
            _actions.Add(action);
        }

        public void Flush()
        {
            foreach (var action in _actions) action();
            _actions.Clear();
        }
    }
}
