# SnapPin Spec - Scope

## Goal

Build a practical Windows screenshot utility using patterns learned from mature screenshot tools and public documentation (including PixPin docs):
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

- Static capture: region/window/fullscreen semantics compatible with mainstream screenshot tools
- Toolbar flow and behavior aligned to common post-capture usage rhythm
- Multi-pin workflow with quick close/destroy operations
- Hotkey defaults compatible with common screenshot-tool expectations where feasible

## External references (feature study)

- PixPin home: `https://pixpin.com/`
- PixPin docs home: `https://pixpin.cn/docs/start/base-use/`
- Static capture flow: `https://pixpin.cn/docs/start/static-screen-capture/`
