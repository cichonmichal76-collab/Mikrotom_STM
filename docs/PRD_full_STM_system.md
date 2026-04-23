# PRD – System sterowania, bezpieczeństwa i traceability (STM32 + Agent + GUI + HMI)

---

# 1. Wprowadzenie

## 1.1 Cel dokumentu
Celem dokumentu jest opis wymagań dla systemu sterowania osią (mikrotom/ZSS), obejmującego:
- firmware STM32,
- agenta (backend lokalny),
- GUI,
- HMI,
- warstwę traceability i analizy danych.

## 1.2 Zakres
Dokument obejmuje:
- architekturę systemu,
- wymagania funkcjonalne,
- wymagania niefunkcjonalne,
- bezpieczeństwo (safety),
- komunikację,
- etapy wdrożenia.

---

# 2. Architektura systemu

## 2.1 Widok wysokopoziomowy

STM32 → UART → Agent → API → GUI / HMI

## 2.2 Komponenty

### STM32 (firmware)
- sterowanie silnikiem (FOC)
- safety
- state machine
- telemetry
- protocol UART

### Agent (Python)
- komunikacja UART
- API REST
- logowanie
- bridge HMI

### GUI
- konfiguracja
- wizualizacja
- commissioning

### HMI
- sterowanie lokalne
- szybki podgląd

---

# 3. Wymagania funkcjonalne

## 3.1 Sterowanie osią

System musi umożliwiać:
- CMD ENABLE
- CMD DISABLE
- CMD STOP
- CMD QSTOP
- CMD MOVE_REL
- CMD MOVE_ABS

### Warunki:
- ruch tylko przy RUN_ALLOWED = 1
- brak faultów
- kalibracja OK (lub override)

---

## 3.2 Commissioning

### Stage 1 – SAFE
- brak ruchu
- tylko diagnostyka

### Stage 2 – ARMING
- przygotowanie do ruchu
- brak pełnego sterowania

### Stage 3 – CONTROLLED MOTION
- ograniczony ruch
- pełna kontrola

---

## 3.3 Safety

System musi obsługiwać:
- watchdog komunikacji
- watchdog systemowy
- fault manager
- limity:
  - prądu
  - prędkości
  - pozycji

---

## 3.4 Fault handling

Każdy fault musi mieć:
- kod
- reakcję
- log

### Przykłady:
- OVERCURRENT
- OVERTEMP
- TRACKING ERROR
- COMM TIMEOUT

---

## 3.5 Telemetria

Format:
```
TEL,ts,pos,pos_set,vel,vel_set,iq_ref,iq_meas,state,fault
```

---

## 3.6 Event log

Logowane zdarzenia:
- boot
- enable/disable
- fault set/clear
- stage change
- calibration

---

## 3.7 Kalibracja

Minimalnie:
- CALIB_ZERO

Docelowo:
- 0 / 120 / 240
- walidacja

---

## 3.8 API

### GET:
- /api/status
- /api/telemetry/latest
- /api/events/recent

### POST:
- /api/cmd/*
- /api/params

---

# 4. Wymagania niefunkcjonalne

## 4.1 Bezpieczeństwo
- fail-safe default
- brak komunikacji → SAFE
- fault → STOP

## 4.2 Determinizm
- ISR krótkie
- brak blokujących operacji

## 4.3 Stabilność
- brak malloc
- statyczne bufory

## 4.4 Diagnostyka
- logi
- telemetry
- API debug

## 4.5 Rozszerzalność
- modularna architektura
- możliwość dodania LIS, traceability

---

# 5. STM – wymagania techniczne

## Must-have:
- state machine
- watchdog
- fault manager
- limits
- protocol UART
- telemetry
- eventlog

---

# 6. Agent

Agent musi:
- obsługiwać UART (MCU)
- obsługiwać UART (HMI)
- udostępniać REST API
- przechowywać dane

---

# 7. GUI

GUI musi:
- sterować urządzeniem
- wizualizować dane
- prowadzić commissioning

---

# 8. Traceability

System musi:
- zapisywać dane testowe
- umożliwiać odtworzenie procesu
- wspierać analizę QA

---

# 9. Ryzyka

| Ryzyko | Mitigacja |
|--------|----------|
| niekontrolowany ruch | SAFE_MODE |
| brak komunikacji | watchdog |
| błędna kalibracja | walidacja |
| przeciążenie | limits |

---

# 10. Etapy wdrożenia

1. SAFE firmware
2. commissioning
3. kalibracja
4. ruch
5. GUI/API

---

# 11. Definition of Done

- brak ruchu w stage 1
- watchdog działa
- telemetry działa
- fault działa
- API działa
- GUI działa

---

# 12. Podsumowanie

System musi być:
- bezpieczny
- modularny
- deterministyczny
- diagnozowalny
- rozszerzalny
