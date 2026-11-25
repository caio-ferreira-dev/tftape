using Microsoft.UI.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace TFTape.Ui;

/// <summary>
/// Botão com cursor de mão no hover. ProtectedCursor é protegido em UIElement,
/// então precisa ser definido em uma classe derivada.
/// </summary>
public sealed class HandCursorButton : Button
{
    public HandCursorButton()
    {
        DefaultStyleKey = typeof(Button);
        ProtectedCursor = InputSystemCursor.Create(InputSystemCursorShape.Hand);

        // Estilos implícitos do WinUI não se aplicam a tipos derivados: aplicamos
        // explicitamente o mesmo estilo de Button do tema, preservando cor, padding
        // e cantos que os botões tinham antes desta classe existir.
        var app = Application.Current;
        if (app != null
            && app.Resources.TryGetValue(typeof(Button), out var style)
            && style is Style buttonStyle)
        {
            Style = buttonStyle;
        }
        else
        {
            Background = app?.Resources["Brush.Primary"] as Brush;
            Foreground = app?.Resources["Brush.TextPrimary"] as Brush;
            Padding = new Thickness(14, 8, 14, 8);
            CornerRadius = new CornerRadius(6);
        }
    }
}
