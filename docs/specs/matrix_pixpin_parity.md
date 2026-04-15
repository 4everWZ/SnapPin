# PixPin Parity and Boundary Matrix

Status values:

- `Implemented`
- `Implemented (pending user verification)`
- `Deferred`
- `Not implemented in the current version.`

| Area | PixPin Reference | SnapPin Status | Boundary Decision | Next Step |
| --- | --- | --- | --- | --- |
| Static capture (`Ctrl+1`, tray) | capture/static-capture | Implemented | Baseline committed | Keep DPI/selection stability checks in smoke tests |
| Artifact actions (`Copy/Save/Pin/Mark/Close`) | capture/static-capture | Implemented | Baseline committed | Continue regression validation after mark changes |
| Mark baseline tools (`Rect/Line/Arrow/Pencil/Text`) | mark/base-use + tool pages | Implemented (pending user verification) | Keep current toolset as baseline | Complete targeted manual verification loop |
| Mark advanced tools (`Mosaic/Erase/Polyline parity`) | mark/mosaic, mark/erase, mark/line | Deferred | Deferred to avoid destabilizing baseline mark loop | Phase 2 implementation |
| Pin image workflow and focused actions | pin/base-use, pin/image | Implemented | Baseline committed | Maintain focused action/context checks |
| Pin text mode | pin/text | Not implemented in the current version. | Deferred by scope | Phase 3 implementation |
| Pin LaTeX mode | pin/latex | Not implemented in the current version. | Deferred by scope | Phase 3 implementation |
| OCR trigger and result flow | capture/static-capture, quick-start text recognition | Not implemented in the current version. | Action exists, execution intentionally blocked | Phase 5 implementation |
| Scrolling capture | capture/long-capture | Not implemented in the current version. | Explicitly deferred | Phase 4 implementation |
| Recording/GIF capture | quick-start GIF capture | Not implemented in the current version. | Deferred for baseline-first sequencing | Post Phase 4 |
| Paid-only PixPin VIP features | long-capture auto-crop VIP, advanced paid modules | Not implemented in the current version. | Keep SnapPin free/open-source; no paid gating introduced | Provide open alternatives if/when implemented |

## Boundary Rules for Current Task Cycle

1. Preserve and harden already-implemented core workflows.
2. Do not silently advertise deferred features as implemented.
3. Keep paid-only PixPin behaviors outside mandatory parity target for this repository.
4. If a deferred feature is started, update this matrix and `docs/tradeoffs.md` in the same change.

## Tradeoff References

- `TO-001`: Baseline-first sequencing over immediate full parity
- `TO-002`: Keep free/open-source scope over paid-feature parity
