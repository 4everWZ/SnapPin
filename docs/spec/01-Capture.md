# SnapPin Spec - Capture

## Entry

- `Ctrl+1` starts static capture overlay.
- Tray menu "Capture" starts the same flow.

## Overlay behavior

- Capture enters a frozen-frame visual pause.
- Whole screen is dimmed.
- Candidate target is highlighted.
- `Esc` cancels and exits overlay cleanly.

## Selection model (PixPin-style)

- Hover selects window candidate automatically.
- Click confirms current window candidate.
- Mouse drag creates a region capture.
- Small click movement is treated as click-select, not region drag.

## Output

- Successful capture creates an active artifact.
- Auto-copy behavior follows config.
- Auto-toolbar behavior follows config.

## Toolbar behavior

Toolbar order target:
- `Pin`
- `Save`
- `Copy`
- `Mark`
- `OCR`
- `Close`

Semantics:
- `Save`: save image, then close artifact toolbar.
- `Copy`: copy image, then close artifact toolbar.
- `Pin`: create pin from active artifact, then close artifact toolbar.
- `Close`: dismiss current artifact context.

## Quality constraints

- No preview flicker while selecting.
- Correct selection coordinates under DPI scaling.
- Overlay and toolbar must clean up correctly after cancel or confirm.

