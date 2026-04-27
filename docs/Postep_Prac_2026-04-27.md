# Postep prac - 2026-04-27

## Cel dnia

Dzisiejsze prace dotyczyly walidacji wariantu `MOTION` na rzeczywistym stanowisku z B-G431B-ESC1, zasilaczem osi 12 V oraz konwerterem USB-UART. Priorytetem bylo potwierdzenie komunikacji, bezpiecznego uzbrajania, kalibracji zera oraz pierwszych mikroruchow bez ryzyka niekontrolowanego ruchu.

## Stanowisko testowe

| Element | Ustalony stan |
| --- | --- |
| MCU / plyta | STM32G431 / B-G431B-ESC1 |
| Firmware | `MICROTOME_AXIS_FW 0.3.0-integrated` |
| Build testowany | `Apr 27 2026 17:18:44` |
| Wsad | `dist/mcu/Mikrotom_STM_latest_motion.hex` |
| UART | `COM6`, `115200 8N1` |
| Zasilanie osi | 12 V, typowo ok. 0.062-0.063 A w stanie spoczynku |
| ST-LINK | Pozostaje podpiety, bo stanowisko traci zasilanie logiki po jego odlaczeniu |

## Zmiany firmware potwierdzone w trakcie dnia

| Obszar | Status |
| --- | --- |
| UART | Potwierdzono prace na 115200 bps. |
| `CALIB_SIGN` | Podlaczono do toru FOC przez `Calibration_PositionToElecRad()` w `motorState.c`. |
| `CALIB_ALIGN_ZERO` | Komenda przyjmowana i konczy sie bez faultu. |
| `FIRST_MOVE_REL` | Komenda jest przyjmowana tylko w przewidzianym trybie `Stage 2 / ARMING_ONLY`. |
| Bezpieczny powrot | Po testach przywracano `SAFE_MODE=1`, `ENABLED=0`, `MAX_CURRENT_PEAK=0.300 A`. |

## Przebieg testow na stanowisku

| Test | Wynik |
| --- | --- |
| Flash `MOTION` przez ST-LINK | Wgranie i weryfikacja zakonczone poprawnie. |
| UART po flashu | MCU odpowiada na `GET VERSION`, `GET STATE`, `GET FAULT`, `GET VBUS`. |
| CV wylaczone | MCU widzi `UNDERVOLTAGE`, `RUN_ALLOWED=0`, `ENABLED=0`. To jest oczekiwane. |
| CV wlaczone | MCU widzi `VBUS` ok. 11.58 V. |
| `CALIB_ALIGN_ZERO` | Bez faultu, `CALIB_VALID=1`, pozycja ustawiona na 0. |
| `FIRST_MOVE_REL 20` | Komenda przyjeta, max pradu telemetrycznego ok. 56 mA, brak faultu, enkoder nadal pokazal 0 um. |
| `FIRST_MOVE_REL 50` | Komenda przyjeta, setpoint doszedl do 50 um, enkoder nadal 0 um, prad doszedl do ok. 400 mA, firmware zglosil `OVERCURRENT`. |

## Wnioski techniczne

1. Tor komunikacyjny UART dziala stabilnie na `COM6` przy 115200 bps.
2. Wariant `MOTION` uruchamia sie, odpowiada na komendy i respektuje blokady safety.
3. `CALIB_ALIGN_ZERO` przechodzi poprawnie i nie powoduje widocznego ryzyka na stanowisku.
4. Przy komendach mikroruchu sterownik generuje zadanie ruchu, ale feedback pozycji nie potwierdza przesuniecia.
5. Nie nalezy zwiekszac skoku powyzej 50 um przed diagnostyka przyczyny braku ruchu/feedbacku.
6. Najbardziej prawdopodobne obszary do sprawdzenia to kolejnosc faz, znak/kierunek kata FOC, mapowanie enkodera, realne sprzezenie mechaniczne oraz skalowanie/odczyt pradow.

## Stan koncowy po testach

| Parametr | Stan |
| --- | --- |
| `SAFE_MODE` | `1` |
| `ENABLED` | `0` |
| `FAULT` | `NONE` po `ACK_FAULT` |
| `MAX_CURRENT_PEAK` | `0.300 A` |
| `CALIB_SIGN` | `1` |
| Pozycja | `0 um` |

## Zalecany nastepny krok

Nie przechodzic jeszcze do wiekszych ruchow. Nastepna sesja powinna zaczac sie od diagnostyki toru napedowego:

1. Potwierdzic fizyczne podlaczenie faz silnika do wyjsc sterownika.
2. Sprawdzic, czy feedback pozycji zmienia sie przy recznym, bezpiecznym przemieszczeniu osi.
3. Zweryfikowac znak kata FOC i zaleznosc `CALIB_SIGN` na ruchu ponizej 50 um.
4. Zweryfikowac, czy limit `MAX_CURRENT_PEAK` i telemetryczne `iq_ref/iq_meas` sa spojne z rzeczywistymi pradami.
5. Dopiero po potwierdzeniu dodatniego feedbacku przejsc do testow 100 um i dalej.
