using Microsoft.UI.Input;
using Microsoft.UI.Xaml.Controls;

namespace TFTape.Ui;

/// <summary>
/// Botão com cursor de mão no hover. ProtectedCursor é protegido em UIElement,
/// então precisa ser definido em uma classe derivada. DefaultStyleKey aponta para
/// Button para continuar usando o estilo implícito do tema (paleta, padding, cantos).
/// </summary>
public sealed class HandCursorButton : Button
{
    public HandCursorButton()
    {
        DefaultStyleKey = typeof(Button);
        ProtectedCursor = InputSystemCursor.Create(InputSystemCursorShape.Hand);
    }
}
