# Shortcuts and Actions Specification

## Goals and Boundaries

- Keep every high-frequency workflow reachable from keyboard.
- Keep action IDs stable and context-validated.
- Prevent silent no-op behavior when action context is invalid.

## Math / Logic / Interfaces

Global shortcuts:

- `Ctrl+1` -> `capture.start`
- `Ctrl+2` -> `pin.create_from_clipboard`

Artifact context actions:

- `export.copy_image`
- `export.save_image`
- `pin.create_from_artifact`
- `annotate.open`
- `ocr.start` (runs system OCR for active artifact, selected OCR region, or focused image pin, copies recognized text to clipboard, and shows a selectable result window)
- `artifact.dismiss`

Pin context actions:

- `pin.copy_focused`
- `pin.save_focused`
- `pin.close_focused`
- `pin.close_all`
- `ocr.start` for focused image pins, exposed through the image pin context menu

Pin focused interactions:

- `Esc` and double-click close current pin.
- `T` toggles always-on-top.

Mark session shortcuts:

- `Ctrl+C`, `Ctrl+S`, `Ctrl+Z`, `Ctrl+Y`, `Delete`, `Esc`, `R`
- Tool switching: `Shift+1`, `Shift+2`, `Shift+3`, `Shift+4`, `Shift+5`, `Shift+6`, `Shift+7`, `Shift+8`, `Shift+9`, `Shift+0`, `V`
- Stroke adjustments: `[` / `]` and mouse wheel
- Serial adjustments: `+` / `-` adjust the selected serial number, or the next serial value when no serial annotation is selected.

OCR interaction baseline:

- `ocr.start` declares optional action parameters `source`, `x`, `y`, `w`, and `h` in the action registry.
- Toolbar `OCR` enters region selection when an overlay artifact is active.
- Region selection dispatches `ocr.start` with `x`, `y`, `w`, and `h` parameters.
- Direct `ocr.start` without region parameters runs OCR over the whole active artifact.
- When an image pin is focused and no active artifact exists, `ocr.start` runs OCR over the focused image pin.
- Image pin context-menu `OCR` dispatches `ocr.start` with `source=focused_pin` so it does not accidentally use an active artifact source.
- Successful OCR emits recognized text through an `ocr.text` action progress event; `AppMain` shows it in a selectable read-only result window after clipboard copy succeeds.
- reference-style pinned-image OCR text overlays, OCR result editing/management UI, formula recognition shortcut, QR/barcode recognition, and table recognition are not implemented.

Error handling rule:

- Context-sensitive actions must return explicit errors when prerequisites are missing.

## Code Mapping

- Action registry: `src/app/ActionRegistry.cpp`
- Action dispatch and context checks: `src/app/ActionDispatcher.cpp`
- Global/local hotkey parser and registration: `src/app/KeybindingsService.cpp`
- UI callback wiring to action IDs: `src/app/AppMain.cpp`

## Tradeoffs

- Default keybindings intentionally include only `Ctrl+1` and `Ctrl+2` global hotkeys to reduce conflict risk.
- Additional workflows depend on context-aware shortcuts instead of broad global hooks.

## Verification

Required verification for action/shortcut changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Validate `Ctrl+1` and `Ctrl+2` default global bindings.
  - Validate artifact context shortcut `Ctrl+C`.
  - Validate focused pin shortcuts.
  - Validate mark session shortcuts.
  - Trigger known invalid context actions and confirm explicit error handling.
