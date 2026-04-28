# Porownanie telemetrii - sesja bazowa vs kontrolna

Raport porownuje dwa pomiary wykonane na starym, dzialajacym wsadzie.
Celem jest sprawdzenie powtarzalnosci ruchu przed kolejnymi zmianami firmware.

## Sesje

| Rola | Session ID | Start UTC | Port | Baudrate | Notatka |
| --- | --- | --- | --- | --- | --- |
| Baseline | `3` | `2026-04-28T11:15:11.000+00:00` | `COM6` | `460800` | `stary wsad stabilny ruch 5 min` |
| Kontrolna | `4` | `2026-04-28T11:37:13.485+00:00` | `COM6` | `460800` | `drugi pomiar kontrolny starego wsadu 5 min` |

## Porownanie liczbowe

| Metryka | Baseline | Kontrolna | Roznica | Roznica % |
| --- | --- | --- | --- | --- |
| Probki | `283317` | `283321` | `4` | `0.00%` |
| Czas | `300000 ms` | `300000 ms` | `0 ms` | `0.00%` |
| Czestotliwosc zapisu | `944.4 Hz` | `944.4 Hz` | `0.0 Hz` | `0.00%` |
| Pozycja min | `-9181 um` | `-9188 um` | `-7 um` | `-0.08%` |
| Pozycja max | `9335 um` | `9323 um` | `-12 um` | `-0.13%` |
| Rozpietosc ruchu | `18516 um` | `18511 um` | `-5 um` | `-0.03%` |
| Predkosc min | `-35 mm/s` | `-36 mm/s` | `-1 mm/s` | `-2.86%` |
| Predkosc max | `36 mm/s` | `36 mm/s` | `0 mm/s` | `0.00%` |
| Predkosc abs p95 | `31.0 mm/s` | `31.0 mm/s` | `0.0 mm/s` | `0.00%` |
| Iu RMS | `1700 mA` | `1699 mA` | `-2 mA` | `-0.11%` |
| Iv RMS | `1262 mA` | `1254 mA` | `-7 mA` | `-0.57%` |
| Iw RMS | `1291 mA` | `1282 mA` | `-9 mA` | `-0.68%` |
| Max abs fazy | `2422 mA` | `2439 mA` | `17 mA` | `0.70%` |
| Max abs fazy p95 | `2295 mA` | `2300 mA` | `5 mA` | `0.22%` |
| Przejscia przez zero | `286` | `286` | `0` | `0.00%` |
| Okres cyklu | `2094 ms` | `2094 ms` | `0 ms` | `0.00%` |

## Wnioski

- Rozpietosc ruchu zmienila sie o `-5 um`, czyli praktycznie bez istotnej roznicy.
- Okres cyklu zmienil sie o `0 ms`, co oznacza powtarzalny rytm pracy.
- Predkosc p95 pozostala na poziomie `31.0 mm/s`.
- Prad fazowy p95 zmienil sie o `5 mA`.
- Czestotliwosc zapisu SQL zmienila sie o `0.0 Hz`.
- Sesja kontrolna potwierdza, ze stary wsad jest dobrym punktem odniesienia do dalszych zmian warstwowych.
- W terminalu loggera dla sesji kontrolnej odnotowano `1` pominieta ramke; przy ponad `283k` probek nie wplywa to na wnioski o ruchu.
