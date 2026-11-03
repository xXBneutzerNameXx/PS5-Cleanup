#!/bin/bash
# Vergleich F: baut Ref105Ablauf.exe = UNVERÄNDERTE Oberfläche von 1.0.5 (Quellcode\*.cs) + Treiber AblaufRef.cs
cd "$(dirname "$0")"
mkdir -p ergebnis_ablauf
exec > ergebnis_ablauf/_bauen.log 2>&1
CSC="/c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe"
FW='C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
Q='C:\pfad\zu\PS5-Cleanup\Quellcode'
"$CSC" -nologo -target:winexe -optimize+ -out:Ref105Ablauf.exe -main:PS5Cleanup.AblaufRef \
  -resource:"$Q\\Fenster.xaml",PS5Cleanup.Fenster.xaml -resource:"$Q\\logo.png",PS5Cleanup.logo.png -resource:"$Q\\cleanup.ico",PS5Cleanup.cleanup.ico \
  -r:"$FW\\WPF\\PresentationFramework.dll" -r:"$FW\\WPF\\PresentationCore.dll" -r:"$FW\\WPF\\WindowsBase.dll" \
  -r:"$FW\\System.Xaml.dll" -r:"$FW\\System.Web.Extensions.dll" -r:"$FW\\System.Xml.dll" -r:"$FW\\System.Core.dll" \
  "$Q\\Sprache.cs" "$Q\\Ftp.cs" "$Q\\Sitzung.cs" "$Q\\Kern.cs" "$Q\\Suche.cs" "$Q\\Ausfuehren.cs" "$Q\\Feedback.cs" \
  "$Q\\Fenster.cs" "$Q\\Programm.cs" "$Q\\Selbsttest.cs" "$Q\\UiTest.cs" AblaufRef.cs
echo "csc rc=$?"
ls -la Ref105Ablauf.exe
sha256sum "$(cygpath -u "$Q")"/{Sprache,Ftp,Sitzung,Kern,Suche,Ausfuehren,Feedback,Fenster,Programm,Selbsttest,UiTest}.cs "$(cygpath -u "$Q")"/Fenster.xaml AblaufRef.cs
