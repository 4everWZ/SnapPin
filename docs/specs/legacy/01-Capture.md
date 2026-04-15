# SnapPin Spec - Capture

## Entry points

- Hotkey `Ctrl+1`
- Tray menu `Capture`

## Static capture behavior

- Enter frozen-frame visual pause.
- Dim full monitor frame with clear selection highlight.
- Support both:
  - Hover + click window selection.
  - Drag region selection.
- `Esc` cancels capture and exits overlay.

## Selection and coordinate correctness

- Click-like movement should be treated as window-select.
- Drag should produce region-select.
- Selection preview must stay stable (no flicker/no curtain shift).
- Final captured bitmap bounds must match previewed rectangle under DPI scaling.

## Artifact lifecycle

- On success, create active artifact with CPU bitmap payload.
- Keep artifact available for toolbar actions.
- Auto-copy and auto-toolbar follow config flags.

## Capture toolbar behavior

Operation priority (right-to-left):
- `Copy`, `Save`, `Pin`, `Close`

Current visual layout (left-to-right):
- `Mark | OCR | Close | Pin | Save | Copy`

Action semantics:
- `Copy`: copy image and dismiss artifact context.
- `Save`: save image and dismiss artifact context.
- `Pin`: create pin from artifact.
- `Mark`: enter annotate mode for current artifact.
- `Close`: dismiss current artifact context.

## Long capture

Target behavior:
- Scroll/long capture mode based on selected area.

Current status:
- Not implemented yet.
