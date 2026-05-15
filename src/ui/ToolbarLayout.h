#pragma once

#include <algorithm>

namespace snappin {

inline constexpr int kArtifactToolbarButtonCount = 6;
inline constexpr int kArtifactToolbarButtonWidth = 44;
inline constexpr int kArtifactToolbarButtonHeight = 24;
inline constexpr int kArtifactToolbarPadding = 6;
inline constexpr int kArtifactToolbarGap = 3;
inline constexpr int kArtifactToolbarHeight = 34;

constexpr int ArtifactToolbarWidth(int button_count, int button_width, int gap,
                                   int padding) {
  return padding * 2 + std::max(0, button_count) * button_width +
         std::max(0, button_count - 1) * gap;
}

constexpr int DefaultArtifactToolbarWidth() {
  return ArtifactToolbarWidth(kArtifactToolbarButtonCount,
                              kArtifactToolbarButtonWidth,
                              kArtifactToolbarGap,
                              kArtifactToolbarPadding);
}

} // namespace snappin
