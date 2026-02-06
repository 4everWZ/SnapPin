# SnapPin Spec - Scope

## Goal

Build a practical Windows screenshot utility with behavior aligned to PixPin:
- Fast capture flow
- Fast post-capture actions
- Persistent pin windows for reference work

## Non-goals (current phase)

- Cross-platform UI support
- Cloud sync or account system
- Full plugin ecosystem

## Product principles

- Capture must feel instant
- Common actions must stay keyboard-first
- No silent failures; user-facing actions must always respond
- Keep runtime memory and CPU stable for long-running sessions

## Target platform

- Windows 10/11
- Per-monitor DPI awareness
- Single-instance app with tray residency

## Release baseline (parity target)

- Static capture: region/window/fullscreen semantics compatible with PixPin
- Toolbar flow and behavior aligned to PixPin usage rhythm
- Multi-pin workflow with quick close/destroy operations
- Hotkey defaults compatible with PixPin expectations where feasible

## External references

- PixPin home: `https://pixpin.com/`
- PixPin docs home: `https://pixpin.cn/docs/start/base-use/`
- Static capture flow: `https://pixpin.cn/docs/start/static-screen-capture/`
