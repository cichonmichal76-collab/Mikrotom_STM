# Future / Backlog - branch DZIALA

Ten plik zbiera pomysly, ktore zostaly omowione, ale nie sa jeszcze realizowane.

## DIAG-2: automatyczny monitoring jakosci pracy z SQL

Status: niezrealizowane / odlozone.

Cel:

Zbudowac narzedzie po stronie PC, ktore automatycznie ocenia, czy nowa sesja telemetryczna miesci sie w zachowaniu referencyjnym starego dzialajacego wsadu.

Zakres pomyslu:

- porownanie nowej sesji z baseline,
- wykrywanie spadku czestotliwosci ramek UART,
- wykrywanie przerw w UART,
- wykrywanie skokow pozycji z enkodera,
- wykrywanie nietypowych pikow pradu fazowego,
- wykrywanie sytuacji: duze `iq_ref`, ale brak zmiany pozycji,
- raport koncowy `PASS`, `WARN` albo `FAIL`,
- zapis raportu do pliku Markdown.

Powod odlozenia:

Na tym etapie nie rozwijamy juz warstwy SQL. SQL spelnil swoje zadanie: potwierdzil baseline starego wsadu, powtarzalnosc ruchu oraz poprawne dzialanie ramki diagnostycznej `D;...`.

Warunek powrotu do tematu:

Wrocic do `DIAG-2`, gdy zaczniemy wprowadzac wiecej zmian w firmware i bedziemy potrzebowali automatycznego testu regresji ruchu.
