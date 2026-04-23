STM INTEGRATED PACKAGE V3

Ta paczka zawiera:
1. NOWE moduły systemowe:
   - axis_state, fault, commissioning, eventlog, telemetry, watchdogs,
     limits, safety_config, config_store, calibration, axis_control,
     fw_version, protocol
2. PODMIENIONE pliki z Twojego projektu:
   - main.c
   - stm32g4xx_it.c
   - motorState.h
   - motorState.c

WAŻNE:
- main.c jest przygotowany pod bezpieczny tryb integracji:
  #define APP_SAFE_INTEGRATION 1
  To wyłącza:
    * startowy open-loop / PWM enable block
    * automatyczne przełączanie trajektorii w while(1)
    * stary streaming UART w callbacku TIM2
- protocol/telemetry przejmują USART2
- w motorState.c funkcja MotorState_UpdateCurrent została zostawiona jako STUB,
  bo pełna oryginalna treść nie była dostępna w całości w źródłach przesłanych do analizy.
  MUSISZ wkleić tam swoją oryginalną implementację z projektu.

Rzeczy do sprawdzenia po rozpakowaniu:
- trajectory.h/.c zostają z oryginalnego projektu
- foc.c/.h zostają z oryginalnego projektu
- pid.c/.h zostają z oryginalnego projektu
- usart.c/.h zostają z oryginalnego projektu
- jeśli kompilator zgłosi różnice sygnatur lub warningi wynikające z volatile,
  zostaw je na następny etap integracji

Pierwszy build/test:
- APP_SAFE_INTEGRATION = 1
- brak ruchu
- testuj tylko GET/SET/CMD/heartbeat/telemetry
