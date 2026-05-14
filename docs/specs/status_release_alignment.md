# Release Alignment Status

Updated: 2026-05-14

## Current Objective

Align SnapPin documentation and implementation planning with the current reference app feature reference set, then iterate mark/OCR/UI behavior toward a release candidate without overstating completeness.

## Completion Audit

Current audit result: **not release-ready**.

| User Requirement | Current Evidence | Status | Remaining Gap |
| --- | --- | --- | --- |
| Align SnapPin docs with current reference app reference | Active specs, matrix, README, implementation status, and handoff distinguish baseline behavior from reference parity; latest audited references are reference app `v3.1.4.0` stable and `v3.2.1.3` beta | Partially covered | Re-audit before release because reference app docs/release notes can change |
| Step-by-step reference app feature comparison | `docs/specs/matrix_reference_parity.md` maps capture, mark, pin, OCR, scroll, recording, and paid/pro boundaries | Partially covered | Manual comparison of live UI behavior is still required |
| Iterate UI/action logic with tests | `snappin_tests` covers overlay selection-hole logic, annotate toolbar tool presence/selection, `Text BG` toolbar state, mark composed-copy pixel smoke paths for Rect, Ellipse, Line, Arrow, Serial, Mosaic, Blur, Highlighter, Spotlight, Polyline, Watermark, Magnifier, Pencil, and Text, Text background fill output changes, Mosaic/Blur wheel-strength output changes, Highlighter wheel opacity/strength output changes, Spotlight wheel dim-strength pixel changes, Watermark direct text and wheel opacity/strength output changes, Magnifier wheel zoom internal-pixel changes, Polyline node-drag edit output change, Polyline segment double-click node insertion, Polyline selected-node deletion, Eraser deletion restoring source pixels, Eraser path-segment removal preserving remaining Polyline pixels, layout helpers, mosaic/OCR helpers, focused-pin OCR context, OCR result-window text selection, OCR result-window repeat-copy callback, OCR result-window empty-result `Copy` disabled state, and tray notification safety | Partially covered | Tests still do not verify actual clipboard contents, OCR runtime recognition, visible tray notifications, advanced/deferred mark tools, or full manual mouse workflows |
| Improve mark tools that are far behind reference app | Added/covered `Ellipse`, `Serial`, `Mosaic`, `Blur`, `Eraser`, `Highlighter`, `Spotlight`, editable multi-click `Polyline`, basic rectangular `Watermark`, and basic rectangular `Magnifier` baseline tools; Polyline now supports basic node drag, whole move, segment double-click node insertion, and selected-node deletion; Text supports a basic background fill toggle; Highlighter supports wheel opacity/strength; Spotlight supports wheel dim-strength; Watermark supports direct text entry and wheel opacity/strength; Magnifier supports wheel zoom; Serial supports direct numeric entry and `+`/`-` value adjustment; Mosaic/Blur support mouse-wheel strength; Eraser supports basic path-segment removal | Partially covered | Missing richer polyline editing/manual verification, smart erase, advanced watermark presets/batch/centered workflow, advanced magnifier modes/configuration, advanced blur/highlighter/spotlight modes, Auto Mosaic/synced mosaic operations, advanced serial formats/settings, text arrows and configurable text background colors, full reference app partial/reverse erasing |
| Improve OCR behavior that is far behind reference app | OCR now covers active artifact, selected region, and focused image pin source paths with clipboard copy, auto-selected selectable result window plus repeat-copy action that is disabled for empty text, tray feedback, and declared source/region action parameters | Partially covered | Missing pin OCR text overlay, formula/QR/barcode/table recognition, language/model configuration, advanced result management, and runtime OCR manual verification |
| Release-ready / formal version | Build, CTest, diff check, and launch smoke pass for current slice | Not covered | Manual end-to-end capture/mark/OCR/pin verification and missing parity decisions are still blockers |

## Prompt-to-Artifact Checklist

| Prompt Requirement / Gate | Concrete Artifact or Command | Evidence Status | Coverage Caveat |
| --- | --- | --- | --- |
| "Continue aligning SnapPin docs" | `README.md`, `docs/Implementation-Status.md`, `docs/HANDOFF.md`, `docs/specs/00_overview.md`, `docs/specs/dev_capture.md`, `docs/specs/dev_mark.md`, `docs/specs/dev_ocr.md`, `docs/specs/dev_pin.md`, `docs/specs/dev_shortcuts_actions.md`, `docs/specs/dev_roadmap.md`, `docs/specs/integration_validation.md`, `docs/specs/issue_backlog_1_0.md`, `docs/specs/matrix_reference_parity.md`, this status file | Updated in this iteration set | Docs still need re-audit before release if reference app publishes new changes |
| "Check against reference app features" | `docs/specs/matrix_reference_parity.md` and reference app references in `docs/Implementation-Status.md` | Matrix distinguishes implemented, partial, deferred, and not implemented areas | Does not replace manual side-by-side UI comparison against reference app |
| "Mark tools are far behind" | `src/ui/AnnotateWindow.h`, `src/ui/AnnotateWindow.cpp`, `src/ui/AnnotationEffects.h`, `src/ui/AnnotateLayout.h` | Added/covered Ellipse, Serial, Mosaic, Blur, Eraser, Highlighter, Spotlight, editable multi-click Polyline, basic rectangular Watermark, basic rectangular Magnifier, Text BG, serial direct numeric entry/adjustment, Highlighter wheel opacity/strength, Spotlight wheel dim-strength, Watermark direct text and wheel opacity/strength, Magnifier wheel zoom, Mosaic/Blur wheel strength, layout helpers, composed-copy pixel smoke paths for baseline drawing tools, Polyline node-edit, segment insertion, and selected-node deletion coverage, Eraser deletion source-restore coverage, and Eraser path-segment coverage | Advanced/deferred reference app mark tools and full manual mouse workflows are not yet automated |
| "OCR is far behind" | `src/app/ActionDispatcher.cpp`, `src/app/OcrRegion.h`, `src/app/OcrResultEvent.h`, `src/app/AppMain.cpp`, `src/app/ActionRegistry.cpp`, `src/app/PinManager.*`, `src/ui/OcrResultWindow.*`, `src/ui/PinWindow.*`, `src/app/TrayIcon.*` | OCR source paths cover active artifact, selected region, and focused image pin; `ocr.start` source/region parameters are declared; result text is copied and shown auto-selected in a selectable window; result-window repeat copy is callback-tested and disabled for empty result text; tray feedback exists | Pin OCR overlays and advanced recognizers are still missing |
| Tests gate | `cmake --build "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --config Release --target all --`; `ctest --test-dir "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --output-on-failure -C Release` | Passed for current slice | Unit tests do not cover every release requirement |
| Whitespace gate | `git diff --check` | Passed with CRLF warnings only | Untracked files are not covered by `git diff --check` until added or staged |
| Launch smoke gate | Start `build\MSVC v143 x64 (vcvars64 + Ninja)-Release\bin\snappin.exe`, wait 2 seconds, stop if still alive | Passed for current slice | Smoke only proves startup survival, not user workflow correctness |
| Release-ready claim | Manual capture/mark/OCR/pin workflow execution, reference parity decisions, and missing advanced features | Not satisfied | Do not claim formal release readiness |

## Accepted Scope

### In Scope

- Static capture, artifact toolbar, pin baseline, mark baseline, and OCR baseline already present in the codebase.
- reference parity audit for mark, OCR, pin, capture expansion, and release-note deltas.
- Documentation that clearly separates implemented, partially implemented, deferred, and not implemented behavior.

### Explicitly Out of Scope

- Claiming formal release readiness before manual UI workflow verification.
- Treating reference app beta-only behavior as mandatory without an accepted release parity target.
- Paid/Pro feature gating.

## Current State

### Implemented

- Static capture and artifact action baseline.
- Capture-context mark baseline: select, rectangle, ellipse, straight line, arrow, serial number with direct numeric entry and `+`/`-` adjustment, mosaic and blur with wheel strength, basic whole-object/path-segment eraser, basic freehand highlighter with wheel opacity/strength, basic rectangular spotlight with wheel dim-strength, basic multi-click polyline with node drag / whole move, segment double-click node insertion, and selected-node deletion, basic rectangular watermark with direct text entry and wheel opacity/strength, basic rectangular magnifier with wheel zoom, pencil, text with background fill toggle, undo/redo, delete, reselect, composed copy/save.
- Image pin baseline and clipboard text/LaTeX-like fallback pins.
- Basic OCR over active artifact bitmap, selected OCR region, or focused image pin, copied to clipboard, shown in an auto-selected selectable result window with a repeat-copy action that is disabled for empty result text, and paired with tray success/failure feedback; `ocr.start` declares optional `source`, `x`, `y`, `w`, and `h` action parameters.

### Partially Implemented

- reference app mark parity: many tools and advanced controls are missing.
- reference app OCR parity: focused image pin OCR now exists as a clipboard/result-window baseline with repeat-copy support, but pinned-image OCR overlays and advanced recognition flows are missing.
- reference app pin parity: file/color pins, pin groups, mouse passthrough, and multi-select alignment are missing.

### Deferred / Not Implemented

- Scrolling capture, GIF/recording, formula recognition, QR/barcode recognition, table recognition, advanced mark pixel effects, and pin groups.

## Validation Snapshot

- Verified:
  - `cmake --build "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --config Release --target all --`
  - `ctest --test-dir "build/MSVC v143 x64 (vcvars64 + Ninja)-Release" --output-on-failure -C Release`
  - Launch smoke: `snappin.exe` started and was stopped after remaining alive for 2 seconds.
- Not yet verified:
  - Manual UI mark loop.
  - Manual OCR whole-artifact and region flow.
  - Runtime OCR result window, result-window copy, clipboard contents, and tray notification after OCR.
  - reference parity behavior for missing mark/OCR tools.
- Relevant tests / harnesses / benchmarks / commands:
  - `snappin_tests` covers core rect defaults, overlay selection-hole logic, AnnotateWindow tool button presence/selection for Ellipse, Serial, Mosaic, Blur, Eraser, Highlighter, Spotlight, Polyline, Watermark, Magnifier, and Text BG, mark composed-copy pixel changes for Rect, Ellipse, Line, Arrow, Serial, Mosaic, Blur, Highlighter, Spotlight, Polyline, Watermark, Magnifier, Pencil, and Text, Text background fill output changes, Mosaic/Blur wheel-strength output changes, Highlighter wheel opacity/strength output changes, Spotlight wheel dim-strength pixel changes, Watermark direct text and wheel opacity/strength output changes, Magnifier wheel zoom internal-pixel changes, Polyline node editing changing composed output, Polyline segment double-click node insertion preserving endpoints, Polyline selected-node deletion preserving remaining segments, Eraser deletion restoring source pixels, Eraser path-segment removal preserving remaining Polyline pixels, serial value adjustment clamping, direct serial numeric entry changing composed output, annotate toolbar min-width and oversized work-area clamp calculations, mosaic block-size calculation, OCR region-to-bitmap crop mapping, `ocr.start` focused-pin context and source/region parameter registration, OCR text progress-event UTF-8 payload roundtrip, OCR result window selectable text behavior, OCR result-window auto-selection, OCR result-window repeat-copy callback, OCR result-window empty-result `Copy` disabled state, image-pin-only OCR command eligibility, and TrayIcon notification safety when uninitialized.
  - `snappin_tests` still does not cover OCR runtime recognition, clipboard contents, tray notification display, manual UI workflows, advanced/deferred mark tools, or pin parity.

## Active Blockers or Open Questions

- Formal SnapPin release parity target is not accepted yet: current reference app stable only, or stable plus selected beta features.
- OCR language/model requirements are not accepted yet.
- Mark advanced tool order is not accepted yet.
- GitHub issue creation is currently blocked by credential/tooling limits; 1.0 issue drafts are tracked in `docs/specs/issue_backlog_1_0.md`.

## Recommended Next Steps

1. Confirm release parity target: current stable reference or stable plus selected beta features.
2. Manually smoke-test current mark/OCR baselines, including Blur, Watermark, Magnifier, the selectable OCR result window, auto-selection, empty-result disabled `Copy`, and the result-window `Copy` action, then record results.
3. Implement remaining mark parity in small verified slices, starting with richer polyline edit-loop verification or advanced effect controls.
4. Design pinned-image OCR overlay UX before expanding formula/QR/barcode/table recognition.
5. Re-audit reference app stable/beta release notes before any release-ready claim.

## Key References

- Overview: `docs/specs/00_overview.md`
- Relevant leaf docs: `docs/specs/dev_mark.md`, `docs/specs/dev_capture.md`, `docs/specs/dev_ocr.md`, `docs/specs/dev_pin.md`, `docs/specs/dev_shortcuts_actions.md`
- Matrix: `docs/specs/matrix_reference_parity.md`
- Integration: `docs/specs/integration_validation.md`
- Tradeoff IDs: `TO-001`, `TO-002`
- Design docs: `docs/design/system_architecture.md`

## Notes for Next Thread

- Preserve the distinction between "baseline exists" and "reference parity".
- Do not call OCR or mark formally complete until manual UI verification and dedicated coverage exist.
- Keep docs, matrix, and status in sync with each behavior-changing iteration.

## Optional Recent Accepted Milestones

- 2026-05-14 / parity audit - active docs updated to current reference app stable/beta references and corrected mark/OCR status from overbroad "implemented" claims to partial parity.
- 2026-05-14 / mark iteration - added Ellipse, Serial, and basic rectangular Mosaic tools with focused UI button tests.
- 2026-05-14 / OCR feedback iteration - added TrayIcon notification plumbing and OCR action success/failure tray feedback; runtime display still needs manual verification.
- 2026-05-14 / OCR action contract iteration - declared `ocr.start` optional `source`, `x`, `y`, `w`, and `h` parameters in the action registry with focused tests.
- 2026-05-14 / OCR result window iteration - added an OCR text progress event payload and selectable read-only OCR result window shown after successful clipboard copy; runtime OCR/manual clipboard/tray verification remains incomplete.
- 2026-05-14 / OCR result copy iteration - added a result-window `Copy` action that routes displayed OCR text back through the export clipboard path; runtime OCR/manual clipboard/tray verification remains incomplete.
- 2026-05-14 / OCR result selection iteration - auto-selects displayed OCR result text for immediate copy/edit-control use; runtime OCR/manual clipboard/tray verification remains incomplete.
- 2026-05-14 / OCR result copy availability iteration - disables the OCR result-window `Copy` action for empty displayed text and re-enables it for non-empty text; runtime OCR/manual clipboard/tray verification remains incomplete.
- 2026-05-14 / OCR spec split - added `dev_ocr.md` to keep OCR source selection, result flow, future recognizer boundaries, and verification gates out of capture/pin-specific docs.
- 2026-05-14 / mark highlighter strength iteration - made Highlighter use mouse-wheel strength to adjust center opacity while preserving the existing freehand baseline; reference-style highlighter modes/configuration remain unimplemented.
- 2026-05-14 / mark watermark strength iteration - made Watermark use mouse-wheel strength to adjust opacity while preserving the basic manually placed text workflow; reference-style watermark presets/batch/centered workflow remain unimplemented.
- 2026-05-14 / 1.0 issue backlog draft - GitHub issue creation was blocked by credential/tooling limits, so the 1.0 issue split was recorded in `docs/specs/issue_backlog_1_0.md`.
- 2026-05-14 / coverage hardening iteration - extracted mosaic block-size and OCR region crop mapping helpers with focused regression coverage; runtime mark drawing and OCR still need manual verification.
- 2026-05-14 / focused pin OCR iteration - allowed `ocr.start` from a focused image pin and exposed it through the image pin context menu by reusing the artifact OCR source path; selectable pin OCR text remains unimplemented.
- 2026-05-14 / mark eraser iteration - added a basic whole-object Eraser tool with toolbar coverage.
- 2026-05-14 / mark eraser path iteration - added basic Eraser path-segment removal for path annotations with Polyline regression coverage; full reference-style partial erasing and reverse mosaic erase remain unimplemented.
- 2026-05-14 / mark highlighter iteration - added a basic freehand translucent Highlighter tool with toolbar coverage; reference-style highlighter modes/configuration remain unimplemented.
- 2026-05-14 / UI layout hardening iteration - extracted annotate toolbar width and work-area clamp helpers with coverage so toolbar expansion does not push the annotate window off-screen.
- 2026-05-14 / serial adjustment iteration - added `+`/`-` serial value adjustment helper coverage and keyboard handling.
- 2026-05-14 / serial direct entry iteration - added direct numeric serial entry for the selected or next serial value with composed-output regression coverage; advanced serial formats/settings remain unimplemented.
- 2026-05-14 / mark spotlight iteration - added a basic rectangular Spotlight tool with toolbar coverage.
- 2026-05-14 / mark spotlight strength iteration - made Spotlight consume wheel strength for dim amount and added outside-focus pixel regression coverage; reference-style spotlight modes/configuration remain unimplemented.
- 2026-05-14 / mark text background iteration - added a discoverable Text BG toolbar/menu toggle with composed-output regression coverage; text arrows and configurable background colors remain unimplemented.
- 2026-05-14 / mark blur iteration - added a basic rectangular Blur tool with toolbar, selection, and composed-copy pixel smoke coverage; reference-style smart erase and advanced blur controls remain unimplemented.
- 2026-05-14 / mark effect strength iteration - made Mosaic and Blur consume mouse-wheel strength and added composed-output regression coverage; Auto Mosaic, synced mosaic operations, and advanced effect modes remain unimplemented.
- 2026-05-14 / mark pixel smoke iteration - added composed-copy pixel regression paths for baseline drawing tools through real annotate mouse messages and the Copy command, plus Eraser deletion source-restore coverage; advanced/deferred mark tools and manual UI verification remain incomplete.
- 2026-05-14 / mark polyline iteration - added a basic multi-click Polyline tool with toolbar, selection, and composed-copy pixel smoke coverage.
- 2026-05-14 / mark polyline edit iteration - added basic Polyline node drag / whole move editing with composed-output regression coverage.
- 2026-05-14 / mark polyline insert iteration - added segment double-click node insertion with regression coverage that verifies inserted-node dragging does not move existing endpoints.
- 2026-05-14 / mark polyline delete iteration - added selected-node deletion with regression coverage that verifies remaining segments are preserved; richer controls and manual edit-loop verification remain incomplete.
- 2026-05-14 / mark watermark iteration - added a basic manually placed Watermark tool with toolbar, selection, and composed-copy pixel smoke coverage.
- 2026-05-14 / mark watermark text iteration - added direct keyboard text entry for Watermark with composed-output regression coverage; reference-style presets/batch/centered workflow remains unimplemented.
- 2026-05-14 / mark magnifier iteration - added a basic rectangular Magnifier tool with toolbar, selection, and composed-copy pixel smoke coverage.
- 2026-05-14 / mark magnifier zoom iteration - made Magnifier consume wheel strength as zoom and added internal-pixel regression coverage; reference-style magnifier modes/configuration remain unimplemented.
