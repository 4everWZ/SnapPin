# reference app Parity and Boundary Matrix

Last audited: 2026-05-14

Reference baseline:

- Current reference app stable reference: `v3.1.4.0` release notes dated 2026-04-23.
- Current reference app beta reference found during audit: `v3.2.1.3` release notes dated 2026-05-13.
- Formal SnapPin release parity target is not yet accepted; beta-only reference app behavior is tracked as a watchlist until the target is decided.

Status values:

- `Implemented`: Code exists and repository verification covers the claimed behavior.
- `Implemented (pending user verification)`: Code exists, but manual UX confirmation is still required.
- `Partially implemented`: A baseline exists, but reference parity is materially incomplete.
- `Deferred`: Intentionally out of the current implementation slice.
- `Not implemented in the current version.`

| Area | Reference Baseline | SnapPin Status | Boundary Decision | Next Step |
| --- | --- | --- | --- | --- |
| Static capture (`Ctrl+1`, tray) | `capture/static-capture`, quick start | Implemented | Baseline committed | Keep DPI/selection stability checks in smoke tests |
| Artifact actions (`Copy/Save/Pin/Mark/OCR/Close`) | `capture/static-capture`, quick start | Partially implemented | Copy/save/pin/mark/close are baseline; OCR is basic artifact OCR only | Smoke-test toolbar order and OCR region flow |
| Mark capture baseline (`Rect/Ellipse/Line/Polyline/Arrow/Serial/Mosaic/Blur/Eraser/Highlighter/Spotlight/Watermark/Magnifier/Pencil/Text`) | `mark/base-use`, `mark/geo`, `mark/line`, `mark/arrow`, `mark/serial`, `mark/mosaic`, `mark/erase`, `mark/highlight`, `mark/watermark`, `mark/magnifier`, `mark/pencil`, `mark/text` | Partially implemented | SnapPin has a usable baseline, not reference parity; polyline supports basic multi-click creation, node drag, whole move, segment double-click node insertion, and selected-node deletion, serial is auto-increment with direct numeric entry and `+`/`-` adjustment only, mosaic and blur are basic rectangular effects with wheel strength only, eraser supports whole-object deletion and basic path-segment removal only, highlighter is basic freehand only, spotlight is basic rectangular dimming with wheel strength only, watermark is basic manually placed text with direct text entry only, magnifier is basic rectangular zoom with wheel adjustment only, and text has a basic background fill toggle | Complete user verification loop, then implement missing mark tools in small slices |
| Mark shape parity | `mark/geo`, `mark/line` | Partially implemented | Rectangle, ellipse, straight line, basic editable polyline, and arrow exist; polyline segment double-click node insertion and selected-node deletion exist, but richer editing parity and manual edit-loop verification are missing | Manually verify polyline edit loop before treating polyline as stable |
| Mark advanced tools | `mark/mark-pencil`, `mark/mosaic`, `mark/erase`, `mark/highlight`, `mark/watermark`, `mark/magnifier` | Partially implemented | Basic mosaic/blur with wheel strength, whole-object/path-segment eraser, freehand highlighter, rectangular spotlight with wheel dim strength, manually placed text watermark with direct text entry, and rectangular magnifier with wheel zoom exist; missing smart erase, full partial/reverse erasing, Auto Mosaic, synced mosaic operations, advanced blur/highlighter/spotlight modes, reference-style watermark presets/batch/centered workflow, and advanced magnifier modes/configuration | Phase 3 mark expansion |
| reference app 3.1/3.2 mark additions | `official-log/3-1-4-0`, `change-log/3-2-1-3` | Partially implemented | Direct serial number entry, direct watermark text entry, and basic text background fill have baselines; Auto Mosaic, synced mosaic operations, advanced serial formats/settings, text arrows, text background color config beyond the fill toggle, and reference-style centered/configurable watermark workflow are missing | Track after baseline mark model supports advanced tools |
| Pin image workflow and focused actions | `pin/base-use`, `pin/image` | Implemented (pending user verification) | Baseline image pin and focused actions are present | Maintain focused action/context checks |
| Pin text mode | `pin/text` | Implemented (pending user verification) | Implemented as clipboard text pin with dedicated text rendering | Validate editing/selection parity against reference app details |
| Pin LaTeX mode | `pin/latex`, `other/formula` | Partially implemented | Clipboard LaTeX-like text pin exists; reference-style formula recognition is missing | Decide whether formula recognition is mandatory for release parity |
| Pin advanced parity | `pin/file`, `pin/color`, `pin/pin-group`, release notes | Not implemented in the current version. | Missing file/color pins, pin groups, mouse passthrough, multi-select align, z-order persistence, taskbar/topmost config | Phase 5 pin expansion |
| OCR trigger and copy flow | quick start text recognition, `configuration/system`, `other/formula`, release notes | Partially implemented | Uses Windows system OCR over active artifact, selected region, or focused image pin, copies text, emits tray feedback, and shows an auto-selected selectable result window with a repeat-copy action; does not expose selectable recognized text overlays on pinned images | Manually verify runtime OCR result window, result-window copy, and clipboard contents |
| OCR advanced recognition | quick start text recognition, `other/formula`, release notes | Not implemented in the current version. | Missing pinned-image selectable OCR overlays, automatic pin OCR, formula recognition, QR/barcode recognition, table recognition, language/model config, and advanced result management UI | Phase 4 OCR expansion |
| Scrolling capture | `capture/long-capture`, beta overflow mode | Not implemented in the current version. | Explicitly deferred | Phase 6 implementation |
| Recording/GIF capture | `capture/gif-capture2`, release notes | Not implemented in the current version. | Deferred for baseline-first sequencing | Phase 6/post-release candidate |
| Paid/Pro reference app features | member/pro release notes | Not implemented in the current version. | Keep SnapPin free/open-source; no paid gating introduced | Provide open alternatives if implemented |

## Boundary Rules for Current Task Cycle

1. Preserve and harden already-implemented core workflows.
2. Do not silently advertise deferred features as implemented.
3. Keep paid-only reference app behaviors outside mandatory parity target for this repository.
4. If a deferred feature is started, update this matrix and `docs/tradeoffs.md` in the same change.

## Tradeoff References

- `TO-001`: Baseline-first sequencing over immediate full parity
- `TO-002`: Keep free/open-source scope over paid-feature parity
