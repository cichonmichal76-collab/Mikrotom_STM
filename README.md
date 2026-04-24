# Mikrotom_STM

Initial import of the STM32G473RCTx control firmware package for the mikrotome axis project.

Repository layout:

- `firmware/` - integrated firmware package sources and headers
- `stm32_cubeide/` - full STM32CubeIDE build target based on the original working project
- `agent/` - local REST bridge from GUI to the STM32 UART protocol
- `docs/` - PRD and system requirements documents
- `gui/` - static operator GUI for dashboard, safety, commissioning, and motion control
- `scripts/` - Windows batch helpers for PC setup, GUI/agent startup, MCU build, and ST-LINK flashing
- `dist/mcu/` - generated safe-integration MCU HEX/BIN files

The imported package currently reflects a safe-integration baseline with protocol, telemetry, state management, commissioning, and safety support modules prepared for further integration with the original motion-control implementation.

Recent updates also align the local REST agent and GUI with the newer program
model, including:

- explicit `CONFIG` state handling
- flash-backed configuration status reporting
- event-log export to the agent/API
- heartbeat supervision between agent and firmware

Architecture note:

- the current firmware sources remain in a flat `firmware/` layout for safe integration
- the importable STM32CubeIDE project is available in `stm32_cubeide/`
- CubeIDE import/build notes are documented in `docs/STM32_CubeIDE_Build.md`
- the target STM32/CubeIDE architecture and module mapping are documented in `docs/STM32_Code_Architecture.md`
- the HMI packet contract is documented in `docs/HMI_Protocol.md`
- the software evolution from the original FOC-centric firmware to the safety and GUI-oriented system is documented in `docs/STM32_Software_Evolution.md`
- the staged bench bring-up sequence is documented in `docs/STM32_Bringup_Checklist.md`
- the first-time installation guide for the target PC/MCU setup is documented in `docs/Wariant_B_Instalacja_i_Pierwsze_Uruchomienie.md`
- the operator GUI manual in Polish is documented in `docs/Wariant_B_Manual_Uzytkownika_GUI.md`
- the novice-friendly all-in-one HTML guide is documented in `docs/Wariant_B_Przewodnik_Kompletny.html`
- a recommended future project layout is scaffolded under `firmware/Core/`

Quick PC setup:

- run `scripts\install_pc.bat`
- run `scripts\configure_gui_live.bat` for real STM32 communication
- run `scripts\run_agent.bat COM3`
- run `scripts\run_gui.bat`

MCU firmware:

- generated HEX/BIN files are in `dist/mcu/`
- regenerate them with `scripts\build_mcu_release.bat`
- flash with `scripts\flash_mcu_stlink.bat`
- create ready-to-share Variant B deployment packages with `scripts\prepare_variant_b_packages.bat`
