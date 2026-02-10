# SnapPin

SnapPin is a Windows screenshot, annotation, and pin utility built for fast keyboard/mouse workflows.

## Features

- Static capture with frozen-frame visual pause
- Dimmed mask with selection highlight
- Auto window hover selection and drag region selection
- Post-capture toolbar actions: `Copy`, `Save`, `Pin`, `Mark`, `OCR (placeholder)`, `Close`
- Annotation editor baseline: `Rect`, `Line`, `Arrow`, `Pencil`, `Text`, `Undo/Redo`, range reselect
- Multi-pin image windows with close/destroy lifecycle actions
- Tray-resident single-instance app

Detailed progress and parity tracking: `docs/Implementation-Status.md`.

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
  ui/        overlay, toolbar, settings, annotate, pin windows
  capture/   capture backends and service interface
  export/    clipboard and file export
  core/      shared types and contracts
tests/
docs/
```

## Specs

- `docs/spec/00-Scope.md`
- `docs/spec/01-Capture.md`
- `docs/spec/02-Pin.md`
- `docs/spec/03-Mark.md`
- `docs/spec/03-Shortcuts-Actions.md`
- `docs/spec/04-Roadmap.md`

## License

TBD
