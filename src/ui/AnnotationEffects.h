#pragma once

#include <algorithm>

namespace snappin {

inline int MosaicBlockSize(int width, int height) {
  if (width <= 1 || height <= 1) {
    return 0;
  }
  return std::clamp(std::max(width, height) / 18, 6, 18);
}

inline int AdjustedSerialValue(int value, int delta) {
  return std::max(1, value + delta);
}

inline int HighlighterAlphaForStrokeWidth(int stroke_width) {
  return std::clamp(70 + std::max(1, stroke_width) * 4, 90, 210);
}

inline int WatermarkAlphaForStrength(int strength) {
  return std::clamp(95 + std::clamp(strength, 1, 10) * 15, 90, 220);
}

} // namespace snappin
