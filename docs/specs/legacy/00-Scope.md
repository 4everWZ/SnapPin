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

## Reference links for feature parity study

- `https://pixpin.cn/docs/capture/static-capture.html`
- `https://pixpin.cn/docs/capture/long-capture.html`
- `https://pixpin.cn/docs/pin/base-use.html`
- `https://pixpin.cn/docs/pin/image.html`
- `https://pixpin.cn/docs/pin/text.html`
- `https://pixpin.cn/docs/pin/latex.html`
- `https://pixpin.cn/docs/mark/base-use.html`
- `https://pixpin.cn/docs/mark/geo.html`
- `https://pixpin.cn/docs/mark/line.html`
- `https://pixpin.cn/docs/mark/arrow.html`
- `https://pixpin.cn/docs/mark/mosaic.html`
- `https://pixpin.cn/docs/mark/text.html`
- `https://pixpin.cn/docs/mark/erase.html`
