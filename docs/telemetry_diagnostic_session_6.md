# Raport telemetrii - firmware DZIALA

Ten raport opisuje sesje telemetryczna firmware z brancha `DZIALA`.
Logger SQL tylko slucha UART i nie wysyla komend do MCU.

## Sesja pomiarowa

| Parametr | Wartosc |
| --- | --- |
| Session ID | `6` |
| Start UTC | `2026-04-28T11:52:56.529+00:00` |
| Koniec UTC | `2026-04-28T11:57:56.566+00:00` |
| Port | `COM6` |
| Baudrate | `460800` |
| Zrodlo | `DZIALA-original-uart` |
| Notatka | `wsad diagnostyczny po osobnym resecie MCU - ruch ruszyl` |
| Probki szybkiej telemetrii | `276990` |
| Probki diagnostyczne | `2689` |
| Czas pomiaru | `300000 ms` |
| Efektywna czestotliwosc zapisu | `923.3 Hz` |

## Ruch osi

| Parametr | Wartosc |
| --- | --- |
| Pozycja min | `-9166 um` |
| Pozycja max | `9352 um` |
| Rozpietosc ruchu | `18518 um` |
| Predkosc min | `-35 mm/s` |
| Predkosc max | `37 mm/s` |
| Predkosc abs p95 | `31.0 mm/s` |
| Przejscia przez zero | `286` |
| Szacowany okres cyklu | `2094 ms` |

## Prady fazowe

| Faza / metryka | Wartosc |
| --- | --- |
| Iu zakres | `-2410 .. 2107 mA` |
| Iv zakres | `-1643 .. 2492 mA` |
| Iw zakres | `-2383 .. 1839 mA` |
| Iu RMS | `1706 mA` |
| Iv RMS | `1285 mA` |
| Iw RMS | `1313 mA` |
| Max abs dowolnej fazy | `2492 mA` |
| Max abs fazy p95 | `2317 mA` |

## Diagnostyka D

| Parametr | Wartosc |
| --- | --- |
| VBUS | `12000 .. 12000 mV` |
| FOC state | `0 .. 0` |
| iq_ref | `1295 .. 2504 mA` |
| iq | `1286 .. 2501 mA` |
| vq_ref | `1391 .. 6846 mV` |
| Homing successful | `1 .. 1` |
| Homing ongoing | `0 .. 0` |

## Punkty zwrotne - podglad

| t_ms | pos_um | vel_mm_s |
| --- | --- | --- |
| `750` | `-9157` | `0` |
| `1797` | `9329` | `2` |
| `2860` | `-9156` | `0` |
| `3907` | `9340` | `2` |
| `4953` | `-9133` | `-1` |
| `6000` | `9334` | `1` |
| `7047` | `-9145` | `-1` |
| `8094` | `9319` | `2` |
| `9141` | `-9156` | `0` |
| `10203` | `9312` | `2` |
| `11235` | `-9155` | `-1` |
| `12313` | `9331` | `0` |
| `13360` | `-9165` | `-1` |
| `14407` | `9318` | `0` |
| `15469` | `-9160` | `-2` |
| `16500` | `9292` | `0` |
| `17547` | `-9138` | `0` |
| `18610` | `9319` | `0` |
| `19657` | `-9158` | `-1` |
| `20703` | `9339` | `1` |

## Wnioski

- Firmware pracuje powtarzalnie na krotkim odcinku okolo `18.5 mm`.
- Efektywny zapis SQL dziala z czestotliwoscia okolo `923.3 Hz`.
- W tej sesji zapisano `2689` ramek diagnostycznych `D;...`.
- Maksymalny zaobserwowany prad fazowy wyniosl okolo `2.49 A`, a wartosc p95 okolo `2.32 A`.
- Ten raport sluzy do porownywania kolejnych zmian bez naruszania dzialajacego ruchu.
