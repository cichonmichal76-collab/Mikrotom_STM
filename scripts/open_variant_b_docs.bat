@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if exist "%ROOT%\docs\START_HERE_Wariant_B.txt" start "" notepad.exe "%ROOT%\docs\START_HERE_Wariant_B.txt"
if exist "%ROOT%\docs\Karta_Operatora_A4_Wariant_B.html" start "" "%ROOT%\docs\Karta_Operatora_A4_Wariant_B.html"
if exist "%ROOT%\docs\Porownanie_Stary_vs_Nowy_Soft.md" start "" "%ROOT%\docs\Porownanie_Stary_vs_Nowy_Soft.md"
if exist "%ROOT%\docs\Wariant_B_Przewodnik_Kompletny.html" start "" "%ROOT%\docs\Wariant_B_Przewodnik_Kompletny.html"
if exist "%ROOT%\docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md" start "" "%ROOT%\docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md"
if exist "%ROOT%\docs\Wariant_B_Manual_Uzytkownika_GUI.md" start "" "%ROOT%\docs\Wariant_B_Manual_Uzytkownika_GUI.md"
if exist "%ROOT%\docs\STM32_Bringup_Checklist.md" start "" "%ROOT%\docs\STM32_Bringup_Checklist.md"
if exist "%ROOT%\docs\Pakiety_Wdrozeniowe_Wariant_B.md" start "" "%ROOT%\docs\Pakiety_Wdrozeniowe_Wariant_B.md"

exit /b 0
