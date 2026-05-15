# SnapPin Implementation Status

This file tracks parity against the current feature reference set and user validation.

Normalized docs entry points:

- Active spec index: `docs/specs/00_overview.md`
- reference parity matrix: `docs/specs/matrix_reference_parity.md`
- Integration validation checklist: `docs/specs/integration_validation.md`

## Validation rule

- `Completed (user-verified)` means explicitly confirmed in manual testing by user feedback.
- `Implemented (pending user verification)` means code is in place but not yet confirmed by user.

## Reference docs used for parity

Reference labels only; direct reference URLs are intentionally kept out of
tracked documentation.

- reference app quick start.
- reference app stable release notes audited on 2026-05-14.
- reference app beta release notes audited on 2026-05-14.
- reference app capture docs: static capture, long capture, recording/GIF capture.
- reference app pin docs: base use, image, text, file, color, LaTeX, pin group.
- reference app mark docs: base use, geometry, line, arrow, serial, pencil,
  mark pencil, mosaic, text, erase, highlight, watermark, magnifier.
- reference app formula recognition docs.

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
  - `Mark` opens annotate inside active capture context without dismissing capture.
  - `Range` button and `R` shortcut re-enter selection in the same capture session.
  - First `Esc` exits current mark selection/edit state; next `Esc` exits capture session.
  - Annotate window client width stays tied to the captured bitmap width, so the mark toolbar does not create blank side bands on narrow captures.
  - The mark toolbar now uses compact button labels for dense controls while preserving the same tool contracts and keyboard behavior.
- Mark tools baseline:
  - `Select`, `Rect`, `Ellipse`, `Line`, `Polyline`, `Arrow`, `Serial`, `Mosaic`, `Blur`, `Eraser`, `Highlighter`, `Spotlight`, `Watermark`, `Magnifier`, `Pencil`, `Text`.
  - Serial values auto-increment, accept direct numeric entry for the selected or next serial value, and can be adjusted with `+` / `-`.
  - Undo/redo stack and delete selected editable annotation.
  - Shift-lock for line/arrow angle snapping.
- Mark edit baseline:
  - Rect move/resize, line/arrow move and endpoint drag, basic polyline node drag / whole-polyline move, segment double-click node insertion, selected polyline node deletion, text move.
  - Text entry with inline typing, compact `BG` background fill toggle, compact `Clr` preset background color cycling, and commit on `Enter`.
- Annotated output pipeline:
  - `Ctrl+C` and `Ctrl+S` export composed image through existing export service.
  - `snappin_tests` covers Rect, Ellipse, Line, Polyline, Arrow, Serial, Mosaic, Blur, Highlighter, Spotlight, Watermark, Magnifier, Pencil, and Text creation paths through `Copy` and verifies composed image pixels differ from the source bitmap.
  - `snappin_tests` covers compact mark toolbar labels, the annotate client-width guard, `BG` toolbar state, `Clr` toolbar presence, text background fill output changes, and text background color cycling output changes.
  - `snappin_tests` covers basic Polyline node editing by verifying selected node drag, segment double-click node insertion, and selected-node deletion semantics.
  - `snappin_tests` covers Eraser deletion of an editable annotation and verifies copied pixels return to the source bitmap.
  - `snappin_tests` covers basic Eraser path-segment removal on Polyline by verifying remaining segments still change copied pixels.
- Pin advanced content baseline:
  - Clipboard text pin is supported.
  - Clipboard LaTeX-like text is recognized and pinned in LaTeX mode.
  - Text/LaTeX pins support copy/save flows (`Copy Text`, `.txt` / `.tex` save).
  - `snappin_tests` covers pin context-menu labels and content-kind gating:
    image pins expose OCR, while text and LaTeX pins do not.
  - `snappin_tests` covers context-menu action to focused-pin command dispatch
    mapping for copy, save, OCR, close, destroy, close all, and destroy all.
- OCR baseline:
  - `ocr.start` runs Windows system OCR against the active artifact bitmap, selected OCR region, or focused image pin, then copies result text to clipboard and shows a selectable OCR result window.
  - The OCR result window auto-selects displayed text and exposes a `Copy` action that routes the currently displayed non-empty text back through the export clipboard path; `Copy` is disabled when the displayed result text is empty.
  - `ocr.start` declares optional `source`, `x`, `y`, `w`, and `h` action parameters so focused-pin and selected-region OCR are visible in the action contract.
  - OCR success emits a text progress event that carries the recognized text as UTF-8 for the result window.
  - Image pin context menu exposes OCR; text and LaTeX pins are not treated as OCR image sources.
  - `snappin_tests` covers strict OCR source selection so explicit focused-pin
    and active-artifact sources do not silently fall back to another source, and
    unknown source values are rejected.
  - Selected OCR region crop mapping is covered by focused tests for in-bounds, clipped, outside, and fallback-coordinate cases.
  - OCR success/failure emits a tray notification through the app action-event path.

## Partially implemented / not reference parity

- Mark parity:
  - Current tools are limited to `Select`, `Rect`, `Ellipse`, `Line`, basic multi-click/editable `Polyline`, `Arrow`, `Serial`, basic rectangular `Mosaic` with wheel strength, basic rectangular `Blur` with wheel strength, basic whole-object/path-segment `Eraser`, basic freehand `Highlighter` with wheel-controlled opacity/strength, basic rectangular `Spotlight` with wheel-controlled dim strength, basic rectangular `Watermark` with direct text entry and wheel-controlled opacity/strength, basic rectangular `Magnifier` with wheel-controlled zoom, `Pencil`, and `Text` with a background fill toggle plus preset background color cycling.
  - Automated pixel verification currently covers baseline drawing-tool composed-copy smoke paths and Eraser deletion, not advanced/deferred reference app mark tools or manual UI workflow.
  - Missing reference-level mark behavior includes smart erase; Polyline supports basic node drag, whole move, segment double-click node insertion, and selected-node deletion, but richer editing parity is still missing. Mosaic, Blur, Watermark, and Magnifier exist only as basic implementations without advanced editing/configuration parity; Watermark direct text/opacity entry and Magnifier wheel zoom are baselines, not the full advanced workflows.
  - reference-style highlighter modes and full configuration are not implemented.
  - reference-style spotlight modes and richer configuration are not implemented.
  - Full reference-style partial erasing and mosaic reverse-erase behavior are not implemented.
  - Missing current reference app release/beta additions include Auto Mosaic, synced mosaic operations, advanced serial formats/settings, text arrows, richer text background color configuration beyond preset cycling, and reference-style centered/configurable watermark workflow.
- OCR parity:
  - Current OCR copies one recognized text result to clipboard and displays it in a selectable, auto-selected result window with a repeat-copy action that is unavailable for empty result text.
  - Focused image pin OCR now exists as a baseline source path, but reference-level selectable recognized text overlays on pinned images are still missing.
  - Missing reference-level OCR includes automatic/manual pin OCR overlay UX, formula recognition, QR/barcode recognition, table recognition, language/model configuration, and advanced result management.
- Pin parity:
  - Missing file pin, color pin, pin groups, mouse passthrough, multi-select align, z-order persistence, and taskbar/topmost configuration.

## Not implemented yet

- Long capture (`capture/long-capture` parity).
- Recording and timeline capture modes.
