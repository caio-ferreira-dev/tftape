using Microsoft.UI.Dispatching;

namespace TFTape.Ui.Services;

/// <summary>Ponte entre DispatcherQueue do WinUI e a abstração usada pelo ViewModel.</summary>
public sealed class UiDispatcher : IUiDispatcher
{
    private readonly DispatcherQueue _queue;

    public UiDispatcher(DispatcherQueue queue) => _queue = queue;

    public void Run(Action action)
    {
        if (_queue.HasThreadAccess) { action(); return; }
        _queue.TryEnqueue(() => action());
    }
}
