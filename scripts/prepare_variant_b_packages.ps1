param(
    [string]$Stamp = "20260424"
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$deployRoot = Join-Path $root "dist\deployment"

$fullName = "Mikrotom_Wariant_B_Pakiet_Pelny_$Stamp"
$pcName = "Mikrotom_Wariant_B_Pakiet_PC_$Stamp"
$mcuName = "Mikrotom_Wariant_B_Pakiet_MCU_$Stamp"

$fullDir = Join-Path $deployRoot $fullName
$pcDir = Join-Path $deployRoot $pcName
$mcuDir = Join-Path $deployRoot $mcuName

$pathsToRemove = @(
    $fullDir,
    $pcDir,
    $mcuDir,
    "$fullDir.zip",
    "$pcDir.zip",
    "$mcuDir.zip"
)

foreach ($path in $pathsToRemove) {
    if (Test-Path $path) {
        Remove-Item $path -Recurse -Force
    }
}

New-Item -ItemType Directory -Path $deployRoot -Force | Out-Null

function Copy-Entry {
    param(
        [string]$Source,
        [string]$Destination
    )

    $sourcePath = Join-Path $root $Source
    $destinationPath = Join-Path $Destination $Source
    $parent = Split-Path $destinationPath -Parent
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }

    Copy-Item $sourcePath $destinationPath -Recurse -Force
}

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

function Add-FullRootFiles {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Opened START HERE guide.
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

    Write-AsciiFile (Join-Path $TargetRoot "03_WGRAJ_MCU_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_stlink.bat"
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

    Write-AsciiFile (Join-Path $TargetRoot "08_URUCHOM_GUI_DEMO.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\configure_gui_demo.bat"
call "scripts\run_gui.bat"
'@
}

function Add-PcRootFiles {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Opened START HERE guide.
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
}

function Add-McuRootFiles {
    param([string]$TargetRoot)

    Write-AsciiFile (Join-Path $TargetRoot "00_START_TUTAJ.bat") @'
@echo off
setlocal
cd /d "%~dp0"
if exist "docs\START_HERE_Wariant_B.txt" start "" notepad.exe "docs\START_HERE_Wariant_B.txt"
if exist "docs\Wariant_B_Przewodnik_Kompletny.html" start "" "docs\Wariant_B_Przewodnik_Kompletny.html"
echo Opened START HERE guide.
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "01_SPRAWDZ_WYMAGANIA_MCU.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\check_variant_b_prerequisites.bat"
pause
'@

    Write-AsciiFile (Join-Path $TargetRoot "02_WGRAJ_MCU_STLINK.bat") @'
@echo off
setlocal
cd /d "%~dp0"
call "scripts\flash_mcu_stlink.bat"
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

$commonDocs = @(
    "docs\START_HERE_Wariant_B.txt",
    "docs\Wariant_B_Przewodnik_Kompletny.html",
    "docs\Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md",
    "docs\Wariant_B_Manual_Uzytkownika_GUI.md",
    "docs\STM32_Bringup_Checklist.md",
    "docs\HMI_Protocol.md",
    "docs\images"
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
) + $commonDocs

$mcuEntries = @(
    "dist\mcu",
    "scripts\flash_mcu_stlink.bat",
    "scripts\check_variant_b_prerequisites.bat",
    "scripts\open_variant_b_docs.bat"
) + $commonDocs

$fullEntries = @(
    "agent",
    "gui",
    "dist\mcu",
    "scripts\install_pc.bat",
    "scripts\configure_gui_live.bat",
    "scripts\configure_gui_demo.bat",
    "scripts\run_agent.bat",
    "scripts\run_agent_prompt.bat",
    "scripts\run_gui.bat",
    "scripts\flash_mcu_stlink.bat",
    "scripts\check_variant_b_prerequisites.bat",
    "scripts\open_variant_b_docs.bat"
) + $commonDocs

foreach ($dir in @($fullDir, $pcDir, $mcuDir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

foreach ($entry in $pcEntries) { Copy-Entry -Source $entry -Destination $pcDir }
foreach ($entry in $mcuEntries) { Copy-Entry -Source $entry -Destination $mcuDir }
foreach ($entry in $fullEntries) { Copy-Entry -Source $entry -Destination $fullDir }

Get-ChildItem -Path $deployRoot -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
Get-ChildItem -Path $deployRoot -Recurse -File -Include "*.pyc" | Remove-Item -Force

Add-PcRootFiles -TargetRoot $pcDir
Add-McuRootFiles -TargetRoot $mcuDir
Add-FullRootFiles -TargetRoot $fullDir

Compress-Archive -Path (Join-Path $pcDir "*") -DestinationPath "$pcDir.zip" -Force
Compress-Archive -Path (Join-Path $mcuDir "*") -DestinationPath "$mcuDir.zip" -Force
Compress-Archive -Path (Join-Path $fullDir "*") -DestinationPath "$fullDir.zip" -Force

Write-Host ""
Write-Host "Variant B deployment packages created:"
Write-Host "  $pcDir"
Write-Host "  $mcuDir"
Write-Host "  $fullDir"
Write-Host "  $pcDir.zip"
Write-Host "  $mcuDir.zip"
Write-Host "  $fullDir.zip"
