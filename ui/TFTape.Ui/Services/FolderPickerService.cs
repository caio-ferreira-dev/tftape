using Windows.Storage.Pickers;
using WinRT.Interop;

namespace TFTape.Ui.Services;

public sealed class FolderPickerService
{
    public async Task<string?> PickFolderAsync(nint hwnd)
    {
        var picker = new FolderPicker();
        picker.FileTypeFilter.Add("*");
        InitializeWithWindow.Initialize(picker, hwnd);
        var folder = await picker.PickSingleFolderAsync();
        return folder?.Path;
    }
}
