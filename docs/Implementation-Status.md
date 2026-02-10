# SnapPin Implementation Status

This file tracks parity against the current feature reference set and user validation.

## Validation rule

- `Completed (user-verified)` means explicitly confirmed in manual testing by user feedback.
- `Implemented (pending user verification)` means code is in place but not yet confirmed by user.

## Reference docs used for parity

- `https://pixpin.cn/docs/capture/static-capture.html`
- `https://pixpin.cn/docs/capture/long-capture.html`
- `https://pixpin.cn/docs/pin/base-use.html`
- `https://pixpin.cn/docs/pin/image.html`
- `https://pixpin.cn/docs/pin/text.html`
- `https://pixpin.cn/docs/pin/latex.html`
- `https://pixpin.cn/docs/mark/base-use.html`
- `https://pixpin.cn/docs/mark/geo.html`
- `https://pixpin.cn/docs/mark/line.html`
- `https://pixpin.cn/docs/mark/arrow.html`
- `https://pixpin.cn/docs/mark/mosaic.html`
- `https://pixpin.cn/docs/mark/text.html`
- `https://pixpin.cn/docs/mark/erase.html`

## Completed (user-verified)

- Tray icon right-click menu interaction works.
- Static capture enters frozen-frame visual pause with dimmed mask.
- Selection highlight and final capture coordinates are correct on single monitor `2560x1600` at `150%` scaling.
- Capture drag preview no longer flickers and no longer has offset mismatch.
- Capture result content is correct.
- `Esc` exits capture overlay cleanly.
- Toolbar copy/save/close baseline works after capture.
- Default save path is desktop and file save path is resolved.
- `Ctrl+C` in capture artifact context maps to copy flow.
- Pin baseline is stable:
  - Create pin from artifact and from clipboard.
  - Destroy does not break subsequent `Ctrl+2` pin-from-clipboard flow.
  - Context menu close/destroy operations are available.

## Implemented (pending user verification)

- Mark session interaction:
  - `Mark` opens annotate without dismissing the active capture context.
  - `Range` button and `R` shortcut re-enter selection in the same capture session.
  - First `Esc` exits current mark selection/edit state; next `Esc` exits capture session.
- Mark tools baseline:
  - `Select`, `Rect`, `Line`, `Arrow`, `Pencil`, `Text`.
  - Undo/redo stack and delete selected editable annotation.
  - Shift-lock for line/arrow angle snapping.
- Mark edit baseline:
  - Rect move/resize, line/arrow move and endpoint drag, text move.
  - Text entry with inline typing and commit on `Enter`.
- Annotated output pipeline:
  - `Ctrl+C` and `Ctrl+S` export composed image through existing export service.

## Not implemented yet

- Long capture (`capture/long-capture` parity).
- OCR implementation (action exists but still placeholder).
- Mark advanced tools:
  - Mosaic.
  - Erase/eraser semantics.
  - Polyline/multi-segment line behavior parity.
- Pin advanced content types:
  - Text pin.
  - LaTeX pin.
- Recording and timeline capture modes.
