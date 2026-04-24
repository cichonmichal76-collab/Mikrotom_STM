STM INTEGRATED PACKAGE V3

Ta paczka zawiera:
1. NOWE moduly systemowe:
   - axis_state, fault, commissioning, eventlog, telemetry, watchdogs,
     limits, safety_config, config_store, calibration, axis_control,
     safety_monitor, fw_version, protocol
2. PODMIENIONE pliki z Twojego projektu:
   - main.c
   - stm32g4xx_it.c
   - motorState.h
   - motorState.c

WAZNE:
- main.c jest przygotowany pod bezpieczny tryb integracji:
  #define APP_SAFE_INTEGRATION 1
  To wylacza:
    * startowy open-loop / PWM enable block
    * automatyczne przelaczanie trajektorii w while(1)
    * stary streaming UART w callbacku TIM2
- protocol/telemetry przejmuja USART2
- w motorState.c funkcja MotorState_UpdateCurrent ma przywrocona oryginalna
  sciezke pomiaru pradow fazowych z projektu referencyjnego.
- bazowanie 1:1 z projektu referencyjnego jest podlaczone dla trybu
  APP_SAFE_INTEGRATION = 0; po udanym homingu soft-limity sa ustawiane z
  wykrytych endstopow z marginesem ochronnym.
- safety_monitor zatrzymuje sterowanie przed petla FOC przy overcurrent,
  overspeed, tracking fault oraz wyjsciu poza soft-limity po homingu.
- opcjonalny external interlock korzysta z PushButton_Pin tylko gdy
  EXTERNAL_INTERLOCK_INSTALLED=1 i IGNORE_EXTERNAL_INTERLOCK=0.
- limit FOC state.maxcurrent jest synchronizowany z MAX_CURRENT_PEAK.
- STOP/QSTOP/fault/disable wymuszaja wygaszenie wyjsc PWM poza aktywnym
  homingiem.
- pomiar VBUS z oryginalnego projektu jest przywrocony; safety_monitor
  wystawia UNDERVOLTAGE/OVERVOLTAGE na podstawie zmierzonego VBUS.
- VBUS jest mierzony takze w APP_SAFE_INTEGRATION=1; dopoki VBUS_VALID=0,
  wyjscia pozostaja blokowane.
- stary debug streaming po USART2 jest domyslnie wylaczony przez
  APP_LEGACY_UART_STREAMING=0, zeby nie kolidowal z protocol/telemetry.

Rzeczy do sprawdzenia po rozpakowaniu:
- dodaj safety_monitor.c do projektu CubeIDE/Makefile/CMake, jesli target
  kompilacji nie zbiera automatycznie wszystkich plikow z katalogu firmware
- trajectory.h/.c zostaja z oryginalnego projektu
- foc.c/.h zostaja z oryginalnego projektu
- pid.c/.h zostaja z oryginalnego projektu
- usart.c/.h zostaja z oryginalnego projektu
- jesli kompilator zglosi roznice sygnatur lub warningi wynikajace z volatile,
  zostaw je na nastepny etap integracji

Pierwszy build/test:
- APP_SAFE_INTEGRATION = 1
- brak ruchu
- testuj tylko GET/SET/CMD/heartbeat/telemetry/VBUS
