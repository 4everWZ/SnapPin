#pragma once

#include "Types.h"

#include <algorithm>

namespace snappin {

inline int AnnotateToolbarMinWidth(int left_button_count,
                                   int right_button_count, int button_width,
                                   int button_gap, int padding) {
  const int left = std::max(0, left_button_count);
  const int right = std::max(0, right_button_count);
  const int total = left + right;
  const int pad = std::max(0, padding);
  if (total <= 0) {
    return pad * 2;
  }
  return (pad * 2) + (total * std::max(0, button_width)) +
         ((total - 1) * std::max(0, button_gap));
}

inline RectPX ClampWindowRectToBounds(RectPX desired, RectPX bounds) {
  const int bounds_w = std::max(0, bounds.w);
  const int bounds_h = std::max(0, bounds.h);
  int width = std::clamp(std::max(0, desired.w), 0, bounds_w);
  int height = std::clamp(std::max(0, desired.h), 0, bounds_h);

  int x = desired.x;
  int y = desired.y;
  if (x < bounds.x) {
    x = bounds.x;
  }
  if (x + width > bounds.x + bounds_w) {
    x = bounds.x + bounds_w - width;
  }
  if (y < bounds.y) {
    y = bounds.y;
  }
  if (y + height > bounds.y + bounds_h) {
    y = bounds.y + bounds_h - height;
  }
  return RectPX{x, y, width, height};
}

} // namespace snappin
