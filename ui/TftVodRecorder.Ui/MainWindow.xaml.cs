using Microsoft.UI.Xaml;
using TftVodRecorder.Ui.Services;
using TftVodRecorder.Ui.ViewModels;
using WinRT.Interop;

namespace TftVodRecorder.Ui;

public sealed partial class MainWindow : Window
{
    public RecorderViewModel ViewModel { get; }
    private readonly FolderPickerService _folderPicker = new();

    public MainWindow()
    {
        ViewModel = new RecorderViewModel(new RecorderProcessService());
        InitializeComponent();
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
