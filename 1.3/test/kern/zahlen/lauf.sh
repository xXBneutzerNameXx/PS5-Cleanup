#!/bin/bash
cd "$(dirname "$0")"
exec > zahlen.log 2>&1
CSC="/c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe"
"$CSC" -nologo -out:Zahlen.exe Zahlen.cs && ./Zahlen.exe > net.txt
cat net.txt
