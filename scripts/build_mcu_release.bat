@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
cd /d "%ROOT%"

if "%STM32CUBEIDE_ROOT%"=="" (
    set "STM32CUBEIDE_ROOT=C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE"
)

set "MAKE_DIR=%STM32CUBEIDE_ROOT%\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin"
set "GCC_DIR=%STM32CUBEIDE_ROOT%\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"

if not exist "%MAKE_DIR%\make.exe" (
    echo ERROR: make.exe not found in:
    echo   %MAKE_DIR%
    echo Set STM32CUBEIDE_ROOT to your STM32CubeIDE installation path.
    exit /b 1
)

if not exist "%GCC_DIR%\arm-none-eabi-objcopy.exe" (
    echo ERROR: arm-none-eabi-objcopy.exe not found in:
    echo   %GCC_DIR%
    echo Set STM32CUBEIDE_ROOT to your STM32CubeIDE installation path.
    exit /b 1
)

if not exist "stm32_cubeide\Debug\makefile" (
    echo ERROR: stm32_cubeide\Debug\makefile missing.
    echo Import/build the project once in STM32CubeIDE or generate Debug first.
    exit /b 1
)

set "PATH=%MAKE_DIR%;%GCC_DIR%;%PATH%"

echo === Building STM32 firmware ===
"%MAKE_DIR%\make.exe" -C stm32_cubeide\Debug -j8 all
if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%

set "ELF=%ROOT%\stm32_cubeide\Debug\SterownikImpulsowySilnika_109-B-G431B-ESC1.elf"
set "OUTDIR=%ROOT%\dist\mcu"
set "BASE=Mikrotom_STM_safe_integration_20260424"

if not exist "%ELF%" (
    echo ERROR: ELF was not generated:
    echo   %ELF%
    exit /b 1
)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Generating HEX and BIN ...
"%GCC_DIR%\arm-none-eabi-objcopy.exe" -O ihex "%ELF%" "%OUTDIR%\%BASE%.hex"
if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%
"%GCC_DIR%\arm-none-eabi-objcopy.exe" -O binary "%ELF%" "%OUTDIR%\%BASE%.bin"
if not %ERRORLEVEL%==0 exit /b %ERRORLEVEL%

copy /Y "%OUTDIR%\%BASE%.hex" "%OUTDIR%\Mikrotom_STM_latest.hex" >nul
copy /Y "%OUTDIR%\%BASE%.bin" "%OUTDIR%\Mikrotom_STM_latest.bin" >nul

certutil -hashfile "%OUTDIR%\%BASE%.hex" SHA256 > "%OUTDIR%\%BASE%.hex.sha256.txt"
certutil -hashfile "%OUTDIR%\%BASE%.bin" SHA256 > "%OUTDIR%\%BASE%.bin.sha256.txt"

echo.
echo MCU release generated:
echo   %OUTDIR%\%BASE%.hex
echo   %OUTDIR%\%BASE%.bin
echo   %OUTDIR%\Mikrotom_STM_latest.hex
echo   %OUTDIR%\Mikrotom_STM_latest.bin
