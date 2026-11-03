// Prüft die Zahlenformatierung N0/N1 von .NET (de-DE/en-US) für den Vergleich mit zahl_n() im C-Kern
using System; using System.Globalization;
class Z { static void Main() {
  double[] t = { 0, 1.5, 2.5, 1023.5, 0.05, 0.15, 0.25, 0.35, 0.45, 1234567.891, 999.95, 9999999.95, 17.5, 1.05, 2.675, 12345678901.5, 1.0/3, 2.0/3, 5.55, 0.95, 1048575.0/1024, 123.45, 6.05, 1022.999 };
  foreach (var v in t) {
    var de = CultureInfo.GetCultureInfo("de-DE"); var en = CultureInfo.GetCultureInfo("en-US");
    Console.WriteLine(v.ToString("R", CultureInfo.InvariantCulture) + " -> N0 " + v.ToString("N0", de) + " N1 " + v.ToString("N1", de) + " | en " + v.ToString("N0", en) + " " + v.ToString("N1", en));
  } } }
