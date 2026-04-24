# Wariant B - instrukcja uzytkownika GUI

Ta instrukcja opisuje sposob pracy z aplikacja operatorska `GUI` w projekcie Mikrotom.

Zrzuty ponizej pokazuja aktualny interfejs. Uklad ekranow w trybie `LIVE` jest taki sam jak w trybie `DEMO`. Zmieniaja sie tylko dane, bo w `LIVE` pochodza one z Agenta i STM32.

## 1. Do czego sluzy GUI

GUI nie jest sterownikiem mocy. GUI jest warstwa operatorsko-diagnostyczna.

Jego rola:

- pokazac aktualny stan systemu,
- pokazac safety i commissioning,
- pozwolic ustawic parametry,
- wyslac komendy do Agenta,
- pokazac logi, SQL, UART i API,
- uporzadkowac pierwsze uruchomienie urzadzenia.

Prawidlowy przeplyw:

```text
GUI -> HTTP API -> Agent -> UART -> STM32
```

## 2. Co GUI robi, a czego nie robi

GUI robi:

- pokazuje statusy firmware,
- wysyla komendy do Agenta,
- wspiera commissioning,
- pomaga w diagnozie.

GUI nie robi:

- nie steruje hardware bezposrednio,
- nie omija safety,
- nie daje prawa do ruchu tylko dlatego, ze przycisk jest widoczny,
- nie zastepuje decyzji firmware.

## 3. Gorna belka

Na gornej belce znajduja sie:

- zakladki aplikacji,
- przelacznik motywu `ciemny/jasny`,
- przelacznik `Help ON/OFF`.

### `Help ON`

Po wlaczeniu:

- pojawiaja sie ikonki `?`,
- po najechaniu widac opis znaczenia pola,
- na koncu opisu jest referencja do zmiennej, funkcji albo endpointu.

### `Help OFF`

Po wylaczeniu:

- ikonki `?` znikaja,
- dymki znikaja,
- interfejs jest prostszy dla zwyklej pracy operatorskiej.

## 4. Podglad online

![Podglad online](images/online.png)

To glowny ekran pracy. Znajdziesz tu:

- status polaczenia,
- stan osi,
- fault,
- `RUN_ALLOWED`,
- etap uruchomienia,
- KPI,
- statusy safety,
- commissioning etapowy,
- przyciski operatorskie.

### Jak czytac ten ekran

- `Polaczenie OK` oznacza, ze GUI ma kontakt ze zrodlem danych,
- `Blad` pokazuje aktywny fault firmware,
- `RUN_ALLOWED` jest centralna zgoda firmware na ruch,
- `VBUS` pokazuje napiecie magistrali,
- `SAFE_INTEGRATION` i `MOTION_IMPLEMENTED` mowia, jaki wariant firmware jest w MCU.

### Jak rozumiec przyciski operatorskie

- `ENABLE` - logiczne wlaczenie napedu,
- `DISABLE` - logiczne wylaczenie napedu,
- `ACK_FAULT` - potwierdzenie bledu po usunieciu przyczyny,
- `STOP` - zatrzymanie normalne,
- `QSTOP` - zatrzymanie szybkie i bardziej restrykcyjne,
- `HOME` - jawny start sekwencji homingu w buildzie ruchowym,
- `MOVE_REL` - ruch wzgledny,
- `MOVE_ABS` - ruch bezwzgledny,
- `CALIB_ZERO` - komenda serwisowa kalibracji zera.

### Co zobaczysz w `SAFE`

W wariancie `SAFE` poprawny wynik to:

- komunikacja dziala,
- telemetria dziala,
- `HOME` i `MOVE_*` nie powoduja realnego ruchu,
- os nie rusza sama.

### Co zobaczysz w `MOTION-ENABLED`

W wariancie `MOTION-ENABLED`:

- `HOME` jest aktywny,
- po `HOME` i spelnieniu bramek safety moze pojawic sie zgoda na ruch,
- `MOVE_REL` i `MOVE_ABS` moga wykonac realny ruch.

## 5. Commissioning etapowy

Panel etapow nie sluzy do ozdoby. Ma wymusic kolejnosc uruchomienia.

### Etap 1

Cel:

- komunikacja,
- status,
- brak ruchu.

Operator:

- sprawdza `Polaczenie`, `FAULT`, `VBUS`,
- sprawdza `UART`, `Logi`, `SQL`,
- potwierdza brak samoczynnego ruchu.

### Etap 2

Cel:

- logika operatorska,
- reakcje safety,
- nadal bez realnego ruchu w `SAFE`.

Operator:

- testuje `ENABLE`, `DISABLE`, `STOP`, `QSTOP`,
- obserwuje logi i statusy,
- nie przechodzi dalej, jesli firmware zachowuje sie niespojnie.

### Etap 3

Cel:

- przejscie do kontrolowanego ruchu.

Wazne:

- w `SAFE` to nadal tylko walidacja workflow,
- w `MOTION-ENABLED` to etap pod `HOME` i pierwszy maly ruch.

## 6. Ustawienia globalne

![Ustawienia globalne](images/global.png)

Zakladka sluzy do ustawiania limitow pracy osi:

- `MAX_CURRENT [A]`,
- `MAX_CURRENT_PEAK [A]`,
- `MAX_VELOCITY [m/s]`,
- `MAX_ACCELERATION [m/s2]`,
- `SOFT_MIN_POS [um]`,
- `SOFT_MAX_POS [um]`.

Jak uzywac:

1. Ustaw zachowawcze limity.
2. Wyslij parametry.
3. Wroc do `Podglad online`.
4. Sprawdz statusy.

Na pierwsze uruchomienie wartosci powinny byc ostrozne.

## 7. Bezpieczenstwo

![Bezpieczenstwo](images/safety.png)

Ta zakladka oddziela zwykle parametry od safety.

Znajdziesz tu:

- obecny hardware safety,
- czujniki i blokady,
- override safety,
- zgode na ruch bez kalibracji,
- stan kalibracji.

Zasada praktyczna:

- pola `zainstalowany` opisuja rzeczywisty hardware,
- pola `ignoruj` i `pozwol` sa obejsciami,
- obejsc nie nalezy uzywac bez wyraznej potrzeby testowej.

## 8. UART

![UART](images/uart.png)

Zakladka `UART` sluzy do diagnozy polaczenia Agent - STM32.

Widzisz tu:

- port COM,
- baudrate,
- status polaczenia,
- dane diagnostyczne z Agenta.

Tu zagladaj, gdy:

- GUI nie ma danych LIVE,
- Agent nie odpowiada,
- sa timeouty,
- nie masz pewnosci, czy wybrales dobry port COM.

## 9. Logi

![Logi](images/logs.png)

Zakladka `Logi` pokazuje zdarzenia z firmware.

Typowe wpisy:

- `BOOT`,
- `ENABLE`,
- `DISABLE`,
- `STOP`,
- `QSTOP`,
- `FAULT_SET`,
- `ACK_FAULT`,
- `CALIB_OK`.

Logi sa przydatne, gdy chcesz zobaczyc kolejnosc zdarzen i porownac ja z tym, co zrobil operator.

## 10. SQL

![SQL](images/sql.png)

Zakladka `SQL` pokazuje logowanie po stronie Agenta.

Znajdziesz tu:

- status SQLite,
- sciezke pliku bazy,
- liczbe rekordow.

To sluzy do:

- traceability,
- diagnozy po testach,
- porownywania komend i odpowiedzi,
- odkladania telemetrii poza MCU.

## 11. API

![API](images/api.png)

Zakladka `API` jest dla integratora i programisty.

Pokazuje:

- endpointy HTTP Agenta,
- ich znaczenie,
- panel komendy `RAW`.

Panel `RAW` jest narzedziem serwisowym. Uzywaj go do:

- recznej diagnostyki,
- pojedynczych komend protokolu,
- testow integracyjnych.

Nie traktuj go jako zwyklej sciezki operatorskiej.

## 12. Typowy sposob pracy

### Pierwsze uruchomienie z `SAFE`

1. Otworz `Podglad online`.
2. Sprawdz `Polaczenie`, `Blad`, `RUN_ALLOWED`, `Etap`.
3. Sprawdz `VBUS`.
4. Potwierdz brak ruchu.
5. Przejdz przez Etap 1 i 2.

### Przejscie do `MOTION-ENABLED`

1. Wgraj build ruchowy.
2. Wroc do `Podglad online`.
3. Potwierdz `MOTION_IMPLEMENTED = 1`.
4. Przejdz do Etapu 3.
5. Wyslij `HOME`.
6. Poczekaj na sukces homingu.
7. Wyslij maly `MOVE_REL`.

### Diagnoza problemu

1. `UART` - sprawdz polaczenie i COM.
2. `Logi` - sprawdz kolejnosc zdarzen.
3. `SQL` - sprawdz, czy zapis dziala.
4. `API` - uzyj komendy serwisowej tylko wtedy, gdy wiesz, co robisz.

## 13. Ograniczenia aktualnej wersji

Wazne rozroznienie:

- `SAFE` sluzy do bezpiecznej integracji i walidacji komunikacji,
- `MOTION-ENABLED` sluzy do pierwszego kontrolowanego ruchu po wykonaniu checklisty.

Nie wolno traktowac `SAFE` jako pelnego builda ruchowego.

## 14. Dokumenty powiazane

- `docs/Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md`
- `docs/STM32_Bringup_Checklist.md`
- `docs/Pakiety_Wdrozeniowe_Wariant_B.md`
- `docs/HMI_Protocol.md`
