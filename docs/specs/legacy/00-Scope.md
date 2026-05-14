# SnapPin Spec - Scope

## Goal

Deliver a practical Windows capture + mark + pin workflow that prioritizes:

- Fast static capture.
- Fast post-capture operations.
- Low-friction pinning for reference work.

## Product boundaries

In scope:
- Static capture interaction and editing loop.
- Pin windows and pin lifecycle.
- Local export pipeline (clipboard and file).

Out of scope for current stage:
- Cloud sync/account.
- Cross-platform support.
- Plugin system.

## Engineering principles

- No silent failures.
- Keyboard path must exist for frequent actions.
- DPI-safe coordinate and rendering behavior.
- Keep session memory and CPU stable for long-running tray usage.

## Target environment

- Windows 10/11
- Per-monitor DPI awareness
- Single-instance tray app

## Reference labels for feature parity study

Direct reference URLs are intentionally kept out of tracked documentation.

- Capture: static capture, long capture.
- Pin: base use, image, text, LaTeX.
- Mark: base use, geometry, line, arrow, mosaic, text, erase.
