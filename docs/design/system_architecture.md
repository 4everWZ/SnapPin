# SnapPin System Architecture

## Architectural Goals

- Keep core screenshot workflows responsive and predictable.
- Keep action routing explicit and context-validated.
- Separate runtime orchestration from UI/window implementations.
- Keep module boundaries simple enough for incremental extension.

## Module Structure

- `src/app/`: runtime orchestration, action registry/dispatch, hotkeys, config, tray, pin manager wiring.
- `src/ui/`: window and interaction surfaces (overlay, toolbar, annotate, pin, settings).
- `src/capture/`: capture service contracts and backends.
- `src/export/`: clipboard/file export service.
- `src/core/`: shared types, IDs, artifacts, errors, stats contracts.

## Runtime Flow

1. App bootstrap initializes services, windows, and action dispatcher.
2. `ActionRegistry` defines action IDs and legal contexts.
3. `ActionDispatcher` validates context and invokes implementation paths.
4. UI callbacks dispatch actions instead of embedding business logic.
5. Runtime state tracks active artifact, overlay visibility, and annotate session status.

## Interaction Contracts

- Global hotkeys are registered through `KeybindingsService`.
- Capture workflow is centralized through action IDs and overlay/toolbar coordination.
- Pin operations route through `PinManager` and focused-pin state.
- Annotation workflow writes composed bitmap back into active artifact before export.

## Extension Boundaries

Modules expected for future expansion:

- `src/ocr/` behind `SNAPPIN_ENABLE_OCR`
- `src/scroll/` behind `SNAPPIN_ENABLE_SCROLL`
- `src/record/` behind `SNAPPIN_ENABLE_RECORD`

These modules are optional in current build configuration and should not regress baseline behavior when disabled.
