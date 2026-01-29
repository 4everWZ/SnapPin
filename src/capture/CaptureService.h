#pragma once
#include "Types.h"

#include <functional>
#include <memory>
#include <optional>

namespace snappin {

enum class CaptureTargetType { REGION, WINDOW, DISPLAY };

struct CaptureTarget {
  CaptureTargetType type{};
  std::optional<RectPX> region_px;
  uint64_t hwnd = 0;
  int32_t display_index = -1;
};

enum class DetectMode { DETECT_ELEMENTS, WINDOW_ONLY, OFF };
enum class CaptureBackend { AUTO, WGC, DXGI };

struct CaptureOptions {
  bool include_cursor = false;
  DetectMode detect_mode = DetectMode::DETECT_ELEMENTS;
  CaptureBackend prefer_backend = CaptureBackend::AUTO;
};

struct CaptureFrame {
  GpuFrameHandle gpu{};
  SizePX size_px{};
  RectPX screen_rect_px{};
  TimeStamp timestamp{};
  float dpi_scale = 1.0f;
};

struct FrameStreamStats {
  uint64_t dropped_frames_total = 0;
  float fps_actual = 0.0f;
};

using StreamId = Id64;

class ICaptureService {
public:
  virtual ~ICaptureService() = default;
  virtual Result<CaptureFrame> CaptureOnce(const CaptureTarget&,
                                           const CaptureOptions&) = 0;
  virtual Result<StreamId> StartFrameStream(
      const CaptureTarget&, const CaptureOptions&, int32_t fps_hint,
      std::function<void(const CaptureFrame&)>) = 0;
  virtual void StopFrameStream(StreamId) = 0;
  virtual FrameStreamStats GetStreamStats(StreamId) = 0;
};

std::unique_ptr<ICaptureService> CreateCaptureService();

} // namespace snappin
