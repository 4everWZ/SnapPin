#pragma once
#include "Types.h"

#include <optional>
#include <string>
#include <vector>

namespace snappin {

enum class ArtifactKind { CAPTURE, SCROLL, RECORD };

struct ExportRecord {
  std::string kind;
  std::string path;
  TimeStamp at{};
};

struct Artifact {
  Id64 artifact_id{};
  ArtifactKind kind = ArtifactKind::CAPTURE;

  std::optional<GpuFrameHandle> base_gpu;
  std::optional<CpuBitmap> base_cpu;

  RectPX screen_rect_px{};
  float dpi_scale = 1.0f;

  std::vector<ExportRecord> exports;
};

class IArtifactStore {
public:
  virtual ~IArtifactStore() = default;
  virtual std::optional<Artifact> Get(Id64 id) = 0;
  virtual void Put(const Artifact& artifact) = 0;
  virtual void ClearActive() = 0;
};

} // namespace snappin
