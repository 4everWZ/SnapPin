#pragma once
#include "Types.h"

#include <memory>
#include <optional>
#include <vector>

namespace snappin {

struct FrozenFrame {
  RectPX screen_rect_px{};
  SizePX size_px{};
  int32_t stride_bytes = 0;
  PixelFormat format = PixelFormat::BGRA8;
  std::shared_ptr<std::vector<uint8_t>> pixels;
};

Result<void> PrepareFrozenFrameForCursorMonitor();
const FrozenFrame* PeekFrozenFrame();
std::optional<FrozenFrame> ConsumeFrozenFrame();
void ClearFrozenFrame();

} // namespace snappin
