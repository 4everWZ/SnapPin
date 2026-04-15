# Mark Specification

## Goals and Boundaries

- Keep annotation session inside the active capture context.
- Preserve low-friction edit loop: annotate, reselect, export, dismiss.
- Support baseline tools needed for daily screenshot markup.
- Keep advanced PixPin mark tools explicitly deferred until baseline remains stable.

## Math / Logic / Interfaces

Session rules:

1. `annotate.open` starts mark session for current active artifact.
2. Overlay interaction is disabled while annotate session is active.
3. `R`/Range re-enters area selection without leaving capture session.
4. First `Esc` exits current mark selection/edit state.
5. Next `Esc` exits capture session when no active mark edit exists.

Baseline tools and commands:

- Tools: `Select`, `Rect`, `Line`, `Arrow`, `Pencil`, `Text`
- Editing: move/resize geometry, adjust line/arrow endpoints, text input/move
- History: `Ctrl+Z`/`Ctrl+Y`
- Delete selected shape/text with `Delete`
- Export composed result via `Ctrl+C` and `Ctrl+S`

## Code Mapping

- Annotate surface and editing runtime: `src/ui/AnnotateWindow.h`, `src/ui/AnnotateWindow.cpp`
- Session wiring and command callbacks: `src/app/AppMain.cpp`
- Action routing and guards: `src/app/ActionDispatcher.cpp`
- Export integration for composed bitmap: `src/export/ExportService.cpp`

## Tradeoffs

- Toolset is intentionally limited to baseline mark operations to avoid introducing unstable UX during capture session transitions.
- Advanced tools (mosaic, erase, polyline parity, rich text controls) are deferred and tracked in roadmap/matrix docs.

## Verification

Required verification for mark-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Manual smoke path:
  - Start capture and enter mark session.
  - Draw with each baseline tool and validate edit/selection interactions.
  - Validate undo/redo and delete behavior.
  - Validate `R` reselect path and double-`Esc` session exit rule.
  - Validate composed image copy/save output.
