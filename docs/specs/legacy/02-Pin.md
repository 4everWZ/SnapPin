# SnapPin Spec - Pin

## Creation sources

- From capture artifact toolbar (`Pin`).
- From clipboard (`Ctrl+2`).

Clipboard source priority:
- If image exists: pin image directly.
- If text exists without image: text-to-pin path (planned).

## Base pin behavior

- Multiple pin windows can coexist.
- Pin windows are independently movable.
- Pin focus state is tracked for focused-pin actions.

## Pin interactions

- Drag left mouse to move.
- Mouse wheel to scale.
- `Ctrl + Mouse wheel` to change opacity.
- `L` toggles lock.
- Middle click resets scale/opacity baseline.

## Pin context menu baseline

- `Copy`
- `Save`
- `Close`
- `Destroy`
- `Close All`
- `Destroy All`
- `Lock/Unlock`

## Focused-pin shortcuts

- `Ctrl+C` copy focused pin.
- `Ctrl+S` save focused pin.
- `Ctrl+W` close focused pin.
- `Ctrl+Shift+W` close all pins.
- `Ctrl+D` destroy focused pin.

## Planned parity extensions

- Pin text mode.
- Pin LaTeX mode.
- Pin-side annotate/OCR actions.
- Pin restore persistence policy.
