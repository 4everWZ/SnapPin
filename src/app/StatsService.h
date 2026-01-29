#pragma once
#include "Stats.h"

#include <atomic>

namespace snappin {

class StatsService final : public IStatsService {
public:
  StatsService() = default;

  void SetOverlayShowMs(double ms);
  void SetCaptureOnceMs(double ms);
  void SetWorkingSetBytes(uint64_t bytes);

  StatsSnapshot Snapshot() override;

private:
  std::atomic<double> overlay_show_ms_{0.0};
  std::atomic<double> capture_once_ms_{0.0};
  std::atomic<uint64_t> working_set_bytes_{0};
};

} // namespace snappin
