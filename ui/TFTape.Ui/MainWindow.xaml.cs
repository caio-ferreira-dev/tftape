using System;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using TFTape.Ui.Models;
using TFTape.Ui.Services;
using TFTape.Ui.ViewModels;
using Windows.Media.Core;
using Windows.Media.Playback;
using WinRT.Interop;

namespace TFTape.Ui;

public sealed partial class MainWindow : Window
{
    public RecorderViewModel ViewModel { get; }
    public VodLibraryViewModel VodLibrary { get; }

    private readonly FolderPickerService _folderPicker = new();
    private readonly RecorderProcessService _recorderService = new();
    private MediaPlayer? _vodPlayer;

    public MainWindow()
    {
        var dispatcher = new UiDispatcher(DispatcherQueue.GetForCurrentThread());
        var outputStore = new LocalFileOutputDirectoryStore();

        ViewModel = new RecorderViewModel(_recorderService, dispatcher, outputStore);
        VodLibrary = new VodLibraryViewModel(
            new VodLibraryService(),
            folderStore: null, // usa %LOCALAPPDATA%\TFTape\vod_directory.txt
            recordingFolderStore: outputStore,
            localAppDataPath: null);

        InitializeComponent();

        VodLibrary.PlayRequested += OnPlayRequested;
        VodLibrary.SeekRequested += OnSeekRequested;
        Loaded += OnLoaded;
        // WinUI 3 expõe Window.Closed (AppWindow.Closing é a versão com cancellation).
        Closed += OnClosed;
    }

    private async void OnLoaded(object sender, RoutedEventArgs e) => await VodLibrary.RefreshAsync();

    private async void OnTabChanged(object sender, SelectionChangedEventArgs e)
    {
        // Atualiza a biblioteca sempre que o usuário entra na aba, para ver gravações novas.
        if (MainTabs.SelectedIndex == 1) await VodLibrary.RefreshAsync();
    }

    private void OnClosed(object sender, WindowEventArgs args)
    {
        // Envia shutdown e aguarda a finalização do MP4 antes de encerrar a UI.
        // O serviço mata a árvore de processos se o core não responder.
        _recorderService.Dispose();
        DisposeVodPlayer();
    }

    private async void OnPickFolder(object sender, RoutedEventArgs e)
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        var path = await _folderPicker.PickFolderAsync(hwnd);
        if (!string.IsNullOrWhiteSpace(path))
        {
            ViewModel.OutputDirectory = path;
        }
    }

    private async void OnPickVodFolder(object sender, RoutedEventArgs e)
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        var path = await _folderPicker.PickFolderAsync(hwnd);
        if (!string.IsNullOrWhiteSpace(path))
        {
            VodLibrary.Folder = path;
            await VodLibrary.RefreshAsync();
        }
    }

    private void OnRoundClick(object sender, RoutedEventArgs e)
    {
        if ((sender as Button)?.DataContext is RoundMarker round)
        {
            VodLibrary.JumpToRound(round);
        }
    }

    private void OnPlayRequested(string path)
    {
        try
        {
            _vodPlayer ??= CreateVodPlayer();
            _vodPlayer.Source = MediaSource.CreateFromUri(new Uri(path));
        }
        catch (Exception ex)
        {
            VodLibrary.ReportError("Não foi possível abrir o VOD: " + ex.Message);
        }
    }

    private void OnSeekRequested(long startMs)
    {
        if (_vodPlayer == null) return;
        try
        {
            _vodPlayer.Position = TimeSpan.FromMilliseconds(startMs);
            _vodPlayer.Play();
        }
        catch (Exception ex)
        {
            VodLibrary.ReportError("Não foi possível ir até o round: " + ex.Message);
        }
    }

    private MediaPlayer CreateVodPlayer()
    {
        var player = new MediaPlayer { AutoPlay = false };
        VodPlayer.SetMediaPlayer(player);
        return player;
    }

    private void DisposeVodPlayer()
    {
        if (_vodPlayer == null) return;
        try
        {
            _vodPlayer.Pause();
            _vodPlayer.Source = null;
            VodPlayer.SetMediaPlayer(null);
        }
        catch
        {
            // Algumas versões do WinUI recusam desanexar; ainda assim liberamos o player abaixo.
        }
        finally
        {
            try { _vodPlayer.Dispose(); } catch { }
            _vodPlayer = null;
        }
    }
}
