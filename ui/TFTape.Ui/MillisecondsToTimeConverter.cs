using System;
using Microsoft.UI.Xaml.Data;

namespace TFTape.Ui;

/// <summary>
/// Formata o valor do slider de timeline (milissegundos) como mm:ss ou hh:mm:ss
/// no tooltip do thumb, em vez de exibir o número bruto de milissegundos.
/// </summary>
public sealed class MillisecondsToTimeConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value == null) return string.Empty;

        double milliseconds = value switch
        {
            double d => d,
            int i => i,
            long l => l,
            _ => double.TryParse(value.ToString(), out var parsed) ? parsed : 0
        };

        return MainWindow.FormatTime(milliseconds);
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
        => value;
}
