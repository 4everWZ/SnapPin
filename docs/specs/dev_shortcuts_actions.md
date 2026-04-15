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
- `ocr.start` (currently returns explicit not-implemented error)
- `artifact.dismiss`

Pin context actions:

- `pin.copy_focused`
- `pin.save_focused`
- `pin.close_focused`
- `pin.close_all`

Mark session shortcuts:

- `Ctrl+C`, `Ctrl+S`, `Ctrl+Z`, `Ctrl+Y`, `Delete`, `Esc`, `R`
- Tool switching: `Shift+1`, `Shift+2`, `Shift+3`, `Shift+5`, `Shift+8`, `V`
- Stroke adjustments: `[` / `]` and mouse wheel

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
