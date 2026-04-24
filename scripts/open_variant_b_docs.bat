@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if exist "%ROOT%\docs\START_HERE_Wariant_B.txt" start "" notepad.exe "%ROOT%\docs\START_HERE_Wariant_B.txt"
if exist "%ROOT%\docs\Wariant_B_Przewodnik_Kompletny.html" start "" "%ROOT%\docs\Wariant_B_Przewodnik_Kompletny.html"
if exist "%ROOT%\docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md" start "" "%ROOT%\docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md"
if exist "%ROOT%\docs\Wariant_B_Manual_Uzytkownika_GUI.md" start "" "%ROOT%\docs\Wariant_B_Manual_Uzytkownika_GUI.md"

exit /b 0
