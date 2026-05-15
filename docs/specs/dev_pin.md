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
- `ocr.start` for focused image pins

Pin interaction baseline:

- Drag to move.
- Mouse wheel to scale.
- `Ctrl + Mouse wheel` to change opacity.
- Image pin context menu includes `OCR`; text and LaTeX pins do not expose OCR.
- Context menu labels are generated from the pin content kind: image pins use
  image copy/save wording, text pins use text copy/save wording, and LaTeX pins
  use `.tex` save wording.
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
- Focused image pin OCR is exposed from the pin context menu, follows the source/result rules in `dev_ocr.md`, copies plain text to the clipboard, and shows the auto-selected selectable OCR result window; it does not provide reference-style selectable OCR overlays on pins.

## Verification

Required verification for pin-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Create pin from artifact and from clipboard.
  - Validate `Ctrl+C`, `Ctrl+S`, `Ctrl+W`, `Ctrl+Shift+W`, `Ctrl+D`, `L`, `T`, `Esc` on focused pin.
  - Validate focused image pin context-menu OCR copies recognized text, shows the auto-selected selectable OCR result window, or reports an explicit OCR failure.
  - Validate context menu operations and labels: image pins expose `Copy`,
    `Save Image`, and `OCR`; text pins expose `Copy Text` and `Save .txt`
    without `OCR`; LaTeX pins expose `Save .tex` without `OCR`; all pin kinds
    expose close/destroy, close-all/destroy-all, lock/unlock, and always-on-top
    toggles.
  - Validate clipboard text fallback creates text/LaTeX pins and save exports `.txt` / `.tex`.
