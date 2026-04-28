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

Szybka ramka tekstowa pozostaje bez zmian:

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

Rozszerzona ramka diagnostyczna ma prefiks `D` i jest wysylana rzadziej:

```text
D;loop_cnt;vbus_mV;pos_um;vel_mm_s;pos_set_um;vel_set_um_s;dest_um;traj_done;foc_state;iq_ref_mA;iq_mA;id_ref_mA;id_mA;vq_ref_mV;theta_mrad;homing_started;homing_ongoing;homing_successful;homing_step
```

Znaczenie dodatkowych pol:

- `loop_cnt` - licznik petli firmware
- `vbus_mV` - napiecie zasilania toru mocy w mV
- `pos_set_um` - pozycja zadana trajektorii w um
- `vel_set_um_s` - predkosc zadana trajektorii w um/s
- `dest_um` - docelowa pozycja trajektorii w um
- `traj_done` - flaga zakonczenia trajektorii
- `foc_state` - aktywny tryb FOC
- `iq_ref_mA` - zadany prad osi q w mA
- `iq_mA` - zmierzony prad osi q w mA
- `id_ref_mA` - zadany prad osi d w mA
- `id_mA` - zmierzony prad osi d w mA
- `vq_ref_mV` - zadane napiecie osi q w mV
- `theta_mrad` - kat elektryczny z pozycji enkodera w mrad
- `homing_started` - flaga startu homingu
- `homing_ongoing` - flaga trwania homingu
- `homing_successful` - flaga poprawnego homingu
- `homing_step` - krok procedury homingu

Po rozszerzeniu mozemy zbierac:

- `5` pol w szybkiej ramce ruchu,
- `19` pol w ramce diagnostycznej,
- razem `24` wartosci telemetryczne z MCU, liczone po formatach ramek.

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
4. Na tym etapie nie dodajemy GUI, agenta, komend ruchu ani rozbudowanych paczek wdrozeniowych.
5. SQL jest dopuszczony tylko po stronie PC jako pasywny zapis telemetrii; firmware MCU pozostaje bez zmian.
6. Dwukierunkowy UART bedzie dodawany pozniej, najpierw jako bezpieczny odczyt/status, a dopiero pozniej jako komendy ruchu.
7. Ten branch nie jest kandydatem do merge do `main`; sluzy jako kontrolowana baza rozwojowa.

## Plan rozwoju

Kolejne funkcje nalezy dodawac warstwowo:

1. Dokumentacja stanu startowego i telemetrii.
2. Stabilny logger telemetrii po stronie PC z zapisem do SQLite.
3. Minimalny odbiornik komend UART bez sterowania ruchem.
4. Komendy diagnostyczne typu `PING` / `GET STATUS`.
5. Komenda bezpiecznego zatrzymania.
6. Dopiero pozniej parametry, commissioning i przyszle GUI.

## Logger SQL telemetrii

Pierwsza nowa warstwa jest pasywna i dziala tylko po stronie PC. Nie zmienia firmware MCU.

Narzędzie:

`tools/telemetry_sql_logger.py`

Domyslnie czyta:

- port: `COM6`
- baudrate: `460800`
- format: `Iu_mA;Iv_mA;Iw_mA;pos_um;vel_mm_s`

Domyslnie zapisuje:

`telemetry/mikrotom_telemetry.sqlite3`

Uruchomienie:

```bat
scripts\run_telemetry_sql_logger.bat
```

Albo recznie:

```bat
python -m pip install -r requirements-pc.txt
python tools\telemetry_sql_logger.py --port COM6 --baud 460800 --db telemetry\mikrotom_telemetry.sqlite3 --echo
```

Schemat bazy:

- `sessions` - jedna sesja pomiarowa
- `telemetry_samples` - szybkie probki telemetrii z czasem, pradami, pozycja i predkoscia
- `diagnostic_samples` - rzadsze probki diagnostyczne z VBUS, zadaniami trajektorii, stanem FOC i homingiem

Zasada bezpieczenstwa:

Logger tylko slucha UART. Nie wysyla zadnych komend do MCU.

## Wzorzec pracy starego wsadu

Referencyjna sesja SQL:

- `session_id = 3`
- czas zbierania: `300 s`
- liczba probek: `283317`
- efektywna czestotliwosc zapisu: `944.4 Hz`
- bledne ramki: `0`

Parametry ruchu:

- zakres pozycji: `-9181 .. 9335 um`
- rozpiętość ruchu: `18516 um`, czyli okolo `18.5 mm`
- predkosc: `-35 .. 36 mm/s`
- `95 percentyl` predkosci bezwzglednej: `31.0 mm/s`
- liczba przejsc przez zero: `286`
- szacowany okres pelnego cyklu gora/dol: `2094 ms`

Prady fazowe:

- `Iu`: `-2395 .. 2110 mA`, RMS `1700 mA`
- `Iv`: `-1655 .. 2422 mA`, RMS `1262 mA`
- `Iw`: `-2307 .. 1867 mA`, RMS `1291 mA`
- maksymalna wartosc bezwzgledna dowolnej fazy: `2422 mA`
- `95 percentyl` maksymalnej wartosci bezwzglednej fazy: `2295 mA`

Wniosek:

Stary dzialajacy wsad pracuje powtarzalnie na krotkim odcinku okolo `18.5 mm`, z cyklem okolo `2.1 s` i pradem fazowym dochodzacym do okolo `2.4 A`.

Pelny raport bazowy:

`docs/telemetry_baseline_session_3.md`

Raport mozna odtworzyc poleceniem:

```bat
python tools\analyze_telemetry_sql.py --db telemetry\mikrotom_telemetry.sqlite3 --session-id 3 --markdown-out docs\telemetry_baseline_session_3.md
```

Druga sesja kontrolna:

- `session_id = 4`
- czas zbierania: `300 s`
- liczba probek: `283321`
- efektywna czestotliwosc zapisu: `944.4 Hz`
- logger odnotowal `1` pominieta ramke przy ponad `283k` probek

Raport sesji kontrolnej:

`docs/telemetry_control_session_4.md`

Porownanie baseline vs kontrola:

`docs/telemetry_comparison_session_3_vs_4.md`

Najwazniejsze roznice sesja `3` -> `4`:

- rozpietosc ruchu: `18516 um` -> `18511 um`, roznica `-5 um`,
- okres cyklu: `2094 ms` -> `2094 ms`, roznica `0 ms`,
- predkosc abs p95: `31.0 mm/s` -> `31.0 mm/s`, roznica `0.0 mm/s`,
- prad fazy p95: `2295 mA` -> `2300 mA`, roznica `5 mA`.

Wniosek:

Drugi pomiar potwierdza powtarzalnosc starego wsadu. Ruch, okres cyklu i poziom pradow sa stabilne, wiec branch `DZIALA` moze sluzyc jako baza do dalszego, warstwowego rozwoju bez burzenia dzialajacej logiki ruchu.

## Test wsadu diagnostycznego

Wsad diagnostyczny dodaje rzadsza ramke `D;...` i nie dodaje komend sterujacych po UART.

Podczas pierwszego uruchomienia po wgraniu program wykonywal sie i wysylal UART, ale uklad nie ruszyl. Krotka sesja SQL `5` pokazala:

- `18506` szybkich ramek,
- `189` ramek diagnostycznych,
- pozycja stala: `1641 .. 1641 um`,
- predkosc: `0 .. 0 mm/s`,
- prady fazowe tylko szumowe,
- `vbus = 12000 mV`,
- `homing_successful = 0`,
- `foc_state = 3`,
- `iq_ref = 0`, `iq = 0`, `vq_ref = 0`.

Po osobnym resecie MCU uklad ruszyl. Pelna sesja SQL `6` pokazala:

- `276990` szybkich ramek,
- `2689` ramek diagnostycznych,
- `0` blednych ramek,
- zakres pozycji: `-9166 .. 9352 um`,
- rozpietosc ruchu: `18518 um`,
- predkosc: `-35 .. 37 mm/s`,
- szacowany okres cyklu: `2094 ms`,
- maksymalna wartosc bezwzgledna dowolnej fazy: `2492 mA`,
- `95 percentyl` maksymalnej wartosci bezwzglednej fazy: `2317 mA`,
- `homing_successful = 1`,
- `foc_state = 0`,
- `iq_ref = 1295 .. 2504 mA`,
- `iq = 1286 .. 2501 mA`,
- `vbus = 12000 mV`.

Porownanie sesji `3` i `6`:

- rozpietosc ruchu: `18516 um` -> `18518 um`, roznica `2 um`,
- okres cyklu: `2094 ms` -> `2094 ms`, roznica `0 ms`,
- predkosc abs p95: `31.0 mm/s` -> `31.0 mm/s`, roznica `0.0 mm/s`,
- prad fazy p95: `2295 mA` -> `2317 mA`, roznica `22 mA`,
- czestotliwosc szybkich ramek: `944.4 Hz` -> `923.3 Hz`, spadek okolo `2.23%`.

Wniosek:

Ramka diagnostyczna `D;...` nie zmienila charakteru ruchu. Ma jednak mierzalny koszt transmisji UART, bo zmniejsza liczbe szybkich ramek o okolo `2.23%`. Do dalszych prac zachowujemy ja jako przydatna diagnostyke, ale nie nalezy zwiekszac jej czestotliwosci bez potrzeby.

Raporty:

```text
docs/telemetry_diagnostic_session_6.md
docs/telemetry_comparison_session_3_vs_6.md
docs/MCU_Flash_Procedure.md
```

Procedura wgrywania MCU zostala rozdzielona na programowanie i osobny reset:

```bat
scripts\flash_mcu_diag_stlink.bat
scripts\reset_mcu_stlink.bat
```

## UART-RX-1

Pierwszy etap odbioru komend `PC -> MCU` zostal dodany w trybie tylko diagnostycznym.

Zaimplementowane komendy:

- `PING` -> `RSP;PONG`,
- `GET_STATUS` -> `RSP;STATUS;pos_um;vel_mm_s;vbus_mV;foc_state;homing_successful;homing_ongoing;homing_step`,
- `GET_VERSION` -> `RSP;VERSION;DZIALA;UART_RX_1_115200;115200`,
- nieznana komenda -> `RSP;ERR;UNKNOWN_CMD`.

Warunek bezpieczenstwa:

Komendy nie zmieniaja trajektorii, pozycji zadanej, pradu zadanego, PWM, FOC, homingu ani parametrow regulatorow.

Dokumentacja:

```text
docs/UART_RX_1.md
```

Build i wgranie tej wersji:

```bat
scripts\build_mcu_uart_rx1_hex.bat
scripts\flash_mcu_uart_rx1_stlink.bat
scripts\reset_mcu_stlink.bat
```

Status:

Pierwszy test wersji `460800` potwierdzil normalny ruch i telemetrie TX, ale brak odpowiedzi `RSP;...` na komendy z PC. Wariant `UART_RX_1_115200` zostal wgrany i potwierdzony na `COM6`: `PING`, `GET_VERSION`, `GET_STATUS` i nieznana komenda zwracaja `RSP;...`. `COM9` pokazuje telemetrie TX, ale nie sluzy jako potwierdzony kanal komend PC -> MCU.

## Dziennik zmian

| Data | Commit | Zmiana | Test / obserwacja | Status |
| --- | --- | --- | --- | --- |
| 2026-04-28 | `7f8054c` | Utworzenie czystego brancha `DZIALA` ze starym dzialajacym projektem CubeIDE. | Stary wsad byl wgrany na MCU i uklad wykonywal plynny ruch gora/dol na krotkim odcinku. Telemetria UART potwierdzona na `COM6`, `460800 8N1`. | Punkt odniesienia |
| 2026-04-28 | `SQL logger` | Dodanie pasywnego loggera `UART -> SQLite` po stronie PC. | Test na `COM6`: zapisano `500` ramek do SQLite, `0` ramek blednych. Zakres z testu: `pos_um -9158 .. 2076`, `vel_mm_s 3 .. 35`. Firmware MCU bez zmian. | Potwierdzone |
| 2026-04-28 | `diagnostic TX` | Dodanie rzadkiej ramki diagnostycznej `D;...` oraz zapisu jej do tabeli `diagnostic_samples`. | Zmiana TX-only. Stara szybka ramka `Iu;Iv;Iw;pos;vel` pozostaje bez zmian. Build CubeIDE: `0 errors`. Nie wgrywano jeszcze do MCU. | Zbudowane |
| 2026-04-28 | `SQL analysis` | Dodanie analizatora bazy `tools/analyze_telemetry_sql.py`. | Analiza sesji `3`: `283317` probek przez `300 s`, zakres ruchu `18.5 mm`, okres cyklu `2094 ms`, prad fazy max `2422 mA`, `0` blednych ramek. | Wzorzec zapisany |
| 2026-04-28 | `baseline report` | Dodanie eksportu raportu Markdown z analizy SQL. | Wygenerowano `docs/telemetry_baseline_session_3.md` dla sesji `3`. Firmware MCU bez zmian. | Raport zapisany |
| 2026-04-28 | `control comparison` | Dodanie porownania dwoch sesji SQL. | Sesja `4`: `283321` probek przez `300 s`. Roznica rozpietosci ruchu wzgledem sesji `3`: `-5 um`, okres cyklu bez zmian, prad fazy p95 `+5 mA`. Firmware MCU bez zmian. | Powtarzalnosc potwierdzona |
| 2026-04-28 | `diag firmware test` | Wgranie wsadu z ramka diagnostyczna `D;...` i zapis procedury flash/reset. | Pierwszy start po flash nie ruszyl, bo firmware wystartowal przed poprawna sekwencja startowa homingu. Po osobnym resecie MCU sesja `6` potwierdzila ruch zgodny z baseline: roznica rozpietosci `2 um`, okres cyklu bez zmian, `2689` ramek diagnostycznych. | Diagnostyka potwierdzona |
| 2026-04-28 | `future backlog` | Zapisanie pomyslu `DIAG-2` jako niezrealizowanego backlogu. | Automatyczny monitoring SQL `PASS/WARN/FAIL` zostal odlozony. Szczegoly w `future.md`. | Odlozone |
| 2026-04-28 | `UART-RX-1` | Dodanie odbioru komend diagnostycznych `PING`, `GET_STATUS`, `GET_VERSION`. | Wariant `460800` po wgraniu nie psul ruchu, ale nie odpowiadal na `RSP;...`. Wariant `115200` na `COM6` potwierdzil `RSP;PONG`, `RSP;VERSION`, `RSP;STATUS` i `RSP;ERR`. Brak komend ruchu. | Potwierdzone |
