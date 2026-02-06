# SnapPin Spec - Scope

## Goal

Build a practical Windows screenshot utility with behavior aligned to reference app:
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

- Static capture: region/window/fullscreen semantics compatible with reference app
- Toolbar flow and behavior aligned to reference app usage rhythm
- Multi-pin workflow with quick close/destroy operations
- Hotkey defaults compatible with reference app expectations where feasible

## External references

- reference app home: `reference app reference
- reference app docs home: `reference app reference
- Static capture flow: `reference app reference
