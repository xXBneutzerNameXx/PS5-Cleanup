#!/bin/bash
# Baut das Referenz-Testprogramm aus den UNVERÄNDERTEN 1.0.5-Quellen (Werkzeuge\PS5\Quellcode)
cd "$(dirname "$0")"
exec > ergebnis/_bauen.log 2>&1
CSC="/c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe"
FW='C:\Windows\Microsoft.NET\Framework64\v4.0.30319'
Q='C:\pfad\zu\PS5-Cleanup\Quellcode'
"$CSC" -nologo -target:exe -optimize+ -out:Ref105.exe -r:"$FW\\System.Web.Extensions.dll" -r:"$FW\\System.Core.dll" \
  "$Q\\Sprache.cs" "$Q\\Ftp.cs" "$Q\\Sitzung.cs" "$Q\\Kern.cs" "$Q\\Suche.cs" "$Q\\Ausfuehren.cs" "$Q\\Feedback.cs" TestKernRef.cs
echo "csc rc=$?"
ls -la Ref105.exe
sha256sum "$(cygpath -u "$Q")"/{Sprache,Ftp,Sitzung,Kern,Suche,Ausfuehren,Feedback}.cs TestKernRef.cs
ls -la "/f/projekt/ComfyUI/python_embeded/python.exe"
