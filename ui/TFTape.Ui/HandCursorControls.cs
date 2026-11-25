using Microsoft.UI.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace TFTape.Ui;

/// <summary>Cursor compartilhado pelos controles clicáveis do app.</summary>
internal static class HandCursorResource
{
    public static readonly InputCursor Cursor = InputSystemCursor.Create(InputSystemCursorShape.Hand);
}

/// <summary>
/// Botão com cursor de mão no hover. ProtectedCursor é protegido em UIElement,
/// então precisa ser definido em uma classe derivada.
/// </summary>
public sealed class HandCursorButton : Button
{
    public HandCursorButton()
    {
        DefaultStyleKey = typeof(Button);
        ProtectedCursor = HandCursorResource.Cursor;

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

/// <summary>Aba clicável com cursor de mão.</summary>
public sealed class HandCursorTabViewItem : TabViewItem
{
    public HandCursorTabViewItem()
    {
        DefaultStyleKey = typeof(TabViewItem);
        ProtectedCursor = HandCursorResource.Cursor;
    }
}

/// <summary>CheckBox clicável com cursor de mão.</summary>
public sealed class HandCursorCheckBox : CheckBox
{
    public HandCursorCheckBox()
    {
        DefaultStyleKey = typeof(CheckBox);
        ProtectedCursor = HandCursorResource.Cursor;
    }
}

/// <summary>Border clicável com cursor de mão (usado pelo seletor de opções).</summary>
public sealed class HandCursorBorder : Border
{
    public HandCursorBorder()
    {
        ProtectedCursor = HandCursorResource.Cursor;
    }
}

/// <summary>
/// Envolve o player e a barra de ações: um toque no vídeo pausa/retoma, então a
/// superfície inteira usa mão. MediaPlayerElement não pode ser derivado no WinUI 3
/// (issue #8973), por isso o cursor fica no Grid pai.
/// </summary>
public sealed class HandCursorGrid : Grid
{
    public HandCursorGrid()
    {
        ProtectedCursor = HandCursorResource.Cursor;
    }
}

/// <summary>ListView cujos itens (VODs) recebem o cursor de mão.</summary>
public sealed class HandCursorListView : ListView
{
    public HandCursorListView()
    {
        // Sem isso o WinUI procuraria um template com a chave do tipo derivado.
        DefaultStyleKey = typeof(ListView);
    }

    protected override DependencyObject GetContainerForItemOverride() => new HandCursorListViewItem();
}

/// <summary>Slider interativo (timeline/volume) com cursor de mão.</summary>
public sealed class HandCursorSlider : Slider
{
    public HandCursorSlider()
    {
        DefaultStyleKey = typeof(Slider);
        ProtectedCursor = HandCursorResource.Cursor;
    }
}

/// <summary>Container de item da lista de VODs com cursor de mão.</summary>
public sealed class HandCursorListViewItem : ListViewItem
{
    public HandCursorListViewItem()
    {
        DefaultStyleKey = typeof(ListViewItem);
        ProtectedCursor = HandCursorResource.Cursor;
    }
}
