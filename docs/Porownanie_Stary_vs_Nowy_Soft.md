# Porownanie starego i nowego oprogramowania

Ten dokument zestawia w jednej tabeli:

- pierwotny, dzialajacy soft MCU,
- nowy system rozbudowany o `GUI`, `Agent`, `API`, `SQL`, `commissioning` i `safety`.

Przyjeta interpretacja:

- `stary soft` = pierwotny firmware sterownika pracujacy bez nowej warstwy operatorskiej,
- `nowy soft` = obecny system zintegrowany z torem `GUI -> Agent -> UART -> STM32`,
- `SAFE` i `MOTION-ENABLED` sa dwiema odmianami nowego firmware.

## 1. Porownanie ogolne

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Cel systemu | bezposrednie sterowanie napedem | sterowanie napedem + safety + diagnostyka + komunikacja operatorska | firmware przestaje byc tylko sterownikiem silnika |
| Glowny punkt ciezkosci | `FOC`, `PID`, trajektoria, PWM | `FOC/PID` pozostaja, ale sa nadzorowane przez `AxisControl`, `Protocol`, `Safety`, `GUI`, `Agent` | zachowany zostaje tor sprzetowy, ale sterowanie jest nadzorowane |
| Typ projektu | lokalny firmware napedowy | firmware + lokalny Agent + GUI + SQL + dokumentacja commissioning | system jest gotowy do pracy operatorskiej i integracji |
| Sposob rozwoju | kod uruchomieniowy / badawczy | architektura warstwowa pod dalsza rozbudowe | latwiejsza dalsza integracja i utrzymanie |

## 2. Architektura i przeplyw sterowania

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Architektura runtime | `ISR -> FOC -> PWM -> silnik` | `GUI/HMI -> Agent/API -> UART -> Protocol -> AxisControl -> FOC -> hardware` | decyzja o ruchu nie jest juz rozproszona po kodzie |
| Miejsce sterowania ruchem | glownie `main.c` i lokalne wywolania FOC | komendy operatora przechodza przez `Protocol` i `AxisControl` | ruch jest filtrowany i warunkowany |
| Warstwa operatorska | brak | `GUI` webowe po polsku | operator ma jeden punkt obserwacji i sterowania |
| Warstwa posrednia PC | brak | `Agent` lokalny | MCU nie musi wystawiac HTTP ani obslugiwac GUI bezposrednio |
| API | brak | REST API po stronie Agenta | mozliwa integracja narzedzi PC, HMI i diagnostyki |
| SQL / logowanie PC | brak | lokalne SQLite po stronie Agenta | komendy, telemetria i zdarzenia mozna archiwizowac poza MCU |

## 3. Uruchomienie i zachowanie po starcie

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Zachowanie po boot | mogly wystepowac automatyczne wywolania ruchu lub open-loop start | start w trybie kontrolowanym, bez samoczynnego ruchu | mniejsze ryzyko uszkodzenia przy pierwszym uruchomieniu |
| Homing po starcie | mogl byc uruchamiany lokalnie przez logike firmware | w buildzie ruchowym homing startuje tylko po jawnym `CMD HOME` | operator decyduje o rozpoczeciu homingu |
| Kolejnosc uruchomienia | niesformalizowana | etapowa: `SAFE -> ARMING -> CONTROLLED MOTION` | uruchomienie ma zdefiniowana sciezke i checkliste |
| Warianty builda | jedna postac firmware | `SAFE` oraz `MOTION-ENABLED` | najpierw mozna sprawdzic komunikacje i safety, potem ruch |

## 4. Sterowanie ruchem

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Zgoda na ruch | brak jednej centralnej bramki | centralna funkcja `RUN_ALLOWED` | latwiej przewidziec, kiedy ruch jest mozliwy |
| Komendy ruchu | lokalne wywolania w kodzie | `CMD MOVE_REL`, `CMD MOVE_ABS` przez `Protocol` | ruch moze byc wyzwalany z GUI i Agenta |
| Ruch w buildzie bezpiecznym | nie dotyczylo | w `SAFE` ruch jest jawnie zablokowany | mozna walidowac tor komunikacji bez poruszania osia |
| Ruch w buildzie ruchowym | bezposredni kod sterowania | ruch dostepny po `HOME` i spelnieniu warunkow safety | bardziej kontrolowany pierwszy ruch |
| Home / bazowanie | zalezne od starej logiki | jawna komenda `HOME` i statusy `HOMING_*` | lepsza obserwowalnosc i mniej automatyki po starcie |

## 5. Safety i fault handling

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Warstwa safety | brak wydzielonej warstwy | `fault`, `watchdogs`, `limits`, `safety_config`, `safety_monitor` | safety staje sie jawna odpowiedzialnoscia software |
| Fault manager | brak jednego miejsca decyzyjnego | centralny fault manager | spojne przejscie do stanu bledu |
| Watchdog komunikacji | brak lub lokalny mechanizm | heartbeat i timeout przez Agenta oraz firmware | brak komunikacji moze byc wykryty i obsluzony |
| Reakcja na blad | zalezna od konkretnego miejsca w kodzie | kontrolowane przejscie do `FAULT`, `STOP`, `QSTOP`, blokada ruchu | mniejsze ryzyko dalszej pracy mimo bledu |
| VBUS i nadzor zasilania | ograniczony lub nieujednolicony | `GET VBUS`, `GET VBUS_VALID`, faulty napieciowe | operator i firmware widza stan magistrali zasilania |
| STOP / QSTOP | logika lokalna | jawne komendy operatorskie z GUI i API | prostsza diagnostyka i testowanie reakcji awaryjnych |

## 6. Stany systemu i commissioning

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| State machine | brak wyraznej maszyny stanow | jawne stany osi, np. `BOOT`, `CONFIG`, `SAFE`, `CALIBRATION`, `ARMED`, `READY`, `MOTION`, `FAULT` | mozna odczytac rzeczywisty stan logiczny systemu |
| Etapy uruchomienia | brak formalnych etapow | `1 SAFE`, `2 ARMING`, `3 CONTROLLED MOTION` | uruchomienie jest uporzadkowane |
| Checklisty operatorskie | brak | checklisty w GUI | operator ma przypomnienie o kolejnosci czynnosci |
| Blokada ruchu przed etapem 3 | nieformalna | jawna przez commissioning i `RUN_ALLOWED` | trudniej uruchomic ruch przypadkiem |

## 7. Komunikacja i diagnostyka

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| UART | glownie debug stream | ustrukturyzowany protokol `GET / SET / CMD` | komputer moze rozmawiac z MCU przewidywalnie |
| Agent PC | brak | tlumaczy REST <-> UART | GUI i inne narzedzia nie musza znac surowego protokolu MCU |
| GUI | brak | dashboard, safety, commissioning, UART, logi, SQL, API | jeden interfejs do kontroli i diagnozy |
| Telemetria | debugowa, lokalna, nieujednolicona | ustandaryzowana telemetria i status endpointy | latwiej porownywac testy i diagnozowac zachowanie |
| Event log | ograniczony | `eventlog` w firmware + odczyt przez Agenta | historia zdarzen jest widoczna po stronie operatora |
| API RAW | brak | panel `RAW` i endpointy `/api/cmd/*` | serwis moze wysylac pojedyncze komendy kontrolowane |

## 8. Parametryzacja i konfiguracja

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Ustawienia pracy osi | glownie w kodzie lub lokalnych strukturach | `SET PARAM ...` oraz formularze GUI | mniej recznej ingerencji w firmware przy zmianie nastaw |
| Soft limity | mniej formalne | jawne parametry `SOFT_MIN_POS`, `SOFT_MAX_POS` | prostsza kontrola zakresu pracy |
| Limity pradu i predkosci | glownie po stronie kodu sterowania | parametry w GUI i firmware | latwiej ustawic konserwatywne wartosci testowe |
| Konfiguracja safety | slabo oddzielona od reszty logiki | osobna zakladka `Bezpieczenstwo` i osobne flagi firmware | override i hardware safety sa jawnie rozroznione |
| Persistencja konfiguracji | ograniczona | `config_store` jako warstwa konfiguracji | przygotowanie pod trwalszy zapis parametrow |

## 9. Obsluga operatora i HMI

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Interfejs operatora | brak dedykowanego UI | GUI webowe po polsku | latwiejsza praca serwisowa i uruchomieniowa |
| Help i opisy | brak | `Help ON/OFF`, opisy pol i referencje do zmiennych / kodu | szybsze zrozumienie przez integratora i programiste MCU |
| Tryby pracy interfejsu | brak | `DEMO` i `LIVE` | mozna sprawdzic GUI bez podlaczonego sprzetu |
| Przygotowanie pod HMI | brak jasnej sciezki | HMI moze byc klientem Agenta / API | latwiejsza dalsza rozbudowa |

## 10. Wdrozenie i utrzymanie

| Obszar | Stary soft | Nowy soft | Znaczenie praktyczne |
| --- | --- | --- | --- |
| Artefakty wdrozeniowe | glownie projekt MCU i wsad | komplet paczek: `JEDEN_PLIK`, `PC`, `MCU SAFE`, `MCU MOTION`, `Dokumentacja` | prostsze przekazanie systemu na inne stanowisko |
| Instrukcje uruchomienia | glownie wiedza zespolu | komplet dokumentacji wdrozeniowej i karta operatora A4 | mniejsze ryzyko bledow proceduralnych |
| Flashowanie | pojedynczy wsad | osobne skrypty `SAFE` i `MOTION` | bezpieczniejsza sekwencja pierwszego uruchomienia |
| Testy software PC | ograniczone | testy Agenta i mostka REST/UART | latwiejsza kontrola regresji po stronie PC |

## 11. Co zostaje bez zmian

| Element | Status w nowym systemie | Uwagi |
| --- | --- | --- |
| tor mocy i hardware niskiego poziomu | zostaje | nie jest przepisywany od zera |
| bazowa struktura projektu STM32 | zostaje | nadal jest projekt CubeIDE |
| fundament FOC / PID / trajektorii | zostaje | ale jest objety nowa warstwa nadzoru |
| rzeczywiste ograniczenia mechaniki urzadzenia | zostaja | nowy soft ich nie uniewaznia, tylko pomaga je kontrolowac |

## 12. Najwazniejsza roznica

Najkrotsze podsumowanie:

| Stary soft | Nowy soft |
| --- | --- |
| dzialajacy firmware sterujacy napedem | nadzorowany system sterowania z `GUI`, `Agent`, `API`, `SQL`, `commissioning` i `safety` |

Czyli:

```text
stary soft = glownie sterowanie silnikiem
nowy soft = sterowanie silnikiem + nadzor + diagnostyka + uruchamianie etapowe + integracja operatorska
```
