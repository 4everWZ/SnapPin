# SnapPin Implementation Status

Reference target: PixPin interaction model.

## Completed (user-verified)

- Tray icon right-click menu works
- Frozen-frame capture pause with dimmed overlay and highlight
- Accurate capture region on 150% DPI scaling
- `Esc` cleanly cancels capture and exits overlay
- Stable drag preview (no flicker / no curtain-like shift)
- PNG save works (desktop default path)
- Auto copy after capture
- Auto toolbar after capture
- Auto window hover select + drag region capture
- `Ctrl+C` in artifact context behaves as toolbar copy

## Implemented, pending verification

- Toolbar order aligned to PixPin flow:
  - `Pin / Save / Copy / Mark / OCR / Close`
- Pin baseline:
  - Multi-pin from artifact and clipboard
  - Pin right-click: `Close / Destroy / Close All / Destroy All`
  - Pin focus shortcuts: `Ctrl+W / Ctrl+Shift+W / Ctrl+D`

## In progress

- Real `Mark` implementation (currently action placeholder)
- Real `OCR` implementation (currently action placeholder)
- PixPin-like cache/history behavior alignment

## Not started

- Scroll capture
- Recording (video/gif)
- Full pin OCR interactions
- Full settings parity and diagnostics panel

