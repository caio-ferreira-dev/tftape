using Microsoft.UI.Input;
using Microsoft.UI.Xaml;

namespace TFTape.Ui;

/// <summary>
/// Habilita o cursor de mão em qualquer elemento marcado pelo estilo implícito de Button.
/// Assim novos botões (inclusive os gerados dinamicamente nos marcadores) herdam o cursor
/// sem precisar de código no code-behind nem de atributo em cada botão do XAML.
/// </summary>
public static class CursorHelper
{
    public static readonly DependencyProperty HandCursorProperty = DependencyProperty.RegisterAttached(
        "HandCursor",
        typeof(bool),
        typeof(CursorHelper),
        new PropertyMetadata(false, OnHandCursorChanged));

    public static void SetHandCursor(DependencyObject element, bool value) =>
        element.SetValue(HandCursorProperty, value);

    public static bool GetHandCursor(DependencyObject element) =>
        (bool)element.GetValue(HandCursorProperty);

    private static void OnHandCursorChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is UIElement element && e.NewValue is true)
            element.ProtectedCursor = InputSystemCursor.Create(InputSystemCursorShape.Hand);
    }
}
