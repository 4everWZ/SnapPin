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

Completion gate:

- Build succeeds.
- Test suite succeeds with no failures.
- Built `snappin.exe` can launch and remain running long enough for a basic smoke check.

## Manual Workflow Checklist

Capture and artifact:

- `Ctrl+1` opens capture overlay.
- Selection highlight and final capture area match.
- Toolbar actions `Copy`, `Save`, `Pin`, `Mark`, `Close` behave as expected.
- Toolbar `OCR` supports whole-artifact OCR and selected-region OCR, then copies recognized text, shows the auto-selected selectable OCR result window, supports result-window copy, and shows tray success/failure feedback.

Mark flow:

- `Mark` enters annotate session.
- `Rect`, `Ellipse`, `Line`, `Polyline`, `Arrow`, `Serial`, `Mosaic`, `Blur`, `Highlighter`, `Spotlight`, `Watermark`, `Magnifier`, `Pencil`, and `Text` can be created.
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
