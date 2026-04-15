# Development Roadmap

## Goals and Boundaries

- Drive SnapPin toward practical PixPin parity while preserving free/open-source constraints.
- Prioritize stability of implemented baseline over breadth expansion.

## Math / Logic / Interfaces

Roadmap phases:

1. Baseline hardening for capture/mark/pin workflows.
2. Mark feature expansion (mosaic/erase/polyline parity).
3. Pin content expansion (text and LaTeX pin modes).
4. Capture expansion (scrolling capture).
5. OCR and diagnostics completion.

Gate rule per phase:

- Do not advance phase status to completed unless build and tests are green and phase-specific smoke checks are recorded.

## Code Mapping

Primary modules affected by roadmap items:

- Capture expansion: `src/capture/`, `src/ui/OverlayWindow.cpp`, `src/app/ActionDispatcher.cpp`
- Mark expansion: `src/ui/AnnotateWindow.cpp`, `src/app/AppMain.cpp`
- Pin expansion: `src/ui/PinWindow.cpp`, `src/app/PinManager.cpp`
- OCR expansion: `src/app/ActionDispatcher.cpp`, future `src/ocr/` module

## Tradeoffs

- Features that require large new modules are intentionally sequenced after baseline reliability, even when PixPin supports richer behavior today.

## Verification

At each milestone:

- Build: release profile succeeds.
- Tests: `snappin_tests` passes.
- Manual workflow checks: capture, pin, and mark core paths still succeed.
- Docs sync: `docs/Implementation-Status.md` and `docs/specs/matrix_pixpin_parity.md` updated.
