# OCR Specification

## Goals and Boundaries

- Keep OCR exposed as a context-aware action, not as ad hoc UI-only behavior.
- Support the current baseline sources: active capture artifact, selected OCR region, and focused image pin.
- Keep recognized text copyable through both the initial OCR action and the OCR result window.
- Track reference parity gaps explicitly without placing direct reference URLs in tracked documentation.
- Defer pinned-image selectable OCR overlays, formula recognition, QR/barcode recognition, table recognition, language/model configuration, and advanced result management until their data model and UI contracts are accepted.

## Math / Logic / Interfaces

OCR source selection:

1. `ocr.start` with `source=focused_pin` must use the focused image pin and must not silently fall back to an active artifact.
2. `ocr.start` with `x`, `y`, `w`, and `h` must crop the active artifact through `OcrRegion.h`; clipped regions are allowed, empty/outside regions are explicit errors.
3. `ocr.start` without source or region parameters uses the whole active artifact when one exists.
4. Text and LaTeX pins are not OCR image sources.

OCR result flow:

1. Run the OCR backend for the selected bitmap source.
2. Copy recognized text through `ExportService::CopyTextToClipboard`.
3. Emit an `ocr.text` progress event containing the recognized text for UI display.
4. Show `OcrResultWindow` with the recognized text auto-selected in a read-only edit control.
5. Route the result-window `Copy` action back through `ExportService::CopyTextToClipboard`.
6. Show tray success or explicit failure feedback.

Future OCR expansion boundary:

- A future `src/ocr/` module should own backend selection, language/model configuration, diagnostics, and recognizer-specific contracts.
- Pinned-image OCR overlays need a separate result data model before UI implementation; plain text clipboard copy is not enough for overlay hit-testing, selection, or persistent pin state.
- Formula, QR/barcode, and table recognition must define output schemas before implementation. Do not represent those outputs as plain OCR text unless the accepted scope explicitly says so.

## Code Mapping

- Action registry: `src/app/ActionRegistry.cpp`
- OCR dispatch and source resolution: `src/app/ActionDispatcher.cpp`
- OCR region-to-bitmap crop mapping: `src/app/OcrRegion.h`
- OCR progress event payload: `src/app/OcrResultEvent.h`
- Runtime result-window and tray wiring: `src/app/AppMain.cpp`
- Result window UI: `src/ui/OcrResultWindow.*`
- Focused image pin entry points: `src/app/PinManager.*`, `src/ui/PinWindow.*`
- Clipboard text copy: `src/export/ExportService.cpp`
- Future backend/config module: `src/ocr/`

## Tradeoffs

- The current baseline uses the Windows system OCR engine directly in `ActionDispatcher.cpp`; this remains acceptable only while OCR has no user-facing engine/language configuration or advanced recognizer outputs.
- Result-window tests use callbacks and edit-control state instead of mutating the global system clipboard in normal `ctest`.
- Tracked docs use reference labels and translated behavior specs only; direct reference URLs belong in local ignored notes.

## Verification

Required verification for OCR-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Automated coverage should include:
  - `ocr.start` source and region parameter registration.
  - Focused image pin OCR context resolution.
  - OCR crop mapping for in-bounds, clipped, outside, and fallback-coordinate cases.
  - OCR text progress-event UTF-8 roundtrip.
  - OCR result window selectable text, auto-selection, and repeat-copy callback.
  - Image-pin-only OCR menu eligibility.
- Manual smoke path:
  - Start capture, run whole-artifact OCR, and verify clipboard text, selected result-window text, result-window `Copy`, and tray feedback.
  - Start capture, choose a selected OCR region, and verify the same result path.
  - Create an image pin, run focused pin OCR from the context menu, and verify copied text or explicit failure feedback.
  - Confirm text and LaTeX pins do not expose OCR.
