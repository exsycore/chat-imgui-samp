# chatimgui — SA-MP ImGui Chat Replacement

A lightweight `.asi` plugin that replaces the default SA-MP chat box with a modern [Dear ImGui](https://github.com/ocornut/imgui) overlay. Supports Thai (TIS-620) text with proper color rendering.

## Features

- **Modern chat UI** — ImGui-based, transparent background, text outline for readability
- **Thai text support** — TIS-620 to UTF-8 conversion
- **Color tags** — `{RRGGBB}` inline color support from SA-MP
- **Custom input box** — T to open, Escape to close, sends commands via `CInput::Send()` and chat via `CLocalPlayer::Chat()`
- **Fade animations** — Configurable fade-in/out with hold duration
- **Mouse wheel scroll** — Scroll through chat history

## Requirements

- **Windows** (x86/32-bit SA-MP client)
- **CMake 3.20+**
- **Visual Studio 2022** (or any generator supporting C++20)
- **DirectX 9 SDK** (included in Windows SDK)

## Project Structure

```
chatimgui/
├── source/
│   ├── dllmain.cpp    — DX9 Present/Reset hooks, MinHook setup, WndProc hook
│   ├── gui.cpp        — ImGui chat rendering, input box, fade system
│   ├── gui.h          — Chat manager class, color helpers, fade state machine
├── libs/
│   ├── imgui/         — Dear ImGui (submodule)
│   ├── minhook/       — MinHook (submodule)
│   ├── samp-api/      — SA-MP reverse-engineered API (submodule)
│   └── tis620.h       — Thai TIS-620 <-> UTF-8 conversion
├── CMakeLists.txt
└── README.md
```

## Build

```bash
# Clone with submodules
git clone --recursive https://github.com/exsycore/chatimgui.git
cd chatimgui

# Configure (Visual Studio 2022, Win32)
cmake -B build -G "Visual Studio 17 2022" -A Win32

# Build Release
cmake --build build --config Release
```

The output is `bin/Release/ImguiPlugin.asi`.

## Installation

1. Copy `ImguiPlugin.asi` to your GTA:SA / SA-MP directory (next to `samp.exe`)
2. Launch SA-MP — the default chat will be replaced automatically

## How It Works

The plugin hooks three things:

1. **`IDirect3DDevice9::Present`** — Initializes ImGui and renders the chat overlay every frame
2. **`IDirect3DDevice9::Reset`** — Handles device lost/reset for ImGui
3. **`WndProc`** — Intercepts keyboard input (T to open chat, Escape to close, Up/Down for history)

It reads chat entries directly from SA-MP's `CChat` memory via the `samp-api` reverse-engineered structs, converts TIS-620 text to UTF-8, and renders them with ImGui.

## Configuration

All settings are in `source/gui.h`:

| Setting | Default | Description |
|---------|---------|-------------|
| `fontSize` | 21 | Chat font size |
| `widthPct` | 55 | Chat width as % of screen |
| `heightPct` | 28 | Chat height as % of screen |
| `fade.holdSeconds` | 2.0 | Seconds to show before fading |
| `fade.fadeInSpeed` | 6.0 | Fade-in speed (alpha/sec) |
| `fade.fadeOutSpeed` | 2.0 | Fade-out speed (alpha/sec) |

## Dependencies

| Library | Purpose | Link |
|---------|---------|------|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI | Compiled in |
| [MinHook](https://github.com/TsudaKageworking/minhook) | x86 function hooking | Static lib |
| [samp-api](https://github.com/BlastHackNet/SAMP-API) | SA-MP memory structures | Static lib |

## License

MIT License. See [LICENSE](LICENSE) for details.
