# SnapPin 1.0 Issue Backlog Draft

## Purpose

Track the GitHub issue split for the 1.0 path without depending on current
remote issue write access.

This file intentionally contains behavior labels and repository references only.
Do not add direct reference URLs to this backlog.

Creation note on 2026-05-14:

- GitHub connector issue creation returned `403 Resource not accessible by personal access token`.
- `gh` CLI is not installed in the current environment.
- These drafts should be copied into GitHub issues once issue write access is available.

## Draft Issues

### 1.0 gate: manual capture, mark, OCR, and pin validation

Goal:

- Record the manual validation gate required before SnapPin can be called
  1.0/release-ready.

Scope:

- Run the manual workflow checklist in `docs/specs/integration_validation.md`.
- Validate capture selection alignment and artifact actions: `Copy`, `Save`,
  `Pin`, `Mark`, `OCR`, `Close`.
- Validate mark creation/edit loops for the current baseline tools, including
  wheel-strength behavior for Mosaic, Blur, Highlighter, Spotlight, Watermark,
  and Magnifier.
- Validate OCR whole-artifact, selected-region, and focused image-pin flows,
  including clipboard text, auto-selected result window text, result-window
  `Copy`, and tray feedback.
- Validate focused pin shortcuts and context menu actions.

Acceptance:

- Manual results are recorded in tracked docs without direct reference URLs.
- Any failed workflow becomes a separate bug or feature issue.
- `docs/specs/status_release_alignment.md` is updated with the validation
  outcome.

### Mark parity phase 3: advanced effects and text tooling

Goal:

- Continue mark feature expansion without collapsing all behavior into
  `AnnotateWindow.cpp`.

Scope:

- Define and implement small slices for smart erase, advanced mosaic/blur
  modes, highlighter/spotlight modes, watermark presets/batch/centered flow,
  advanced magnifier controls, advanced serial formats/settings, text arrows,
  and configurable text background colors.
- Keep each slice covered by focused tests and doc updates.
- Extract helper logic into cohesive files when behavior outgrows local drawing
  code.

Acceptance:

- `docs/specs/dev_mark.md`, `docs/specs/matrix_reference_parity.md`, and
  `docs/specs/status_release_alignment.md` remain aligned after each slice.
- Build, `snappin_tests`, diff check, reference URL scan, and launch smoke pass for
  each committed slice.

### OCR parity phase 4: pinned overlays and recognizer schemas

Goal:

- Expand OCR beyond plain clipboard text without guessing output schemas.

Scope:

- Design a pinned-image OCR overlay result model before UI implementation.
- Define output schemas for formula, QR/barcode, and table recognition before
  implementation.
- Move backend selection, language/model configuration, and diagnostics toward
  a future `src/ocr/` module when the current dispatcher path becomes too
  coupled.

Acceptance:

- `docs/specs/dev_ocr.md` is updated before code changes that affect OCR data
  semantics.
- Runtime OCR clipboard/result-window baseline remains stable.
- Tests cover new data contracts before recognizer-specific UI is claimed.

### Pin parity phase 5: advanced pin workflows

Goal:

- Expand pin behavior while preserving focused-action boundaries.

Scope:

- Implement file pins, color pins, pin groups, mouse passthrough, multi-select
  align, z-order persistence, and taskbar/topmost configuration in separate
  slices.
- Keep image/text/LaTeX pin baselines stable.
- Avoid adding pin-specific branching into unrelated capture or annotate paths.

Acceptance:

- `docs/specs/dev_pin.md` and `docs/specs/matrix_reference_parity.md` are updated
  for each implemented pin slice.
- Focused pin shortcuts and context menu actions remain covered by tests or
  manual validation notes.

### Release packaging after 1.0 baseline

Goal:

- Add GitHub Actions release packaging only after the 1.0 baseline and manual
  validation gate are accepted.

Scope:

- Add CI build/test workflow if missing.
- Add release packaging for the MSVC/Ninja Windows build.
- Add artifact upload and release-tag packaging.
- Keep packaging disabled from claiming 1.0 until the release alignment status
  says the product baseline is accepted.

Acceptance:

- CI runs build and `snappin_tests`.
- Release artifacts are produced from a tag workflow.
- Packaging docs clearly state prerequisites and do not imply reference parity is
  complete before the parity matrix says so.
