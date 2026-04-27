# UART Binary Protocol v2

## Cel

Ten dokument definiuje docelowy binarny protokol UART `v2` dla sciezki:

```text
MCU <-> PC logger / agent <-> analiza / algorytm
```

Jego zadania sa trzy:

- szybka telemetria ruchu odporna na zaklocenia,
- kompletne zrzuty stanu i konfiguracji ukladu do PC,
- deterministyczne logowanie pod pozniejszy algorytm strojenia i kompensacji.

To jest **projekt docelowy**, nie opis obecnej implementacji. Aktualny firmware
repozytorium nadal uzywa tekstowego `ASCII protocol v1` na `USART2`.

## Zalozenia projektowe

- warstwa fizyczna: `UART TTL`
- zalecana predkosc dla `v2`: `230400`, `8N1`
- wszystkie pola wielobajtowe: `little-endian`
- ramki sa binarne, nie tekstowe
- CRC liczone bez pol `SOF`
- numer `SEQ` jest inkrementowany niezaleznie przez kazdego nadawce
- PC loguje zarowno surowe ramki, jak i rekordy zdekodowane

## Format ramki

```text
SOF1 SOF2 LEN TYPE SEQ_LO SEQ_HI PAYLOAD... CRC_LO CRC_HI
0xAA 0x55  1B  1B     1B     1B     N B         1B    1B
```

## Znaczenie pol

- `SOF1/SOF2` = staly naglowek synchronizacyjny `0xAA 0x55`
- `LEN` = dlugosc pola `TYPE + SEQ + PAYLOAD`
- `TYPE` = typ ramki
- `SEQ` = numer kolejny ramki nadawcy
- `PAYLOAD` = dane wlasciwe
- `CRC16` = kontrola poprawnosci

## CRC

Przyjmujemy:

- algorytm: `CRC-16/CCITT-FALSE`
- polynomial: `0x1021`
- init: `0xFFFF`
- xorout: `0x0000`

CRC liczymy po bajtach:

```text
LEN + TYPE + SEQ_LO + SEQ_HI + PAYLOAD
```

Nie liczymy `SOF1/SOF2`.

## Typy ramek

- `0x01` = `TELEMETRY`
- `0x02` = `COMMAND`
- `0x03` = `RESPONSE`
- `0x04` = `ERROR`
- `0x05` = `HEARTBEAT`

## Ogolna zasada dla `PAYLOAD`

Pierwszy bajt `PAYLOAD` jest zawsze identyfikatorem podtypu:

- dla `TELEMETRY` = `stream_id`
- dla `COMMAND` = `command_id`
- dla `RESPONSE` = `response_id`
- dla `ERROR` = `error_code`
- dla `HEARTBEAT` = `heartbeat_role`

To pozwala utrzymac tylko 5 glownych typow ramek, ale jednoczesnie rozszerzac
protokol bez zmiany warstwy transportowej.

## Strumienie telemetryczne

Nie wysylamy "wszystkiego" w jednej szybkiej ramce. To byloby niepotrzebnie
ciezkie i utrudnialoby analize. Zamiast tego rozdzielamy dane na:

- szybki strumien ruchowy `100-500 Hz`
- wolniejsze snapshoty statusu `5-20 Hz`
- snapshoty konfiguracji tylko po starcie i po zmianie
- eventy i faulty tylko przy wystapieniu

Dzieki temu PC finalnie dostaje komplet stanu ukladu, ale bez przeciadzania
UART.

### `TELEMETRY / stream_id = 0x01` Fast Motion Snapshot

To jest podstawowa szybka ramka do logowania ruchu i pozniejszego algorytmu.

#### Payload

```text
stream_id           uint8   = 0x01
timestamp_us        uint32
position_counts     int32
position_um         int32
position_set_um     int32
velocity_um_s       int32
velocity_set_um_s   int32
iq_ref_mA           int16
iq_meas_mA          int16
id_ref_mA           int16
id_meas_mA          int16
vbus_mV             uint16
axis_state          uint8
commission_stage    uint8
status_flags        uint16
error_flags         uint16
```

#### Rozmiar

`41 B payload`

Cala ramka:

- `2 SOF`
- `1 LEN`
- `1 TYPE`
- `2 SEQ`
- `41 PAYLOAD`
- `2 CRC`

Razem `49 B`.

Przy `230400 baud`:

- `230400 / 10 = 23040 B/s`
- `23040 / 49 ~= 470 ramek/s`

Czyli sensownie:

- `200 Hz` bezpiecznie
- `250 Hz` bardzo rozsadnie
- `500 Hz` tylko jesli MCU i PC nie robia nic ciezkiego rownolegle

### `TELEMETRY / stream_id = 0x02` Runtime Status Snapshot

To jest wolniejsza ramka logiczna, ktora zbiera biezacy stan systemowy.

#### Payload

```text
stream_id               uint8   = 0x02
timestamp_us            uint32
fault_code              uint8
fault_mask              uint32
axis_state              uint8
commission_stage        uint8
status_flags            uint16
override_flags          uint16
event_count             uint16
first_move_max_delta_um int16
telemetry_period_ms     uint16
vbus_min_mV             uint16
vbus_max_mV             uint16
reserved                uint16
```

Ta ramka moze isc np. `10 Hz`.

### `TELEMETRY / stream_id = 0x03` Limits Snapshot

Ta ramka niesie limity ruchu i pradu.

#### Payload

```text
stream_id               uint8   = 0x03
timestamp_us            uint32
max_current_nominal_mA  uint16
max_current_peak_mA     uint16
max_velocity_um_s       uint32
max_accel_um_s2         uint32
soft_min_pos_um         int32
soft_max_pos_um         int32
```

Wysylka:

- raz po bootcie,
- po kazdej zmianie konfiguracji,
- opcjonalnie na zadanie PC.

### `TELEMETRY / stream_id = 0x04` Safety Config Snapshot

Ta ramka niesie konfiguracje safety i override.

#### Payload

```text
stream_id               uint8   = 0x04
timestamp_us            uint32
install_flags           uint16
override_flags          uint16
behavior_flags          uint16
reserved                uint16
```

#### `install_flags`

Bitowo:

- bit 0 = `brake_installed`
- bit 1 = `collision_sensor_installed`
- bit 2 = `ptc_installed`
- bit 3 = `backup_supply_installed`
- bit 4 = `external_interlock_installed`

#### `override_flags`

Bitowo:

- bit 0 = `ignore_brake_feedback`
- bit 1 = `ignore_collision_sensor`
- bit 2 = `ignore_external_interlock`

#### `behavior_flags`

Bitowo:

- bit 0 = `allow_motion_without_calibration`
- bit 1 = `telemetry_enabled`
- bit 2 = `config_loaded`
- bit 3 = `safe_integration`
- bit 4 = `motion_implemented`

### `TELEMETRY / stream_id = 0x05` Calibration Snapshot

#### Payload

```text
stream_id               uint8   = 0x05
timestamp_us            uint32
calib_valid             uint8
calib_sign              int8
reserved0               uint16
zero_pos_um             int32
pitch_um_x1000          uint32
endstop_left_um         int32
endstop_right_um        int32
```

`pitch_um_x1000` pozwala uniknac `float` w binarnym logu.

### `TELEMETRY / stream_id = 0x06` Event Frame

Ta ramka idzie tylko przy zdarzeniu, faultcie albo waznej zmianie stanu.

#### Payload

```text
stream_id               uint8   = 0x06
event_time_ms           uint32
event_code              uint16
event_value             int32
axis_state              uint8
fault_code              uint8
status_flags            uint16
```

### `TELEMETRY / stream_id = 0x07` Version / Capability Snapshot

#### Payload

```text
stream_id               uint8   = 0x07
protocol_version_major  uint8
protocol_version_minor  uint8
fw_version_major        uint8
fw_version_minor        uint8
fw_version_patch        uint16
build_unix_time         uint32
git_hash32              uint32
capability_flags        uint32
```

Ta ramka powinna byc wysylana:

- po bootcie,
- po zestawieniu polaczenia,
- na zadanie PC.

## Status flags

`status_flags` sa wspolne dla snapshotow runtime i fast telemetry.

- bit 0 = `enabled`
- bit 1 = `run_allowed`
- bit 2 = `safe_mode`
- bit 3 = `arming_only`
- bit 4 = `controlled_motion`
- bit 5 = `telemetry_enabled`
- bit 6 = `config_loaded`
- bit 7 = `vbus_valid`
- bit 8 = `calibrated`
- bit 9 = `homing_started`
- bit 10 = `homing_ongoing`
- bit 11 = `homing_successful`
- bit 12 = `first_move_test_active`
- bit 13 = `safe_integration`
- bit 14 = `motion_implemented`
- bit 15 = `power_ready`

## Error flags

`error_flags` sa szybkim skrotem najwazniejszych faultow do telemetrii `fast`.

- bit 0 = `comm_timeout`
- bit 1 = `overcurrent`
- bit 2 = `undervoltage`
- bit 3 = `overvoltage`
- bit 4 = `overspeed`
- bit 5 = `tracking`
- bit 6 = `estop`
- bit 7 = `config_invalid`
- bit 8 = `limit_violation`
- bit 9 = `encoder_invalid`
- bit 10 = `current_invalid`
- bit 11 = `temperature`
- bit 12-15 = reserved

Pelne `fault_mask` idzie w `Runtime Status Snapshot`.

## Ramki `COMMAND`

### Ogolny format

```text
command_id   uint8
flags        uint8
timeout_ms   uint16
body...
```

`flags` na razie:

- bit 0 = `ack_required`
- bit 1 = `urgent`

### `command_id = 0x01` Set IQ

#### Payload body

```text
iq_target_mA     int16
iq_hold_ms       uint16
iq_limit_mA      uint16
```

Uzycie:

- test minimalnego pradu ruszenia,
- otwarta procedura laboratoryjna,
- nie do normalnego ruchu operatorskiego.

### `command_id = 0x02` Move Relative

#### Payload body

```text
target_um        int32
velocity_um_s    int32
accel_um_s2      int32
iq_limit_mA      uint16
```

### `command_id = 0x03` Move Absolute

#### Payload body

```text
target_um        int32
velocity_um_s    int32
accel_um_s2      int32
iq_limit_mA      uint16
```

### `command_id = 0x04` Stop

Bez dodatkowego body.

### `command_id = 0x05` Reset Fault

Bez dodatkowego body.

### `command_id = 0x06` Request Snapshot

#### Payload body

```text
request_mask     uint16
```

`request_mask`:

- bit 0 = `runtime status`
- bit 1 = `limits`
- bit 2 = `safety config`
- bit 3 = `calibration`
- bit 4 = `version`

### `command_id = 0x07` Enable

Bez dodatkowego body.

### `command_id = 0x08` Disable

Bez dodatkowego body.

### `command_id = 0x09` Set Stream Rate

#### Payload body

```text
stream_id        uint8
period_ms        uint16
reserved         uint8
```

### `command_id = 0x0A` Set Limits

#### Payload body

```text
max_current_peak_mA   uint16
max_velocity_um_s     uint32
max_accel_um_s2       uint32
```

### `command_id = 0x0B` Calibration Zero

Bez dodatkowego body.

## Ramki `RESPONSE`

### Ogolny format

```text
response_id   uint8
ack_seq       uint16
status_code   uint16
body...
```

`ack_seq` wskazuje numer komendy PC, ktorej dotyczy odpowiedz.

### `response_id = 0x01` ACK

Potwierdzenie wykonania komendy.

### `response_id = 0x02` NACK

Komenda odrzucona logicznie.

### `response_id = 0x03` SNAPSHOT

W odpowiedzi body zawiera jedna z ramek snapshotowych z sekcji `TELEMETRY`.

## Ramki `ERROR`

### Ogolny format

```text
error_code     uint8
ref_seq        uint16
detail_code    uint16
fault_mask     uint32
axis_state     uint8
```

### `error_code`

- `0x01` = `CRC_FAIL`
- `0x02` = `LEN_FAIL`
- `0x03` = `UNKNOWN_TYPE`
- `0x04` = `UNKNOWN_COMMAND`
- `0x05` = `RANGE_ERROR`
- `0x06` = `STATE_REJECTED`
- `0x07` = `FAULT_ACTIVE`
- `0x08` = `BUSY`

## Ramki `HEARTBEAT`

### Ogolny format

```text
heartbeat_role   uint8
uptime_ms        uint32
last_rx_seq      uint16
status_flags     uint16
```

### `heartbeat_role`

- `0x01` = `PC -> MCU`
- `0x02` = `MCU -> PC`

Zalecenie:

- PC wysyla heartbeat co `100-250 ms`
- MCU moze odpowiadac wlasnym heartbeat co `500-1000 ms`

## Synchronizacja logow MCU i PSU

### Zrodla danych

```text
PC
|- COM_MCU  -> szybka telemetria binarna z MCU
|- COM_PSU  -> wolniejsze V/A z zasilacza
`- ST-LINK  -> flash / debug
```

### Zasada

- MCU: `100-500 Hz`
- PSU: `1-10 Hz`
- brak synchronizacji sprzetowej
- synchronizacja czasem po stronie PC

PC powinien kazda ramke opatrywac wlasnym:

- `pc_time_ns = time.monotonic_ns()`

## Schemat logowania na PC

### Minimalny rekord

```text
pc_time_ns
source
seq
mcu_time_us
position_counts
position_um
velocity_um_s
iq_mA
id_mA
psu_voltage_mV
psu_current_mA
psu_output
psu_mode
command_id
target_um
test_id
state
error_flags
crc_ok
```

### Rekomendacja magazynu danych

- szybki zapis surowy: `CSV`
- docelowa analiza: `Parquet`
- dodatkowo osobna tabela / plik na surowe ramki hex

## Przyklad logiki testu minimalnego pradu ruszenia

### Cel

Wyznaczyc:

- minimalny `Iq`, przy ktorym enkoder pokaze realny ruch

### Przebieg

1. `PSU OUT0`
2. `flash MCU`
3. `PSU VSET 12.00`
4. `PSU ISET 1.00 A`
5. `PSU OUT1`
6. `MCU init`
7. `SET_IQ = 0`
8. zwiekszaj `Iq` co `50-100 mA`
9. obserwuj enkoder
10. po wykryciu trwalego ruchu zapisz `Iq_start`

### Warunek wykrycia ruchu

Nie:

```text
position != previous_position
```

Tylko:

```text
abs(position_um - position_start_um) > threshold_um
```

oraz:

- ruch widoczny przez `3-5` kolejnych probek

Startowo:

- `threshold_um = 2-5`

## Mapowanie do obecnego firmware

Aktualny kod repo juz ma wiekszosc zrodel danych potrzebnych do `v2`:

- `state.pos_um`
- `state.pos_m`
- `traj.pos_set_m`
- `state.vel_m_s`
- `traj.vel_set_m_s`
- `foc.iq_ref`
- `foc.iq_meas`
- `state.Vbus`
- `AxisState`
- `Fault`
- `Limits`
- `SafetyConfig`
- `Calibration`
- `Commissioning`
- `EventLog`

Do pelnego wdrozenia `v2` trzeba bedzie:

1. dodac binarny encoder/dekoder ramek do `protocol.c`
2. zostawic obecny `ASCII v1` jako tryb serwisowy lub fallback
3. wprowadzic periodyczne streamy `0x01..0x07`
4. dodac w agencie parser binarny i logowanie `raw frame -> parsed record`

## Rekomendowany podzial odpowiedzialnosci

### MCU / driver

- reguluje ruch
- czyta enkoder i prady
- wysyla szybka telemetrie binarna
- pilnuje safety

### PSU

- ustawia napiecie stanowiska
- ogranicza maksymalny prad stanowiska
- daje wolne pomiary `V/A`

### PC

- zarzadza testem
- wysyla komendy
- odbiera telemetrie
- skleja czas MCU z czasem PSU
- zapisuje logi
- liczy algorytm

## Najwazniejsza decyzja architektoniczna

Do R&D i pozniejszego algorytmu najlepsza jest architektura:

- szybkie dane z MCU po wlasnym binarnym `UART v2`
- wolne dane z PSU po osobnym porcie
- synchronizacja i logowanie wylacznie po stronie PC

To jest prostsze, odporniejsze i lepsze analitycznie niz proba upychania
wszystkich pomiarow i wszystkich urzadzen do jednego strumienia w MCU.
