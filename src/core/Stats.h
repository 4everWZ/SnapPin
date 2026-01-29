#pragma once

#include <cstdint>

namespace snappin {

struct StatsSnapshot {
  double overlay_show_ms_p95 = 0;
  double capture_once_ms_p95 = 0;
  double ocr_ms_last = 0;

  uint64_t dropped_frames_total = 0;
  double encode_ms_per_frame_avg = 0;

  uint64_t working_set_bytes = 0;
};

class IStatsService {
public:
  virtual ~IStatsService() = default;
  virtual StatsSnapshot Snapshot() = 0;
};

} // namespace snappin
