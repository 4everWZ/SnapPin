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

## Manual Workflow Checklist

Capture and artifact:

- `Ctrl+1` opens capture overlay.
- Selection highlight and final capture area match.
- Toolbar actions `Copy`, `Save`, `Pin`, `Mark`, `Close` behave as expected.

Mark flow:

- `Mark` enters annotate session.
- `R` reselect works inside current capture session.
- Undo/redo and delete selected annotation work.
- `Ctrl+C` and `Ctrl+S` export composed image.

Pin flow:

- Create pin from artifact and clipboard.
- Focused pin shortcuts work (`Ctrl+C`, `Ctrl+S`, `Ctrl+W`, `Ctrl+Shift+W`, `Ctrl+D`, `L`).
- Context menu close/destroy and lock/unlock work.

## Documentation Sync Gate

After behavior changes:

- Update `docs/Implementation-Status.md`.
- Update `docs/specs/matrix_pixpin_parity.md`.
- If behavior deviates materially from planned parity, update `docs/tradeoffs.md`.
