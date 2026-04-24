# STM32 Bring-up Checklist

Ta checklista jest praktyczna sekwencja przejscia:

```text
SAFE -> walidacja komunikacji -> walidacja safety -> MOTION -> HOME -> pierwszy maly ruch
```

## 1. Przed jakimkolwiek ruchem

- mechanika nie moze stac na skraju zakresu,
- operator musi miec pod reka STOP albo odciecie zasilania,
- pierwszy test ma byc z konserwatywnymi limitami,
- USB-UART musi byc stabilnie podpiety,
- trzeba znac wlasciwy port COM,
- trzeba znac, jaki wariant firmware jest w MCU.

## 2. Wariant `SAFE` - obowiazkowy pierwszy krok

Potwierdz:

- `GET SAFE_INTEGRATION` zwraca `1`,
- `GET MOTION_IMPLEMENTED` zwraca `0`,
- telemetria przychodzi,
- `VBUS_VALID` staje sie `1`,
- `FAULT` jest pusty albo czytelny,
- os nie porusza sie samoczynnie.

Nie przechodz dalej, jesli:

- wystepuje samoczynny ruch,
- statusy sa niespojne,
- UART jest niestabilny,
- `STOP` lub `QSTOP` nie dzialaja.

## 3. Etap 1 - komunikacja i diagnostyka

Sprzet:

- logika MCU wlaczona,
- USB-UART podpiety,
- tor mocy odlaczony albo ograniczony.

Operator sprawdza:

- `STATE`,
- `FAULT`,
- `VBUS`,
- event log,
- brak ruchu.

## 4. Etap 2 - logika operatorska

Sprzet:

- USB-UART podpiety,
- STOP i odciecie zasilania w zasiegu,
- tor mocy nadal ostrozny.

Operator sprawdza:

- `ENABLE`,
- `DISABLE`,
- `STOP`,
- `QSTOP`,
- reakcje na timeout komunikacji.

Nie przechodz dalej, jesli firmware zachowuje sie nielogicznie.

## 5. Przejscie na `MOTION-ENABLED`

Wgraj build ruchowy dopiero po pozytywnej walidacji `SAFE`.

Potwierdz:

- `GET SAFE_INTEGRATION` zwraca `0`,
- `GET MOTION_IMPLEMENTED` zwraca `1`,
- nadal nie ma samoczynnego homingu po starcie,
- fault jest pusty,
- limity sa konserwatywne.

## 6. Etap 3 - gotowosc do ruchu

Sprzet:

- tor mocy wlaczony,
- mechanika wolna od kolizji,
- zapas od ogranicznikow,
- operator gotowy na natychmiastowe zatrzymanie.

Operator sprawdza:

- aktywny jest etap `3`,
- brak aktywnego faultu,
- `RUN_ALLOWED` nie jest blokowany przez oczywisty powod,
- `HOME` jest gotowy do uzycia.

## 7. HOME

Pierwsza komenda ruchowa w buildzie `MOTION-ENABLED` to:

```text
CMD HOME
```

Potwierdz:

- `HOMING_STARTED = 1`,
- `HOMING_ONGOING` raportuje progres,
- `HOMING_SUCCESSFUL = 1`,
- po HOME nie ma faultu.

Nie wykonuj `MOVE_REL` ani `MOVE_ABS`, jesli `HOME` nie zakonczyl sie poprawnie.

## 8. Pierwszy kontrolowany ruch

Pierwszy ruch ma byc bardzo maly, na przyklad:

```text
MOVE_REL 100 um
```

Obserwuj:

- kierunek ruchu,
- pozycje mierzona,
- pozycje zadana,
- predkosc,
- prad,
- `VBUS`,
- logi.

Ruch jest dopuszczalny dopiero wtedy, gdy:

- `HOME` jest zakonczony sukcesem,
- fault jest pusty,
- aktywny jest etap `3`,
- `RUN_ALLOWED = 1`.

## 9. Natychmiast przerwij test, jesli

- os rusza sama po boot,
- kierunek ruchu jest przeciwny do oczekiwanego,
- pojawia sie niekontrolowane przyspieszenie,
- `STOP` lub `QSTOP` nie daja oczekiwanej reakcji,
- `VBUS` jest nielogiczny,
- prady sa nielogiczne albo nasycone,
- GUI i firmware pokazuja sprzeczne informacje o ruchu.
