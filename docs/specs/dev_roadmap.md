# Development Roadmap

## Goals and Boundaries

- Drive SnapPin toward practical reference parity while preserving free/open-source constraints.
- Prioritize stability of implemented baseline over breadth expansion.

## Math / Logic / Interfaces

Roadmap phases:

1. Documentation and parity audit against current reference app stable/beta references.
2. Baseline hardening for capture/mark/pin/OCR workflows already present in code.
3. Mark feature expansion toward reference parity: advanced polyline add/remove node controls, advanced highlighter/spotlight modes, advanced blur/smart erase, reference-style partial erasing, Auto Mosaic/synced mosaic operations, configurable/centered watermark workflow, advanced magnifier controls, and advanced serial formats/settings.
4. OCR expansion toward reference parity: pinned-image text recognition, selectable/copyable OCR text, formula recognition, QR/barcode recognition, table recognition, configuration, and diagnostics.
5. Pin workflow expansion: file/color pins, pin groups, mouse passthrough, multi-select alignment, z-order persistence, and pin-side annotate/OCR entry points.
6. Capture expansion: scrolling capture, GIF/recording, and delayed/quick recording workflows.

Gate rule per phase:

- Do not advance phase status to completed unless build and tests are green and phase-specific smoke checks are recorded.

## Code Mapping

Primary modules affected by roadmap items:

- Capture expansion: `src/capture/`, `src/ui/OverlayWindow.cpp`, `src/app/ActionDispatcher.cpp`
- Mark expansion: `src/ui/AnnotateWindow.cpp`, `src/app/AppMain.cpp`
- Pin expansion: `src/ui/PinWindow.cpp`, `src/app/PinManager.cpp`
- OCR expansion: `docs/specs/dev_ocr.md`, `src/app/ActionDispatcher.cpp`, future `src/ocr/` module

## Tradeoffs

- Features that require large new modules are intentionally sequenced after baseline reliability, even when reference app supports richer behavior today.
- Current reference app beta features are tracked as references, but formal release readiness requires an explicit SnapPin acceptance target before beta-only parity is considered mandatory.

## Verification

At each milestone:

- Build: release profile succeeds.
- Tests: `snappin_tests` passes.
- Manual workflow checks: capture, pin, and mark core paths still succeed.
- Docs sync: `docs/Implementation-Status.md` and `docs/specs/matrix_reference_parity.md` updated.
