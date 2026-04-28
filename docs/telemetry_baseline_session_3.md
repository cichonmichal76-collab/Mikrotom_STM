# Raport telemetrii - firmware DZIALA

Ten raport opisuje sesje telemetryczna firmware z brancha `DZIALA`.
Logger SQL tylko slucha UART i nie wysyla komend do MCU.

## Sesja pomiarowa

| Parametr | Wartosc |
| --- | --- |
| Session ID | `3` |
| Start UTC | `2026-04-28T11:15:11.000+00:00` |
| Koniec UTC | `2026-04-28T11:20:11.063+00:00` |
| Port | `COM6` |
| Baudrate | `460800` |
| Zrodlo | `DZIALA-original-uart` |
| Notatka | `stary wsad stabilny ruch 5 min` |
| Probki szybkiej telemetrii | `283317` |
| Probki diagnostyczne | `0` |
| Czas pomiaru | `300000 ms` |
| Efektywna czestotliwosc zapisu | `944.4 Hz` |

## Ruch osi

| Parametr | Wartosc |
| --- | --- |
| Pozycja min | `-9181 um` |
| Pozycja max | `9335 um` |
| Rozpietosc ruchu | `18516 um` |
| Predkosc min | `-35 mm/s` |
| Predkosc max | `36 mm/s` |
| Predkosc abs p95 | `31.0 mm/s` |
| Przejscia przez zero | `286` |
| Szacowany okres cyklu | `2094 ms` |

## Prady fazowe

| Faza / metryka | Wartosc |
| --- | --- |
| Iu zakres | `-2395 .. 2110 mA` |
| Iv zakres | `-1655 .. 2422 mA` |
| Iw zakres | `-2307 .. 1867 mA` |
| Iu RMS | `1700 mA` |
| Iv RMS | `1262 mA` |
| Iw RMS | `1291 mA` |
| Max abs dowolnej fazy | `2422 mA` |
| Max abs fazy p95 | `2295 mA` |

## Punkty zwrotne - podglad

| t_ms | pos_um | vel_mm_s |
| --- | --- | --- |
| `14` | `-9146` | `0` |
| `1000` | `9290` | `2` |
| `2061` | `-9143` | `0` |
| `3108` | `9303` | `1` |
| `4156` | `-9142` | `0` |
| `5202` | `9306` | `0` |
| `6250` | `-9152` | `-1` |
| `7296` | `9301` | `1` |
| `8343` | `-9141` | `-2` |
| `9406` | `9306` | `0` |
| `10452` | `-9137` | `0` |
| `11500` | `9293` | `0` |
| `12561` | `-9148` | `-2` |
| `13593` | `9276` | `1` |
| `14656` | `-9144` | `0` |
| `15702` | `9311` | `0` |
| `16750` | `-9144` | `0` |
| `17796` | `9308` | `0` |
| `18858` | `-9152` | `0` |
| `19906` | `9308` | `0` |

## Wnioski

- Firmware pracuje powtarzalnie na krotkim odcinku okolo `18.5 mm`.
- Efektywny zapis SQL dziala z czestotliwoscia okolo `944.4 Hz`.
- W tej sesji zapisano `0` ramek diagnostycznych `D;...`.
- Maksymalny zaobserwowany prad fazowy wyniosl okolo `2.42 A`, a wartosc p95 okolo `2.29 A`.
- Ten raport sluzy do porownywania kolejnych zmian bez naruszania dzialajacego ruchu.
