# Mikrotom_STM

Initial import of the STM32G473RCTx control firmware package for the mikrotome axis project.

Repository layout:

- `firmware/` - integrated firmware package sources and headers
- `docs/` - PRD and system requirements documents
- `gui/` - static operator GUI for dashboard, safety, commissioning, and motion control

The imported package currently reflects a safe-integration baseline with protocol, telemetry, state management, commissioning, and safety support modules prepared for further integration with the original motion-control implementation.

Architecture note:

- the current firmware sources remain in a flat `firmware/` layout for safe integration
- the target STM32/CubeIDE architecture and module mapping are documented in `docs/STM32_Code_Architecture.md`
- the software evolution from the original FOC-centric firmware to the safety and GUI-oriented system is documented in `docs/STM32_Software_Evolution.md`
- a recommended future project layout is scaffolded under `firmware/Core/`
