# Integration Validation Checklist

## Goal

Provide a repeatable checklist for validating the active SnapPin baseline.

## Build and Test Gate

Run these commands from repository root:

```powershell
cmake -S . -B "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --config Release --target all --
ctest --test-dir "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --output-on-failure
```

`ctest` includes `no_reference_terms` and `no_reference_terms_history`, which fail if
tracked content, tracked file names, commit messages, ref names, or reachable
repository history contain forbidden reference terms. Keep source URLs in
ignored local notes only.

`ctest` also includes `ui_capture_smoke`, which launches the built app, sends
the capture command through the hidden main window, drag-selects a capture
region, verifies the compact artifact toolbar is visible with valid bounds,
triggers toolbar `Pin`, verifies a real pin window is visible with valid bounds,
closes the pin window, repeats capture selection, triggers toolbar `Mark`,
verifies the child mark window and overlay session state, and closes the mark
session.

Completion gate:

- Build succeeds.
- Test suite succeeds with no failures.
- Forbidden reference terms do not appear in tracked files, tracked file names,
  commit messages, ref names, or reachable history.
- Built `snappin.exe` can launch and remain running long enough for a basic smoke check.
- The process-level UI smoke can open the capture overlay, drag-select a region,
  show the compact toolbar, create a pin from that artifact, and close the pin
  window; it can also repeat capture, open mark from the toolbar, and close the
  mark session.

## Manual Workflow Checklist

Capture and artifact:

- `Ctrl+1` opens capture overlay.
- Reopening capture after a prior cancel/exit does not flash stale previous
  capture position or selection state.
- Selection highlight and final capture area match.
- Artifact toolbar remains compact and does not affect the captured bitmap dimensions.
- Toolbar actions `Copy`, `Save`, `Pin`, `Mark`, `Close` behave as expected.
- Toolbar `OCR` supports whole-artifact OCR; `Shift+OCR` supports
  selected-region OCR. Both paths copy recognized text, show the auto-selected
  selectable OCR result window, support result-window copy, and show tray
  success/failure feedback.

Mark flow:

- `Mark` enters annotate session.
- Narrow captures do not gain blank side bands from the mark toolbar; the mark
  window client width stays tied to the captured bitmap width.
- `Rect`, `Ellipse`, `Line`, `Polyline`, `Arrow`, `Serial`, `Mosaic`, `Blur`, `Highlighter`, `Spotlight`, `Watermark`, `Magnifier`, `Pencil`, and `Text` can be created.
- Text `BG` and `Clr` controls affect composed output.
- `Polyline` can be selected and adjusted with basic node drag / whole-polyline move.
- `Serial` can set the selected or next serial value with direct numeric entry and adjust it with `+` / `-`.
- `Mosaic` and `Blur` output changes when mouse-wheel strength changes before creation.
- `Eraser` deletes editable annotations without deleting the source image.
- `R` reselect works inside current capture session.
- Undo/redo and delete selected annotation work.
- `Ctrl+C` and `Ctrl+S` export composed image.

Pin flow:

- Create pin from artifact and clipboard.
- Focused pin shortcuts work (`Ctrl+C`, `Ctrl+S`, `Ctrl+W`, `Ctrl+Shift+W`, `Ctrl+D`, `L`).
- Focused image pin context-menu `OCR` runs through `ocr.start` and reports copied text, the auto-selected OCR result window, or an explicit OCR failure.
- Image, text, and LaTeX pin context menus use content-specific copy/save labels; only image pins expose `OCR`.
- Context menu close/destroy and lock/unlock work.
- Clipboard text and LaTeX-like fallback pins copy/save as text payloads.

reference parity release checklist:

- Mark gaps in `docs/specs/matrix_reference_parity.md` are either implemented and smoke-tested or explicitly deferred.
- OCR gaps in `docs/specs/matrix_reference_parity.md` are either implemented and smoke-tested or explicitly deferred.
- User-facing docs do not call partially implemented reference parity areas complete.
- Launch smoke does not replace manual capture/mark/OCR/pin workflow verification.

## Documentation Sync Gate

After behavior changes:

- Update `docs/Implementation-Status.md`.
- Update `docs/specs/matrix_reference_parity.md`.
- If behavior deviates materially from planned parity, update `docs/tradeoffs.md`.
