# Pin Specification

## Goals and Boundaries

- Support multi-pin image windows for persistent on-screen reference.
- Provide predictable focused-pin keyboard workflows.
- Preserve pin lifecycle semantics: close vs destroy.
- Keep non-image pin types (text, LaTeX, file, color) out of current implementation boundary.

## Math / Logic / Interfaces

Pin creation and lifecycle model:

1. Create pin from active artifact (`pin.create_from_artifact`) or clipboard (`pin.create_from_clipboard`).
2. Track focused pin for context-sensitive actions.
3. Route focused pin operations through explicit action checks.
4. Keep close operations recoverable and destroy operations terminal.

Current pin actions:

- `pin.create_from_clipboard`
- `pin.create_from_artifact`
- `pin.copy_focused`
- `pin.save_focused`
- `pin.close_focused`
- `pin.close_all`

Pin interaction baseline:

- Drag to move.
- Mouse wheel to scale.
- `Ctrl + Mouse wheel` to change opacity.
- `L` toggles lock.
- Middle-click resets scale/opacity baseline.

## Code Mapping

- Pin manager contract and state: `src/app/PinManager.h`
- Pin manager implementation: `src/app/PinManager.cpp`
- Pin window behavior and context menu: `src/ui/PinWindow.h`, `src/ui/PinWindow.cpp`
- Dispatcher hooks for pin actions: `src/app/ActionDispatcher.cpp`
- Registry declarations: `src/app/ActionRegistry.cpp`

## Tradeoffs

- Clipboard-first image pin flow is prioritized over multi-type clipboard decoding to keep baseline deterministic.
- Advanced pin modes (text/LaTeX) are deferred until core image pin reliability and lifecycle paths remain stable.

## Verification

Required verification for pin-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Create pin from artifact and from clipboard.
  - Validate `Ctrl+C`, `Ctrl+S`, `Ctrl+W`, `Ctrl+Shift+W`, `Ctrl+D`, `L` on focused pin.
  - Validate context menu operations: `Copy`, `Save`, `Close`, `Destroy`, `Close All`, `Destroy All`, `Lock/Unlock`.
