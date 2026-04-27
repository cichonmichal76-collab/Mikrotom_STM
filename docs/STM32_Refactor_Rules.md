# Zasady refaktoru STM32

Ten refaktor porządkuje kod pod dalszy rozwój sterowania i przyszłą kompensację,
bez zmiany obecnego zachowania osi.

## Zasady

1. Safety i state machine są nadrzędne wobec kompensacji.
2. Kompensacja nie zastępuje regulatorów, tylko działa jako korekta setpointu
   albo feedforward.
3. Setpoint, feedback i compensation muszą być rozdzielone na osobne moduły.
4. ISR pozostaje lekkie: bez alokacji, bez blokowania, bez logiki konfiguracyjnej.
5. Refaktor najpierw usuwa duplikację i wyznacza punkty wpięcia, a dopiero potem
   wprowadza nowy algorytm.
6. Każda przyszła kompensacja musi mieć bezpieczny tryb `no-op`, który nie zmienia
   zachowania układu po wyłączeniu.
7. Kompensacja nie może obchodzić ograniczeń `RUN_ALLOWED`, faultów ani soft limitów.

## Wdrożony etap

Aktualny etap dodaje moduł `compensation` jako warstwę przygotowawczą:

- korekta targetu pozycji przed startem trajektorii
- korekta referencji prędkości w pętli pozycji
- korekta referencji `iq` w pętli prędkości/prądu

Na ten moment wszystkie hooki są neutralne (`no-op`) i zwracają wartości wejściowe
bez modyfikacji.
