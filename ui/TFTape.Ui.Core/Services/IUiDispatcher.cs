namespace TFTape.Ui.Services;

/// <summary>
/// Abstração do despachante da UI para permitir testes sem WinUI.
/// A implementação real usa DispatcherQueue; em testes, executa a ação inline.
/// </summary>
public interface IUiDispatcher
{
    void Run(Action action);
}

public sealed class InlineUiDispatcher : IUiDispatcher
{
    public void Run(Action action) => action();
}
