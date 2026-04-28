# Porownanie telemetrii - sesja bazowa vs kontrolna

Raport porownuje dwa pomiary wykonane na firmware z brancha `DZIALA`.
Celem jest sprawdzenie powtarzalnosci ruchu po kolejnych malych zmianach firmware.

## Sesje

| Rola | Session ID | Start UTC | Port | Baudrate | Notatka |
| --- | --- | --- | --- | --- | --- |
| Baseline | `3` | `2026-04-28T11:15:11.000+00:00` | `COM6` | `460800` | `stary wsad stabilny ruch 5 min` |
| Kontrolna | `6` | `2026-04-28T11:52:56.529+00:00` | `COM6` | `460800` | `wsad diagnostyczny po osobnym resecie MCU - ruch ruszyl` |

## Porownanie liczbowe

| Metryka | Baseline | Kontrolna | Roznica | Roznica % |
| --- | --- | --- | --- | --- |
| Probki | `283317` | `276990` | `-6327` | `-2.23%` |
| Czas | `300000 ms` | `300000 ms` | `0 ms` | `0.00%` |
| Czestotliwosc zapisu | `944.4 Hz` | `923.3 Hz` | `-21.1 Hz` | `-2.23%` |
| Pozycja min | `-9181 um` | `-9166 um` | `15 um` | `0.16%` |
| Pozycja max | `9335 um` | `9352 um` | `17 um` | `0.18%` |
| Rozpietosc ruchu | `18516 um` | `18518 um` | `2 um` | `0.01%` |
| Predkosc min | `-35 mm/s` | `-35 mm/s` | `0 mm/s` | `0.00%` |
| Predkosc max | `36 mm/s` | `37 mm/s` | `1 mm/s` | `2.78%` |
| Predkosc abs p95 | `31.0 mm/s` | `31.0 mm/s` | `0.0 mm/s` | `0.00%` |
| Iu RMS | `1700 mA` | `1706 mA` | `6 mA` | `0.34%` |
| Iv RMS | `1262 mA` | `1285 mA` | `23 mA` | `1.83%` |
| Iw RMS | `1291 mA` | `1313 mA` | `22 mA` | `1.69%` |
| Max abs fazy | `2422 mA` | `2492 mA` | `70 mA` | `2.89%` |
| Max abs fazy p95 | `2295 mA` | `2317 mA` | `22 mA` | `0.96%` |
| Przejscia przez zero | `286` | `286` | `0` | `0.00%` |
| Okres cyklu | `2094 ms` | `2094 ms` | `0 ms` | `0.00%` |

## Wnioski

- Rozpietosc ruchu zmienila sie o `2 um`, czyli praktycznie bez istotnej roznicy.
- Okres cyklu zmienil sie o `0 ms`, co oznacza powtarzalny rytm pracy.
- Predkosc p95 pozostala na poziomie `31.0 mm/s`.
- Prad fazowy p95 zmienil sie o `22 mA`.
- Czestotliwosc zapisu SQL zmienila sie o `-21.1 Hz`.
- Porownanie pokazuje, czy branch `DZIALA` pozostaje dobrym punktem odniesienia do dalszych zmian warstwowych.
- Porownanie dotyczy poprawnie sparsowanych ramek zapisanych w SQLite.
