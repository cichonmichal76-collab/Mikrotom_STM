# Mikrotom STM - branch DZIALA

Ten branch jest czysta referencja starego, dzialajacego firmware STM32 dla sterownika impulsowego silnika liniowego.

Nie jest przeznaczony do bezposredniego merge do `main`. Ma byc stabilnym punktem odniesienia, na ktorym sukcesywnie i kontrolowanie bedziemy obudowywac dzialajacy rdzen nowymi funkcjami.

## Stan startowy

Projekt pochodzi z pierwotnego dzialajacego projektu STM32CubeIDE:

`SterownikImpulsowySilnika_109-B-G431B-ESC1`

Obecny kod:

- uruchamia peryferia STM32G431 na plytce B-G431B-ESC1,
- startuje PWM, ADC, OPAMP, enkoder TIM4 i USART2,
- wykonuje FOC w przerwaniu ADC,
- wykonuje homing automatycznie po starcie,
- po homingu jezdzi cyklicznie po krotkim odcinku,
- wysyla telemetrie tekstowa po UART.

## Potwierdzone zachowanie na urzadzeniu

Na fizycznym urzadzeniu stary wsad:

- uruchamia naped,
- wykonuje plynny ruch gora/dol,
- pracuje na krotkim odcinku,
- wysyla stabilna telemetrie UART.

Zarejestrowany zakres ruchu w oknie telemetrycznym byl okolo `18 mm`, co odpowiada ustawieniom ruchu miedzy `-10 mm` i `+10 mm`.

## Logika ruchu

Po starcie firmware sprawdza, czy homing zostal wykonany. Jezeli nie, uruchamia:

- `FOC_MotorHomeStart(...)`
- `FOC_MotorHomeUpdate(...)`

Po zakonczeniu trajektorii kod przelacza cel ruchu pomiedzy:

- `pos_A = 0.01 m`
- `pos_B = -0.01 m`

Oznacza to cykliczny ruch po odcinku okolo `20 mm`.

Kluczowe miejsca:

- `Core/Src/main.c` - start, petla glowna, telemetryka UART, przelaczanie trajektorii
- `Core/Src/foc.c` - FOC, regulacja pradu/predkosci/pozycji, homing
- `Core/Src/motorState.c` - stan silnika, enkoder, prad, pozycja, kat elektryczny
- `Core/Src/trajectory.c` - trajektoria ruchu

## UART

UART jest skonfigurowany jako:

- `USART2`
- `460800`
- `8N1`
- `TX/RX` sprzetowo

W tym stanie firmware uzywa UART praktycznie jednokierunkowo:

`MCU -> PC`

Kod wysyla telemetrie przez `HAL_UART_Transmit_IT(...)`.

Nie ma jeszcze aktywnego parsera komend po stronie MCU:

- brak `HAL_UART_Receive_IT`,
- brak `HAL_UART_Receive_DMA`,
- brak `HAL_UART_RxCpltCallback`,
- brak protokolu komend.

## Format telemetrii

Ramka tekstowa:

```text
Iu_mA;Iv_mA;Iw_mA;pos_um;vel_mm_s
```

Przyklad:

```text
1854;-1537;-317;-9225;2
```

Znaczenie pol:

- `Iu_mA` - prad fazy U w mA
- `Iv_mA` - prad fazy V w mA
- `Iw_mA` - prad fazy W w mA
- `pos_um` - pozycja z enkodera w um
- `vel_mm_s` - predkosc w mm/s

Na stanowisku telemetria byla widoczna na:

- `COM6` - USB-Enhanced-SERIAL-A CH344
- `COM9` - STMicroelectronics STLink Virtual COM Port

Do dalszej pracy jako glowny port roboczy przyjmujemy `COM6`.

## Istotne parametry startowe

W kodzie startowym:

- `pos_A = 0.01 m`
- `pos_B = -0.01 m`
- `czasTrajektorii = 1.0 s`
- `state.maxcurrent = 3.0 A`
- `state.um_per_elec_rad = 30000`
- `USART2 baudrate = 460800`

## Zasady pracy na tym branchu

1. Rdzen ruchu, FOC, homing i konfiguracja peryferiow pozostaja punktem odniesienia.
2. Kazda zmiana musi byc mala, opisana i testowana na urzadzeniu.
3. Po kazdym tescie sprzetowym dopisujemy obserwacje do sekcji `Dziennik zmian`.
4. Nie dodajemy na tym etapie GUI, agenta, SQL ani rozbudowanych paczek wdrozeniowych.
5. Dwukierunkowy UART bedzie dodawany pozniej, najpierw jako bezpieczny odczyt/status, a dopiero pozniej jako komendy ruchu.
6. Ten branch nie jest kandydatem do merge do `main`; sluzy jako kontrolowana baza rozwojowa.

## Plan rozwoju

Kolejne funkcje nalezy dodawac warstwowo:

1. Dokumentacja stanu startowego i telemetrii.
2. Stabilny logger telemetrii po stronie PC.
3. Minimalny odbiornik komend UART bez sterowania ruchem.
4. Komendy diagnostyczne typu `PING` / `GET STATUS`.
5. Komenda bezpiecznego zatrzymania.
6. Dopiero pozniej parametry, commissioning i przyszle GUI.

## Dziennik zmian

| Data | Commit | Zmiana | Test / obserwacja | Status |
| --- | --- | --- | --- | --- |
| 2026-04-28 | `7f8054c` | Utworzenie czystego brancha `DZIALA` ze starym dzialajacym projektem CubeIDE. | Stary wsad byl wgrany na MCU i uklad wykonywal plynny ruch gora/dol na krotkim odcinku. Telemetria UART potwierdzona na `COM6`, `460800 8N1`. | Punkt odniesienia |

