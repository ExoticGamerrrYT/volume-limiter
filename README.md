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

All tuneable values are at the top of `main.cpp`:

```cpp
// Maximum allowed volume (0.0f – 1.0f). 0.20f = 20 %.
static constexpr float MAX_VOLUME = 0.20f;

// How often the volume is checked, in milliseconds.
static constexpr DWORD TICK_INTERVAL_MS = 500;
```

Change `MAX_VOLUME` to any value between `0.0f` and `1.0f` and recompile.

## Running at startup (optional)

To have the limiter start automatically with Windows, add the compiled `.exe` to the `Shell:startup` folder:

1. Press `Win + R`, type `shell:startup`, press Enter.
2. Copy `volume-limiter.exe` into that folder.

## Stopping the process

Open Task Manager → **Details** tab → find `volume-limiter.exe` → **End Task**.

## License

MIT
