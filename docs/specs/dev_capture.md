# Capture Specification

## Goals and Boundaries

- Provide a fast static capture flow triggered by `Ctrl+1` and tray entry points.
- Keep selection visuals stable under DPI scaling.
- Support artifact actions `Copy`, `Save`, `Pin`, `Mark`, `OCR`, and `Close`.
- Keep scrolling capture explicitly outside the current implemented baseline.
- Treat OCR as a basic artifact action with an auto-selected, selectable result window and repeat-copy action, not yet as reference-level pinned-image OCR overlay recognition.

## Math / Logic / Interfaces

Capture session state logic:

1. Start capture (`capture.start`) and show frozen-frame overlay.
2. User selects area by hover-select or drag-select.
3. Materialize active artifact with CPU bitmap payload.
4. Route toolbar actions through action dispatcher with explicit context guards.
5. End session on dismiss, copy/save completion, or pin creation.

Core capture interfaces and action IDs:

- `capture.start`
- `export.copy_image`
- `export.save_image`
- `pin.create_from_artifact`
- `annotate.open`
- `ocr.start` (runs Windows system OCR over the active artifact bitmap, an OCR region, or a focused image pin, copies recognized text to clipboard, shows an auto-selected selectable result window with a `Copy` action, and reports success/failure through a tray notification)
- `artifact.dismiss`

## Code Mapping

- Capture entry and runtime orchestration: `src/app/AppMain.cpp`
- Action routing and context checks: `src/app/ActionDispatcher.cpp`
- OCR region-to-bitmap crop mapping: `src/app/OcrRegion.h`
- Overlay selection UI: `src/ui/OverlayWindow.cpp`
- Artifact toolbar UI: `src/ui/ToolbarWindow.cpp`
- Capture backend contracts: `src/capture/CaptureService.h`
- GDI fallback backend: `src/capture/CaptureServiceGdi.cpp`
- Artifact persistence: `src/app/ArtifactStore.cpp`
- Export behavior: `src/export/ExportService.cpp`

## Tradeoffs

- Scrolling capture remains deferred to preserve baseline stability in the free/open-source core.
- OCR currently uses the Windows system OCR engine directly from `ActionDispatcher.cpp`; reusable region mapping lives in `src/app/OcrRegion.h`, result text is shown by `src/ui/OcrResultWindow.*`, the result-window `Copy` action is wired from `src/app/AppMain.cpp` through `ExportService`, and a dedicated `src/ocr/` module remains future work if language/runtime configuration or richer recognition backends are added.
- `annotate.open` includes a recapture fallback path when bitmap backing is missing, prioritizing recoverability over strict purity of initial artifact path.

## Verification

Required verification for capture-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Start capture with `Ctrl+1`.
  - Select area and validate mask/preview alignment.
  - Trigger `Copy`, `Save`, `Pin`, `OCR`, and `Close` from toolbar.
  - For OCR, validate whole-artifact OCR and selected-region OCR, then confirm recognized text reaches the clipboard, appears selected in the selectable result window, can be copied again from that window, and a tray notification reports success or an explicit OCR error.
  - Confirm `Esc` cancels overlay without stale session state.
