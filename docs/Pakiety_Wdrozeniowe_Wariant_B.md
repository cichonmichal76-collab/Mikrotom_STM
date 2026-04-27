# Pakiety wdrozeniowe Wariant B

Ten dokument porzadkuje zestaw archiwow przygotowanych do wdrozenia na stanowisku z MCU, USB-UART i komputerem operatorskim.

## 1. Pakiet zalecany

Do standardowego wdrozenia zalecany jest jeden plik:

- `Mikrotom_Wariant_B_JEDEN_PLIK_20260427.zip`

Ten pakiet zawiera kompletny zestaw:

- GUI,
- Agenta,
- skrypty `.bat`,
- firmware `SAFE`,
- firmware `MOTION-ENABLED`,
- dokumentacje operatorska i uruchomieniowa.

To jest rekomendowany punkt startowy, jesli jedna osoba:

- przygotowuje komputer,
- wgrywa wsad do MCU,
- przechodzi przez commissioning etapowy.

## 2. Jak pracowac z pakietem zalecanym

Zalecana kolejnosc jest nastepujaca:

1. pobierz `Mikrotom_Wariant_B_JEDEN_PLIK_20260427.zip`,
2. rozpakuj archiwum na komputerze docelowym,
3. uruchom `00_START_TUTAJ.bat`,
4. uruchom `01_SPRAWDZ_WYMAGANIA.bat`,
5. uruchom `02_INSTALUJ_PC.bat`,
6. wgraj `SAFE` przez `03_WGRAJ_MCU_SAFE_STLINK.bat`,
7. ustaw GUI `LIVE` przez `04_USTAW_GUI_LIVE.bat`,
8. uruchom Agenta przez `05_URUCHOM_AGENT.bat`,
9. uruchom GUI przez `06_URUCHOM_GUI.bat`,
10. po zaliczeniu Etapu 1 i Etapu 2 wgraj `MOTION-ENABLED` przez `08_WGRAJ_MCU_MOTION_STLINK.bat`.

W praktyce ten jeden pakiet wystarcza do przejscia:

```text
SAFE -> walidacja komunikacji i safety -> MOTION -> Etap 2 -> CALIB_ZERO -> FIRST_MOVE_REL -> HOME -> MOVE_REL
```

## 3. Zestawienie zmian: stary soft vs nowy soft

| Stary soft | Nowy soft / wykonana zmiana |
| --- | --- |
| bezposrednie sterowanie napedem z naciskiem na `FOC`, `PID`, trajektorie i PWM | sterowanie napedem zostalo zachowane, ale obudowane warstwa `Protocol`, `AxisControl`, `Safety`, `Agent`, `GUI` |
| brak dedykowanej warstwy operatorskiej | dodano `GUI` webowe i lokalnego `Agenta` |
| brak REST API | dodano lokalne API HTTP po stronie Agenta |
| UART glownie jako debug | UART pracuje jako ustrukturyzowany kanal `GET / SET / CMD` |
| brak SQL / logowania po stronie PC | dodano lokalne logowanie `SQLite` po stronie Agenta |
| brak jawnego commissioning | dodano etapy `1 SAFE`, `2 ARMING`, `3 CONTROLLED MOTION` |
| brak centralnej bramki ruchu | dodano `RUN_ALLOWED` i warunki dopuszczenia ruchu |
| brak wydzielonego fault managera | dodano `fault`, `watchdogs`, `limits`, `safety_config`, `safety_monitor` |
| zachowania startowe mogly uruchamiac ruch automatycznie | startup zostal uporzadkowany, a ruch automatyczny usuniety z sekwencji bezpiecznej |
| brak jawnego rozroznienia trybu walidacyjnego i ruchowego | dodano dwa buildy: `SAFE` i `MOTION-ENABLED` |
| brak jawnego `HOME` wywolywanego z nowego toru operatorskiego | dodano `CMD HOME`, endpoint `/api/cmd/home` i obsluge w GUI |
| brak malego testu ruchu przed pelnym homingiem | dodano `CMD FIRST_MOVE_REL`, endpoint `/api/cmd/first-move-rel` i krok kreatora ograniczony do maksymalnie 100 um |
| komendy ruchu byly lokalne, zalezne od kodu firmware | `FIRST_MOVE_REL`, `MOVE_REL` i `MOVE_ABS` sa dostepne przez `Protocol`, `Agent` i `GUI` |
| brak wyraznej obserwowalnosci procesu uruchomienia | dodano telemetrie, event log i zakladki `UART`, `Logi`, `SQL`, `API` |
| trudniejsze wdrozenie na nowym komputerze | dodano gotowe paczki ZIP i skrypty `.bat` do instalacji i flashowania |

Najkrotsze podsumowanie:

```text
stary soft = dzialajacy firmware sterujacy napedem
nowy soft = sterowanie napedem + safety + commissioning + Agent + GUI + API + SQL
```

## 4. Paczki rozdzielone

Rozdzielone archiwa zostaly zachowane w podfolderze:

- `dist/deployment/warianty_techniczne_20260427/`

Sa przydatne wtedy, gdy chcesz rozdysponowac elementy osobno, na przyklad:

- oddzielnie komputer operatorski,
- oddzielnie pierwszy wsad `SAFE`,
- oddzielnie drugi wsad `MOTION-ENABLED`,
- oddzielnie sama dokumentacja.

## 5. Zawartosc paczek rozdzielonych

| Paczka | Zawartosc | Typowe zastosowanie |
| --- | --- | --- |
| `Mikrotom_Wariant_B_Pakiet_PC_20260427.zip` | Agent, GUI, API, SQL, skrypty uruchomieniowe, dokumentacja | przygotowanie komputera podpietego do USB-UART |
| `Mikrotom_Wariant_B_Pakiet_MCU_SAFE_20260427.zip` | firmware `SAFE` i skrypty flashowania | pierwszy flash i walidacja komunikacji oraz safety |
| `Mikrotom_Wariant_B_Pakiet_MCU_MOTION_20260427.zip` | firmware `MOTION-ENABLED` i skrypty flashowania | drugi flash po pozytywnym przejsciu `SAFE` |
| `Mikrotom_Wariant_B_Pakiet_Dokumentacja_20260427.zip` | instrukcje, przewodnik HTML, karta operatora A4, zrzuty ekranow | przeglad dokumentacji bez kopiowania calego pakietu |
| `Mikrotom_Wariant_B_Pakiet_Pelny_20260427.zip` | komplet techniczny w jednej paczce | alternatywa dla pakietu `JEDEN_PLIK` |

## 6. Kiedy uzyc paczek rozdzielonych

Paczki rozdzielone maja sens, gdy:

- komputer przygotowuje jedna osoba, a wsad wgrywa inna,
- chcesz osobno przekazac tylko `SAFE`,
- chcesz zostawic `MOTION-ENABLED` na pozniejszy etap,
- potrzebujesz przekazac sama dokumentacje do akceptacji lub przegladu.

Jesli nie ma takiej potrzeby organizacyjnej, zostan przy:

- `Mikrotom_Wariant_B_JEDEN_PLIK_20260427.zip`

## 7. Ktory plik wybrac w praktyce

### Przypadek A - jedna osoba robi cale uruchomienie

Pobierz:

- `Mikrotom_Wariant_B_JEDEN_PLIK_20260427.zip`

### Przypadek B - najpierw tylko walidacja komunikacji i safety

Pobierz:

- `Mikrotom_Wariant_B_Pakiet_PC_20260427.zip`
- `Mikrotom_Wariant_B_Pakiet_MCU_SAFE_20260427.zip`

### Przypadek C - stanowisko jest juz przygotowane i przechodzisz tylko na build ruchowy

Pobierz:

- `Mikrotom_Wariant_B_Pakiet_MCU_MOTION_20260427.zip`

## 8. Uwagi praktyczne

- `SAFE` jest pierwszym krokiem i nie powinien wykonywac realnego ruchu.
- `MOTION-ENABLED` wgrywaj dopiero po potwierdzeniu poprawnej pracy `SAFE`.
- Pierwsza komenda ruchowa w buildzie ruchowym to `FIRST_MOVE_REL` w Etapie 2, po `CALIB_ZERO` i `ENABLE`.
- `HOME` uruchamiaj dopiero po potwierdzeniu, ze pierwszy maly test ruchu jest stabilny i zgodny z oczekiwanym kierunkiem.

## 9. Dokumenty powiazane

- `docs/Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md`
- `docs/Wariant_B_Manual_Uzytkownika_GUI.md`
- `docs/STM32_Bringup_Checklist.md`
- `docs/Karta_Operatora_A4_Wariant_B.html`
- `docs/Porownanie_Stary_vs_Nowy_Soft.md`
