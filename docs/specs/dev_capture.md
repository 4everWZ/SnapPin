# Capture Specification

## Goals and Boundaries

- Provide a fast static capture flow triggered by `Ctrl+1` and tray entry points.
- Keep selection visuals stable under DPI scaling.
- Support artifact actions `Copy`, `Save`, `Pin`, `Mark`, and `Close`.
- Keep OCR and scrolling capture explicitly outside the current implemented baseline.

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
- `ocr.start` (deferred, currently returns explicit not-implemented error)
- `artifact.dismiss`

## Code Mapping

- Capture entry and runtime orchestration: `src/app/AppMain.cpp`
- Action routing and context checks: `src/app/ActionDispatcher.cpp`
- Overlay selection UI: `src/ui/OverlayWindow.cpp`
- Artifact toolbar UI: `src/ui/ToolbarWindow.cpp`
- Capture backend contracts: `src/capture/CaptureService.h`
- GDI fallback backend: `src/capture/CaptureServiceGdi.cpp`
- Artifact persistence: `src/app/ArtifactStore.cpp`
- Export behavior: `src/export/ExportService.cpp`

## Tradeoffs

- OCR and scrolling capture remain deferred to preserve baseline stability in the free/open-source core.
- `annotate.open` includes a recapture fallback path when bitmap backing is missing, prioritizing recoverability over strict purity of initial artifact path.

## Verification

Required verification for capture-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Start capture with `Ctrl+1`.
  - Select area and validate mask/preview alignment.
  - Trigger `Copy`, `Save`, `Pin`, and `Close` from toolbar.
  - Confirm `Esc` cancels overlay without stale session state.
