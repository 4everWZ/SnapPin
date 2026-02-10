# SnapPin Spec - Pin

## Creation paths

- From capture toolbar `Pin` action.
- From clipboard using `Ctrl+2`.

Clipboard priority:
- If clipboard has image data: pin image directly.
- If no image but text exists: render text to image, then pin (planned).

## Multi-pin behavior

- Multiple pin windows can exist concurrently.
- Each pin is independently movable and controllable.

## Pin window interactions

- Drag with left mouse button to move.
- Mouse wheel to scale.
- `Ctrl + wheel` to change opacity.
- `L` to toggle lock state.
- Middle click to reset scale/opacity.

## Context menu (must)

- `Copy`
- `Save`
- `Close` (hide current pin)
- `Destroy` (remove current pin permanently)
- `Close All` (hide all pins)
- `Destroy All` (remove all pins)
- `Lock/Unlock`

## Keyboard actions

- `Ctrl+C` copies focused pin image.
- `Ctrl+S` saves focused pin image.
- `Ctrl+W` closes focused pin.
- `Ctrl+Shift+W` closes all pins.
- `Ctrl+D` destroys focused pin.

## Runtime state

- App tracks focused pin ID for context-sensitive actions.
- Pin operations must update focus state safely.

## Future parity items

- Full right-click action set with annotate/ocr on pin
- Pin persistence policy and restore behavior
- Fine-grained scale/opacity config parity

