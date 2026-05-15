#pragma once

#include <optional>
#include <string>

namespace snappin {

enum class OcrSourceSelection {
  ActiveArtifact,
  FocusedPin,
  NoSource,
  InvalidSource,
};

inline OcrSourceSelection ResolveOcrSourceSelection(
    const std::optional<std::string>& source_param,
    bool has_active_artifact, bool has_focused_pin) {
  if (!source_param.has_value() || *source_param == "auto") {
    if (has_active_artifact) {
      return OcrSourceSelection::ActiveArtifact;
    }
    if (has_focused_pin) {
      return OcrSourceSelection::FocusedPin;
    }
    return OcrSourceSelection::NoSource;
  }

  if (*source_param == "active_artifact") {
    return has_active_artifact ? OcrSourceSelection::ActiveArtifact
                               : OcrSourceSelection::NoSource;
  }
  if (*source_param == "focused_pin") {
    return has_focused_pin ? OcrSourceSelection::FocusedPin
                           : OcrSourceSelection::NoSource;
  }
  return OcrSourceSelection::InvalidSource;
}

} // namespace snappin
