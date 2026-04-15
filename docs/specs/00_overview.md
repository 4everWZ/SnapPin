# SnapPin Active Specification Overview

## Purpose

This directory is the active source of truth for product behavior, implementation boundaries, and parity tracking.

SnapPin goals for this repository:

- Keep SnapPin fully free and open-source.
- Deliver practical Windows screenshot, annotate, and pin workflows.
- Track parity against PixPin using explicit boundaries and status labels.

## Scope Boundaries

Current in-scope product surface:

- Static capture workflow (`Ctrl+1`) with frozen-frame selection.
- Artifact actions (`Copy`, `Save`, `Pin`, `Mark`, `Close`) and context-aware errors.
- Pin baseline (`Ctrl+2`, focused pin shortcuts, lock, close/destroy operations).
- Mark baseline in capture context (Rect/Line/Arrow/Pencil/Text, undo/redo, reselect).

Current out-of-scope or deferred surface:

- Scrolling capture.
- Recording.
- OCR workflow.
- Advanced mark tools (mosaic, erase, polyline parity).
- Non-image pin types (text pin, LaTeX pin).

## Active Spec Topology

- `dev_capture.md`: capture and artifact lifecycle contracts.
- `dev_pin.md`: pin lifecycle, focused actions, and interaction model.
- `dev_mark.md`: annotate session rules and tool/edit contracts.
- `dev_shortcuts_actions.md`: action IDs, contexts, and keyboard mapping.
- `dev_roadmap.md`: execution phases and acceptance gates.
- `matrix_pixpin_parity.md`: PixPin parity matrix and current task boundaries.
- `integration_validation.md`: end-to-end build and verification checklist.

## Related Docs

- Architecture: `docs/design/system_architecture.md`
- Tradeoffs: `docs/tradeoffs.md`
- Implementation status log: `docs/Implementation-Status.md`
- Historical specs snapshot: `docs/specs/legacy/`
