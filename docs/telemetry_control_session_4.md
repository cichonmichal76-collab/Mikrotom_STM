# Raport telemetrii - firmware DZIALA

Ten raport opisuje sesje telemetryczna firmware z brancha `DZIALA`.
Logger SQL tylko slucha UART i nie wysyla komend do MCU.

## Sesja pomiarowa

| Parametr | Wartosc |
| --- | --- |
| Session ID | `4` |
| Start UTC | `2026-04-28T11:37:13.485+00:00` |
| Koniec UTC | `2026-04-28T11:42:13.512+00:00` |
| Port | `COM6` |
| Baudrate | `460800` |
| Zrodlo | `DZIALA-original-uart` |
| Notatka | `drugi pomiar kontrolny starego wsadu 5 min` |
| Probki szybkiej telemetrii | `283321` |
| Probki diagnostyczne | `0` |
| Czas pomiaru | `300000 ms` |
| Efektywna czestotliwosc zapisu | `944.4 Hz` |

## Ruch osi

| Parametr | Wartosc |
| --- | --- |
| Pozycja min | `-9188 um` |
| Pozycja max | `9323 um` |
| Rozpietosc ruchu | `18511 um` |
| Predkosc min | `-36 mm/s` |
| Predkosc max | `36 mm/s` |
| Predkosc abs p95 | `31.0 mm/s` |
| Przejscia przez zero | `286` |
| Szacowany okres cyklu | `2094 ms` |

## Prady fazowe

| Faza / metryka | Wartosc |
| --- | --- |
| Iu zakres | `-2417 .. 2087 mA` |
| Iv zakres | `-1657 .. 2439 mA` |
| Iw zakres | `-2299 .. 1879 mA` |
| Iu RMS | `1699 mA` |
| Iv RMS | `1254 mA` |
| Iw RMS | `1282 mA` |
| Max abs dowolnej fazy | `2439 mA` |
| Max abs fazy p95 | `2300 mA` |

## Punkty zwrotne - podglad

| t_ms | pos_um | vel_mm_s |
| --- | --- | --- |
| `813` | `9282` | `0` |
| `1860` | `-9139` | `-2` |
| `2907` | `9309` | `1` |
| `3953` | `-9138` | `-1` |
| `5000` | `9307` | `2` |
| `6047` | `-9146` | `-1` |
| `7110` | `9302` | `1` |
| `8157` | `-9154` | `-1` |
| `9203` | `9283` | `2` |
| `10250` | `-9153` | `-1` |
| `11297` | `9308` | `0` |
| `12344` | `-9138` | `0` |
| `13391` | `9297` | `1` |
| `14453` | `-9168` | `0` |
| `15500` | `9295` | `1` |
| `16547` | `-9156` | `0` |
| `17610` | `9300` | `0` |
| `18641` | `-9157` | `-2` |
| `19719` | `9292` | `1` |
| `20750` | `-9163` | `-2` |

## Wnioski

- Firmware pracuje powtarzalnie na krotkim odcinku okolo `18.5 mm`.
- Efektywny zapis SQL dziala z czestotliwoscia okolo `944.4 Hz`.
- W tej sesji zapisano `0` ramek diagnostycznych `D;...`.
- Maksymalny zaobserwowany prad fazowy wyniosl okolo `2.44 A`, a wartosc p95 okolo `2.30 A`.
- Ten raport sluzy do porownywania kolejnych zmian bez naruszania dzialajacego ruchu.
