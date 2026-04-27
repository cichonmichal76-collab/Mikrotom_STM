param(
    [string]$Stamp = "20260424",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$deployRoot = Join-Path $root "dist\deployment"
$techRoot = Join-Path $deployRoot "warianty_techniczne_$Stamp"
$singleZip = Join-Path $deployRoot "Mikrotom_Wariant_B_JEDEN_PLIK_$Stamp.zip"

if (-not $SkipBuild) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "scripts\build_mcu_variants.ps1") both -Stamp $Stamp
    if ($LASTEXITCODE -ne 0) {
        throw "MCU build variants failed."
    }
}

$fullName = "Mikrotom_Wariant_B_Pakiet_Pelny_$Stamp"
$pcName = "Mikrotom_Wariant_B_Pakiet_PC_$Stamp"
$mcuSafeName = "Mikrotom_Wariant_B_Pakiet_MCU_SAFE_$Stamp"
$mcuMotionName = "Mikrotom_Wariant_B_Pakiet_MCU_MOTION_$Stamp"
$docsName = "Mikrotom_Wariant_B_Pakiet_Dokumentacja_$Stamp"

$fullDir = Join-Path $techRoot $fullName
$pcDir = Join-Path $techRoot $pcName
$mcuSafeDir = Join-Path $techRoot $mcuSafeName
$mcuMotionDir = Join-Path $techRoot $mcuMotionName
$docsDir = Join-Path $techRoot $docsName

New-Item -ItemType Directory -Path $deployRoot -Force | Out-Null
if (Test-Path $techRoot) {
    Remove-Item $techRoot -Recurse -Force
}
if (Test-Path $singleZip) {
    Remove-Item $singleZip -Force
}

Get-ChildItem -Path $deployRoot -Force -ErrorAction SilentlyContinue | Where-Object {
    $_.Name -like "Mikrotom_Wariant_B_Pakiet_*"
} | Remove-Item -Recurse -Force

New-Item -ItemType Directory -Path $techRoot -Force | Out-Null

function Write-AsciiFile {
    param(
        [string]$Path,
        [string]$Content
    )

    $parent = Split-Path $Path -Parent
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }

    $normalized = $Content -replace "`r?`n", "`r`n"
    [System.IO.File]::WriteAllText($Path, $normalized, [System.Text.Encoding]::ASCII)
}

function Copy-Relative {
    param(
        [string]$Source,
        [string]$TargetRoot
    )

    $sourcePath = Join-Path $root $Source
    $destinationPath = Join-Path $TargetRoot $Source
    $parent = Split-Path $destinationPath -Parent
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }

    Copy-Item $sourcePath $destinationPath -Recurse -Force
}

function Copy-RelativeIfPresent {
    param(
        [string]$Source,
        [string]$TargetRoot
    )

    $sourcePath = Join-Path $root $Source
    if (Test-Path $sourcePath) {
        Copy-Relative -Source $Source -TargetRoot $TargetRoot
    }
}

function Copy-McuArtifacts {
    param(
        [string]$TargetRoot,
        [ValidateSet("safe", "motion", "all")]
        [string]$Variant
    )

    $targetMcuDir = Join-Path $TargetRoot "dist\mcu"
    New-Item -ItemType Directory -Path $targetMcuDir -Force | Out-Null

    Copy-Item (Join-Path $root "dist\mcu\README.md") (Join-Path $targetMcuDir "README.md") -Force

    $fileNames = switch ($Variant) {
        "safe" {
            @(
                "Mikrotom_STM_safe_integration_$Stamp.hex",
                "Mikrotom_STM_safe_integration_$Stamp.bin",
                "Mikrotom_STM_safe_integration_$Stamp.hex.sha256.txt",
                "Mikrotom_STM_safe_integration_$Stamp.bin.sha256.txt",
                "Mikrotom_STM_latest.hex",
                "Mikrotom_STM_latest.bin",
                "Mikrotom_STM_latest_safe.hex",
                "Mikrotom_STM_latest_safe.bin"
            )
        }
        "motion" {
            @(
                "Mikrotom_STM_motion_enabled_$Stamp.hex",
                "Mikrotom_STM_motion_enabled_$Stamp.bin",
                "Mikrotom_STM_motion_enabled_$Stamp.hex.sha256.txt",
                "Mikrotom_STM_motion_enabled_$Stamp.bin.sha256.txt",
                "Mikrotom_STM_latest_motion.hex",
                "Mikrotom_STM_latest_motion.bin"
            )
        }
        default {
            Get-ChildItem (Join-Path $root "dist\mcu") -File | ForEach-Object { $_.Name }
        }
    }

    foreach ($name in $fileNames) {
        $source = Join-Path $root "dist\mcu\$name"
        if (-not (Test-Path $source)) {
            throw "Missing MCU artifact: $source"
        }
        Copy-Item $source (Join-Path $targetMcuDir $name) -Force
    }
}

function Add-RootGuide {
    param(
        [string]$TargetRoot,
        [string]$Title,
        [string]$Body
    )

    Write-AsciiFile (Join-Path $TargetRoot "README_PAKIET.txt") @"
$Title
$(("=" * $Title.Length))

$Body
"@
}

function Add-FullLaunchers {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Otworzono przewodnik startowy.
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "01_SPRAWDZ_WYMAGANIA.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\check_variant_b_prerequisites.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "02_INSTALUJ_PC.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\install_pc.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "03_WGRAJ_MCU_SAFE_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_safe_stlink.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "04_USTAW_GUI_LIVE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\configure_gui_live.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "05_URUCHOM_AGENT.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\run_agent_prompt.bat"
'@

    Write-AsciiFile (Join-Path $TargetRoot "06_URUCHOM_GUI.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\run_gui.bat"
'@

    Write-AsciiFile (Join-Path $TargetRoot "07_OTWORZ_DOKUMENTACJE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\open_variant_b_docs.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "08_WGRAJ_MCU_MOTION_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_motion_stlink.bat"
pause
'@
}

function Add-PcLaunchers {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Otworzono przewodnik startowy.
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "01_SPRAWDZ_WYMAGANIA_PC.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\check_variant_b_prerequisites.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "02_INSTALUJ_PC.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\install_pc.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "03_USTAW_GUI_LIVE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\configure_gui_live.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "04_URUCHOM_AGENT.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\run_agent_prompt.bat"
'@

    Write-AsciiFile (Join-Path $TargetRoot "05_URUCHOM_GUI.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\run_gui.bat"
'@

    Write-AsciiFile (Join-Path $TargetRoot "06_OTWORZ_DOKUMENTACJE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\open_variant_b_docs.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "07_URUCHOM_GUI_DEMO.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\configure_gui_demo.bat"
call "scripts\run_gui.bat"
'@
}

function Add-McuSafeLaunchers {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Otworzono przewodnik startowy.
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "01_SPRAWDZ_WYMAGANIA_MCU.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\check_variant_b_prerequisites.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "02_WGRAJ_MCU_SAFE_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_safe_stlink.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "03_OTWORZ_DOKUMENTACJE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\open_variant_b_docs.bat"
pause
'@
}

function Add-McuMotionLaunchers {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Otworzono przewodnik startowy.
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "01_SPRAWDZ_WYMAGANIA_MCU.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\check_variant_b_prerequisites.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "02_WGRAJ_MCU_MOTION_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_motion_stlink.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "03_OTWORZ_DOKUMENTACJE.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\open_variant_b_docs.bat"
pause
'@
}

function Add-DocsLaunchers {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
if exist "docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md" start "" "docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md"
if exist "docs\Wariant_B_Manual_Uzytkownika_GUI.md" start "" "docs\Wariant_B_Manual_Uzytkownika_GUI.md"
if exist "docs\STM32_Bringup_Checklist.md" start "" "docs\STM32_Bringup_Checklist.md"
if exist "docs\Pakiety_Wdrozeniowe_Wariant_B.md" start "" "docs\Pakiety_Wdrozeniowe_Wariant_B.md"
echo Otworzono dokumentacje.
pause
'@
}

$docEntries = @(
    "docs\START_HERE_Wariant_B.txt",
    "docs\Karta_Operatora_A4_Wariant_B.html",
    "docs\Porownanie_Stary_vs_Nowy_Soft.md",
    "docs\Wariant_B_Przewodnik_Kompletny.html",
    "docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md",
    "docs\Wariant_B_Manual_Uzytkownika_GUI.md",
    "docs\STM32_Bringup_Checklist.md",
    "docs\Pakiety_Wdrozeniowe_Wariant_B.md",
    "docs\HMI_Protocol.md",
    "docs\images"
)

$optionalDocEntries = @(
    "docs\Pakiety_Wdrozeniowe_Wariant_B.pdf",
    "docs\Porownanie_Stary_vs_Nowy_Soft.pdf"
)

$pcEntries = @(
    "agent",
    "gui",
    "scripts\install_pc.bat",
    "scripts\configure_gui_live.bat",
    "scripts\configure_gui_demo.bat",
    "scripts\run_agent.bat",
    "scripts\run_agent_prompt.bat",
    "scripts\run_gui.bat",
    "scripts\check_variant_b_prerequisites.bat",
    "scripts\open_variant_b_docs.bat"
) + $docEntries

$mcuCommonEntries = @(
    "scripts\flash_mcu_stlink.bat",
    "scripts\flash_mcu_safe_stlink.bat",
    "scripts\flash_mcu_motion_stlink.bat",
    "scripts\check_variant_b_prerequisites.bat",
    "scripts\open_variant_b_docs.bat"
) + $docEntries

$fullEntries = @(
    "agent",
    "gui",
    "scripts\install_pc.bat",
    "scripts\configure_gui_live.bat",
    "scripts\configure_gui_demo.bat",
    "scripts\run_agent.bat",
    "scripts\run_agent_prompt.bat",
    "scripts\run_gui.bat",
    "scripts\flash_mcu_stlink.bat",
    "scripts\flash_mcu_safe_stlink.bat",
    "scripts\flash_mcu_motion_stlink.bat",
    "scripts\check_variant_b_prerequisites.bat",
    "scripts\open_variant_b_docs.bat"
) + $docEntries

foreach ($dir in @($fullDir, $pcDir, $mcuSafeDir, $mcuMotionDir, $docsDir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

foreach ($entry in $pcEntries) { Copy-Relative -Source $entry -TargetRoot $pcDir }
foreach ($entry in $mcuCommonEntries) { Copy-Relative -Source $entry -TargetRoot $mcuSafeDir }
foreach ($entry in $mcuCommonEntries) { Copy-Relative -Source $entry -TargetRoot $mcuMotionDir }
foreach ($entry in $fullEntries) { Copy-Relative -Source $entry -TargetRoot $fullDir }
foreach ($entry in $docEntries) { Copy-Relative -Source $entry -TargetRoot $docsDir }
foreach ($entry in $optionalDocEntries) { Copy-RelativeIfPresent -Source $entry -TargetRoot $pcDir }
foreach ($entry in $optionalDocEntries) { Copy-RelativeIfPresent -Source $entry -TargetRoot $mcuSafeDir }
foreach ($entry in $optionalDocEntries) { Copy-RelativeIfPresent -Source $entry -TargetRoot $mcuMotionDir }
foreach ($entry in $optionalDocEntries) { Copy-RelativeIfPresent -Source $entry -TargetRoot $fullDir }
foreach ($entry in $optionalDocEntries) { Copy-RelativeIfPresent -Source $entry -TargetRoot $docsDir }

Copy-McuArtifacts -TargetRoot $mcuSafeDir -Variant safe
Copy-McuArtifacts -TargetRoot $mcuMotionDir -Variant motion
Copy-McuArtifacts -TargetRoot $fullDir -Variant all

Get-ChildItem -Path $deployRoot -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
Get-ChildItem -Path $deployRoot -Recurse -File -Include "*.pyc" | Remove-Item -Force
Get-ChildItem -Path $deployRoot -Recurse -File -Include "*.sqlite3", "*.sqlite3-shm", "*.sqlite3-wal" | Remove-Item -Force

Add-PcLaunchers -TargetRoot $pcDir
Add-McuSafeLaunchers -TargetRoot $mcuSafeDir
Add-McuMotionLaunchers -TargetRoot $mcuMotionDir
Add-FullLaunchers -TargetRoot $fullDir
Add-DocsLaunchers -TargetRoot $docsDir

Add-RootGuide -TargetRoot $pcDir -Title "Pakiet PC" -Body @"
To archiwum jest przeznaczone na komputer podpiety do USB-UART.

Zawiera:
- GUI,
- Agenta,
- API,
- SQL,
- skrypty uruchomieniowe,
- dokumentacje.

Kolejnosc:
1. 01_SPRAWDZ_WYMAGANIA_PC.bat
2. 02_INSTALUJ_PC.bat
3. 03_USTAW_GUI_LIVE.bat
4. 04_URUCHOM_AGENT.bat
5. 05_URUCHOM_GUI.bat
"@

Add-RootGuide -TargetRoot $mcuSafeDir -Title "Pakiet MCU SAFE" -Body @"
To archiwum sluzy do pierwszego flashowania MCU.

Zawiera:
- firmware SAFE,
- skrypty flashowania,
- dokumentacje.

Ten wariant daje:
- pelny feedback z urzadzenia,
- telemetrie,
- logi,
- komendy UART,
- brak realnego ruchu.
"@

Add-RootGuide -TargetRoot $mcuMotionDir -Title "Pakiet MCU MOTION" -Body @"
To archiwum sluzy do drugiego flashowania MCU, po pozytywnej walidacji SAFE.

Zawiera:
- firmware MOTION-ENABLED,
- skrypty flashowania,
- dokumentacje.

Ten wariant daje:
- HOME,
- MOVE_REL,
- MOVE_ABS,
- pierwszy kontrolowany ruch.

Nie zaczynaj od tej paczki.
"@

Add-RootGuide -TargetRoot $docsDir -Title "Pakiet Dokumentacja" -Body @"
To archiwum zawiera komplet instrukcji:
- instalacja i pierwsze uruchomienie,
- instrukcja GUI,
- checklista bring-up,
- tabelaryczne porownanie starego i nowego softu,
- przewodnik HTML,
- karta operatora A4,
- zrzuty ekranow,
- opis paczek wdrozeniowych.
"@

Add-RootGuide -TargetRoot $fullDir -Title "Pakiet Pelny" -Body @"
To archiwum zawiera wszystko:
- GUI,
- Agenta,
- skrypty .bat,
- firmware SAFE,
- firmware MOTION-ENABLED,
- dokumentacje.

Zalecana kolejnosc:
1. 01_SPRAWDZ_WYMAGANIA.bat
2. 02_INSTALUJ_PC.bat
3. 03_WGRAJ_MCU_SAFE_STLINK.bat
4. 04_USTAW_GUI_LIVE.bat
5. 05_URUCHOM_AGENT.bat
6. 06_URUCHOM_GUI.bat
7. 08_WGRAJ_MCU_MOTION_STLINK.bat
"@

Compress-Archive -Path (Join-Path $pcDir "*") -DestinationPath "$pcDir.zip" -Force
Compress-Archive -Path (Join-Path $mcuSafeDir "*") -DestinationPath "$mcuSafeDir.zip" -Force
Compress-Archive -Path (Join-Path $mcuMotionDir "*") -DestinationPath "$mcuMotionDir.zip" -Force
Compress-Archive -Path (Join-Path $docsDir "*") -DestinationPath "$docsDir.zip" -Force
Compress-Archive -Path (Join-Path $fullDir "*") -DestinationPath "$fullDir.zip" -Force
Copy-Item "$fullDir.zip" $singleZip -Force

Write-Host ""
Write-Host "Variant B deployment packages created:"
Write-Host "  Main package: $singleZip"
Write-Host "  Technical variants root: $techRoot"
Write-Host "  $pcDir.zip"
Write-Host "  $mcuSafeDir.zip"
Write-Host "  $mcuMotionDir.zip"
Write-Host "  $docsDir.zip"
Write-Host "  $fullDir.zip"
