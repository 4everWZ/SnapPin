#include "StatsService.h"

namespace snappin {

void StatsService::SetOverlayShowMs(double ms) { overlay_show_ms_.store(ms); }

void StatsService::SetCaptureOnceMs(double ms) { capture_once_ms_.store(ms); }

void StatsService::SetWorkingSetBytes(uint64_t bytes) {
  working_set_bytes_.store(bytes);
}

StatsSnapshot StatsService::Snapshot() {
  StatsSnapshot snap;
  snap.overlay_show_ms_p95 = overlay_show_ms_.load();
  snap.capture_once_ms_p95 = capture_once_ms_.load();
  snap.working_set_bytes = working_set_bytes_.load();
  return snap;
}

} // namespace snappin
