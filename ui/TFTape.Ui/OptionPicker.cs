using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Animation;
using Microsoft.UI.Xaml.Shapes;

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
    private readonly HandCursorContentControl _header = new();
    private readonly TextBlock _value = new();
    private readonly Microsoft.UI.Xaml.Shapes.Path _chevron = new();
    private readonly Canvas _overlay = new();
    private readonly Border _panel = new();
    private readonly StackPanel _options = new();
    private readonly TranslateTransform _panelTransform = new();
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
        var headerContent = new Grid();
        headerContent.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        headerContent.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        _value.FontSize = 14;
        _value.VerticalAlignment = VerticalAlignment.Center;
        _value.Foreground = PaletteBrush("Brush.TextPrimary");
        Grid.SetColumn(_value, 0);
        headerContent.Children.Add(_value);

        // Path em vez de FontIcon: o glifo de seta varia entre as fontes do sistema.
        var chevronFigure = new PathFigure { StartPoint = new Windows.Foundation.Point(0, 0) };
        chevronFigure.Segments.Add(new LineSegment { Point = new Windows.Foundation.Point(5, 5) });
        chevronFigure.Segments.Add(new LineSegment { Point = new Windows.Foundation.Point(10, 0) });
        var chevronGeometry = new PathGeometry();
        chevronGeometry.Figures.Add(chevronFigure);
        _chevron.Data = chevronGeometry;
        _chevron.Stroke = PaletteBrush("Brush.TextSecondary");
        _chevron.StrokeThickness = 1.5;
        _chevron.VerticalAlignment = VerticalAlignment.Center;
        _chevron.HorizontalAlignment = HorizontalAlignment.Center;
        Grid.SetColumn(_chevron, 1);
        headerContent.Children.Add(_chevron);

        _header.Background = PaletteBrush("Brush.Surface");
        _header.BorderBrush = PaletteBrush("Brush.Divider");
        _header.BorderThickness = new Thickness(1);
        _header.CornerRadius = new CornerRadius(6);
        _header.Padding = new Thickness(10, 8, 10, 8);
        _header.Content = headerContent;
        _header.PointerEntered += (_, _) => _header.Background = PaletteBrush("Brush.Primary");
        _header.PointerExited += (_, _) => _header.Background = PaletteBrush("Brush.Surface");
        _header.Tapped += (_, _) => SetOpen(_panel.Visibility != Visibility.Visible);
        _root.Children.Add(_header);

        // Canvas não participa do medidas do Grid: abrir a lista não altera a altura
        // do cartão pai. O painel fica posicionado logo abaixo do cabeçalho.
        _overlay.ClipToBounds = false;
        _overlay.HorizontalAlignment = HorizontalAlignment.Stretch;
        _overlay.VerticalAlignment = VerticalAlignment.Stretch;
        Canvas.SetZIndex(_overlay, 1);
        _root.Children.Add(_overlay);

        _panel.Background = PaletteBrush("Brush.Surface");
        _panel.BorderBrush = PaletteBrush("Brush.Divider");
        _panel.BorderThickness = new Thickness(1);
        _panel.CornerRadius = new CornerRadius(6);
        _panel.Padding = new Thickness(4);
        _panel.RenderTransform = _panelTransform;
        _panel.Visibility = Visibility.Collapsed;
        _panel.Child = _options;
        _overlay.Children.Add(_panel);

        _root.SizeChanged += (_, _) =>
        {
            if (_panel.Visibility == Visibility.Visible) PositionPanel();
        };

        Content = _root;
        MinWidth = 116;
    }

    private void PositionPanel()
    {
        Canvas.SetLeft(_panel, 0);
        Canvas.SetTop(_panel, _header.ActualHeight + 4);
        _panel.Width = _root.ActualWidth;
    }

    private void RebuildOptions()
    {
        _options.Children.Clear();
        if (ItemsSource == null) return;

        foreach (var item in ItemsSource)
        {
            var option = new HandCursorContentControl
            {
                Tag = item,
                Background = TransparentBrush,
                CornerRadius = new CornerRadius(4),
                Padding = new Thickness(10, 6, 10, 6),
                Content = new TextBlock
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

    private void SetOpen(bool open)
    {
        if (!open)
        {
            _panel.Visibility = Visibility.Collapsed;
            return;
        }

        PositionPanel();
        _panel.Visibility = Visibility.Visible;

        var duration = new Duration(TimeSpan.FromMilliseconds(140));
        var easing = new CubicEase { EasingMode = EasingMode.EaseOut };

        var fade = new DoubleAnimation
        {
            From = 0,
            To = 1,
            Duration = duration,
            EasingFunction = easing
        };
        Storyboard.SetTarget(fade, _panel);
        Storyboard.SetTargetProperty(fade, "Opacity");

        var slide = new DoubleAnimation
        {
            From = -6,
            To = 0,
            Duration = duration,
            EasingFunction = easing
        };
        Storyboard.SetTarget(slide, _panelTransform);
        Storyboard.SetTargetProperty(slide, "Y");

        var storyboard = new Storyboard();
        storyboard.Children.Add(fade);
        storyboard.Children.Add(slide);
        storyboard.Begin();
    }

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
