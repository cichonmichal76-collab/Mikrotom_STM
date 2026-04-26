# Agent SQLite Schema

SQLite po stronie agenta jest zaprojektowany jako warstwa:

- audytowa
- diagnostyczna
- wejściowa do przyszłych algorytmów sterowania i kompensacji

## Główne tabele

- `sessions`
- `firmware_builds`
- `config_snapshots`
- `config_changes`
- `command_log`
- `motion_runs`
- `telemetry_log`
- `event_log`
- `fault_log`
- `calibration_runs`
- `raw_protocol_log`

## Zasady

- logi są append-only
- agent zapisuje `recorded_at_utc`
- jeśli MCU podaje własny czas, zapisujemy też `source_ts_ms`
- ustawienia zapisujemy jako snapshoty oraz jako historię zmian
- komendy operatora i API są zapisywane osobno od surowego UART
- telemetria i eventy są podpinalne do `session_id` i `motion_run_id`

## Cel

Ten schemat ma umożliwić później:

- strojenie PID i feedforward
- budowę map kompensacji pozycji
- analizę błędów pozycjonowania i overshoot
- analizę zależności `iq -> ruch`
- wykrywanie anomalii i degradacji mechaniki
