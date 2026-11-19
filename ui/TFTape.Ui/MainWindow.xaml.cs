using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using TFTape.Ui.Services;
using TFTape.Ui.ViewModels;
using WinRT.Interop;

namespace TFTape.Ui;

public sealed partial class MainWindow : Window
{
    public RecorderViewModel ViewModel { get; }
    private readonly FolderPickerService _folderPicker = new();
    private readonly RecorderProcessService _recorderService = new();

    public MainWindow()
    {
        ViewModel = new RecorderViewModel(_recorderService, new UiDispatcher(DispatcherQueue.GetForCurrentThread()));
        InitializeComponent();
        // WinUI 3 expõe Window.Closed (AppWindow.Closing é a versão com cancellation).
        Closed += OnClosed;
    }

    private void OnClosed(object sender, WindowEventArgs args)
    {
        // Envia shutdown e aguarda a finalização do MP4 antes de encerrar a UI.
        // O serviço mata a árvore de processos se o core não responder.
        _recorderService.Dispose();
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
}
