using System;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
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
    private readonly DispatcherQueue _uiQueue = DispatcherQueue.GetForCurrentThread();
    private readonly DispatcherQueueTimer _controlsHideTimer;
    private MediaPlayer? _vodPlayer;
    private Windows.Foundation.Size _vodViewportSize;
    private double _vodAspectRatio = 16.0 / 9.0;
    private bool _suppressTimelineSeek;

    public MainWindow()
    {
        var dispatcher = new UiDispatcher(_uiQueue);
        var outputStore = new LocalFileOutputDirectoryStore();

        ViewModel = new RecorderViewModel(_recorderService, dispatcher, outputStore);
        VodLibrary = new VodLibraryViewModel(
            new VodLibraryService(),
            folderStore: null, // usa %LOCALAPPDATA%\TFTape\vod_directory.txt
            recordingFolderStore: outputStore,
            localAppDataPath: null);

        InitializeComponent();

        _controlsHideTimer = _uiQueue.CreateTimer();
        _controlsHideTimer.Interval = TimeSpan.FromSeconds(3);
        _controlsHideTimer.IsRepeating = false;
        _controlsHideTimer.Tick += OnControlsHideTimerTick;

        VodLibrary.PlayRequested += OnPlayRequested;
        VodLibrary.SeekRequested += OnSeekRequested;
        // Window não tem evento Loaded no WinUI 3: usa o elemento raiz do conteúdo XAML.
        if (Content is FrameworkElement root) root.Loaded += OnRootLoaded;
        // WinUI 3 expõe Window.Closed (AppWindow.Closing é a versão com cancellation).
        Closed += OnClosed;
    }

    private async void OnRootLoaded(object sender, RoutedEventArgs e) => await VodLibrary.RefreshAsync();

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
            // Começa com a proporção do sidecar; NaturalVideoSizeChanged depois usa a real.
            _vodAspectRatio = GetAspectRatio(VodLibrary.SelectedVod);
            UpdatePlayerSize();

            _vodPlayer.Source = MediaSource.CreateFromUri(new Uri(path));
            ResetVodControls();
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
            _vodPlayer.PlaybackSession.Position = TimeSpan.FromMilliseconds(startMs);
            _vodPlayer.Play();
            ShowVodControls();
        }
        catch (Exception ex)
        {
            VodLibrary.ReportError("Não foi possível ir até o round: " + ex.Message);
        }
    }

    // ---------------------------------------------------------------- player

    private MediaPlayer CreateVodPlayer()
    {
        var player = new MediaPlayer
        {
            AutoPlay = false,
            Volume = Math.Clamp(VodVolume.Value / 100.0, 0.0, 1.0)
        };

        var session = player.PlaybackSession;
        session.PlaybackStateChanged += OnVodPlaybackStateChanged;
        session.PositionChanged += OnVodPositionChanged;
        session.NaturalDurationChanged += OnVodNaturalDurationChanged;
        session.NaturalVideoSizeChanged += OnVodNaturalVideoSizeChanged;

        VodPlayer.SetMediaPlayer(player);
        return player;
    }

    private void DisposeVodPlayer()
    {
        if (_vodPlayer == null) return;
        _controlsHideTimer.Stop();
        try
        {
            var session = _vodPlayer.PlaybackSession;
            session.PlaybackStateChanged -= OnVodPlaybackStateChanged;
            session.PositionChanged -= OnVodPositionChanged;
            session.NaturalDurationChanged -= OnVodNaturalDurationChanged;
            session.NaturalVideoSizeChanged -= OnVodNaturalVideoSizeChanged;

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

    // --------------------------------------------------------- enquadramento

    private void OnVodViewportSizeChanged(object sender, SizeChangedEventArgs e)
    {
        _vodViewportSize = e.NewSize;
        UpdatePlayerSize();
    }

    /// <summary>Ajusta o quadro à proporção do vídeo: sem barras pretas e sem cortes.</summary>
    private void UpdatePlayerSize()
    {
        if (_vodViewportSize.Width <= 0 || _vodViewportSize.Height <= 0) return;

        var aspect = _vodAspectRatio > 0 ? _vodAspectRatio : 16.0 / 9.0;
        var width = _vodViewportSize.Width;
        var height = width / aspect;

        if (height > _vodViewportSize.Height)
        {
            height = _vodViewportSize.Height;
            width = height * aspect;
        }

        VodPlayerFrame.Width = width;
        VodPlayerFrame.Height = height;
    }

    private static double GetAspectRatio(VodEntry? vod)
    {
        if (vod != null && !string.IsNullOrWhiteSpace(vod.Resolution))
        {
            var parts = vod.Resolution.Split(new[] { 'x', 'X' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 2
                && int.TryParse(parts[0], out var width)
                && int.TryParse(parts[1], out var height)
                && width > 0 && height > 0)
            {
                return (double)width / height;
            }
        }
        return 16.0 / 9.0;
    }

    private void OnVodNaturalVideoSizeChanged(MediaPlaybackSession sender, object args)
    {
        var width = sender.NaturalVideoWidth;
        var height = sender.NaturalVideoHeight;
        _uiQueue.TryEnqueue(() =>
        {
            if (_vodPlayer == null || width <= 0 || height <= 0) return;
            _vodAspectRatio = (double)width / height;
            UpdatePlayerSize();
        });
    }

    // ---------------------------------------------------------- barra de ações

    private void OnVodPlaybackStateChanged(MediaPlaybackSession sender, object args)
    {
        // O MediaPlayer pode notificar fora da thread de UI; marshal para o DispatcherQueue.
        var state = sender.PlaybackState;
        _uiQueue.TryEnqueue(() =>
        {
            if (_vodPlayer == null) return;
            UpdateVodControls(state);
        });
    }

    private void UpdateVodControls(MediaPlaybackState state)
    {
        VodPlayPauseIcon.Symbol = state == MediaPlaybackState.Playing ? Symbol.Pause : Symbol.Play;

        if (state == MediaPlaybackState.Playing)
        {
            _controlsHideTimer.Start();
        }
        else
        {
            // Pausado/finalizado: a barra fica visível, conforme definimos.
            _controlsHideTimer.Stop();
            VodControlBar.Visibility = Visibility.Visible;
            UpdateVodTimeText();
        }
    }

    private void OnControlsHideTimerTick(DispatcherQueueTimer sender, object args)
    {
        _controlsHideTimer.Stop();
        VodControlBar.Visibility = Visibility.Collapsed;
    }

    private void OnVodPointerMoved(object sender, PointerRoutedEventArgs e) => ShowVodControls();
    private void OnVodPointerEntered(object sender, PointerRoutedEventArgs e) => ShowVodControls();

    private void OnVodPointerExited(object sender, PointerRoutedEventArgs e)
    {
        // Se estiver rodando, deixa o timer esconder; se estiver pausado, mantém visível.
        if (_vodPlayer?.PlaybackSession.PlaybackState == MediaPlaybackState.Playing)
            _controlsHideTimer.Start();
    }

    private void ShowVodControls()
    {
        VodControlBar.Visibility = Visibility.Visible;
        if (_vodPlayer?.PlaybackSession.PlaybackState == MediaPlaybackState.Playing)
            _controlsHideTimer.Start();
    }

    private void OnVodPlayPauseClick(object sender, RoutedEventArgs e)
    {
        if (_vodPlayer == null) return;
        var session = _vodPlayer.PlaybackSession;

        if (session.PlaybackState == MediaPlaybackState.Playing)
        {
            _vodPlayer.Pause();
        }
        else
        {
            // MediaPlaybackState não tem "Ended": se a posição está no fim, reinicia.
            var duration = session.NaturalDuration;
            if (duration > TimeSpan.Zero
                && session.Position >= duration - TimeSpan.FromMilliseconds(250))
            {
                session.Position = TimeSpan.Zero;
            }
            _vodPlayer.Play();
        }
        ShowVodControls();
    }

    private void OnVodVolumeValueChanged(object sender, RangeBaseValueChangedEventArgs e)
    {
        if (_vodPlayer == null) return;
        _vodPlayer.Volume = Math.Clamp(VodVolume.Value / 100.0, 0.0, 1.0);
        ShowVodControls();
    }

    // --------------------------------------------------------------- timeline

    private void OnVodNaturalDurationChanged(MediaPlaybackSession sender, object args)
        => _uiQueue.TryEnqueue(UpdateVodTimeline);

    private void OnVodPositionChanged(MediaPlaybackSession sender, object args)
        => _uiQueue.TryEnqueue(UpdateVodTimeline);

    private void ResetVodControls()
    {
        var sessionDuration = _vodPlayer?.PlaybackSession.NaturalDuration.TotalMilliseconds ?? 0;
        var durationMs = sessionDuration > 0 ? sessionDuration : VodLibrary.SelectedVod?.DurationMs ?? 0;
        VodTimeline.Maximum = durationMs > 0 ? durationMs : 1;

        _suppressTimelineSeek = true;
        VodTimeline.Value = 0;
        _suppressTimelineSeek = false;

        UpdateVodTimeText();
        VodControlBar.Visibility = Visibility.Visible;
        _controlsHideTimer.Stop();
    }

    private void UpdateVodTimeline()
    {
        if (_vodPlayer == null) return;
        var session = _vodPlayer.PlaybackSession;

        var durationMs = session.NaturalDuration.TotalMilliseconds;
        if (double.IsFinite(durationMs) && durationMs > 0)
            VodTimeline.Maximum = durationMs;

        _suppressTimelineSeek = true;
        VodTimeline.Value = Math.Min(session.Position.TotalMilliseconds, VodTimeline.Maximum);
        _suppressTimelineSeek = false;

        UpdateVodTimeText();
    }

    private void OnVodTimelineValueChanged(object sender, RangeBaseValueChangedEventArgs e)
    {
        if (_suppressTimelineSeek || _vodPlayer == null) return;

        var target = TimeSpan.FromMilliseconds(e.NewValue);
        var current = _vodPlayer.PlaybackSession.Position;
        // Ignora os ajustes programáticos do PositionChanged; só reage ao arraste do usuário.
        if (Math.Abs((target - current).TotalMilliseconds) > 400)
            _vodPlayer.PlaybackSession.Position = target;

        ShowVodControls();
    }

    private void UpdateVodTimeText()
        => VodTimeText.Text = $"{FormatTime(VodTimeline.Value)} / {FormatTime(VodTimeline.Maximum)}";

    private static string FormatTime(double milliseconds)
    {
        if (!double.IsFinite(milliseconds) || milliseconds < 0) milliseconds = 0;
        var time = TimeSpan.FromMilliseconds(milliseconds);
        return time.TotalHours >= 1
            ? time.ToString(@"hh\:mm\:ss")
            : time.ToString(@"mm\:ss");
    }
}
