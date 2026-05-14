# Mark Specification

## Goals and Boundaries

- Keep annotation session inside the active capture context.
- Preserve low-friction edit loop: annotate, reselect, export, dismiss.
- Support baseline tools needed for daily screenshot markup.
- Track reference app mark parity explicitly instead of treating the current baseline as complete parity.
- Keep advanced reference app mark tools explicitly deferred until baseline remains stable.

## Math / Logic / Interfaces

Session rules:

1. `annotate.open` starts mark session for current active artifact.
2. Overlay interaction is disabled while annotate session is active.
3. `R`/Range re-enters area selection without leaving capture session.
4. First `Esc` exits current mark selection/edit state.
5. Next `Esc` exits capture session when no active mark edit exists.

Baseline tools and commands:

- Tools: `Select`, `Rect`, `Ellipse`, `Line`, `Polyline`, `Arrow`, `Serial`, `Mosaic`, `Blur`, `Eraser`, `Highlighter`, `Spotlight`, `Watermark`, `Magnifier`, `Pencil`, `Text`
- Editing: move/resize geometry, adjust line/arrow endpoints, text input/move
- History: `Ctrl+Z`/`Ctrl+Y`
- Delete selected shape/text with `Delete`
- Export composed result via `Ctrl+C` and `Ctrl+S`

Known reference parity gaps:

- Shape tools: rectangle and ellipse are implemented.
- Line tools: straight line and basic multi-click polyline are implemented; basic polyline node drag, whole-polyline move, segment double-click node insertion, and selected-node deletion are implemented, but richer editing parity is not implemented.
- Serial Number: auto-increment placement, direct numeric entry, and `+`/`-` value adjustment are implemented; advanced serial formats/settings are not implemented.
- Mosaic/Blur: basic rectangular pixelation and basic rectangular blur are implemented with mouse-wheel strength control; smart erase, auto mosaic, synced mosaic operations, and advanced effect modes are not implemented.
- Eraser: basic whole-object annotation deletion and path-segment erasing for path tools are implemented; full reference-style partial erasing and mosaic reverse-erase behavior are not implemented.
- Highlighter: basic freehand translucent highlighter is implemented; reference-style highlighter modes and configuration are not implemented.
- Spotlight: basic rectangular spotlight is implemented; reference-style spotlight modes and configuration are not implemented.
- Watermark: a basic manually placed rectangular text watermark is implemented; reference-style configurable/centered watermark workflow is not implemented.
- Magnifier: a basic rectangular 2x magnifier is implemented; reference-style magnifier modes and configuration are not implemented.
- Pin-side annotation entry is not implemented; SnapPin annotation is currently capture-artifact scoped.
- Advanced text features from current reference app beta notes, including text arrows and text background color settings, are not implemented.

## Code Mapping

- Annotate surface and editing runtime: `src/ui/AnnotateWindow.h`, `src/ui/AnnotateWindow.cpp`
- Annotation effect helpers: `src/ui/AnnotationEffects.h`
- Annotation toolbar layout helpers: `src/ui/AnnotateLayout.h`
- Session wiring and command callbacks: `src/app/AppMain.cpp`
- Action routing and guards: `src/app/ActionDispatcher.cpp`
- Export integration for composed bitmap: `src/export/ExportService.cpp`

## Tradeoffs

- Toolset is intentionally limited to baseline mark operations to avoid introducing unstable UX during capture session transitions.
- Advanced tools are deferred and tracked in roadmap/matrix docs; do not describe this area as reference app-complete until the parity gaps above are implemented and smoke-tested.

## Verification

Required verification for mark-related changes:

- Build succeeds for default release profile.
- Unit test target `snappin_tests` passes.
- Automated smoke coverage verifies that baseline drawing tools created through annotate mouse messages change composed `Copy` output pixels, and that Eraser deletion restores copied pixels to the source bitmap.
- Manual smoke path:
  - Start capture and enter mark session.
  - Draw with each baseline tool and validate edit/selection interactions, including Eraser deletion of editable annotations and Spotlight/Watermark/Magnifier move/resize.
  - Validate undo/redo and delete behavior.
  - Validate `R` reselect path and double-`Esc` session exit rule.
  - Validate composed image copy/save output.
