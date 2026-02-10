# SnapPin Spec - Shortcuts and Actions

## Default shortcuts

- `Ctrl+1`: start capture
- `Ctrl+2`: create pin from clipboard
- `Ctrl+C` (artifact context): copy artifact image and close toolbar
- `Esc`: cancel current overlay/session

Pin context:
- `Ctrl+C`: copy focused pin image
- `Ctrl+S`: save focused pin image
- `Ctrl+W`: close focused pin
- `Ctrl+Shift+W`: close all pins
- `Ctrl+D`: destroy focused pin
- `L`: lock/unlock focused pin

## Action IDs (current)

Global:
- `capture.start`
- `pin.create_from_clipboard`
- `settings.open`
- `app.exit`

Artifact context:
- `export.copy_image`
- `export.save_image`
- `pin.create_from_artifact`
- `annotate.open` (placeholder)
- `ocr.start` (placeholder)
- `artifact.dismiss`

Pin context:
- `pin.copy_focused`
- `pin.save_focused`
- `pin.close_focused`
- `pin.close_all`

## Behavior rules

- Actions must fail with explicit error context; no silent no-op.
- Context guards must be enforced:
  - Artifact actions require active artifact.
  - Focused-pin actions require focused pin.
- Session-level hotkeys may be dynamically registered and removed to avoid global shortcut conflicts.


