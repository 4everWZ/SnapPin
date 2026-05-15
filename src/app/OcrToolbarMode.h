#pragma once

namespace snappin {

enum class OcrToolbarAction {
  InvokeOcr,
  SelectRegion,
};

inline OcrToolbarAction ResolveOcrToolbarAction(bool overlay_visible,
                                                bool has_active_artifact,
                                                bool shift_down) {
  if (overlay_visible && has_active_artifact && shift_down) {
    return OcrToolbarAction::SelectRegion;
  }
  return OcrToolbarAction::InvokeOcr;
}

} // namespace snappin
