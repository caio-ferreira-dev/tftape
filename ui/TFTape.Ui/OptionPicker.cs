using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;

namespace TFTape.Ui;

/// <summary>
/// Seletor de opções com a listagem desenhada na própria árvore visual.
/// O ComboBox nativo abre um Popup, e o WinUI 3 não aplica ProtectedCursor
/// dentro de popups (issue microsoft-ui-xaml#9023); assim o cursor de mão e as
/// cores do tema valem também para cada opção.
/// </summary>
public sealed class OptionPicker : UserControl
{
    private static readonly Brush TransparentBrush = new SolidColorBrush(Windows.UI.Color.FromArgb(0, 0, 0, 0));

    private readonly Grid _root = new();
    private readonly HandCursorBorder _header = new();
    private readonly TextBlock _value = new();
    private readonly FontIcon _chevron = new();
    private readonly Border _panel = new();
    private readonly StackPanel _options = new();
    private FrameworkElement? _outsideListener;

    public static readonly DependencyProperty ItemsSourceProperty = DependencyProperty.Register(
        nameof(ItemsSource),
        typeof(IEnumerable<string>),
        typeof(OptionPicker),
        new PropertyMetadata(null, (d, _) => ((OptionPicker)d).RebuildOptions()));

    public static readonly DependencyProperty SelectedItemProperty = DependencyProperty.Register(
        nameof(SelectedItem),
        typeof(string),
        typeof(OptionPicker),
        new PropertyMetadata(null, (d, _) => ((OptionPicker)d).UpdateValue()));

    public IEnumerable<string>? ItemsSource
    {
        get => (IEnumerable<string>?)GetValue(ItemsSourceProperty);
        set => SetValue(ItemsSourceProperty, value);
    }

    public string? SelectedItem
    {
        get => (string?)GetValue(SelectedItemProperty);
        set => SetValue(SelectedItemProperty, value);
    }

    public OptionPicker()
    {
        BuildUi();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
    }

    private void BuildUi()
    {
        _root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        _root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var headerContent = new Grid();
        headerContent.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        headerContent.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        _value.FontSize = 14;
        _value.VerticalAlignment = VerticalAlignment.Center;
        _value.Foreground = PaletteBrush("Brush.TextPrimary");
        Grid.SetColumn(_value, 0);
        headerContent.Children.Add(_value);

        _chevron.Glyph = "\uE0D7";
        _chevron.FontSize = 12;
        _chevron.VerticalAlignment = VerticalAlignment.Center;
        _chevron.Foreground = PaletteBrush("Brush.TextSecondary");
        Grid.SetColumn(_chevron, 1);
        headerContent.Children.Add(_chevron);

        _header.Background = PaletteBrush("Brush.Surface");
        _header.BorderBrush = PaletteBrush("Brush.Divider");
        _header.BorderThickness = new Thickness(1);
        _header.CornerRadius = new CornerRadius(6);
        _header.Padding = new Thickness(10, 8, 10, 8);
        _header.Child = headerContent;
        _header.PointerEntered += (_, _) => _header.Background = PaletteBrush("Brush.Primary");
        _header.PointerExited += (_, _) => _header.Background = PaletteBrush("Brush.Surface");
        _header.Tapped += (_, _) => SetOpen(_panel.Visibility != Visibility.Visible);
        Grid.SetRow(_header, 0);
        _root.Children.Add(_header);

        _panel.Background = PaletteBrush("Brush.Surface");
        _panel.BorderBrush = PaletteBrush("Brush.Divider");
        _panel.BorderThickness = new Thickness(1);
        _panel.CornerRadius = new CornerRadius(6);
        _panel.Padding = new Thickness(4);
        _panel.Margin = new Thickness(0, 4, 0, 0);
        _panel.Visibility = Visibility.Collapsed;
        _panel.Child = _options;
        Grid.SetRow(_panel, 1);
        _root.Children.Add(_panel);

        Content = _root;
        MinWidth = 116;
    }

    private void RebuildOptions()
    {
        _options.Children.Clear();
        if (ItemsSource == null) return;

        foreach (var item in ItemsSource)
        {
            var option = new HandCursorBorder
            {
                Tag = item,
                Background = TransparentBrush,
                CornerRadius = new CornerRadius(4),
                Padding = new Thickness(10, 6, 10, 6),
                Child = new TextBlock
                {
                    Text = item,
                    FontSize = 14,
                    Foreground = PaletteBrush("Brush.TextPrimary")
                }
            };
            option.PointerEntered += (_, _) => option.Background = PaletteBrush("Brush.Primary");
            option.PointerExited += (_, _) => option.Background = TransparentBrush;
            option.Tapped += (_, _) =>
            {
                SelectedItem = item;
                SetOpen(false);
            };
            _options.Children.Add(option);
        }
    }

    private void UpdateValue() => _value.Text = SelectedItem ?? string.Empty;

    private void SetOpen(bool open) => _panel.Visibility = open ? Visibility.Visible : Visibility.Collapsed;

    private static Brush PaletteBrush(string key)
    {
        var app = Application.Current;
        if (app != null && app.Resources.TryGetValue(key, out var brush) && brush is Brush b)
            return b;
        return TransparentBrush;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        // Escuta cliques fora do seletor na raiz da janela para fechar a listagem.
        FrameworkElement? root = this;
        while (root.Parent is FrameworkElement parent) root = parent;
        _outsideListener = root;
        _outsideListener.PointerPressed += OnOutsidePointerPressed;
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        if (_outsideListener != null)
            _outsideListener.PointerPressed -= OnOutsidePointerPressed;
        _outsideListener = null;
    }

    private void OnOutsidePointerPressed(object sender, PointerRoutedEventArgs e)
    {
        if (_panel.Visibility != Visibility.Visible) return;

        DependencyObject? source = e.OriginalSource as DependencyObject;
        while (source != null)
        {
            if (ReferenceEquals(source, this)) return;
            source = VisualTreeHelper.GetParent(source);
        }
        SetOpen(false);
    }
}
