# SnapPin Spec - Shortcuts and Actions

## Global shortcuts

- `Ctrl+1`: start static capture.
- `Ctrl+2`: create pin from clipboard.
- `Esc`: cancel active overlay/session when applicable.

## Capture artifact context

- `Ctrl+C`: copy active artifact image (session copy flow).
- Toolbar actions: `Copy`, `Save`, `Pin`, `Mark`, `OCR`, `Close`.

## Mark session shortcuts

- `Ctrl+C`: copy composed image and close mark session.
- `Ctrl+S`: save composed image and close mark session.
- `Ctrl+Z` / `Ctrl+Y`: undo / redo mark history.
- `Delete`: delete selected editable annotation.
- `Esc`: exit current mark edit/selection state; if already idle, exit capture session.
- `R`: reselect capture range in current capture session.
- `Shift+1`: rect tool.
- `Shift+2`: line tool.
- `Shift+3`: arrow tool.
- `Shift+5`: pencil tool.
- `Shift+8`: text tool.
- `V`: select tool.
- `[` / `]` or mouse wheel: decrease/increase stroke thickness.

## Pin context shortcuts

- `Ctrl+C`: copy focused pin image.
- `Ctrl+S`: save focused pin image.
- `Ctrl+W`: close focused pin.
- `Ctrl+Shift+W`: close all pins.
- `Ctrl+D`: destroy focused pin.
- `L`: toggle focused pin lock.

## Action IDs

Global:
- `capture.start`
- `pin.create_from_clipboard`
- `settings.open`
- `app.exit`

Artifact:
- `export.copy_image`
- `export.save_image`
- `pin.create_from_artifact`
- `annotate.open`
- `ocr.start`
- `artifact.dismiss`

Pin:
- `pin.copy_focused`
- `pin.save_focused`
- `pin.close_focused`
- `pin.close_all`

## Error handling rules

- No silent no-op for user-triggered actions.
- Guard context-sensitive actions with explicit failures:
  - Artifact actions require active artifact.
  - Focused-pin actions require focused pin.
