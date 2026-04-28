# Procedura wgrywania MCU - branch DZIALA

Ten dokument zapisuje sprawdzona procedure wgrywania firmware na stanowisku `B-G431B-ESC1`.

## Dlaczego procedura jest rozdzielona

Stary firmware uruchamia homing automatycznie zaraz po resecie MCU. Podczas testu wsadu diagnostycznego potwierdzono, ze jezeli reset po programowaniu nastapi w momencie, w ktorym tor mocy nie jest gotowy do startu firmware, homing moze nie wykonac ruchu. MCU nadal pracuje i wysyla UART, ale naped pozostaje bez ruchu.

Dlatego od teraz rozdzielamy:

1. programowanie flash,
2. przygotowanie stanowiska do startu firmware,
3. osobny reset MCU.

## Narzedzia

Uzywany programator:

```text
C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.200.202503041107\tools\bin\STM32_Programmer_CLI.exe
```

Na tym komputerze zapis firmware dziala przez opcje CubeProgrammer:

```text
-d
```

Opcja `-w` zwracala w tym srodowisku blad Windows `Odmowa dostepu`, dlatego nie uzywamy jej w skryptach.

## Pliki

Domyslny plik HEX dla wsadu diagnostycznego:

```text
Debug\SterownikImpulsowySilnika_109-B-G431B-ESC1_diag.hex
```

Skrypty:

```text
scripts\flash_mcu_diag_stlink.bat
scripts\reset_mcu_stlink.bat
```

## Kroki

1. Zbuduj projekt `Debug` w STM32CubeIDE albo przez makefile CubeIDE.
2. Upewnij sie, ze istnieje plik:

```text
Debug\SterownikImpulsowySilnika_109-B-G431B-ESC1_diag.hex
```

3. Wgraj firmware bez automatycznego resetu:

```bat
scripts\flash_mcu_diag_stlink.bat
```

4. Przygotuj stanowisko do startu firmware zgodnie z procedura testowa.
5. Wykonaj osobny reset MCU:

```bat
scripts\reset_mcu_stlink.bat
```

6. Sprawdz UART na `COM6`, `460800 8N1`.
7. Potwierdz, ze po resecie firmware wykonuje ruch oraz wysyla ramki:

```text
Iu_mA;Iv_mA;Iw_mA;pos_um;vel_mm_s
D;loop_cnt;vbus_mV;pos_um;...
```

## Wniosek z testu 2026-04-28

Po pierwszym wgraniu wsadu diagnostycznego MCU wysylalo UART, ale uklad nie ruszyl. Telemetria pokazala:

- `pos_um = 1641` bez zmian,
- `vel_mm_s = 0`,
- `iq_ref_ma = 0`,
- `iq_ma = 0`,
- `homing_successful = 0`,
- `foc_state = 3`.

Po wykonaniu osobnego resetu MCU uklad ruszyl poprawnie. Sesja SQL `6` potwierdzila:

- `homing_successful = 1`,
- `foc_state = 0`,
- `iq_ref_ma = 1295 .. 2504`,
- `iq_ma = 1286 .. 2501`,
- ruch zgodny z baseline.

To potwierdza, ze problem byl w sekwencji startu po programowaniu, a nie w samej ramce diagnostycznej.
