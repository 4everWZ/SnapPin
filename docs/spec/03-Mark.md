# SnapPin Spec - Mark

## Entry and session model

- `Mark` opens from current capture artifact.
- Mark session must stay in capture workflow, not switch to a separate pin workflow.
- Mark supports reselecting the capture range without leaving current capture session.

## Session control rules

- First `Esc`: exit current mark edit/selection state.
- Next `Esc` (when no mark is selected/being edited): exit capture session.
- `R` or toolbar `Range`: re-enter area selection and continue mark on new captured area.

## Base mark tools

- `Select`
- `Rect`
- `Line`
- `Arrow`
- `Pencil`
- `Text`

## Base editing operations

- Undo/redo.
- Delete selected editable annotation.
- Drag/resize for geometric shapes.
- Move endpoints for line/arrow.
- Text input and move text block.

## Advanced parity targets (not finished)

- Geometric style parity (fill/stroke variants).
- Multi-segment line/polyline interaction parity.
- Mosaic tool.
- Erase tool.
- Rich text editor behaviors and style controls.
