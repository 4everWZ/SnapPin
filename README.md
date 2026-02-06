# SnapPin

SnapPin is a lightweight Windows screenshot and pin tool, built to match PixPin-style workflows.

Current focus:
- Fast static capture with frozen-frame overlay
- Post-capture toolbar actions
- Multi-pin workflow (create, close, destroy)
- Practical keyboard-first interaction

## Current status

Implemented and actively tested:
- Tray icon with working right-click menu
- Frozen-frame capture overlay with dim mask and highlight
- Accurate region capture under DPI scaling
- Auto window hover selection and drag-to-region selection
- `Esc` to cancel capture
- PNG save (default desktop path)
- Auto copy and auto toolbar after capture
- `Ctrl+C` in artifact context = Copy and close toolbar
- Basic pin windows (multiple instances), with right-click close/destroy actions

In progress:
- Full PixPin-level pin behavior parity
- Annotate and OCR real implementations
- Scroll capture and recording parity

Detailed checklist: `docs/Implementation-Status.md`

## Build

Requirements:
- Windows 10/11
- Visual Studio 2022 Build Tools (MSVC v143)
- CMake 3.24+
- Ninja

Configure:

```powershell
cmake -S . -B "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Build:

```powershell
cmake --build "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --config Release --target all --
```

Run:

```powershell
.\build\MSVC v143 x64 (vcvars64 + Ninja)-Release\bin\snappin.exe
```

## Project layout

```text
src/
  app/       app wiring, actions, tray, runtime services
  ui/        overlay, toolbar, settings, pin windows
  capture/   capture backends and service interface
  export/    clipboard and file export
  core/      shared types and contracts
tests/
docs/
```

## Specs

The old specification tree was replaced with a compact PixPin-aligned spec set:
- `docs/spec/00-Scope.md`
- `docs/spec/01-Capture.md`
- `docs/spec/02-Pin.md`
- `docs/spec/03-Shortcuts-Actions.md`
- `docs/spec/04-Roadmap.md`

## License

TBD
