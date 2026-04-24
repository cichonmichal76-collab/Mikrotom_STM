# Wariant B - instalacja, uruchomienie i commissioning

Instrukcja opisuje kompletny wariant wdrożenia na drugim komputerze:

- PC podłączony do urządzenia,
- MCU STM32 podłączone przez ST-LINK oraz UART,
- pierwsze uruchomienie od zera,
- uruchomienie Agenta, GUI i komunikacji z firmware,
- commissioning etapowy.

Dokument jest przygotowany dla aktualnego stanu repozytorium na dzień `2026-04-24`.

## 1. Po co powstało to oprogramowanie

Pierwotny software sterownika realizował głównie warstwę napędową:

- pomiary,
- FOC,
- PID,
- trajektorię,
- PWM,
- komunikację debug.

To podejście było wystarczające, żeby urządzenie działało sprzętowo, ale nie tworzyło jeszcze pełnego systemu operatorskiego i uruchomieniowego.

Nowe oprogramowanie powstało po to, aby z istniejącego sterownika zrobić:

- narzędzie bezpieczniejszego uruchamiania,
- warstwę diagnostyczną dla operatora i integratora,
- bazę pod GUI, API, HMI i logowanie danych,
- podstawę pod późniejsze pełne sterowanie ruchem z większą kontrolą ryzyka.

## 2. Jak to się ma do starego softu

### Co zostawiamy ze starego softu

Zostawiamy sprawdzone elementy niskiego poziomu, które są związane bezpośrednio ze sprzętem:

- tor pomiarowy,
- PWM,
- ADC / OPAMP / TIM,
- bazową strukturę projektu STM32,
- niskopoziomową obsługę silnika i hardware,
- sprawdzony timing oraz ISR tam, gdzie nie wolno ryzykować przypadkowego rozjazdu z urządzeniem.

W praktyce oznacza to:

- nie przepisujemy od zera działającego toru mocy,
- nie wymieniamy pochopnie sterowania sprzętowego,
- nie ruszamy na ślepo tego, co już działa na konkretnym urządzeniu.

### Co poprawiamy względem starego softu

Nowe podejście porządkuje i zabezpiecza sterownik:

- wyłącza niepożądane automatyczne zachowania po starcie,
- porządkuje logikę uruchamiania,
- oddziela konfigurację operatora od logiki bezpieczeństwa,
- dodaje spójny stan systemu i fault handling,
- przenosi komunikację operatorską do warstwy `GUI -> Agent -> UART -> STM32`,
- przygotowuje system do logowania i analizy.

Najważniejsza zmiana koncepcyjna jest taka:

```text
stary soft = głównie sterowanie napędem
nowy soft = sterowanie napędem + safety + commissioning + diagnostyka + API + GUI
```

### Co wdrażamy teraz

Aktualnie wdrażany pakiet to **safe integration baseline**. Obejmuje:

- Agent lokalny,
- GUI operatorskie,
- API HTTP,
- UART do komunikacji z MCU,
- event log,
- SQL / SQLite po stronie Agenta,
- commissioning etapowy,
- warstwę safety i raportowania statusu,
- bezpieczny start bez samoczynnego ruchu.

### Co dojdzie później

Ten pakiet jest pomostem do kolejnych funkcjonalności:

- firmware z realnym ruchem sterowanym z nowej architektury,
- pełniejsze wdrożenie `RUN_ALLOWED`,
- bardziej rozbudowana kalibracja,
- trwalszy zapis konfiguracji,
- HMI jako kolejny klient Agenta,
- szersza telemetria i analiza jakości ruchu.

## 3. Co oznacza aktualny wsad MCU

Aktualny firmware w `dist\mcu\` jest zbudowany jako:

```c
APP_SAFE_INTEGRATION=1
APP_MOTION_IMPLEMENTED=0
```

W praktyce oznacza to:

- GUI, Agent, API, UART i telemetria mają działać,
- firmware ma startować bez automatycznego ruchu,
- ruch rzeczywisty nie jest jeszcze dopuszczony,
- `RUN_ALLOWED` pozostaje zablokowany,
- commissioning w GUI działa jako bezpieczny workflow uruchomieniowy,
- etap 3 w tej wersji służy do walidacji przepływu i logiki, a nie do produkcyjnego testu ruchu.

Jeżeli po uruchomieniu:

- oś nie rusza sama,
- GUI pokazuje status i telemetrię,
- `RUN_ALLOWED = 0`,

to dla tego wydania jest to stan **prawidłowy**.

## 4. Architektura systemu

Docelowy przepływ wygląda tak:

```text
GUI / HMI
    ->
Agent PC / API / SQL
    ->
UART
    ->
STM32 firmware
    ->
warstwa sterowania osi
    ->
hardware urządzenia
```

To bardzo ważne:

- GUI nie steruje hardware bezpośrednio,
- HMI nie powinno omijać Agenta,
- ostateczna decyzja o ruchu należy do firmware STM32.

## 5. Co trzeba przygotować

### Sprzęt

- komputer z Windows,
- sterownik z MCU STM32,
- przewód ST-LINK do programowania przez SWD,
- konwerter USB-UART podłączony do właściwego portu UART sterownika,
- zasilanie urządzenia,
- fizyczny STOP lub możliwość szybkiego odcięcia zasilania,
- mechanika ustawiona tak, aby pierwszy test nie kończył się uderzeniem w ogranicznik.

### Oprogramowanie na PC

- Python `3.12+`,
- `STM32CubeProgrammer`,
- opcjonalnie `STM32CubeIDE`, jeśli na tym komputerze chcesz także kompilować firmware.

### Co przenieść na komputer docelowy

Najbezpieczniej przenieść gotową paczkę wdrożeniową przygotowaną w `dist\deployment\`.

Jeżeli pracujesz bez paczki, minimalny zestaw to:

- `agent\`,
- `gui\`,
- `scripts\`,
- `docs\`,
- `dist\mcu\`.

## 6. Commissioning etapowy - po co on jest

Przebieg etapów nie jest ozdobą GUI. To narzędzie obniżające ryzyko uruchomienia.

Jego sens jest prosty:

- nie przechodzimy od razu do ruchu,
- najpierw sprawdzamy komunikację i faulty,
- potem logikę enable/disable i reakcje ochronne,
- dopiero na końcu przygotowujemy warunki pod kontrolowany ruch.

Etapy mają wymusić kolejność:

```text
komunikacja -> diagnostyka -> uzbrojenie -> dopiero potem ruch
```

## 7. Commissioning etapowy - jak używać

### Etap 1 - bezpieczny / komunikacja i diagnostyka

To jest etap pierwszego kontaktu z urządzeniem po wgraniu firmware.

#### Co fizycznie ma być podłączone

- MCU zasilone logicznie,
- USB-UART podłączony,
- ST-LINK może być podłączony podczas flashowania, później nie jest konieczny,
- tor mocy najlepiej wyłączony albo ograniczony prądowo,
- mechanika ustawiona bez ryzyka przypadkowego uszkodzenia.

#### Co ustawić w sofcie

- GUI w trybie `LIVE`,
- etap uruchomienia = `1`,
- wszystkie override safety wyłączone,
- nie wysyłać komend ruchu,
- nie traktować `ENABLE` jako zgody na ruch.

#### Co zrobić

1. Wgraj firmware do MCU.
2. Uruchom Agenta.
3. Uruchom GUI.
4. Otwórz `Podgląd online`.
5. Sprawdź `Połączenie`, `FAULT`, `Etap`, `VBUS`, `RUN_ALLOWED`.
6. Otwórz `UART`, `Logi`, `SQL`.
7. Potwierdź, że oś nie rusza.

#### Jaki wynik jest poprawny

- komunikacja działa,
- brak samoczynnego ruchu,
- `RUN_ALLOWED = 0`,
- fault jest pusty albo jasno opisany,
- statusy są spójne.

### Etap 2 - uzbrajanie i test logiki

To etap sprawdzania, czy system logicznie reaguje na komendy operatorskie.

#### Co fizycznie ma być podłączone

- USB-UART podłączony,
- zasilanie logiki aktywne,
- tor mocy nadal ostrożnie: wyłączony, ograniczony albo przygotowany laboratoryjnie,
- mechanika sprawdzona,
- operator gotowy do użycia `STOP` / `QSTOP`.

#### Co ustawić w sofcie

- przełącz etap na `2`,
- limity ustaw zachowawczo,
- override safety pozostaw wyłączone, jeśli nie ma wyraźnej potrzeby,
- używaj tylko testów logicznych: `ENABLE`, `DISABLE`, `STOP`, `QSTOP`.

#### Co zrobić

1. Przejdź do etapu `2`.
2. Sprawdź reakcję na `ENABLE`.
3. Sprawdź reakcję na `DISABLE`.
4. Sprawdź `STOP`.
5. Sprawdź `QSTOP`.
6. Sprawdź, czy fault i logi zapisują zdarzenia.

#### Jaki wynik jest poprawny

- komendy są przyjmowane albo odrzucane logicznie,
- system nie przechodzi w niekontrolowany stan,
- `STOP` i `QSTOP` działają,
- nadal nie ma ruchu rzeczywistego w obecnym buildzie.

### Etap 3 - przygotowanie pod ruch kontrolowany

To etap końcowy workflow commissioning.

#### Bardzo ważne zastrzeżenie

W **aktualnym pakiecie safe integration** etap 3:

- służy do walidacji GUI i logiki,
- nie jest jeszcze równoznaczny z gotowością do realnego ruchu,
- nie powinien być traktowany jako zgoda na test produkcyjny osi.

#### Co fizycznie ma być podłączone

Dla obecnego builda:

- dokładnie tak samo ostrożnie jak dla etapu 2,
- nadal traktuj układ jako wersję uruchomieniowo-diagnostyczną.

Dla przyszłego builda z ruchem:

- tor mocy aktywny i kontrolowany,
- mechanika wolna od kolizji,
- ograniczenia ruchu ustawione konserwatywnie,
- kalibracja potwierdzona,
- operator stale nadzoruje urządzenie.

#### Co ustawić w sofcie

- etap `3`,
- zachowawcze limity prądu, prędkości i przyspieszenia,
- brak niepotrzebnych override,
- potwierdzona kalibracja, gdy tylko pojawi się build z ruchem.

#### Co zrobić teraz

1. Wejdź do etapu `3` tylko po przejściu etapów 1 i 2.
2. Sprawdź, czy GUI i statusy commissioning są spójne.
3. Potwierdź, że nadal nie dochodzi do niekontrolowanego ruchu.
4. Traktuj ten etap jako przygotowanie pod przyszły firmware motion-enabled.

## 8. Sprawdzenie połączeń przed uruchomieniem

Zanim cokolwiek wgrasz lub uruchomisz:

1. Upewnij się, że urządzenie nie stoi na końcu zakresu mechanicznego.
2. Przygotuj zewnętrzny STOP lub odcięcie zasilania.
3. Podłącz ST-LINK do SWD.
4. Podłącz USB-UART do właściwego portu komunikacyjnego.
5. Zanotuj numer portu COM w Menedżerze urządzeń.
6. Jeżeli to pierwszy test - ogranicz energię układu.

## 9. Wgranie firmware do MCU

### 9.1. Otwórz katalog projektu lub paczki

Na komputerze docelowym otwórz `cmd` lub PowerShell i przejdź do katalogu paczki:

```bat
cd C:\sciezka\do\pakietu
```

### 9.2. Sprawdź, czy jest STM32CubeProgrammer

Skrypt flashujący oczekuje standardowej instalacji `STM32CubeProgrammer`.

### 9.3. Wgraj wsad

Uruchom:

```bat
scripts\flash_mcu_stlink.bat
```

Domyślnie zostanie użyty:

```text
dist\mcu\Mikrotom_STM_latest.hex
```

Jeżeli chcesz wskazać plik ręcznie:

```bat
scripts\flash_mcu_stlink.bat dist\mcu\Mikrotom_STM_safe_integration_20260424.hex
```

### 9.4. Co ma się stać po flashowaniu

- brak automatycznego ruchu,
- dostępny UART,
- możliwy odczyt statusu,
- `SAFE_INTEGRATION = 1`,
- `MOTION_IMPLEMENTED = 0`,
- `RUN_ALLOWED = 0`.

## 10. Instalacja części PC

### 10.1. Utworzenie środowiska Python

Uruchom:

```bat
scripts\install_pc.bat
```

Skrypt wykona:

- wykrycie Pythona,
- utworzenie `.venv`,
- instalację zależności Agenta.

### 10.2. Przełączenie GUI na tryb LIVE

Uruchom:

```bat
scripts\configure_gui_live.bat
```

To przełącza GUI z trybu demonstracyjnego na pracę z Agentem pod adresem:

```text
http://127.0.0.1:8000
```

## 11. Uruchomienie Agenta

### 11.1. Ustal numer portu COM

Sprawdź numer portu w Menedżerze urządzeń, np.:

```text
COM3
```

### 11.2. Uruchom Agenta

W pierwszym oknie terminala:

```bat
scripts\run_agent.bat COM3
```

albo skryptem z pytaniem o port:

```bat
scripts\run_agent_prompt.bat
```

### 11.3. Co oznacza poprawny start Agenta

- port COM zostaje otwarty,
- Agent nasłuchuje UART,
- API działa na `127.0.0.1:8000`,
- GUI może pobierać statusy, telemetrię i logi.

## 12. Uruchomienie GUI

W drugim oknie terminala uruchom:

```bat
scripts\run_gui.bat
```

Następnie otwórz:

```text
http://127.0.0.1:8080/gui/
```

## 13. Pierwsza kontrola po uruchomieniu

Po wejściu do GUI przejdź do `Podgląd online` i sprawdź:

1. status połączenia,
2. stan osi,
3. aktywny fault,
4. etap uruchomienia,
5. `RUN_ALLOWED`,
6. `VBUS`,
7. brak samoczynnego ruchu.

Następnie sprawdź:

- `UART`,
- `Logi`,
- `SQL`,
- `API`.

## 14. Minimalny scenariusz dla laika

Najprostsza kolejność działań:

1. Skopiuj paczkę na komputer docelowy.
2. Uruchom skrypt sprawdzający wymagania.
3. Wgraj firmware do MCU.
4. Uruchom instalację PC.
5. Przełącz GUI na `LIVE`.
6. Uruchom Agenta z właściwym `COMx`.
7. Uruchom GUI.
8. Otwórz dokumentację i wykonaj commissioning etap 1.

## 15. Co wolno oczekiwać od aktualnej wersji

Aktualna wersja jest przeznaczona do:

- sprawdzenia komunikacji PC <-> Agent <-> UART <-> STM32,
- sprawdzenia statusów safety,
- odczytu telemetrii i zdarzeń,
- walidacji commissioning workflow,
- sprawdzenia logowania do SQLite,
- przygotowania pod późniejszy firmware z ruchem.

Aktualna wersja nie jest jeszcze przeznaczona do:

- produkcyjnego ruchu osi,
- testów dynamicznych z pełnym obciążeniem,
- ostatecznego odbioru funkcji ruchowych.

## 16. Najczęstsze problemy

### `Python was not found`

Przyczyna:

- Python nie jest zainstalowany albo nie ma go w `PATH`.

Rozwiązanie:

- zainstaluj Python `3.12+`,
- uruchom ponownie `scripts\install_pc.bat`.

### Agent nie łączy się z portem COM

Przyczyna:

- zły numer portu,
- port jest zajęty przez inny program,
- USB-UART jest podłączony nie tam, gdzie trzeba.

Rozwiązanie:

- sprawdź Menedżer urządzeń,
- zamknij inne programy używające COM,
- uruchom `scripts\run_agent.bat COMx` z poprawnym numerem.

### GUI otwiera się, ale pokazuje DEMO

Przyczyna:

- GUI nie zostało przełączone na LIVE.

Rozwiązanie:

- uruchom:

```bat
scripts\configure_gui_live.bat
```

### GUI działa, ale brak danych LIVE

Przyczyna:

- Agent nie działa,
- UART nie działa,
- MCU nie odpowiada po protokole.

Rozwiązanie:

- sprawdź terminal Agenta,
- sprawdź zakładkę `UART`,
- sprawdź `Logi`,
- sprawdź `API`.

### Flashowanie nie działa

Przyczyna:

- brak `STM32CubeProgrammer`,
- brak połączenia ST-LINK,
- MCU nie jest widziane przez SWD.

Rozwiązanie:

- sprawdź przewód ST-LINK,
- sprawdź zasilanie logiki,
- uruchom `STM32CubeProgrammer` ręcznie,
- ponów `scripts\flash_mcu_stlink.bat`.

## 17. Zalecenia bezpieczeństwa

- Nie zakładaj, że działające GUI oznacza gotowość do ruchu.
- Nie używaj etapu 3 jako zgody na ruch w obecnym buildzie.
- Nie interpretuj `Połączenie OK` jako potwierdzenia bezpieczeństwa.
- Na początku zawsze pracuj zachowawczo.
- Fault, VBUS i commissioning traktuj jako sygnały decyzyjne, nie dekoracyjne.

## 18. Dokumenty powiązane

- `docs/Wariant_B_Manual_Uzytkownika_GUI.md`
- `docs/STM32_Bringup_Checklist.md`
- `docs/HMI_Protocol.md`
