# SnapPin Active Specification Overview

## Purpose

This directory is the active source of truth for product behavior, implementation boundaries, and parity tracking.

SnapPin goals for this repository:

- Keep SnapPin fully free and open-source.
- Deliver practical Windows screenshot, annotate, and pin workflows.
- Track parity against reference app using explicit boundaries and status labels.

## Scope Boundaries

Current in-scope product surface:

- Static capture workflow (`Ctrl+1`) with frozen-frame selection.
- Artifact actions (`Copy`, `Save`, `Pin`, `Mark`, `OCR`, `Close`) and context-aware errors.
- Pin baseline (`Ctrl+2`, focused pin shortcuts, lock, close/destroy operations, clipboard text/LaTeX-like fallback).
- Mark baseline in capture context (Rect/Ellipse/Line/Polyline/Arrow/Serial/Mosaic/Blur/Eraser/Highlighter/Spotlight/Watermark/Magnifier/Pencil/Text, undo/redo, reselect).
- OCR baseline for active artifact bitmap, user-selected OCR region, or focused image pin, with recognized text copied to clipboard and shown in an auto-selected selectable result window.

Current out-of-scope or deferred surface:

- Scrolling capture.
- Recording.
- reference-level OCR overlays on pinned images, formula recognition, QR/barcode recognition, table recognition, and advanced OCR result management.
- Advanced mark tools (advanced polyline add/remove node controls, advanced blur/smart erase, advanced highlighter/spotlight modes, reference-style partial erasing, Auto Mosaic/synced mosaic operations, configurable/centered watermark workflow, advanced magnifier controls, and advanced serial formats/settings).
- Non-clipboard pin types such as file pin, color pin, pin groups, and mouse passthrough.

## Active Spec Topology

- `dev_capture.md`: capture and artifact lifecycle contracts.
- `dev_pin.md`: pin lifecycle, focused actions, and interaction model.
- `dev_mark.md`: annotate session rules and tool/edit contracts.
- `dev_ocr.md`: OCR source selection, result flow, parity gaps, and future recognizer boundaries.
- `dev_shortcuts_actions.md`: action IDs, contexts, and keyboard mapping.
- `dev_roadmap.md`: execution phases and acceptance gates.
- `matrix_reference_parity.md`: reference parity matrix and current task boundaries.
- `integration_validation.md`: end-to-end build and verification checklist.

## Related Docs

- Architecture: `docs/design/system_architecture.md`
- Tradeoffs: `docs/tradeoffs.md`
- Implementation status log: `docs/Implementation-Status.md`
- Historical specs snapshot: `docs/specs/legacy/`
