# Mikrotom Scripts

## PC Setup

Run:

```bat
scripts\install_pc.bat
```

This creates `.venv` and installs the local agent dependencies.

Prerequisite check:

```bat
scripts\check_variant_b_prerequisites.bat
```

## GUI Mode

Live mode:

```bat
scripts\configure_gui_live.bat
```

Demo mode:

```bat
scripts\configure_gui_demo.bat
```

## Run PC Components

Agent:

```bat
scripts\run_agent.bat COM3
```

Interactive COM prompt:

```bat
scripts\run_agent_prompt.bat
```

GUI:

```bat
scripts\run_gui.bat
```

Open:

```text
http://127.0.0.1:8080/gui/
```

## MCU Firmware

Build release files:

```bat
scripts\build_mcu_release.bat
```

Flash through ST-LINK and STM32CubeProgrammer:

```bat
scripts\flash_mcu_stlink.bat
```

## Deployment Packages

Prepare ready-to-share Variant B deployment packages:

```bat
scripts\prepare_variant_b_packages.bat
```
