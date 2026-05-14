#pragma once

#include "Types.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace snappin {

inline std::optional<RectPX> ResolveOcrCropRect(
    RectPX artifact_screen_rect, const SizePX& bitmap_size,
    const RectPX& requested_screen_rect) {
  if (bitmap_size.w <= 0 || bitmap_size.h <= 0 ||
      requested_screen_rect.w <= 0 || requested_screen_rect.h <= 0) {
    return std::nullopt;
  }

  if (artifact_screen_rect.w <= 0 || artifact_screen_rect.h <= 0) {
    artifact_screen_rect.x = 0;
    artifact_screen_rect.y = 0;
    artifact_screen_rect.w = bitmap_size.w;
    artifact_screen_rect.h = bitmap_size.h;
  }

  const int64_t rel_left =
      static_cast<int64_t>(requested_screen_rect.x) - artifact_screen_rect.x;
  const int64_t rel_top =
      static_cast<int64_t>(requested_screen_rect.y) - artifact_screen_rect.y;
  const int64_t rel_right =
      rel_left + static_cast<int64_t>(requested_screen_rect.w);
  const int64_t rel_bottom =
      rel_top + static_cast<int64_t>(requested_screen_rect.h);

  const int64_t crop_left = std::max<int64_t>(0, rel_left);
  const int64_t crop_top = std::max<int64_t>(0, rel_top);
  const int64_t crop_right = std::min<int64_t>(bitmap_size.w, rel_right);
  const int64_t crop_bottom = std::min<int64_t>(bitmap_size.h, rel_bottom);
  const int64_t crop_w = crop_right - crop_left;
  const int64_t crop_h = crop_bottom - crop_top;
  if (crop_w <= 0 || crop_h <= 0) {
    return std::nullopt;
  }

  return RectPX{static_cast<int32_t>(crop_left),
                static_cast<int32_t>(crop_top),
                static_cast<int32_t>(crop_w),
                static_cast<int32_t>(crop_h)};
}

} // namespace snappin
