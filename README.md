# volume-limiter

A silent background process for Windows that caps the system master volume at a configurable maximum (default **20 %**). The process runs with no console window, no GUI, and no taskbar icon — it is only visible in the **Details** tab of Task Manager.

## How it works

Every 500 ms the process reads the current master volume of the default audio playback device via the Windows Core Audio API (`IAudioEndpointVolume`). If the volume exceeds the configured ceiling it is silently forced back down.

## Requirements

| Tool | Version |
|---|---|
| MSYS2 | latest |
| MinGW-w64 toolchain (UCRT64) | GCC 13+ recommended |

Install the toolchain inside MSYS2 if you haven't already:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
```

## Building

Open the **UCRT64** terminal from MSYS2 and run:

```bash
g++ main.cpp -o volume-limiter.exe -mwindows -static -O2 -s -std=c++17 -lole32 -loleaut32 -luuid
```

| Flag | Purpose |
|---|---|
| `-mwindows` | Windows subsystem entry point (`WinMain`); no console window |
| `-static` | Links CRT and `libstdc++` statically — no external DLL dependencies |
| `-O2` | Balanced speed/size optimization |
| `-s` | Strips symbol table (~50 % smaller binary) |
| `-lole32 -loleaut32 -luuid` | COM + Core Audio API linkage |

## Configuration

The limiter reads its settings from **`volume-limiter.cfg`**, which must be placed in the same directory as the executable.

If the file does not exist when the process starts, it is **created automatically** with the default configuration:

```ini
# volume-limiter configuration
# Volumen maximo permitido: valor entre 0.0 (silencio) y 1.0 (100 %).
# Ejemplos: 0.20 = 20 %   |   0.50 = 50 %   |   1.0 = sin limite
max_volume=0.20
```

Edit `max_volume` to any value between `0.0` and `1.0` and restart the process — no recompile needed.

> **Note:** `volume-limiter.cfg` is excluded from version control (`.gitignore`). Each machine keeps its own local copy.

## Running at startup (with high priority)

The included **`install-startup.bat`** registers a Windows Scheduled Task that launches `volume-limiter.exe` automatically at every logon with **Above Normal** CPU priority.

### Requirements

- `volume-limiter.exe` must be in the **same folder** as `install-startup.bat`.
- The script must be run as **Administrator** (it will prompt for elevation automatically).

### Install

Double-click `install-startup.bat` (or run it from an elevated command prompt). It will:

1. Elevate to Administrator if needed.
2. Register a task named **`VolumeLimiter`** via the Windows Task Scheduler.
3. Configure it to run at logon with **Above Normal priority** and no time limit.

### Uninstall

To remove the scheduled task completely, run the following in an elevated command prompt:

```cmd
schtasks /delete /tn "VolumeLimiter" /f
```

Or open **Task Scheduler** → find `VolumeLimiter` → right-click → **Delete**.

## Stopping the process

Open Task Manager → **Details** tab → find `volume-limiter.exe` → **End Task**.

## License

MIT
