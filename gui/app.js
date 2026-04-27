const cfg = window.APP_CONFIG || {
  apiBaseUrl: "http://127.0.0.1:8000",
  refreshMs: 300,
  demoMode: true
};

const uiState = {
  status: null,
  telemetry: null,
  events: [],
  diagnostics: null,
  history: [],
  connected: false,
  lastError: "",
  diagnosticsError: "",
  helpEnabled: false,
  helpTooltip: null,
  helpHideTimer: null,
  activeHelpTrigger: null,
  theme: "dark"
};

const API_ENDPOINTS = [
  ["GET", "/api/status", "stan osi, bezpieczeństwo, konfiguracja i RUN_ALLOWED"],
  ["GET", "/api/telemetry/latest", "ostatnia próbka telemetrii z UART"],
  ["GET", "/api/events/recent", "ostatnie zdarzenia firmware"],
  ["GET", "/api/debug/vars", "diagnostyka UART, SQL, wersja i pola protokolu"],
  ["POST", "/api/params", "zmiana parametrów i konfiguracji"],
  ["POST", "/api/cmd/enable", "włączenie osi"],
  ["POST", "/api/cmd/disable", "wyłączenie osi"],
  ["POST", "/api/cmd/ack-fault", "potwierdzenie błędu po usunięciu przyczyny"],
  ["POST", "/api/cmd/home", "jawne uruchomienie sekwencji HOME"],
  ["POST", "/api/cmd/stop", "STOP"],
  ["POST", "/api/cmd/qstop", "QSTOP"],
  ["POST", "/api/cmd/first-move-rel", "pierwszy mały test ruchu w Etapie 2"],
  ["POST", "/api/cmd/move-rel", "kontrolowany ruch względny"],
  ["POST", "/api/cmd/move-abs", "kontrolowany ruch do pozycji bezwzględnej"],
  ["POST", "/api/cmd/raw", "surowa komenda serwisowa"]
];

const ERROR_TRANSLATIONS = {
  UNKNOWN_COMMAND: "Nieznana komenda",
  COMMAND_REJECTED: "Komenda odrzucona",
  UNKNOWN_ERROR: "Nieznany błąd",
  DIAGNOSTICS_UNAVAILABLE: "Diagnostyka niedostępna",
  FAULT_ACTIVE: "Aktywny błąd - najpierw usuń przyczynę i potwierdź błąd",
  RUN_NOT_ALLOWED: "Ruch zablokowany przez RUN_ALLOWED",
  LIMIT_REJECTED: "Ruch odrzucony przez limit",
  FIRST_MOVE_REL_REJECTED: "Pierwszy test ruchu odrzucony przez firmware",
  UNKNOWN_DEMO_COMMAND: "Nieznana komenda w trybie DEMO"
};

const HELP_TEXTS = [
  [".hero-copy h1", "Główny ekran operatorski systemu Mikrotom. Z tego miejsca operator widzi stan osi, blokady bezpieczeństwa, telemetrię, uruchamianie etapowe, komunikację UART, logi i dostęp serwisowy. To panel nadzorczy, nie bezpośredni sterownik mocy; decyzja o dopuszczeniu ruchu pozostaje w firmware STM32."],
  [".hero-copy p", "Opisuje zakres aplikacji. GUI zbiera w jednym miejscu podgląd online, ustawienia, safety, UART, logi, SQL i API. Operator dostaje wygodny interfejs, ale firmware nadal wymusza RUN_ALLOWED, limity, fault manager i commissioning."],
  ["#badge-connection", "Pokazuje, czy GUI ma aktualny kontakt z Agentem i źródłem danych."],
  ["#badge-state", "Pokazuje stan osi zgłaszany przez firmware, np. SAFE, CONFIG, READY, MOTION albo FAULT."],
  ["#badge-fault", "Pokazuje aktywny błąd firmware. NONE oznacza brak aktywnego błędu."],
  ["#badge-run", "RUN_ALLOWED to centralna bramka ruchu. Ruch wolno wykonać tylko wtedy, gdy firmware ustawi tę wartość na 1."],
  ["#badge-stage", "Aktualny etap uruchomienia. Etap 1 to diagnostyka bez ruchu, etap 3 dopuszcza kontrolowany ruch."],
  [".top-menu a[href='#online']", "Przejście do ekranu podglądu online: status, telemetria, ruch i podstawowe akcje operatora."],
  [".top-menu a[href='#wizard']", "Przejście do kreatora uruchomienia. To sekwencja od fizycznego podłączenia stanowiska do pierwszego małego testu FIRST_MOVE_REL."],
  [".top-menu a[href='#global']", "Przejście do parametrów osi i konfiguracji bazowej, takich jak prąd, prędkość i limity pozycji."],
  [".top-menu a[href='#safety']", "Przejście do ustawień bezpieczeństwa oraz jawnych obejść używanych tylko podczas kontrolowanych testów."],
  [".top-menu a[href='#uart']", "Przejście do diagnostyki połączenia UART między PC Agentem i STM32."],
  [".top-menu a[href='#logs']", "Przejście do podglądu zdarzeń zgłaszanych przez firmware."],
  [".top-menu a[href='#sql']", "Przejście do informacji o lokalnej bazie SQLite używanej przez Agenta do logowania."],
  [".top-menu a[href='#api']", "Przejście do mapy API i panelu surowych komend serwisowych."],
  ["#online .section-title .panel-kicker", "Zakładka Podgląd online jest głównym ekranem pracy operatora. Służy do szybkiej oceny, czy system komunikuje się z Agentem, czy firmware raportuje poprawny stan i czy ruch jest aktualnie dopuszczony."],
  ["#online .section-title h2", "Ten nagłówek oznacza obszar bieżącego nadzoru. W jednym widoku łączy status firmware, telemetrię osi, wizualizację pozycji, przyciski operatora i commissioning. Przy pierwszym uruchomieniu to miejsce, od którego zaczynamy ocenę ryzyka."],
  ["#online .section-title .panel-note", "Krótka informacja operatorska: ten ekran nie jest archiwum ani konfiguracją, tylko szybkim pulpitem bieżącej sytuacji. Jeżeli dane są w trybie DEMO, wartości są symulowane; jeżeli LIVE, przychodzą przez Agent API z UART."],
  ["#online .column-main .panel:nth-of-type(1) .panel-kicker", "Panel online pokazuje najbardziej aktualny stan urządzenia. Obejmuje wartości odświeżane cyklicznie: pozycję, cel, prędkość, prąd Iq, VBUS oraz flagi bezpieczeństwa. Jest przeznaczony do szybkiego sprawdzenia, czy firmware i Agent żyją oraz czy nie ma blokad ruchu."],
  ["#online .column-main .panel:nth-of-type(1) h2", "Aktualny stan systemu to syntetyczny obraz tego, co firmware zgłasza teraz. Jeżeli występuje rozbieżność między oczekiwanym stanem a tym panelem, przyjmujemy dane firmware jako ważniejsze od intencji GUI."],
  ["#connection-detail", "Szczegół źródła danych: tryb DEMO albo adres API Agenta w trybie LIVE."],
  [".kpi-grid .kpi:nth-child(1) > span", "Aktualna pozycja osi odczytana z telemetrii lub statusu firmware."],
  [".kpi-grid .kpi:nth-child(2) > span", "Pozycja zadana, czyli punkt, do którego regulator ma prowadzić oś."],
  [".kpi-grid .kpi:nth-child(3) > span", "Aktualna prędkość osi w mikrometrach na sekundę."],
  [".kpi-grid .kpi:nth-child(4) > span", "Zmierzony prąd Iq. To ważny wskaźnik obciążenia napędu."],
  [".kpi-grid .kpi:nth-child(5) > span", "Napięcie magistrali zasilania mocy. Ruch powinien być blokowany, jeśli pomiar VBUS nie jest poprawny."],
  ["#online .column-main .panel:nth-of-type(2) .panel-kicker", "Wizualizacja osi zamienia liczby na obraz ruchu. Pomaga szybko zobaczyć, gdzie znajduje się oś, gdzie jest cel i czy pozycje mieszczą się w zadanych limitach programowych."],
  ["#online .column-main .panel:nth-of-type(2) h2", "Obszar ruchu przedstawia zakres pracy osi liniowej. Żółty odcinek pokazuje limity programowe, niebieski znacznik pozycję aktualną, zielony pozycję docelową, a czerwony sygnał wskazuje błąd lub stan niebezpieczny."],
  ["#online .column-main .panel:nth-of-type(2) .panel-note", "Opis kolorów i interpretacji wykresu. Ten widok nie zastępuje mechanicznych krańcówek ani safety firmware, ale ułatwia operatorowi wykrycie nielogicznego celu, zbliżania się do limitu lub braku poprawnej telemetrii."],
  [".legend-item:nth-of-type(1)", "Niebieski znacznik pokazuje aktualną pozycję osi."],
  [".legend-item:nth-of-type(2)", "Zielony znacznik pokazuje pozycję zadaną."],
  [".legend-item:nth-of-type(3)", "Żółty zakres pokazuje dopuszczalne limity programowe."],
  [".legend-item:nth-of-type(4)", "Czerwony sygnał oznacza błąd albo stan niebezpieczny."],
  ["#online .column-main .panel:nth-of-type(3) .panel-kicker", "Sterowanie osią zawiera akcje operatora, które są tłumaczone przez GUI na komendy API, a następnie przez Agenta na komendy UART do firmware. To nadal nie omija safety; każda komenda ruchu musi przejść przez RUN_ALLOWED i limity."],
  ["#online .column-main .panel:nth-of-type(3) h2", "Akcje operatora to miejsce ręcznego włączania, wyłączania, potwierdzania błędu, zatrzymania i zlecania małych ruchów testowych. STOP i QSTOP są celowo stale widoczne, bo w trakcie uruchamiania mają większy priorytet niż wygoda interfejsu."],
  ["#move-warning", "Opisuje, dlaczego ruch jest aktualnie zablokowany albo kiedy jest dopuszczony."],
  ["#online .column-side .panel-kicker", "Uruchamianie etapowe prowadzi operatora od trybu bez ruchu do kontrolowanego ruchu. To praktyczna warstwa commissioning: najpierw komunikacja i diagnostyka, potem uzbrajanie, dopiero na końcu mały ruch pod nadzorem."],
  ["#online .column-side h2", "Przebieg etapów pokazuje, na którym poziomie uruchomienia pracuje urządzenie. Etapy zmniejszają ryzyko, bo operator nie przechodzi od razu do ruchu, tylko potwierdza kolejne warunki bezpieczeństwa."],
  ["#stage-1-card h3", "Etap 1: diagnostyka i komunikacja bez ruchu osi."],
  ["#stage-2-card h3", "Etap 2: uzbrajanie i test ENABLE/DISABLE bez pełnego ruchu."],
  ["#stage-3-card h3", "Etap 3: ruch kontrolowany, nadal zależny od RUN_ALLOWED i limitów firmware."],
  ["#global .section-title .panel-kicker", "Ustawienia globalne obejmują normalne parametry pracy osi, a nie awaryjne obejścia. To miejsce na limity prądu, prędkości, przyspieszenia i zakresu pozycji."],
  ["#global .section-title h2", "Parametry osi i konfiguracja bazowa definiują bezpieczną kopertę pracy. Podczas pierwszych testów wartości powinny być konserwatywne, bo ograniczają energię ruchu oraz ryzyko mechaniczne."],
  ["#global .section-title .panel-note", "Ta sekcja nie służy do wyłączania zabezpieczeń. Jej zadaniem jest ustawienie normalnych limitów pracy, które firmware powinien stosować niezależnie od tego, czy komenda przyszła z GUI, HMI czy panelu serwisowego."],
  ["#global .panel .panel-kicker", "Konfiguracja globalna to zestaw flag i limitów wysyłanych przez Agenta do STM32. Dane są używane przez firmware do walidacji ruchu i parametrów trajektorii."],
  ["#global .panel h2", "Flagi pracy i limity ruchu pokazują zarówno statusy tylko do odczytu, jak i parametry ustawialne. Statusy informują, co firmware już wie, a pola liczbowe pozwalają zmienić bezpieczne granice pracy."],
  ["#global .panel .panel-note", "Zmiany trafiają do firmware przez HTTP API Agenta i UART. Programista powinien traktować tę ścieżkę jako warstwę konfiguracyjną nad modułami limits, safety_config i protocol."],
  ["#safety .section-title .panel-kicker", "Zakładka Bezpieczeństwo skupia konfigurację czujników oraz jawne obejścia. Oddzielenie jej od ustawień globalnych ma zapobiec przypadkowemu wyłączeniu mechanizmów ochronnych."],
  ["#safety .section-title h2", "Zabezpieczenia, czujniki i obejścia opisują, które elementy safety są fizycznie obecne oraz które sygnały firmware może tymczasowo ignorować. Każde obejście zwiększa ryzyko i powinno być używane świadomie."],
  ["#safety .section-title .panel-note", "Ten opis przypomina, że obejścia nie są zwykłymi ustawieniami operatorskimi. Ich przypadkowe włączenie może dopuścić ruch mimo braku sygnału hamulca, czujnika kolizji, interlocka albo kalibracji."],
  ["#safety .panel .panel-kicker", "Panel bezpieczeństwa jest miejscem konfiguracji hardware safety. Ustawienia informują firmware, czy hamulec, czujnik kolizji i zewnętrzny interlock są obecne oraz czy wolno tymczasowo ignorować ich feedback."],
  ["#safety .panel h2", "Konfiguracja i obejścia pokazują parametry, które bezpośrednio wpływają na decyzję RUN_ALLOWED. GUI może wysłać żądanie, ale firmware powinien nadal rozstrzygać, czy ruch jest bezpieczny."],
  ["#safety .panel .panel-note", "Firmware nadal podejmuje finalną decyzję. Jeżeli logika safety w STM32 blokuje ruch, GUI nie powinno mieć ścieżki do wymuszenia ruchu poza AxisControl i fault managerem."],
  ["#uart .section-title .panel-kicker", "Zakładka UART opisuje fizyczny i logiczny kanał komunikacji między PC Agentem a STM32. To tutaj operator sprawdza port COM, baudrate i stan połączenia."],
  ["#uart .section-title h2", "Połączenie PC Agent - STM32 jest mostem między GUI/API a firmware. Jeżeli ten kanał nie działa, GUI może nadal się wyświetlać, ale komendy i telemetria nie będą wiarygodne."],
  ["#uart .section-title .panel-note", "Agent utrzymuje port COM, wysyła komendy, odbiera odpowiedzi RSP, zdarzenia EVT i telemetrię TEL. To ważne miejsce diagnostyczne przy problemach z brakiem danych lub timeoutami heartbeat."],
  ["#logs .section-title .panel-kicker", "Podgląd logów pokazuje historię krótkich zdarzeń zgłaszanych przez firmware. Logi pomagają odtworzyć kolejność: boot, enable, stop, fault, ack, zmiana etapu, kalibracja."],
  ["#logs .section-title h2", "Zdarzenia firmware i ostatnie reakcje systemu są używane do diagnostyki uruchamiania. Jeżeli system zablokował ruch, log powinien pomóc ustalić, czy powodem był fault, limit, etap commissioning czy komunikacja."],
  ["#logs .panel h2", "Ostatnie zdarzenia to tabela z czasem MCU, kodem zdarzenia i wartością pomocniczą. Nie zastępuje pełnej bazy SQL, ale daje szybki podgląd bez opuszczania GUI."],
  ["#sql .section-title .panel-kicker", "Zakładka SQL opisuje lokalne logowanie po stronie Agenta. Dane nie są zapisywane w MCU, tylko na PC, co ułatwia analizę po testach bez obciążania firmware."],
  ["#sql .section-title h2", "Lokalna baza logów Agenta przechowuje protokół UART, telemetrię i zdarzenia. Dzięki temu można później porównać zachowanie urządzenia, odtworzyć sekwencję komend i sprawdzić odpowiedzi firmware."],
  ["#sql .section-title .panel-note", "SQLite zapisuje historię lokalnie na komputerze. To narzędzie traceability i diagnostyki, szczególnie przy testach safety i powtarzalności ruchu."],
  ["#api .section-title .panel-kicker", "Zakładka API pokazuje interfejs HTTP wystawiany przez lokalnego Agenta. GUI nie rozmawia bezpośrednio z MCU przez HTTP; Agent tłumaczy endpointy API na komendy UART."],
  ["#api .section-title h2", "Endpointy i dostęp serwisowy są mapą integracji dla GUI, HMI i narzędzi testowych. Programista może sprawdzić, który endpoint odpowiada za status, telemetrię, parametry, STOP, QSTOP i komendy RAW."],
  ["#api .section-title .panel-note", "GUI rozmawia z Agentem HTTP, a Agent tłumaczy żądania na UART. Taki podział pozwala rozwijać GUI i HMI bez przepisywania firmware STM32."],
  ["#api .layout .panel:nth-of-type(1) .panel-kicker", "Mapa API dokumentuje, z jakich endpointów korzysta GUI. To szybka ściąga dla integracji z HMI, testami automatycznymi albo zewnętrznym narzędziem diagnostycznym."],
  ["#api .layout .panel:nth-of-type(1) h2", "Dostępne endpointy pokazują ścieżki HTTP Agenta oraz ich rolę. Każdy endpoint ma w helpie referencję do funkcji Agenta albo komendy UART, żeby programista mógł prześledzić przepływ."],
  ["#api .layout .panel:nth-of-type(2) .panel-kicker", "Panel serwisowy jest przeznaczony do ręcznej diagnostyki protokołu, a nie do normalnej pracy operatora. Pozwala wysłać pojedynczą komendę RAW do Agenta."],
  ["#api .layout .panel:nth-of-type(2) h2", "Dostęp do komend surowych pozwala wysłać ręczną komendę protokołu. Używaj go ostrożnie, bo omija wygodne przyciski GUI, chociaż nadal nie powinien omijać zabezpieczeń firmware."],
  ["#raw-command", "Tu wpisujesz surową komendę protokołu, np. CMD CALIB_ZERO albo GET STATE."],
  ["#uart-port", "Port COM używany przez Agenta do komunikacji z STM32."],
  ["#uart-baud", "Prędkość transmisji UART. Musi być zgodna po stronie STM32 i Agenta."],
  ["#uart-connected", "Status połączenia widziany przez Agenta i GUI."],
  ["#sql-enabled", "Informuje, czy lokalne logowanie do SQLite jest aktywne."],
  ["#sql-path", "Ścieżka do pliku bazy danych SQLite tworzonego przez Agenta."],
  ["#sql-counts", "Liczba zapisanych rekordów: protokół UART / telemetria / zdarzenia."],
  [".events-table th:nth-of-type(1)", "Czas zdarzenia według zegara MCU."],
  [".events-table th:nth-of-type(2)", "Kod zdarzenia, np. BOOT, STOP albo FAULT_SET."],
  [".events-table th:nth-of-type(3)", "Dodatkowa wartość liczbowa zdarzenia, jeśli firmware ją przekazał."],
  ["#footer-left", "Adres API, z którym komunikuje się GUI."],
  ["#footer-right", "Aktualny tryb GUI: DEMO albo LIVE oraz informacja, czy wersja firmware obsługuje ruch."]
];

const FIELD_HELP_TEXTS = {
  "move-rel": "Wartość ruchu względnego w mikrometrach. Dodatnia wartość przesuwa oś w jedną stronę, ujemna w przeciwną. Firmware nadal sprawdza RUN_ALLOWED i limity.",
  "first-move-rel": "Wartość pierwszego testu ruchu w mikrometrach. Firmware przyjmuje tylko bardzo mały krok w Etapie 2, bez pełnego HOME.",
  "move-abs": "Docelowa pozycja bezwzględna w mikrometrach. Komenda jest odrzucana, jeśli narusza limity albo RUN_ALLOWED = 0.",
  "cfg-max-current": "MAX_CURRENT [A] ogranicza nominalny prąd sterowania. Zbyt wysoka wartość może przegrzać napęd lub mechanikę, dlatego na start ustawiaj konserwatywnie.",
  "cfg-max-current-peak": "MAX_CURRENT_PEAK [A] to krótkotrwały limit szczytowy prądu. Powinien być wyższy od MAX_CURRENT, ale nadal bezpieczny dla sprzętu.",
  "cfg-max-velocity": "MAX_VELOCITY [m/s] ogranicza maksymalną prędkość osi. Na pierwsze uruchomienie ustaw nisko.",
  "cfg-max-acceleration": "MAX_ACCELERATION [m/s2] ogranicza przyspieszenie. Mniejsza wartość zmniejsza szarpnięcia i ryzyko mechaniczne.",
  "cfg-soft-min": "SOFT_MIN_POS [um] to dolny programowy limit pozycji. Ruch poniżej tej pozycji powinien zostać odrzucony.",
  "cfg-soft-max": "SOFT_MAX_POS [um] to górny programowy limit pozycji. Ruch powyżej tej pozycji powinien zostać odrzucony.",
  "raw-command": "Surowa komenda protokołu. To narzędzie serwisowe, więc nie używaj go do omijania normalnego przepływu safety."
};

const SWITCH_HELP_TEXTS = {
  "cfg-config-loaded": "Status tylko do odczytu. TAK oznacza, że firmware załadował konfigurację z pamięci flash.",
  "cfg-enabled": "Status tylko do odczytu. TAK oznacza, że napęd jest logicznie włączony.",
  "cfg-brake-installed": "Informuje firmware, czy układ ma hamulec. Jeśli hamulec jest fizycznie obecny, ta flaga powinna być włączona.",
  "cfg-collision-installed": "Informuje firmware, czy dostępny jest czujnik kolizji. Jeśli czujnik jest zamontowany, nie wyłączaj tej flagi.",
  "cfg-external-interlock-installed": "Informuje firmware, czy używana jest zewnętrzna blokada bezpieczeństwa maszyny.",
  "cfg-ignore-brake": "Tymczasowo ignoruje sygnał zwrotny hamulca. Używać tylko podczas kontrolowanych testów bez ruchu lub z bardzo niskim ryzykiem.",
  "cfg-ignore-collision": "Tymczasowo ignoruje czujnik kolizji. To obejście podnosi ryzyko i powinno być logowane oraz świadomie użyte.",
  "cfg-ignore-interlock": "Tymczasowo ignoruje zewnętrzną blokadę. Nie używać przy normalnej pracy urządzenia.",
  "cfg-allow-no-calib": "Pozwala na ruch bez potwierdzonej kalibracji. To ustawienie jest ryzykowne i powinno być używane tylko w kontrolowanym commissioning.",
  "cfg-calib-valid": "Status tylko do odczytu. TAK oznacza, że firmware uznaje kalibrację pozycji za ważną."
};

const BUTTON_HELP_TEXTS = {
  "btn-enable": "Wysyła CMD ENABLE. Samo ENABLE nie wystarcza do ruchu, bo firmware nadal sprawdza RUN_ALLOWED.",
  "btn-disable": "Wysyła CMD DISABLE i blokuje napęd logicznie.",
  "btn-ack-fault": "Wysyła CMD ACK_FAULT. Użyj dopiero po usunięciu przyczyny błędu.",
  "btn-home": "Wysyła CMD HOME. W buildzie ruchowym uruchamia jawną sekwencję homingu zamiast automatycznego startu po boot.",
  "btn-stop": "Wysyła CMD STOP. To normalne zatrzymanie, które powinno być zawsze dostępne.",
  "btn-qstop": "Wysyła CMD QSTOP. To szybkie zatrzymanie i wyjście do bezpieczniejszego stanu.",
  "btn-move-rel": "Wysyła CMD MOVE_REL z wartością z pola ruchu względnego. Przycisk jest blokowany, gdy RUN_ALLOWED nie pozwala na ruch.",
  "btn-first-move-rel": "Wysyła CMD FIRST_MOVE_REL. To mały test ruchu przed pełnym homingiem, dostępny tylko w buildzie MOTION i w Etapie 2.",
  "btn-move-abs": "Wysyła CMD MOVE_ABS z pozycją docelową. Firmware nadal weryfikuje limity.",
  "btn-calib-zero": "Wysyła CMD CALIB_ZERO, czyli ustawienie aktualnej pozycji jako zera kalibracyjnego.",
  "btn-stage-1": "Ustawia etap 1 po zaznaczeniu checklisty. To tryb bez ruchu.",
  "btn-stage-2": "Ustawia etap 2 po zaznaczeniu checklisty. Służy do uzbrajania i kontroli bez pełnego ruchu.",
  "btn-stage-3": "Ustawia etap 3 po zaznaczeniu checklisty. Dopiero tutaj można dopuścić ruch kontrolowany.",
  "btn-save-safety": "Wysyła parametry globalne do firmware przez Agenta.",
  "btn-save-safety-overrides": "Wysyła konfigurację safety i obejścia. Używaj świadomie, bo te ustawienia wpływają na ryzyko uruchomienia.",
  "btn-refresh-diagnostics": "Odświeża informacje UART i SQL z endpointu diagnostycznego Agenta.",
  "btn-send-raw": "Wysyła surową komendę z pola RAW.",
  "btn-refresh": "Ręcznie odświeża dane z Agenta albo modelu DEMO.",
  "btn-wizard-refresh": "Odświeża stan kreatora z MCU.",
  "btn-wizard-ack": "Wysyła ACK_FAULT w kreatorze po usunięciu przyczyny błędu.",
  "btn-wizard-stage1": "Przywraca tryb SAFE i Etap 1.",
  "btn-wizard-stage2": "Ustawia Etap 2, czyli ARMING_ONLY bez CONTROLLED_MOTION.",
  "btn-wizard-calib-zero": "Wysyła CMD CALIB_ZERO z kreatora. Aktualna pozycja zostaje punktem zera do pierwszego testu.",
  "btn-wizard-enable": "Wysyła ENABLE jako przygotowanie do małego testu FIRST_MOVE_REL.",
  "btn-wizard-disable": "Wyłącza oś po teście albo przy przerwaniu sekwencji.",
  "btn-wizard-qstop": "Szybkie zatrzymanie z poziomu kreatora."
};

const CHECK_HELP_TEXTS = [
  [".stage1-check", "Checklistę etapu 1 zaznacz dopiero po ręcznym potwierdzeniu warunku."],
  [".stage2-check", "Checklistę etapu 2 zaznacz dopiero po sprawdzeniu mechaniki i gotowości operatora."],
  [".stage3-check", "Checklistę etapu 3 zaznacz dopiero przy gotowości do kontrolowanego ruchu."]
];

const STATUS_HELP_TEXTS = {
  "Oś włączona": "Informuje, czy firmware ma logicznie włączony napęd.",
  "Konfiguracja z flash załadowana": "TAK oznacza, że konfiguracja została poprawnie odczytana z pamięci flash.",
  "Ruch zaimplementowany": "NIE oznacza build bezpiecznej integracji bez aktywnego ruchu.",
  "Wersja bezpiecznej integracji": "TAK oznacza wersję startującą bez automatycznego ruchu.",
  "Etap uruchomienia": "Aktualny etap commissioning: 1, 2 albo 3.",
  "Tryb bezpieczny": "Tryb blokujący ruch i wymuszający stan bezpieczny.",
  "Tylko uzbrajanie": "Tryb do testu gotowości bez pełnego ruchu.",
  "Ruch kontrolowany": "Flaga dopuszczająca ruch w etapie 3, jeśli pozostałe warunki safety są spełnione.",
  "Ruch bez kalibracji dozwolony": "Ryzykowny override. Przy normalnej pracy powinien być wyłączony.",
  "Homing rozpoczęty": "TAK oznacza, że firmware otrzymał żądanie HOME i wszedł w sekwencję bazowania.",
  "Homing w toku": "TAK oznacza, że oś wykonuje sekwencję HOME i nie należy jeszcze zlecać ruchów testowych.",
  "Homing zakończony": "TAK oznacza, że firmware zakończył sekwencję HOME sukcesem i może odblokować ruch po spełnieniu reszty warunków.",
  "Kalibracja ważna": "TAK oznacza, że firmware uznaje punkt odniesienia osi za poprawny.",
  "VBUS": "Zmierzona wartość napięcia zasilania mocy.",
  "VBUS poprawny": "TAK oznacza, że pomiar VBUS jest dostępny i mieści się w oczekiwanym zakresie.",
  "PTC zainstalowany": "Informacja o czujniku temperatury PTC, jeśli sprzęt go używa.",
  "Zasilanie zapasowe zainstalowane": "Informacja o obecności zasilania zapasowego.",
  "Maska błędów": "Bitowa maska aktywnych błędów firmware."
};

const SELECTOR_HELP_REFS = {
  ".hero-copy h1": "gui/index.html / app.js HELP_TEXTS",
  ".hero-copy p": "gui/index.html / AgentBridge + protocol.c",
  "#badge-connection": "AgentBridge.connected / GET /api/status",
  "#badge-state": "status.axis_state / AxisState",
  "#badge-fault": "status.fault / Fault_GetActive()",
  "#badge-run": "status.run_allowed / AxisControl_RunAllowed()",
  "#badge-stage": "status.commissioning_stage / SET CONFIG COMMISSION_STAGE",
  "#online .section-title .panel-kicker": "GET /api/status + GET /api/telemetry/latest",
  "#online .section-title h2": "renderAll() / status + telemetry",
  "#online .section-title .panel-note": "cfg.demoMode / cfg.apiBaseUrl",
  "#online .column-main .panel:nth-of-type(1) .panel-kicker": "renderStatusDetails() / status-list",
  "#online .column-main .panel:nth-of-type(1) h2": "status.* / telemetry.*",
  "#connection-detail": "cfg.apiBaseUrl / APP_CONFIG",
  ".kpi-grid .kpi:nth-child(1) > span": "telemetry.pos_um / status.position_um",
  ".kpi-grid .kpi:nth-child(2) > span": "telemetry.pos_set_um / status.position_set_um",
  ".kpi-grid .kpi:nth-child(3) > span": "telemetry.vel_um_s",
  ".kpi-grid .kpi:nth-child(4) > span": "telemetry.iq_meas_mA",
  ".kpi-grid .kpi:nth-child(5) > span": "status.vbus_V / status.vbus_valid",
  "#online .column-main .panel:nth-of-type(2) .panel-kicker": "renderAxis() / axis-svg",
  "#online .column-main .panel:nth-of-type(2) h2": "soft_min_pos, soft_max_pos, pos_um, pos_set_um",
  "#online .column-main .panel:nth-of-type(2) .panel-note": "renderAxis() legend colors",
  "#online .column-main .panel:nth-of-type(3) .panel-kicker": "executeCommand() / API command path",
  "#online .column-main .panel:nth-of-type(3) h2": "CMD ENABLE/DISABLE/STOP/QSTOP/MOVE",
  "#move-warning": "explainRunAllowed(status) / AxisControl_RunAllowed()",
  "#online .column-side .panel-kicker": "commissioning_stage / Commissioning module",
  "#online .column-side h2": "SET CONFIG COMMISSION_STAGE",
  "#global .section-title .panel-kicker": "limits.c / safety_config.c",
  "#global .section-title h2": "Limits_t / AgentBridge.apply_params()",
  "#global .section-title .panel-note": "SET PARAM MAX_CURRENT/MAX_VELOCITY/SOFT_*",
  "#global .panel .panel-kicker": "POST /api/params",
  "#global .panel h2": "status.config_loaded + Limits_Get()",
  "#global .panel .panel-note": "AgentBridge.apply_params() -> SET PARAM",
  "#safety .section-title .panel-kicker": "safety_config.c / fault.c",
  "#safety .section-title h2": "SET CONFIG BRAKE/COLLISION/INTERLOCK",
  "#safety .section-title .panel-note": "ignore_* overrides / eventlog",
  "#safety .panel .panel-kicker": "SafetyConfig / Commissioning",
  "#safety .panel h2": "RUN_ALLOWED / AxisControl_RunAllowed()",
  "#safety .panel .panel-note": "firmware safety gate",
  "#uart .section-title .panel-kicker": "USART2 / Protocol_Init()",
  "#uart .section-title h2": "Agent SerialBridge <-> STM32 USART2",
  "#uart .section-title .panel-note": "TEL/RSP/EVT frames",
  "#logs .section-title .panel-kicker": "eventlog.c / EVT frames",
  "#logs .section-title h2": "GET /api/events/recent",
  "#logs .panel h2": "events-body / EventEntry_t",
  "#sql .section-title .panel-kicker": "agent/log_store.py / SQLite",
  "#sql .section-title h2": "diagnostics.log_store",
  "#sql .section-title .panel-note": "MIKROTOM_SQLITE_PATH",
  "#api .section-title .panel-kicker": "agent/main.py FastAPI routes",
  "#api .section-title h2": "API_ENDPOINTS / Agent API",
  "#api .section-title .panel-note": "HTTP -> AgentBridge -> UART",
  "#api .layout .panel:nth-of-type(1) .panel-kicker": "API_ENDPOINTS",
  "#api .layout .panel:nth-of-type(1) h2": "agent/main.py routes",
  "#api .layout .panel:nth-of-type(2) .panel-kicker": "POST /api/cmd/raw",
  "#api .layout .panel:nth-of-type(2) h2": "CMD RAW / protocol.c",
  "#raw-command": "POST /api/cmd/raw -> protocol.c",
  "#uart-port": "diagnostics.transport.port",
  "#uart-baud": "diagnostics.transport.baudrate",
  "#uart-connected": "diagnostics.transport.connected",
  "#sql-enabled": "diagnostics.log_store.enabled",
  "#sql-path": "diagnostics.log_store.path",
  "#sql-counts": "diagnostics.log_store protocol_rows/telemetry_rows/event_rows",
  "#footer-left": "cfg.apiBaseUrl",
  "#footer-right": "cfg.demoMode / status.motion_implemented"
};

const FIELD_HELP_REFS = {
  "move-rel": "CMD MOVE_REL / payload.delta_um",
  "move-abs": "CMD MOVE_ABS / payload.target_um",
  "cfg-max-current": "SET PARAM MAX_CURRENT / Limits_SetMaxCurrentNominal()",
  "cfg-max-current-peak": "SET PARAM MAX_CURRENT_PEAK / Limits_SetMaxCurrentPeak()",
  "cfg-max-velocity": "SET PARAM MAX_VELOCITY / Limits_SetMaxVelocity()",
  "cfg-max-acceleration": "SET PARAM MAX_ACCELERATION / Limits_SetMaxAcceleration()",
  "cfg-soft-min": "SET PARAM SOFT_MIN_POS / Limits_SetSoftMinPos()",
  "cfg-soft-max": "SET PARAM SOFT_MAX_POS / Limits_SetSoftMaxPos()",
  "raw-command": "POST /api/cmd/raw / AgentBridge.send_raw()"
};

const SWITCH_HELP_REFS = {
  "cfg-config-loaded": "status.config_loaded / GET CONFIG CONFIG_LOADED",
  "cfg-enabled": "status.enabled / CMD ENABLE, CMD DISABLE",
  "cfg-brake-installed": "SET CONFIG BRAKE_INSTALLED",
  "cfg-collision-installed": "SET CONFIG COLLISION_SENSOR_INSTALLED",
  "cfg-external-interlock-installed": "SET CONFIG EXTERNAL_INTERLOCK_INSTALLED",
  "cfg-ignore-brake": "SET CONFIG IGNORE_BRAKE_FEEDBACK",
  "cfg-ignore-collision": "SET CONFIG IGNORE_COLLISION_SENSOR",
  "cfg-ignore-interlock": "SET CONFIG IGNORE_EXTERNAL_INTERLOCK",
  "cfg-allow-no-calib": "SET CONFIG ALLOW_MOTION_WITHOUT_CALIBRATION",
  "cfg-calib-valid": "status.calib_valid / Calibration_IsValid()"
};

const BUTTON_HELP_REFS = {
  "btn-enable": "POST /api/cmd/enable -> CMD ENABLE",
  "btn-disable": "POST /api/cmd/disable -> CMD DISABLE",
  "btn-ack-fault": "POST /api/cmd/ack-fault -> CMD ACK_FAULT",
  "btn-home": "POST /api/cmd/home -> CMD HOME",
  "btn-stop": "POST /api/cmd/stop -> CMD STOP",
  "btn-qstop": "POST /api/cmd/qstop -> CMD QSTOP",
  "btn-move-rel": "POST /api/cmd/move-rel -> CMD MOVE_REL",
  "btn-first-move-rel": "POST /api/cmd/first-move-rel -> CMD FIRST_MOVE_REL",
  "btn-move-abs": "POST /api/cmd/move-abs -> CMD MOVE_ABS",
  "btn-calib-zero": "POST /api/cmd/raw -> CMD CALIB_ZERO",
  "btn-stage-1": "POST /api/params commissioning_stage=1",
  "btn-stage-2": "POST /api/params commissioning_stage=2",
  "btn-stage-3": "POST /api/params commissioning_stage=3",
  "btn-save-safety": "POST /api/params -> SET PARAM ...",
  "btn-save-safety-overrides": "POST /api/params -> SET CONFIG ...",
  "btn-refresh-diagnostics": "GET /api/debug/vars",
  "btn-send-raw": "POST /api/cmd/raw",
  "btn-refresh": "refreshData() / GET status+telemetry+events",
  "btn-wizard-refresh": "refreshData() / GET status+telemetry+events",
  "btn-wizard-ack": "POST /api/cmd/ack-fault -> CMD ACK_FAULT",
  "btn-wizard-stage1": "POST /api/params -> commissioning_stage=1",
  "btn-wizard-stage2": "POST /api/params -> commissioning_stage=2",
  "btn-wizard-calib-zero": "POST /api/cmd/raw -> CMD CALIB_ZERO",
  "btn-wizard-enable": "POST /api/cmd/enable -> CMD ENABLE",
  "btn-wizard-disable": "POST /api/cmd/disable -> CMD DISABLE",
  "btn-wizard-qstop": "POST /api/cmd/qstop -> CMD QSTOP"
};

const CHECK_HELP_REFS = {
  ".stage1-check": "commissioning_stage=1 / Stage 1 checklist",
  ".stage2-check": "commissioning_stage=2 / Stage 2 checklist",
  ".stage3-check": "commissioning_stage=3 / Stage 3 checklist"
};

const STATUS_HELP_REFS = {
  "Oś włączona": "status.enabled",
  "Konfiguracja z flash załadowana": "status.config_loaded / ConfigStore_Load()",
  "Ruch zaimplementowany": "status.motion_implemented / APP_MOTION_IMPLEMENTED",
  "Wersja bezpiecznej integracji": "status.safe_integration / APP_SAFE_INTEGRATION",
  "Etap uruchomienia": "status.commissioning_stage / COMMISSION_STAGE",
  "Tryb bezpieczny": "status.safe_mode / Commissioning_IsSafeMode()",
  "Tylko uzbrajanie": "status.arming_only / Commissioning_IsArmingOnly()",
  "Ruch kontrolowany": "status.controlled_motion / Commissioning_IsControlledMotion()",
  "Ruch bez kalibracji dozwolony": "status.allow_motion_without_calibration",
  "Homing rozpoczęty": "status.homing_started / CMD HOME",
  "Homing w toku": "status.homing_ongoing / state.HomingOngoing",
  "Homing zakończony": "status.homing_successful / state.HomingSuccessful",
  "Kalibracja ważna": "status.calib_valid / Calibration_IsValid()",
  "VBUS": "status.vbus_V",
  "VBUS poprawny": "status.vbus_valid",
  "PTC zainstalowany": "status.ptc_installed",
  "Zasilanie zapasowe zainstalowane": "status.backup_supply_installed",
  "Maska błędów": "status.fault_mask / Fault_GetMask()"
};

const API_HELP_REFS = {
  "/api/status": "agent/main.py api_status() -> AgentBridge.status()",
  "/api/telemetry/latest": "agent/main.py api_telemetry_latest() -> UART TEL",
  "/api/events/recent": "agent/main.py api_events_recent() -> EVT/RSP EVENT",
  "/api/debug/vars": "agent/main.py api_debug_vars() -> diagnostics",
  "/api/params": "agent/main.py api_params() -> AgentBridge.apply_params()",
  "/api/cmd/enable": "agent/main.py api_cmd_enable() -> CMD ENABLE",
  "/api/cmd/disable": "agent/main.py api_cmd_disable() -> CMD DISABLE",
  "/api/cmd/ack-fault": "agent/main.py api_cmd_ack_fault() -> CMD ACK_FAULT",
  "/api/cmd/home": "agent/main.py api_cmd_home() -> CMD HOME",
  "/api/cmd/stop": "agent/main.py api_cmd_stop() -> CMD STOP",
  "/api/cmd/qstop": "agent/main.py api_cmd_qstop() -> CMD QSTOP",
  "/api/cmd/move-rel": "agent/main.py api_cmd_move_rel() -> CMD MOVE_REL",
  "/api/cmd/move-abs": "agent/main.py api_cmd_move_abs() -> CMD MOVE_ABS",
  "/api/cmd/raw": "agent/main.py api_cmd_raw() -> UART raw command"
};

const $ = (id) => document.getElementById(id);

function fmt(value, digits = 0) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) return "--";
  return Number(value).toLocaleString("pl-PL", {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  });
}

function translateError(message) {
  const text = String(message || "UNKNOWN_ERROR");
  return ERROR_TRANSLATIONS[text] || text;
}

function showToast(message, tone = "info") {
  const node = $("toast");
  node.textContent = message;
  node.dataset.tone = tone;
  node.classList.add("visible");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => node.classList.remove("visible"), 3200);
}

function setBadge(id, text, tone) {
  const node = $(id);
  node.textContent = text;
  node.dataset.tone = tone;
}

function escapeHtml(value) {
  return String(value ?? "--")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function resolveHelpTarget(target) {
  if (!target) return null;
  if (target.classList?.contains("help-dot") || target.closest?.(".help-dot")) return null;
  if (target.tagName === "INPUT") {
    const label = target.closest("label");
    return label?.querySelector("span") || label || target;
  }
  return target;
}

function ensureHelpTooltip() {
  if (uiState.helpTooltip) return uiState.helpTooltip;

  const tooltip = document.createElement("aside");
  tooltip.id = "help-tooltip";
  tooltip.className = "help-tooltip";
  tooltip.setAttribute("role", "tooltip");
  tooltip.innerHTML = `
    <div class="help-tooltip-body"></div>
    <div class="help-tooltip-ref"></div>
  `;
  tooltip.addEventListener("mouseenter", () => {
    clearTimeout(uiState.helpHideTimer);
  });
  tooltip.addEventListener("mouseleave", () => {
    hideHelpTooltip();
  });
  document.body.appendChild(tooltip);
  uiState.helpTooltip = tooltip;
  return tooltip;
}

function positionHelpTooltip(trigger, tooltip) {
  const rect = trigger.getBoundingClientRect();
  const margin = 12;
  const width = tooltip.offsetWidth;
  const height = tooltip.offsetHeight;
  const preferredLeft = rect.left + (rect.width / 2) - (width / 2);
  const left = Math.min(window.innerWidth - width - margin, Math.max(margin, preferredLeft));
  const belowTop = rect.bottom + 10;
  const aboveTop = rect.top - height - 10;
  const top = belowTop + height + margin <= window.innerHeight
    ? belowTop
    : Math.max(margin, aboveTop);

  tooltip.style.left = `${left}px`;
  tooltip.style.top = `${top}px`;
  tooltip.classList.add("visible");
}

function showHelpTooltip(trigger) {
  if (!uiState.helpEnabled || !trigger?.dataset.tooltip) return;
  clearTimeout(uiState.helpHideTimer);

  const tooltip = ensureHelpTooltip();
  tooltip.querySelector(".help-tooltip-body").textContent = trigger.dataset.tooltip;
  tooltip.querySelector(".help-tooltip-ref").textContent = `Kod/zmienna: ${trigger.dataset.ref || "GUI help metadata"}`;
  uiState.activeHelpTrigger = trigger;
  positionHelpTooltip(trigger, tooltip);
}

function scheduleHideHelpTooltip() {
  clearTimeout(uiState.helpHideTimer);
  uiState.helpHideTimer = setTimeout(() => {
    const tooltip = uiState.helpTooltip;
    const triggerHovered = uiState.activeHelpTrigger?.matches?.(":hover");
    const tooltipHovered = tooltip?.matches?.(":hover");
    if (!triggerHovered && !tooltipHovered) hideHelpTooltip();
  }, 260);
}

function hideHelpTooltip() {
  clearTimeout(uiState.helpHideTimer);
  uiState.activeHelpTrigger = null;
  uiState.helpTooltip?.classList.remove("visible");
}

function attachHelp(target, text, ref = "") {
  const node = resolveHelpTarget(target);
  if (!node || !text) return;

  let dot = Array.from(node.children).find((child) => child.classList.contains("help-dot"));
  if (!dot) {
    dot = document.createElement("span");
    dot.className = "help-dot";
    dot.textContent = "?";
    dot.tabIndex = 0;
    dot.setAttribute("role", "button");
    dot.addEventListener("click", (event) => event.stopPropagation());
    node.appendChild(dot);
  }

  if (dot.dataset.helpBound !== "1") {
    dot.dataset.helpBound = "1";
    dot.addEventListener("mouseenter", () => showHelpTooltip(dot));
    dot.addEventListener("mouseleave", scheduleHideHelpTooltip);
    dot.addEventListener("focus", () => showHelpTooltip(dot));
    dot.addEventListener("blur", scheduleHideHelpTooltip);
  }

  node.classList.add("help-target");
  dot.dataset.tooltip = text;
  dot.dataset.ref = ref || node.id || node.dataset.helpRef || node.dataset.helpCode || "GUI selector";
  dot.setAttribute("aria-label", `Pomoc: ${text}`);
  dot.hidden = !uiState.helpEnabled;
  dot.style.display = uiState.helpEnabled ? "inline-flex" : "none";
  dot.tabIndex = uiState.helpEnabled ? 0 : -1;
  dot.setAttribute("aria-hidden", uiState.helpEnabled ? "false" : "true");
  if (uiState.helpEnabled) {
    dot.setAttribute("title", `${text}\n\nKod/zmienna: ${dot.dataset.ref}`);
  } else {
    dot.removeAttribute("title");
  }

  if (node.dataset.helpBound !== "1") {
    node.dataset.helpBound = "1";
    node.addEventListener("mouseenter", () => {
      const activeDot = Array.from(node.children).find((child) => child.classList.contains("help-dot"));
      showHelpTooltip(activeDot);
    });
    node.addEventListener("mouseleave", scheduleHideHelpTooltip);
  }
}

function attachHelpBySelector(selector, text, ref = "") {
  document.querySelectorAll(selector).forEach((target) => {
    if (!target.classList.contains("help-dot") && !target.closest(".help-dot")) {
      attachHelp(target, text, ref || SELECTOR_HELP_REFS[selector] || `GUI selector: ${selector}`);
    }
  });
}

function cleanupHelpDots() {
  document.querySelectorAll(".help-dot .help-dot").forEach((dot) => dot.remove());
  document.querySelectorAll(".help-target").forEach((node) => {
    const dots = Array.from(node.children).filter((child) => child.classList.contains("help-dot"));
    dots.slice(1).forEach((dot) => dot.remove());
  });
}

function attachHelpTriggers() {
  cleanupHelpDots();
  HELP_TEXTS.forEach(([selector, text]) => {
    attachHelpBySelector(selector, text, SELECTOR_HELP_REFS[selector] || `GUI selector: ${selector}`);
  });

  Object.entries(FIELD_HELP_TEXTS).forEach(([id, text]) => {
    const field = $(id);
    if (field) attachHelp(field, text, FIELD_HELP_REFS[id] || `input#${id}`);
  });

  Object.entries(SWITCH_HELP_TEXTS).forEach(([id, text]) => {
    const input = $(id);
    const target = input?.closest(".switch-row")?.querySelector("strong") || input;
    attachHelp(target, text, SWITCH_HELP_REFS[id] || `switch#${id}`);
  });

  Object.entries(BUTTON_HELP_TEXTS).forEach(([id, text]) => {
    const button = $(id);
    if (button) attachHelp(button, text, BUTTON_HELP_REFS[id] || `button#${id}`);
  });

  CHECK_HELP_TEXTS.forEach(([selector, text]) => {
    document.querySelectorAll(selector).forEach((input) => {
      attachHelp(input.closest("label"), text, CHECK_HELP_REFS[selector] || `checklist ${selector}`);
    });
  });

  document.querySelectorAll("[data-help-text]").forEach((target) => {
    attachHelp(target, target.dataset.helpText, target.dataset.helpRef || "dynamic GUI data");
  });

  cleanupHelpDots();
}

function setHelpEnabled(enabled) {
  uiState.helpEnabled = Boolean(enabled);
  document.body.classList.toggle("help-enabled", uiState.helpEnabled);
  document.querySelectorAll(".help-dot").forEach((dot) => {
    dot.hidden = !uiState.helpEnabled;
    dot.style.display = uiState.helpEnabled ? "inline-flex" : "none";
    dot.tabIndex = uiState.helpEnabled ? 0 : -1;
    dot.setAttribute("aria-hidden", uiState.helpEnabled ? "false" : "true");
    if (uiState.helpEnabled && dot.dataset.tooltip) {
      dot.setAttribute("title", `${dot.dataset.tooltip}\n\nKod/zmienna: ${dot.dataset.ref || "GUI help metadata"}`);
    } else {
      dot.removeAttribute("title");
    }
  });

  const button = $("btn-help-toggle");
  if (!button) return;
  button.textContent = uiState.helpEnabled ? "Help ON" : "Help OFF";
  button.classList.toggle("active", uiState.helpEnabled);
  button.setAttribute("aria-pressed", String(uiState.helpEnabled));
  if (!uiState.helpEnabled) hideHelpTooltip();
}

function setTheme(theme) {
  uiState.theme = theme === "light" ? "light" : "dark";
  document.body.classList.toggle("theme-light", uiState.theme === "light");

  const button = $("btn-theme-toggle");
  if (button) {
    button.textContent = uiState.theme === "light" ? "Motyw: jasny" : "Motyw: ciemny";
    button.classList.toggle("active", uiState.theme === "light");
  }

  try {
    window.localStorage.setItem("mikrotom-theme", uiState.theme);
  } catch {
    // Local storage is optional; the UI still works without it.
  }
}

function loadInitialTheme() {
  try {
    return window.localStorage.getItem("mikrotom-theme") || "dark";
  } catch {
    return "dark";
  }
}

function bindHelpTooltipGuards() {
  document.addEventListener("mousemove", (event) => {
    if (!uiState.helpTooltip?.classList.contains("visible")) return;
    if (event.target.closest?.("#help-tooltip") || event.target.closest?.(".help-dot")) return;
    if (!document.body.contains(uiState.activeHelpTrigger)) scheduleHideHelpTooltip();
  });

  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") hideHelpTooltip();
  });

  window.addEventListener("resize", hideHelpTooltip);
}

async function readJson(response) {
  return await response.json().catch(() => ({}));
}

function extractErrorMessage(body, status) {
  return body.error || body.detail || `HTTP_${status}`;
}

async function apiGet(path) {
  const response = await fetch(`${cfg.apiBaseUrl}${path}`);
  const body = await readJson(response);
  if (!response.ok) throw new Error(extractErrorMessage(body, response.status));
  return body;
}

async function apiPost(path, payload) {
  const response = await fetch(`${cfg.apiBaseUrl}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload || {})
  });
  const body = await readJson(response);
  if (!response.ok) throw new Error(extractErrorMessage(body, response.status));
  return body;
}

async function executeCommand(type, payload = {}) {
  let result;

  if (cfg.demoMode) {
    result = window.demoHandleCommand(type, payload);
  } else {
    switch (type) {
      case "enable":
        result = await apiPost("/api/cmd/enable", {});
        break;
      case "disable":
        result = await apiPost("/api/cmd/disable", {});
        break;
      case "ackFault":
        result = await apiPost("/api/cmd/ack-fault", {});
        break;
      case "home":
        result = await apiPost("/api/cmd/home", {});
        break;
      case "stop":
        result = await apiPost("/api/cmd/stop", {});
        break;
      case "qstop":
        result = await apiPost("/api/cmd/qstop", {});
        break;
      case "moveRel":
        result = await apiPost("/api/cmd/move-rel", payload);
        break;
      case "firstMoveRel":
        result = await apiPost("/api/cmd/first-move-rel", payload);
        break;
      case "moveAbs":
        result = await apiPost("/api/cmd/move-abs", payload);
        break;
      case "raw":
        result = await apiPost("/api/cmd/raw", payload);
        break;
      case "params":
        result = await apiPost("/api/params", payload);
        break;
      default:
        throw new Error("UNKNOWN_COMMAND");
    }
  }

  if (result && result.ok === false) {
    throw new Error(result.error || "COMMAND_REJECTED");
  }

  await refreshData();
  return result;
}

async function refreshData() {
  try {
    if (cfg.demoMode) {
      window.demoTick();
      uiState.status = JSON.parse(JSON.stringify(window.DEMO_STATE.status));
      uiState.telemetry = JSON.parse(JSON.stringify(window.DEMO_STATE.telemetry));
      uiState.events = JSON.parse(JSON.stringify(window.DEMO_STATE.events));
    } else {
      const [status, telemetry, events] = await Promise.all([
        apiGet("/api/status"),
        apiGet("/api/telemetry/latest"),
        apiGet("/api/events/recent")
      ]);
      uiState.status = status;
      uiState.telemetry = telemetry;
      uiState.events = events || [];
    }

    uiState.connected = true;
    uiState.lastError = "";

    if (uiState.telemetry) {
      uiState.history.push({
        pos: Number(uiState.telemetry.pos_um || 0),
        target: Number(uiState.telemetry.pos_set_um || 0)
      });
      uiState.history = uiState.history.slice(-100);
    }
  } catch (error) {
    uiState.connected = false;
    uiState.lastError = error.message || "UNKNOWN_ERROR";
  }

  renderAll();
}

function demoDiagnostics() {
  return {
    transport: {
      port: "DEMO",
    baudrate: 115200,
      connected: true,
      heartbeat_interval_s: 0.35,
      event_cache_depth: window.DEMO_STATE.events.length
    },
    firmware_version: {
      name: "Mikrotom STM Demo",
      version: "demo",
      build_date: "local",
      git_hash: "demo"
    },
    log_store: {
      enabled: false,
      path: "Tryb DEMO - SQLite nieużywany",
      protocol_rows: 0,
      telemetry_rows: 0,
      event_rows: window.DEMO_STATE.events.length
    }
  };
}

async function refreshDiagnostics(showToastOnSuccess = false) {
  try {
    uiState.diagnostics = cfg.demoMode ? demoDiagnostics() : await apiGet("/api/debug/vars");
    uiState.diagnosticsError = "";
    if (showToastOnSuccess) showToast("Diagnostyka odświeżona", "ok");
  } catch (error) {
    uiState.diagnosticsError = error.message || "DIAGNOSTICS_UNAVAILABLE";
    if (showToastOnSuccess) showToast(translateError(uiState.diagnosticsError), "err");
  }

  renderDiagnostics();
}

function stateTone(axisState) {
  switch (String(axisState || "UNKNOWN")) {
    case "FAULT":
    case "ESTOP":
      return "err";
    case "SAFE":
    case "CONFIG":
    case "CALIBRATION":
    case "STOPPING":
    case "ARMED":
      return "warn";
    case "READY":
    case "MOTION":
      return "ok";
    default:
      return "muted";
  }
}

function formatBool(value) {
  return Number(value || 0) === 1 ? "TAK" : "NIE";
}

function formatEventTime(tsMs) {
  const value = Math.max(0, Number(tsMs || 0));
  if (value < 1000) return `${value} ms`;
  if (value < 60000) return `${(value / 1000).toFixed(2)} s`;
  return `${(value / 60000).toFixed(1)} min`;
}

function explainRunAllowed(status) {
  const blockers = [];
  const calibrationRequired = (
    Number(status.calib_valid || 0) !== 1 &&
    Number(status.allow_motion_without_calibration || 0) !== 1
  );

  if (Number(status.motion_implemented ?? 1) !== 1) blockers.push("ta wersja ma nadal wyłączony ruch");
  if (Number(status.vbus_valid || 0) !== 1) blockers.push("VBUS nie został jeszcze poprawnie zmierzony");
  if (Number(status.config_loaded || 0) !== 1) blockers.push("konfiguracja nie została załadowana z flash");
  if (Number(status.fault_mask || 0) !== 0) blockers.push("aktywny błąd");
  if (Number(status.motion_implemented ?? 1) === 1 && Number(status.homing_successful || 0) !== 1) blockers.push("homing nie został jeszcze zakończony");
  if (String(status.axis_state || "") === "CONFIG") blockers.push("oś czeka na konfigurację albo kalibrację");
  if (Number(status.commissioning_stage || 0) !== 3) blockers.push("aktywny etap nie jest etapem 3");
  if (Number(status.safe_mode || 0) !== 0) blockers.push("aktywny tryb bezpieczny");
  if (Number(status.enabled || 0) !== 1) blockers.push("napęd nie jest włączony");
  if (calibrationRequired) blockers.push("wymagana jest kalibracja");

  if (Number(status.run_allowed || 0) === 1 && Number(status.motion_implemented ?? 1) === 1) {
    return "Ruch jest dopuszczony. Używaj zachowawczych przemieszczeń i trzymaj STOP/QSTOP w gotowości.";
  }

  return `Ruch jest zablokowany, ponieważ: ${blockers.join(", ") || "backend nie jest gotowy"}.`;
}

function renderStatusDetails(status) {
  const vbusValid = Number(status.vbus_valid || 0) === 1;
  const items = [
    ["Oś włączona", formatBool(status.enabled)],
    ["Konfiguracja z flash załadowana", formatBool(status.config_loaded)],
    ["Ruch zaimplementowany", formatBool(status.motion_implemented)],
    ["Wersja bezpiecznej integracji", formatBool(status.safe_integration)],
    ["Etap uruchomienia", status.commissioning_stage ?? "--"],
    ["Tryb bezpieczny", Number(status.safe_mode || 0) ? "WŁ." : "WYŁ."],
    ["Tylko uzbrajanie", Number(status.arming_only || 0) ? "WŁ." : "WYŁ."],
    ["Ruch kontrolowany", Number(status.controlled_motion || 0) ? "WŁ." : "WYŁ."],
    ["Ruch bez kalibracji dozwolony", formatBool(status.allow_motion_without_calibration)],
    ["Homing rozpoczęty", formatBool(status.homing_started)],
    ["Homing w toku", formatBool(status.homing_ongoing)],
    ["Homing zakończony", formatBool(status.homing_successful)],
    ["Pierwszy test ruchu aktywny", formatBool(status.first_move_test_active)],
    ["Maksymalny pierwszy ruch", `${status.first_move_max_delta_um ?? 100} um`],
    ["Telemetria UART włączona", formatBool(status.telemetry_enabled ?? 1)],
    ["Kalibracja ważna", formatBool(status.calib_valid)],
    ["VBUS", vbusValid ? `${fmt(status.vbus_V, 2)} V` : "brak próbki"],
    ["VBUS poprawny", formatBool(status.vbus_valid)],
    ["PTC zainstalowany", formatBool(status.ptc_installed)],
    ["Zasilanie zapasowe zainstalowane", formatBool(status.backup_supply_installed)],
    ["Maska błędów", status.fault_mask ?? "--"]
  ];

  $("status-list").innerHTML = items.map(([label, value]) => `
    <div class="status-row">
      <span data-help-text="${escapeHtml(STATUS_HELP_TEXTS[label] || `Status firmware: ${label}.`)}" data-help-ref="${escapeHtml(STATUS_HELP_REFS[label] || `status.${label}`)}">${label}</span>
      <strong>${value}</strong>
    </div>
  `).join("");
}

function setWizardStep(id, ok, text) {
  const card = $(id);
  card.classList.toggle("ok", ok);
  card.classList.toggle("pending", !ok);
  const value = card.querySelector("b");
  if (value) value.textContent = text;
}

function wizardFirstMoveReady(status) {
  const delta = Number($("first-move-rel")?.value || 0);
  const calibrationOk = (
    Number(status.calib_valid || 0) === 1 ||
    Number(status.allow_motion_without_calibration || 0) === 1
  );
  return (
    uiState.connected &&
    Number(status.motion_implemented || 0) === 1 &&
    Number(status.safe_integration || 0) === 0 &&
    Number(status.vbus_valid || 0) === 1 &&
    Number(status.fault_mask || 0) === 0 &&
    Number(status.safe_mode || 0) === 0 &&
    Number(status.arming_only || 0) === 1 &&
    Number(status.controlled_motion || 0) === 0 &&
    Number(status.enabled || 0) === 1 &&
    Number(status.homing_ongoing || 0) === 0 &&
    calibrationOk &&
    Math.abs(delta) > 0 &&
    Math.abs(delta) <= Number(status.first_move_max_delta_um || 100)
  );
}

function wizardBlockers(status) {
  const blockers = [];
  const delta = Number($("first-move-rel")?.value || 0);

  if (!uiState.connected) blockers.push("brak połączenia z Agentem/MCU");
  if (Number(status.motion_implemented || 0) !== 1) blockers.push("aktywny jest wsad SAFE bez ruchu");
  if (Number(status.safe_integration || 0) !== 0) blockers.push("firmware jest w trybie bezpiecznej integracji");
  if (Number(status.vbus_valid || 0) !== 1) blockers.push("VBUS nie jest poprawny");
  if (Number(status.fault_mask || 0) !== 0) blockers.push("aktywny błąd firmware");
  if (Number(status.arming_only || 0) !== 1) blockers.push("nie ustawiono Etapu 2 / ARMING_ONLY");
  if (Number(status.controlled_motion || 0) !== 0) blockers.push("CONTROLLED_MOTION ma być wyłączony dla pierwszego testu");
  if (Number(status.enabled || 0) !== 1) blockers.push("brak ENABLE");
  if (Number(status.calib_valid || 0) !== 1 && Number(status.allow_motion_without_calibration || 0) !== 1) blockers.push("brak kalibracji albo świadomego pozwolenia na ruch bez kalibracji");
  if (Math.abs(delta) === 0 || Math.abs(delta) > Number(status.first_move_max_delta_um || 100)) blockers.push("wartość FIRST_MOVE_REL musi mieścić się w zakresie +/-100 um");

  return blockers;
}

function renderWizard(status) {
  const motion = Number(status.motion_implemented || 0) === 1;
  const safe = Number(status.safe_integration || 0) === 1;
  const faultClear = Number(status.fault_mask || 0) === 0 && String(status.fault || "") === "NONE";
  const powerOk = Number(status.vbus_valid || 0) === 1;
  const arming = Number(status.arming_only || 0) === 1 && Number(status.controlled_motion || 0) === 0;
  const calibrationOk = Number(status.calib_valid || 0) === 1 || Number(status.allow_motion_without_calibration || 0) === 1;
  const enabled = Number(status.enabled || 0) === 1;
  const firstMoveReady = wizardFirstMoveReady(status);
  const blockers = wizardBlockers(status);

  setWizardStep("wizard-step-usb", uiState.connected, uiState.connected ? "OK" : "BRAK");
  setWizardStep("wizard-step-uart", uiState.connected, uiState.connected ? "OK" : "BRAK");
  setWizardStep("wizard-step-power", powerOk, powerOk ? `${fmt(status.vbus_V, 2)} V` : "CZEKA");
  setWizardStep("wizard-step-fault", faultClear, faultClear ? "NONE" : String(status.fault || "--"));
  setWizardStep("wizard-step-arming", arming, arming ? "ETAP 2" : `ETAP ${status.commissioning_stage ?? "--"}`);
  setWizardStep("wizard-step-calib", calibrationOk, calibrationOk ? "OK" : "CZEKA");
  setWizardStep("wizard-step-enable", enabled, enabled ? "ENABLE" : "OFF");
  setWizardStep("wizard-step-first-move", firstMoveReady, firstMoveReady ? "GOTOWY" : motion && !safe ? "CZEKA" : "SAFE");

  $("wizard-summary").textContent = safe
    ? "MCU jest w bezpiecznym wsadzie SAFE. Kreator może sprawdzić komunikację i zasilanie, ale nie wykona testu ruchu."
    : "MCU jest w buildzie MOTION. Pierwszy test używa FIRST_MOVE_REL w Etapie 2, bez pełnego HOME.";

  $("wizard-blockers").textContent = blockers.length
    ? `Blokady pierwszego testu: ${blockers.join(", ")}.`
    : "Warunki pierwszego testu są spełnione. Operator musi fizycznie nadzorować oś i zasilacz.";

  $("btn-wizard-ack").disabled = !uiState.connected || faultClear || !powerOk;
  $("btn-wizard-stage1").disabled = !uiState.connected;
  $("btn-wizard-stage2").disabled = !uiState.connected || !faultClear || !powerOk;
  $("btn-wizard-calib-zero").disabled = !uiState.connected || !faultClear || !powerOk || !arming;
  $("btn-wizard-enable").disabled = !uiState.connected || !faultClear || !arming || !powerOk;
  $("btn-first-move-rel").disabled = !firstMoveReady;
  $("btn-wizard-disable").disabled = !uiState.connected;
  $("btn-wizard-qstop").disabled = !uiState.connected;
}

function renderEvents() {
  const body = $("events-body");
  body.innerHTML = "";

  (uiState.events || []).slice(0, 24).forEach((event) => {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${formatEventTime(event.ts_ms)}</td>
      <td>${event.code || event.type || "--"}</td>
      <td>${event.value ?? event.details ?? "--"}</td>
    `;
    body.appendChild(row);
  });
}

function renderApiMap() {
  $("api-base").textContent = `API ${cfg.apiBaseUrl}`;
  $("api-endpoints").innerHTML = API_ENDPOINTS.map(([method, path, note]) => `
    <div class="api-row">
      <span class="api-method">${method}</span>
      <code>${escapeHtml(path)}</code>
      <p data-help-text="${escapeHtml(`${method} ${path}: ${note}. Ten endpoint obsługuje Agent po stronie PC.`)}" data-help-ref="${escapeHtml(`${method} ${path} / ${API_HELP_REFS[path] || "Agent API"}`)}">${escapeHtml(note)}</p>
    </div>
  `).join("");
}

function renderDiagnostics() {
  const diagnostics = uiState.diagnostics || {};
  const transport = diagnostics.transport || {};
  const logStore = diagnostics.log_store || {};
  const port = transport.port || (cfg.demoMode ? "DEMO" : "ustaw w środowisku Agenta");
  const baudrate = transport.baudrate || 115200;
  const connected = transport.connected ?? uiState.connected;
  const sqlEnabled = logStore.enabled;
  const protocolRows = logStore.protocol_rows ?? "--";
  const telemetryRows = logStore.telemetry_rows ?? "--";
  const eventRows = logStore.event_rows ?? "--";

  $("uart-port").textContent = port;
  $("uart-baud").textContent = baudrate;
  $("uart-connected").textContent = connected ? "POŁĄCZONO" : "BRAK POŁĄCZENIA";
  $("sql-enabled").textContent = sqlEnabled === undefined ? "--" : sqlEnabled ? "TAK" : "NIE";
  $("sql-path").textContent = logStore.path || "agent/mikrotom_agent.sqlite3";
  $("sql-counts").textContent = `${protocolRows} / ${telemetryRows} / ${eventRows}`;

  if (uiState.diagnosticsError) {
    $("uart-connected").textContent = `BŁĄD: ${translateError(uiState.diagnosticsError)}`;
  }
}

function clamp01(value) {
  return Math.max(0, Math.min(1, value));
}

function mapAxisX(value, min, max, width) {
  if (max <= min) return width / 2;
  const ratio = clamp01((value - min) / (max - min));
  return 78 + ratio * (width - 156);
}

function renderAxis(status, telemetry) {
  const svg = $("axis-svg");
  const styles = getComputedStyle(document.body);
  const axisPlate = styles.getPropertyValue("--axis-plate").trim() || "#08111f";
  const axisText = styles.getPropertyValue("--axis-text").trim() || "#f4f7fb";
  const axisLimitText = styles.getPropertyValue("--axis-limit-text").trim() || "#f6d991";
  const axisTargetText = styles.getPropertyValue("--axis-target-text").trim() || "#71e0a4";
  const width = 1040;
  const height = 220;
  const min = Number(status.soft_min_pos ?? -10000);
  const max = Number(status.soft_max_pos ?? 10000);
  const pos = Number(telemetry.pos_um ?? status.position_um ?? 0);
  const target = Number(telemetry.pos_set_um ?? status.position_set_um ?? 0);
  const runAllowed = Number(status.run_allowed || 0) === 1;
  const fault = String(status.fault || "NONE");
  const color = fault !== "NONE" ? "#ff5e5b" : runAllowed ? "#5b8cff" : "#f0b54a";
  const history = uiState.history.map((point, index) => {
    const x = mapAxisX(point.pos, min, max, width);
    const y = 150 - Math.min(72, index * 0.55);
    return `${x},${y}`;
  }).join(" ");

  const xMin = mapAxisX(min, min, max, width);
  const xMax = mapAxisX(max, min, max, width);
  const xPos = mapAxisX(pos, min, max, width);
  const xTarget = mapAxisX(target, min, max, width);

  svg.innerHTML = `
    <defs>
      <linearGradient id="railGradient" x1="0%" y1="0%" x2="100%" y2="0%">
        <stop offset="0%" stop-color="#f0b54a" />
        <stop offset="100%" stop-color="#ffd17a" />
      </linearGradient>
    </defs>
    <rect x="0" y="0" width="${width}" height="${height}" rx="22" fill="${axisPlate}" />
    <circle cx="${xPos}" cy="112" r="18" fill="${color}" />
    <line x1="${xMin}" y1="112" x2="${xMax}" y2="112" stroke="url(#railGradient)" stroke-width="10" stroke-linecap="round" />
    <line x1="78" y1="112" x2="${xMin}" y2="112" stroke="rgba(255,94,91,0.35)" stroke-width="4" stroke-dasharray="8 7" />
    <line x1="${xMax}" y1="112" x2="${width - 78}" y2="112" stroke="rgba(255,94,91,0.35)" stroke-width="4" stroke-dasharray="8 7" />
    <line x1="${xTarget}" y1="46" x2="${xTarget}" y2="174" stroke="#29c272" stroke-width="4" stroke-dasharray="8 6" />
    ${history ? `<polyline points="${history}" fill="none" stroke="rgba(91,140,255,0.35)" stroke-width="2" />` : ""}
    <text x="${xMin}" y="34" text-anchor="middle" fill="${axisLimitText}" font-size="14">MIN ${fmt(min)} um</text>
    <text x="${xMax}" y="34" text-anchor="middle" fill="${axisLimitText}" font-size="14">MAX ${fmt(max)} um</text>
    <text x="${xPos}" y="194" text-anchor="middle" fill="${axisText}" font-size="14">POZ ${fmt(pos)} um</text>
    <text x="${xTarget}" y="62" text-anchor="middle" fill="${axisTargetText}" font-size="14">CEL ${fmt(target)} um</text>
  `;
}

function renderAll() {
  const status = uiState.status || {};
  const telemetry = uiState.telemetry || {};
  const axisState = status.axis_state || status.state || "UNKNOWN";
  const fault = status.fault || "UNKNOWN";
  const runAllowed = Number(status.run_allowed || 0);
  const stage = Number(status.commissioning_stage || 1);
  const motionImplemented = Number(status.motion_implemented ?? 1);
  const homingOngoing = Number(status.homing_ongoing || 0) === 1;
  const vbusValid = Number(status.vbus_valid || 0) === 1;
  const liveMotionReady = runAllowed === 1 && motionImplemented === 1 && vbusValid;

  setBadge("badge-connection", uiState.connected ? "Połączenie OK" : "Brak połączenia", uiState.connected ? "ok" : "err");
  setBadge("badge-state", `Oś ${axisState}`, stateTone(axisState));
  setBadge("badge-fault", `Błąd ${fault}`, fault === "NONE" ? "ok" : "err");
  setBadge("badge-run", `RUN_ALLOWED ${runAllowed}`, liveMotionReady ? "ok" : "warn");
  setBadge("badge-stage", `Etap ${stage}`, stage === 3 ? "ok" : stage === 2 ? "warn" : "muted");

  $("kpi-pos").textContent = `${fmt(telemetry.pos_um ?? status.position_um ?? 0)} um`;
  $("kpi-pos-set").textContent = `${fmt(telemetry.pos_set_um ?? status.position_set_um ?? 0)} um`;
  $("kpi-vel").textContent = `${fmt(telemetry.vel_um_s ?? 0)} um/s`;
  $("kpi-iq").textContent = `${fmt(telemetry.iq_meas_mA ?? 0)} mA`;
  $("kpi-vbus").textContent = Number(status.vbus_valid || 0) === 1 ? `${fmt(status.vbus_V, 2)} V` : "CZEKA";

  $("cfg-config-loaded").checked = Number(status.config_loaded || 0) === 1;
  $("cfg-enabled").checked = Number(status.enabled || 0) === 1;
  $("cfg-brake-installed").checked = Number(status.brake_installed || 0) === 1;
  $("cfg-collision-installed").checked = Number(status.collision_sensor_installed || 0) === 1;
  $("cfg-external-interlock-installed").checked = Number(status.external_interlock_installed || 0) === 1;
  $("cfg-ignore-brake").checked = Number(status.ignore_brake_feedback || 0) === 1;
  $("cfg-ignore-collision").checked = Number(status.ignore_collision_sensor || 0) === 1;
  $("cfg-ignore-interlock").checked = Number(status.ignore_external_interlock || 0) === 1;
  $("cfg-allow-no-calib").checked = Number(status.allow_motion_without_calibration || 0) === 1;
  $("cfg-calib-valid").checked = Number(status.calib_valid || 0) === 1;
  $("cfg-max-current").value = status.max_current ?? 0.2;
  $("cfg-max-current-peak").value = status.max_current_peak ?? 0.3;
  $("cfg-max-velocity").value = status.max_velocity ?? 0.005;
  $("cfg-max-acceleration").value = status.max_acceleration ?? 0.02;
  $("cfg-soft-min").value = status.soft_min_pos ?? -10000;
  $("cfg-soft-max").value = status.soft_max_pos ?? 10000;

  $("move-warning").textContent = explainRunAllowed(status);
  $("connection-detail").textContent = uiState.connected
    ? `Źródło: ${cfg.demoMode ? "model danych DEMO" : cfg.apiBaseUrl} | wersja ${motionImplemented ? "z ruchem" : "bezpieczna integracja"}`
    : `Źródło niedostępne: ${translateError(uiState.lastError || "UNKNOWN_ERROR")}`;

  $("btn-move-rel").disabled = !liveMotionReady;
  $("btn-move-abs").disabled = !liveMotionReady;
  $("btn-home").disabled = !uiState.connected || motionImplemented !== 1 || homingOngoing;
  $("btn-ack-fault").disabled = Number(status.fault_mask || 0) === 0;

  ["stage-1-card", "stage-2-card", "stage-3-card"].forEach((id, index) => {
    $(id).classList.toggle("active", stage === index + 1);
  });

  renderStatusDetails(status);
  renderAxis(status, telemetry);
  renderEvents();
  renderDiagnostics();
  renderApiMap();
  renderWizard(status);

  $("footer-left").textContent = `API ${cfg.apiBaseUrl}`;
  $("footer-right").textContent = `Tryb ${cfg.demoMode ? "DEMO" : "LIVE"} | ${motionImplemented ? "RUCH" : "BEZPIECZNA-INTEGRACJA"}`;
  attachHelpTriggers();
}

function stageChecksSatisfied(stage) {
  const boxes = [...document.querySelectorAll(`.stage${stage}-check`)];
  return boxes.length > 0 && boxes.every((box) => box.checked);
}

async function runAction(action, payload, successMessage) {
  try {
    await executeCommand(action, payload);
    showToast(successMessage, "ok");
  } catch (error) {
    showToast(translateError(error.message || "COMMAND_REJECTED"), "err");
  }
}

function bindActions() {
  $("btn-theme-toggle").addEventListener("click", () => {
    setTheme(uiState.theme === "light" ? "dark" : "light");
  });

  $("btn-help-toggle").addEventListener("click", () => {
    setHelpEnabled(!uiState.helpEnabled);
  });

  $("btn-enable").addEventListener("click", () => runAction("enable", {}, "Wysłano ENABLE"));
  $("btn-disable").addEventListener("click", () => runAction("disable", {}, "Wysłano DISABLE"));
  $("btn-ack-fault").addEventListener("click", () => runAction("ackFault", {}, "Wysłano ACK_FAULT"));
  $("btn-home").addEventListener("click", () => runAction("home", {}, "Wysłano HOME"));
  $("btn-stop").addEventListener("click", () => runAction("stop", {}, "Wysłano STOP"));
  $("btn-qstop").addEventListener("click", () => runAction("qstop", {}, "Wysłano QSTOP"));

  $("btn-move-rel").addEventListener("click", () => runAction("moveRel", {
    delta_um: Number($("move-rel").value || 0)
  }, "Wysłano MOVE_REL"));

  $("btn-first-move-rel").addEventListener("click", () => runAction("firstMoveRel", {
    delta_um: Number($("first-move-rel").value || 0)
  }, "Wysłano FIRST_MOVE_REL"));

  $("btn-move-abs").addEventListener("click", () => runAction("moveAbs", {
    target_um: Number($("move-abs").value || 0)
  }, "Wysłano MOVE_ABS"));

  $("btn-calib-zero").addEventListener("click", () => runAction("raw", {
    command: "CMD CALIB_ZERO"
  }, "Wysłano CALIB_ZERO"));

  $("btn-send-raw").addEventListener("click", () => runAction("raw", {
    command: $("raw-command").value
  }, "Wysłano komendę RAW"));

  $("btn-refresh").addEventListener("click", async () => {
    await refreshData();
    showToast("Dane odświeżone", "info");
  });

  $("btn-wizard-refresh").addEventListener("click", async () => {
    await refreshData();
    showToast("Status kreatora odświeżony", "info");
  });

  $("btn-wizard-ack").addEventListener("click", () => runAction("ackFault", {}, "Wysłano ACK_FAULT"));
  $("btn-wizard-stage1").addEventListener("click", () => runAction("params", {
    commissioning_stage: 1,
    safe_mode: 1,
    arming_only: 0,
    controlled_motion: 0
  }, "Ustawiono SAFE"));
  $("btn-wizard-stage2").addEventListener("click", () => runAction("params", {
    commissioning_stage: 2,
    safe_mode: 0,
    arming_only: 1,
    controlled_motion: 0
  }, "Ustawiono Etap 2"));
  $("btn-wizard-calib-zero").addEventListener("click", () => runAction("raw", {
    command: "CMD CALIB_ZERO"
  }, "Wysłano CALIB_ZERO"));
  $("btn-wizard-enable").addEventListener("click", () => runAction("enable", {}, "Wysłano ENABLE"));
  $("btn-wizard-disable").addEventListener("click", () => runAction("disable", {}, "Wysłano DISABLE"));
  $("btn-wizard-qstop").addEventListener("click", () => runAction("qstop", {}, "Wysłano QSTOP"));

  $("btn-stage-1").addEventListener("click", async () => {
    if (!stageChecksSatisfied(1)) {
      showToast("Najpierw uzupełnij checklistę etapu 1", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 1 }, "Aktywowano etap 1");
  });

  $("btn-stage-2").addEventListener("click", async () => {
    if (!stageChecksSatisfied(2)) {
      showToast("Najpierw uzupełnij checklistę etapu 2", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 2 }, "Aktywowano etap 2");
  });

  $("btn-stage-3").addEventListener("click", async () => {
    if (!stageChecksSatisfied(3)) {
      showToast("Najpierw uzupełnij checklistę etapu 3", "warn");
      return;
    }
    await runAction("params", { commissioning_stage: 3 }, "Aktywowano etap 3");
  });

  $("btn-save-safety").addEventListener("click", async () => {
    await runAction("params", {
      max_current: Number($("cfg-max-current").value || 0.2),
      max_current_peak: Number($("cfg-max-current-peak").value || 0.3),
      max_velocity: Number($("cfg-max-velocity").value || 0.005),
      max_acceleration: Number($("cfg-max-acceleration").value || 0.02),
      soft_min_pos: Number($("cfg-soft-min").value || -10000),
      soft_max_pos: Number($("cfg-soft-max").value || 10000)
    }, "Wysłano parametry globalne");
  });

  $("btn-save-safety-overrides").addEventListener("click", async () => {
    await runAction("params", {
      brake_installed: $("cfg-brake-installed").checked ? 1 : 0,
      collision_sensor_installed: $("cfg-collision-installed").checked ? 1 : 0,
      external_interlock_installed: $("cfg-external-interlock-installed").checked ? 1 : 0,
      ignore_brake_feedback: $("cfg-ignore-brake").checked ? 1 : 0,
      ignore_collision_sensor: $("cfg-ignore-collision").checked ? 1 : 0,
      ignore_external_interlock: $("cfg-ignore-interlock").checked ? 1 : 0,
      allow_motion_without_calibration: $("cfg-allow-no-calib").checked ? 1 : 0
    }, "Wysłano konfigurację bezpieczeństwa");
  });

  $("btn-refresh-diagnostics").addEventListener("click", async () => {
    await refreshDiagnostics(true);
  });
}

async function bootstrap() {
  bindActions();
  bindHelpTooltipGuards();
  setTheme(loadInitialTheme());
  setHelpEnabled(false);
  await refreshData();
  await refreshDiagnostics();
  setInterval(refreshData, cfg.refreshMs || 300);
}

bootstrap();
