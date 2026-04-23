# 📄 PRD — Rozbudowany System Sterowania Osi (STM32 + Agent + GUI + HMI)

---

# 1. 🎯 CEL PRODUKTU

Celem systemu jest stworzenie **modularnej, bezpiecznej i skalowalnej platformy sterowania osią liniową** (np. mikrotom), która:

- zapewnia **precyzyjne sterowanie ruchem**
- spełnia wymagania **fail-safe**
- umożliwia **traceability procesu produkcyjnego**
- pozwala na **integrację z systemami IT (API, analiza danych)**

---

# 2. 🧱 ARCHITEKTURA SYSTEMU

```
GUI / WEB
   ↓ REST / WebSocket
Agent (Python)
   ↓ UART
STM32 (real-time)
   ↓
Napęd / czujniki
```

---

# 3. 🔧 MODUŁY SYSTEMU

## 3.1 MCU (STM32G473)

### Główne odpowiedzialności:
- sterowanie FOC
- zarządzanie stanem systemu
- safety
- telemetry
- wykonanie poleceń

### Moduły firmware:

- axis_state
- fault
- commissioning
- axis_control
- limits
- watchdogs
- telemetry
- eventlog
- protocol

---

## 3.2 Agent

### Funkcje:
- komunikacja UART
- REST API
- logowanie danych
- integracja HMI

### Technologie:
- Python
- FastAPI
- pyserial

---

## 3.3 GUI

### Funkcjonalności:
- wizualizacja stanu
- konfiguracja parametrów
- commissioning
- sterowanie osią
- logi

---

## 3.4 HMI

Minimalne sterowanie lokalne:
- START / STOP
- status
- pozycja

---

# 4. ⚙️ FUNKCJONALNOŚĆ

## 4.1 State Machine

```
BOOT
SAFE
CALIBRATION
READY
ARMED
MOTION
STOPPING
FAULT
ESTOP
```

---

## 4.2 Commissioning

### Etap 1
- SAFE_MODE = 1
- brak ruchu

### Etap 2
- ARMING_ONLY
- test sprzętu

### Etap 3
- CONTROLLED_MOTION
- dopuszczenie ruchu

---

## 4.3 Safety

### Warunki ruchu

```
RUN_ALLOWED =
    !Fault &&
    !SAFE_MODE &&
    CONTROLLED_MOTION &&
    Calibration_OK &&
    Limits_OK
```

---

## 4.4 Fault System

- overcurrent
- overspeed
- tracking error
- encoder error
- comm timeout

### Reakcje:
- STOP
- SAFE
- blokada ruchu

---

## 4.5 Telemetry

Format:
```
TEL,timestamp,pos,pos_set,vel,vel_set,current,state,fault
```

Częstotliwość:
- 50–100 Hz

---

## 4.6 Event Log

Zdarzenia:
- boot
- enable/disable
- calibration
- fault set/clear
- watchdog timeout

---

## 4.7 Protocol

### GET
- STATE
- FAULT
- PARAM
- CONFIG

### SET
- PARAM
- CONFIG

### CMD
- ENABLE
- DISABLE
- STOP
- QSTOP
- MOVE_REL
- MOVE_ABS
- CALIB_ZERO

---

# 5. 🔒 BEZPIECZEŃSTWO

## Fail-safe

System MUSI:
- przejść do SAFE przy błędzie
- zatrzymać ruch przy fault
- blokować niepoprawne komendy

---

## Watchdog

- timeout komunikacji
- reakcja: STOP + FAULT

---

# 6. 🧠 WYMAGANIA TECHNICZNE

## MCU
- brak dynamicznej pamięci
- deterministyczny kod
- ISR minimalne

## Komunikacja
- UART 460800
- ASCII (v1)

## GUI
- PySide6

---

# 7. 📊 DANE I ANALIZA

- zapis telemetry
- eksport CSV
- API do analizy

---

# 8. 🧪 TESTY

## Krytyczne:
- brak ruchu w SAFE
- watchdog działa
- fault zatrzymuje system
- commissioning blokuje ruch

---

# 9. 🚀 ROADMAP

## V1
- safety
- protocol
- GUI

## V2
- flash config
- lepsza kalibracja

## V3
- cloud
- AI analiza

---

# 10. 📌 ZASADY PROJEKTOWE

- Safety > funkcjonalność
- deterministyczny kod
- brak side effects
- pełna diagnostyka

---

# 11. 🧾 PODSUMOWANIE

System to **warstwa sterowania ruchem z pełną kontrolą bezpieczeństwa i diagnostyki**, gotowa do integracji z systemami IT.
