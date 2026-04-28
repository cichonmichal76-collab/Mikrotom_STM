# UART-RX-1 - komendy diagnostyczne bez sterowania ruchem

Status: wariant `115200` przygotowany po tescie wersji `460800`.

## Cel

Dodac pierwszy odbior komend `PC -> MCU`, ale bez zadnej mozliwosci sterowania ruchem.

Ten etap sprawdza tylko dwukierunkowa komunikacje UART i przygotowuje fundament pod przyszle GUI/HMI.

## Warunek bezpieczenstwa

Komendy `UART-RX-1` nie zmieniaja:

- trajektorii,
- pozycji zadanej,
- pradu zadanego,
- PWM,
- FOC,
- homingu,
- parametrow regulatorow.

Odpowiedzi korzystaja z istniejacego UART i moga jedynie chwilowo opoznic jedna ramke telemetryczna, jezeli operator wysle komende dokladnie w czasie transmisji.

## Port

```text
USART2
115200
8N1
COM6 na stanowisku testowym
```

Uwaga:

Pierwszy test wersji `460800` potwierdzil normalny ruch i telemetrie TX, ale brak odpowiedzi `RSP;...` na komendy z PC. Poniewaz wczesniejsza dzialajaca wersja stanowiska uzywala `115200`, wariant `UART-RX-1` zostal przestawiony na `115200`.

Zmiana baudrate wymaga ograniczenia strumienia telemetrycznego:

- szybka ramka `Iu;Iv;Iw;pos;vel` co `50` taktow petli TIM2,
- ramka diagnostyczna `D;...` co `5000` taktow petli TIM2.

## Komendy

Kazda komenda konczy sie znakiem konca linii `\r`, `\n` albo `\r\n`.

| Komenda | Odpowiedz | Znaczenie |
| --- | --- | --- |
| `PING` | `RSP;PONG` | Test, czy MCU odbiera i odpowiada. |
| `GET_STATUS` | `RSP;STATUS;pos_um;vel_mm_s;vbus_mV;foc_state;homing_successful;homing_ongoing;homing_step` | Odczyt stanu diagnostycznego bez zmiany pracy napedu. |
| `GET_VERSION` | `RSP;VERSION;DZIALA;UART_RX_1_115200;115200` | Identyfikacja wersji funkcji UART. |
| inna komenda | `RSP;ERR;UNKNOWN_CMD` | Komenda nierozpoznana. |

## Implementacja

Odbior dziala przez:

```c
HAL_UART_Receive_IT(&huart2, &uart2_rx_byte, 1);
```

Parser:

- odbiera po jednym bajcie,
- sklada linie do bufora `UART_RX_LINE_LEN`,
- po koncu linii ustawia tylko flage odpowiedzi,
- nie wykonuje zadnego dzialania ruchowego.

Wysylka odpowiedzi jest wykonywana pozniej przez istniejacy tor UART, gdy `huart2.gState == HAL_UART_STATE_READY`.

## Pliki

Kod:

```text
Core/Src/main.c
```

Build HEX:

```bat
scripts\build_mcu_uart_rx1_hex.bat
```

Wgranie:

```bat
scripts\flash_mcu_uart_rx1_stlink.bat
scripts\reset_mcu_stlink.bat
```

## Wynik builda

Build `Debug` zakonczony bez bledow.

Wygenerowany plik:

```text
Debug\SterownikImpulsowySilnika_109-B-G431B-ESC1_uart_rx1_115200.hex
```

Rozmiar ELF po buildzie:

```text
text = 51832
data = 468
bss  = 3544
```

Ostrzezenia kompilatora sa zgodne z istniejacymi ostrzezeniami starego projektu, glownie `volatile` qualifier oraz wczesniejsze nieuzywane zmienne.
