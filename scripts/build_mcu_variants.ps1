param(
    [ValidateSet("safe", "motion", "both")]
    [string]$Variant = "both",
    [string]$Stamp = "20260424"
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$cubeIdeRoot = if ($env:STM32CUBEIDE_ROOT) { $env:STM32CUBEIDE_ROOT } else { "C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE" }
$makeDir = Join-Path $cubeIdeRoot "plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin"
$gccDir = Join-Path $cubeIdeRoot "plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
$makeExe = Join-Path $makeDir "make.exe"
$objcopyExe = Join-Path $gccDir "arm-none-eabi-objcopy.exe"
$headerPath = Join-Path $root "stm32_cubeide\Core\Inc\app_build_config.h"
$debugDir = Join-Path $root "stm32_cubeide\Debug"
$elfPath = Join-Path $debugDir "SterownikImpulsowySilnika_109-B-G431B-ESC1.elf"
$outDir = Join-Path $root "dist\mcu"
$originalHeader = Get-Content $headerPath -Raw

if (-not (Test-Path $makeExe)) {
    throw "make.exe not found. Set STM32CUBEIDE_ROOT to your STM32CubeIDE installation path."
}

if (-not (Test-Path $objcopyExe)) {
    throw "arm-none-eabi-objcopy.exe not found. Set STM32CUBEIDE_ROOT to your STM32CubeIDE installation path."
}

if (-not (Test-Path (Join-Path $debugDir "makefile"))) {
    throw "stm32_cubeide\Debug\makefile missing. Build the project once in STM32CubeIDE first."
}

if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$env:Path = "$makeDir;$gccDir;$env:Path"

function Set-AppSafeIntegration {
    param([int]$Value)

    $updated = $script:originalHeader -replace '(?m)^#define APP_SAFE_INTEGRATION\s+\d+', "#define APP_SAFE_INTEGRATION $Value"
    [System.IO.File]::WriteAllText($script:headerPath, $updated, [System.Text.Encoding]::ASCII)
}

function Invoke-Build {
    param(
        [string]$Name,
        [int]$SafeIntegration
    )

    $baseName = if ($SafeIntegration -eq 1) {
        "Mikrotom_STM_safe_integration_$Stamp"
    } else {
        "Mikrotom_STM_motion_enabled_$Stamp"
    }

    Write-Host ""
    Write-Host "=== Building variant: $Name ==="
    Set-AppSafeIntegration -Value $SafeIntegration

    Push-Location $debugDir
    try {
        & $makeExe -j8 clean | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "make clean failed for $Name" }
        & $makeExe -j8 all | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "make all failed for $Name" }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path $elfPath)) {
        throw "ELF was not generated for ${Name}: $elfPath"
    }

    $hexPath = Join-Path $outDir "$baseName.hex"
    $binPath = Join-Path $outDir "$baseName.bin"

    & $objcopyExe -O ihex $elfPath $hexPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "objcopy HEX failed for $Name" }
    & $objcopyExe -O binary $elfPath $binPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "objcopy BIN failed for $Name" }

    certutil -hashfile $hexPath SHA256 > "$hexPath.sha256.txt"
    certutil -hashfile $binPath SHA256 > "$binPath.sha256.txt"

    if ($SafeIntegration -eq 1) {
        Copy-Item $hexPath (Join-Path $outDir "Mikrotom_STM_latest.hex") -Force
        Copy-Item $binPath (Join-Path $outDir "Mikrotom_STM_latest.bin") -Force
        Copy-Item $hexPath (Join-Path $outDir "Mikrotom_STM_latest_safe.hex") -Force
        Copy-Item $binPath (Join-Path $outDir "Mikrotom_STM_latest_safe.bin") -Force
    } else {
        Copy-Item $hexPath (Join-Path $outDir "Mikrotom_STM_latest_motion.hex") -Force
        Copy-Item $binPath (Join-Path $outDir "Mikrotom_STM_latest_motion.bin") -Force
    }

    Write-Host "Generated:"
    Write-Host "  $hexPath"
    Write-Host "  $binPath"
}

try {
    switch ($Variant) {
        "motion" { $buildOrder = @("motion") }
        "safe" { $buildOrder = @("safe") }
        default { $buildOrder = @("motion", "safe") }
    }

    foreach ($item in $buildOrder) {
        if ($item -eq "safe") {
            Invoke-Build -Name "SAFE" -SafeIntegration 1
        } else {
            Invoke-Build -Name "MOTION-ENABLED" -SafeIntegration 0
        }
    }
}
finally {
    [System.IO.File]::WriteAllText($headerPath, $originalHeader, [System.Text.Encoding]::ASCII)
}

Write-Host ""
Write-Host "MCU variants ready in:"
Write-Host "  $outDir"
