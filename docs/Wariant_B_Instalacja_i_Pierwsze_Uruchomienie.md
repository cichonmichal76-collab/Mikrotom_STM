# Wariant B - instalacja, pierwsze uruchomienie i commissioning

Ten dokument opisuje wariant wdrozenia na drugim komputerze:

- PC podpiety do urzadzenia,
- MCU STM32 podpiete przez ST-LINK i USB-UART,
- pierwsze uruchomienie od zera,
- uruchomienie Agenta, GUI i komunikacji z firmware,
- commissioning etapowy,
- przejscie od wariantu `SAFE` do wariantu `MOTION-ENABLED`.

Stan dokumentu: `2026-04-24`.

## 1. Po co powstalo to oprogramowanie

Pierwotny software sterownika realizowal glownie warstwe napedowa:

- pomiary,
- FOC,
- PID,
- trajektorie,
- PWM,
- debug UART.

To wystarczalo, aby urzadzenie pracowalo na poziomie sprzetowym, ale nie dawalo jeszcze pelnego systemu operatorskiego. Brakowalo jednej, spojnej warstwy do:

- bezpiecznego uruchamiania,
- diagnostyki,
- logowania,
- konfiguracji,
- API,
- GUI,
- przygotowania pod HMI.

Nowy system powstal po to, aby nad dzialajacym sterowaniem MCU dodac warstwe:

- safety,
- commissioning,
- telemetrii,
- event logu,
- Agenta PC,
- GUI i API,
- pozniejszej integracji z HMI.

## 2. Jak nowy system ma sie do starego softu

### Co zostawiamy

Zostawiamy to, co jest bezposrednio zwiazane z hardware i bylo juz sprawdzone na urzadzeniu:

- tor pomiarowy,
- PWM,
- ADC / OPAMP / TIM,
- przerwania i timing niskiego poziomu,
- strukture projektu STM32,
- dzialajacy fundament sterowania napedem.

Najwazniejsza zasada:

```text
nie przepisujemy od zera sprawdzonego toru sprzetowego
```

### Co poprawiamy

Nowa architektura porzadkuje i zabezpiecza system:

- usuwa niekontrolowane automaty po starcie,
- rozdziela konfiguracje od safety,
- porzadkuje fault handling,
- dodaje commissioning etapowy,
- przenosi sterowanie operatorskie na sciezke `GUI -> Agent -> UART -> STM32`,
- przygotowuje system pod logowanie i analize danych.

### Co wdrazamy teraz

Aktualnie wdrazamy dwa warianty firmware:

| Wariant | Co daje |
| --- | --- |
| `SAFE` | pelny tor komunikacji, feedback, telemetrie, logi, commissioning, komendy UART bez realnego ruchu |
| `MOTION-ENABLED` | to samo co `SAFE`, ale dodatkowo realny `HOME`, `MOVE_REL`, `MOVE_ABS` i mozliwosc przejscia do kontrolowanego ruchu |

### Co dojdzie dalej

Ten pakiet jest baza pod kolejne kroki:

- dalsze rozwijanie sterowania ruchem,
- bardziej rozbudowana kalibracje,
- trwalszy zapis konfiguracji,
- HMI jako kolejny klient Agenta,
- szersza telemetrie i analize testow.

## 3. Dwie paczki firmware MCU

## `SAFE`

Flagi:

```c
APP_SAFE_INTEGRATION=1
APP_MOTION_IMPLEMENTED=0
```

Co to oznacza w praktyce:

- `GUI -> Agent -> UART -> MCU` dziala,
- statusy i telemetria dzialaja,
- komendy UART dochodza do MCU,
- `MOVE_REL` i `MOVE_ABS` sa celowo odrzucane,
- os nie powinna wykonac realnego ruchu.

To jest wariant zalecany do pierwszego flashowania i pierwszej walidacji.

## `MOTION-ENABLED`

Flagi:

```c
APP_SAFE_INTEGRATION=0
APP_MOTION_IMPLEMENTED=1
```

Co to oznacza w praktyce:

- tor komunikacji i telemetrii zostaje bez zmian,
- ruch jest odblokowany od strony implementacji,
- homing nie startuje automatycznie po boot,
- operator musi jawnie wyslac `CMD HOME`,
- dopiero po HOME i spelnieniu bramek safety mozliwy jest kontrolowany ruch.

To jest wariant do kolejnego kroku, po potwierdzeniu poprawnej pracy `SAFE`.

## 4. Co jest nowe funkcjonalnie

Nowy system wnosi:

- GUI operatorskie po polsku,
- lokalnego Agenta HTTP/UART,
- logowanie SQL po stronie PC,
- commissioning etapowy,
- centralna logike safety,
- event log firmware,
- jawny `HOME` dla builda ruchowego,
- rozdzielenie walidacji komunikacji od pierwszego ruchu.

## 5. Architektura calego systemu

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
hardware
```

Wazne:

- GUI nie steruje hardware bezposrednio,
- HMI nie powinno omijac Agenta,
- ostateczna zgoda na ruch nalezy do firmware.

## 6. Co musi byc przygotowane

### Sprzet

- komputer z Windows,
- sterownik z MCU STM32,
- ST-LINK do SWD,
- konwerter USB-UART podpiety do wlasciwego UART sterownika,
- zasilanie urzadzenia,
- zewnetrzny STOP albo mozliwosc szybkiego odciecia zasilania,
- mechanika ustawiona tak, aby pierwszy test nie skonczyl sie kolizja.

### Oprogramowanie

- Python `3.12+`,
- `STM32CubeProgrammer`,
- opcjonalnie `STM32CubeIDE`, jesli na tym komputerze chcesz tez kompilowac firmware.

## 7. Co pobrac na drugi komputer

Najprostsza sciezka:

- pobierz paczke `PC`,
- pobierz paczke `MCU SAFE`,
- pobierz paczke `MCU MOTION`,
- pobierz paczke `Dokumentacja`.

Jesli wolisz wszystko w jednym archiwum:

- pobierz paczke `Pelny`.

Opis paczek jest w `docs/Pakiety_Wdrozeniowe_Wariant_B.md`.

## 8. Commissioning etapowy - po co on jest

Etapy 1, 2 i 3 nie sa ozdoba GUI. To wymuszona kolejnosc uruchomienia:

```text
komunikacja -> logika safety -> HOME -> maly ruch testowy
```

Ich sens jest prosty:

- nie przechodzimy od razu do ruchu,
- najpierw sprawdzamy feedback i faulty,
- potem testujemy logike operatorska,
- dopiero na koncu przygotowujemy os do pierwszego ruchu.

## 9. Etap 1 - komunikacja i diagnostyka

### Co ma byc fizycznie podlaczone

| Element | Wymaganie |
| --- | --- |
| ST-LINK | tylko do flashowania, moze zostac po flashu albo byc odpiety |
| USB-UART | podlaczony obowiazkowo |
| Zasilanie logiki MCU | wlaczone |
| Tor mocy | najlepiej odlaczony albo mocno ograniczony pradowo |
| Mechanika | w bezpiecznej pozycji, bez kolizji |

### Co ustawic w sofcie

- GUI w trybie `LIVE`,
- aktywny etap `1`,
- brak override safety,
- nie wysylac komend ruchu.

### Co zrobic

1. Wgraj wariant `SAFE`.
2. Uruchom Agenta.
3. Uruchom GUI.
4. Otworz `Podglad online`.
5. Sprawdz `Polaczenie`, `FAULT`, `Etap`, `VBUS`, `SAFE_INTEGRATION`, `MOTION_IMPLEMENTED`.
6. Otworz `UART`, `Logi`, `SQL`, `API`.
7. Potwierdz, ze os pozostaje nieruchoma.

### Jaki wynik jest poprawny

- komunikacja dziala,
- telemetria przychodzi,
- komendy UART sa przyjmowane,
- brak samoczynnego ruchu,
- `SAFE_INTEGRATION = 1`,
- `MOTION_IMPLEMENTED = 0`.

## 10. Etap 2 - logika operatorska i safety

### Co ma byc fizycznie podlaczone

| Element | Wymaganie |
| --- | --- |
| USB-UART | podlaczony |
| Zasilanie logiki | wlaczone |
| Tor mocy | nadal ostroznie, najlepiej ograniczony pradowo |
| Mechanika | sprawdzona i gotowa na ewentualna reakcje awaryjna |
| STOP / odciecie zasilania | w zasiegu operatora |

### Co ustawic w sofcie

- aktywny etap `2`,
- limity konserwatywne,
- brak niepotrzebnych override.

### Co zrobic

1. Wyslij `ENABLE`.
2. Wyslij `DISABLE`.
3. Wyslij `STOP`.
4. Wyslij `QSTOP`.
5. Sprawdz logi i odpowiedzi firmware.
6. Sprawdz timeout komunikacji, jesli Agent przestanie wysylac heartbeat.

### Jaki wynik jest poprawny

- firmware reaguje logicznie,
- faulty i zdarzenia sa widoczne,
- `STOP` i `QSTOP` dzialaja,
- w wariancie `SAFE` nadal nie ma realnego ruchu.

## 11. Etap 3 - przygotowanie do ruchu kontrolowanego

To jest etap, w ktorym zaczynamy pracowac z wariantem `MOTION-ENABLED`.

### Co ma byc fizycznie podlaczone

| Element | Wymaganie |
| --- | --- |
| USB-UART | podlaczony |
| Zasilanie logiki | wlaczone |
| Tor mocy | wlaczony, ale z zachowawczymi limitami i kontrola operatora |
| Mechanika | wolna od kolizji i ustawiona z zapasem od ogranicznikow |
| STOP / odciecie zasilania | gotowe do natychmiastowego uzycia |

### Co ustawic w sofcie

- wgraj wariant `MOTION-ENABLED`,
- aktywny etap `3`,
- konserwatywne `MAX_CURRENT`, `MAX_VELOCITY`, `MAX_ACCELERATION`,
- potwierdzone soft limity,
- brak niepotrzebnych override.

### Co zrobic po kolei

1. Sprawdz, czy `SAFE_INTEGRATION = 0`.
2. Sprawdz, czy `MOTION_IMPLEMENTED = 1`.
3. Potwierdz brak aktywnego faultu.
4. Wyslij `HOME`.
5. Obserwuj statusy `HOMING_STARTED`, `HOMING_ONGOING`, `HOMING_SUCCESSFUL`.
6. Gdy homing sie powiedzie, sprawdz czy `RUN_ALLOWED` moze przejsc na `1`.
7. Wyslij bardzo maly `MOVE_REL`, na przyklad `100 um`.
8. Potwierdz kierunek ruchu i zatrzymanie.
9. Dopiero potem wolno przejsc do wiekszego testu.

### Jaki wynik jest poprawny

- homing startuje tylko po `CMD HOME`,
- os nie rusza sama po boot,
- ruch jest mozliwy dopiero po HOME i spelnieniu bramek safety,
- pierwszy maly ruch jest zgodny z oczekiwanym kierunkiem.

## 12. Krok po kroku - pierwszy raz na nowym komputerze

### Krok 1. Rozpakuj paczki

Na komputerze docelowym rozpakuj:

- `Pakiet PC`,
- `Pakiet MCU SAFE`,
- `Pakiet Dokumentacja`.

`Pakiet MCU MOTION` zachowaj na pozniej.

### Krok 2. Sprawdz wymagania

Uruchom:

```bat
01_SPRAWDZ_WYMAGANIA_PC.bat
```

albo w pakiecie pelnym:

```bat
01_SPRAWDZ_WYMAGANIA.bat
```

### Krok 3. Wgraj `SAFE`

Uruchom:

```bat
02_WGRAJ_MCU_SAFE_STLINK.bat
```

albo recznie:

```bat
scripts\flash_mcu_safe_stlink.bat
```

### Krok 4. Zainstaluj czesc PC

Uruchom:

```bat
02_INSTALUJ_PC.bat
```

Skrypt:

- wykryje Pythona,
- utworzy `.venv`,
- zainstaluje zaleznosci Agenta.

### Krok 5. Przelacz GUI na `LIVE`

Uruchom:

```bat
03_USTAW_GUI_LIVE.bat
```

albo:

```bat
scripts\configure_gui_live.bat
```

### Krok 6. Sprawdz port COM

W Menedzerze urzadzen Windows znajdz numer portu USB-UART, na przyklad `COM3`.

### Krok 7. Uruchom Agenta

```bat
04_URUCHOM_AGENT.bat
```

albo:

```bat
scripts\run_agent.bat COM3
```

### Krok 8. Uruchom GUI

```bat
05_URUCHOM_GUI.bat
```

albo:

```bat
scripts\run_gui.bat
```

Po uruchomieniu otworz:

```text
http://127.0.0.1:8080/gui/
```

### Krok 9. Wykonaj Etap 1 i Etap 2

Na tym etapie nie wgrywaj jeszcze `MOTION-ENABLED`. Najpierw potwierdz:

- komunikacje,
- telemetrie,
- logi,
- API,
- reakcje na `ENABLE`, `DISABLE`, `STOP`, `QSTOP`,
- brak niekontrolowanego ruchu.

### Krok 10. Dopiero potem wgraj `MOTION-ENABLED`

Uruchom:

```bat
02_WGRAJ_MCU_MOTION_STLINK.bat
```

albo:

```bat
scripts\flash_mcu_motion_stlink.bat
```

Po flashu przejdz do Etapu 3 i wykonaj `HOME`.

## 13. Co sprawdzic przed pierwszym ruchem

- poprawny kierunek pomiaru pozycji,
- sensowne `VBUS`,
- brak aktywnego faultu,
- dzialanie `STOP` i `QSTOP`,
- poprawne soft limity,
- centralna pozycje mechaniki z dala od skrajow,
- gotowe odciecie zasilania,
- bardzo male pierwsze przemieszczenie.

Pelna checklista jest w `docs/STM32_Bringup_Checklist.md`.

## 14. Najczestsze problemy

### Python nie jest znaleziony

Przyczyna:

- Python nie jest zainstalowany albo nie ma go w `PATH`.

Rozwiazanie:

- zainstaluj Python `3.12+`,
- uruchom ponownie `02_INSTALUJ_PC.bat`.

### Agent nie laczy sie z UART

Przyczyna:

- zly `COM`,
- port zajety przez inny program,
- bledne podpienie USB-UART.

Rozwiazanie:

- sprawdz Menedzer urzadzen,
- zamknij inne programy korzystajace z portu,
- uruchom Agenta z poprawnym `COMx`.

### GUI pokazuje DEMO

Przyczyna:

- GUI nie zostalo przelaczone na `LIVE`.

Rozwiazanie:

```bat
scripts\configure_gui_live.bat
```

### SAFE dziala, ale ruchu nadal nie ma

To jest poprawne zachowanie wariantu `SAFE`.

### MOTION jest wgrany, ale nadal nie ma ruchu

Najczestsze przyczyny:

- nie wykonano `HOME`,
- aktywny fault,
- aktywny etap inny niz `3`,
- `RUN_ALLOWED` nadal jest zablokowany przez safety.

## 15. Dokumenty powiazane

- `docs/Wariant_B_Manual_Uzytkownika_GUI.md`
- `docs/STM32_Bringup_Checklist.md`
- `docs/Pakiety_Wdrozeniowe_Wariant_B.md`
- `docs/HMI_Protocol.md`
