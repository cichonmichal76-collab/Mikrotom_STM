param(
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Firmware = Join-Path $Root "dist\mcu\Mikrotom_STM_latest_safe.hex"

$Candidates = @(
    "$env:ProgramFiles\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    "${env:ProgramFiles(x86)}\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
)

$CubeIdeRoots = Get-ChildItem -Path "C:\ST" -Directory -Filter "STM32CubeIDE_*" -ErrorAction SilentlyContinue
foreach ($CubeIdeRoot in $CubeIdeRoots) {
    $PluginRoot = Join-Path $CubeIdeRoot.FullName "STM32CubeIDE\plugins"
    $Plugins = Get-ChildItem -Path $PluginRoot -Directory -Filter "com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_*" -ErrorAction SilentlyContinue
    foreach ($Plugin in $Plugins) {
        $Candidates += Join-Path $Plugin.FullName "tools\bin\STM32_Programmer_CLI.exe"
    }
}

$Programmer = $Candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1

if (-not $Programmer) {
    Write-Host "ERROR: Nie znaleziono STM32_Programmer_CLI.exe." -ForegroundColor Red
    Write-Host "Zainstaluj STM32CubeProgrammer albo STM32CubeIDE z komponentem CubeProgrammer."
    exit 1
}

if (-not (Test-Path $Firmware)) {
    Write-Host "ERROR: Nie znaleziono pliku firmware SAFE:" -ForegroundColor Red
    Write-Host "  $Firmware"
    exit 1
}

Write-Host "=== Wgrywanie bezpiecznego wsadu SAFE ===" -ForegroundColor Cyan
Write-Host "Programator: $Programmer"
Write-Host "Wsad:        $Firmware"
Write-Host ""
Write-Host "Warunki przed uruchomieniem:" -ForegroundColor Yellow
Write-Host "  1. ST-LINK podlaczony do PC i MCU."
Write-Host "  2. Wyjscie zasilacza osi wylaczone."
Write-Host "  3. USB-UART moze pozostac podlaczony."
Write-Host ""

if (-not $NoPause) {
    Read-Host "Nacisnij Enter, aby rozpoczac programowanie SAFE"
}

& $Programmer -c port=SWD mode=UR -w $Firmware -v -rst
$ExitCode = $LASTEXITCODE

if ($ExitCode -eq 0) {
    Write-Host ""
    Write-Host "SAFE wgrany poprawnie." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "ERROR: Programowanie SAFE nie powiodlo sie. Kod: $ExitCode" -ForegroundColor Red
}

if (-not $NoPause) {
    Read-Host "Nacisnij Enter, aby zamknac okno"
}

exit $ExitCode
