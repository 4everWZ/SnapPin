# Pin Specification

## Goals and Boundaries

- Support multi-pin image windows for persistent on-screen reference.
- Support clipboard text and LaTeX-like text pin workflows.
- Provide predictable focused-pin keyboard workflows.
- Preserve pin lifecycle semantics: close vs destroy.
- Keep non-clipboard advanced pin payload types (file, color, rich object) out of current implementation boundary.

## Math / Logic / Interfaces

Pin creation and lifecycle model:

1. Create pin from active artifact (`pin.create_from_artifact`) or clipboard (`pin.create_from_clipboard`).
2. Clipboard flow prefers image first, then falls back to text pin creation.
3. Text payloads that match LaTeX-like markers are created as LaTeX pins.
4. Track focused pin for context-sensitive actions.
5. Route focused pin operations through explicit action checks.
6. Keep close operations recoverable and destroy operations terminal.

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
- `T` toggles always-on-top.
- `Esc` and double-click close current pin.
- Middle-click resets scale/opacity baseline.

## Code Mapping

- Pin manager contract and state: `src/app/PinManager.h`
- Pin manager implementation: `src/app/PinManager.cpp`
- Pin window behavior and context menu: `src/ui/PinWindow.h`, `src/ui/PinWindow.cpp`
- Dispatcher hooks for pin actions: `src/app/ActionDispatcher.cpp`
- Registry declarations: `src/app/ActionRegistry.cpp`

## Tradeoffs

- Clipboard-first image pin flow is still prioritized, with text and LaTeX fallback for missing image payloads.
- Text/LaTeX pins use native text rendering baseline instead of full math layout engine parity in this stage.

## Verification

Required verification for pin-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Create pin from artifact and from clipboard.
  - Validate `Ctrl+C`, `Ctrl+S`, `Ctrl+W`, `Ctrl+Shift+W`, `Ctrl+D`, `L`, `T`, `Esc` on focused pin.
  - Validate context menu operations: `Copy`, `Save`, `Close`, `Destroy`, `Close All`, `Destroy All`, `Lock/Unlock`, `Always On Top`.
  - Validate clipboard text fallback creates text/LaTeX pins and save exports `.txt` / `.tex`.
