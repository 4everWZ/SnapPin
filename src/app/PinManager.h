#pragma once
#include "Artifact.h"
#include "ErrorCodes.h"
#include "PinWindow.h"
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace snappin {

struct RuntimeState;

class PinManager {
public:
  static constexpr UINT kWindowCommandMessage = WM_APP + 37;

  PinManager() = default;
  ~PinManager();

  bool Initialize(HINSTANCE instance, HWND main_hwnd, RuntimeState* runtime_state);
  void Shutdown();

  Result<Id64> CreateFromArtifact(const Artifact& art);
  Result<Id64> CreateFromClipboard();

  Result<void> CloseFocused();
  Result<void> CloseAll();

  bool HandleWindowCommand(WPARAM wparam, LPARAM lparam);

private:
  struct PinEntry {
    std::unique_ptr<PinWindow> window;
    std::shared_ptr<std::vector<uint8_t>> storage;
    SizePX size_px{};
    int32_t stride_bytes = 0;
  };

  bool CaptureRectToBitmap(const RectPX& rect,
                           std::shared_ptr<std::vector<uint8_t>>* storage_out,
                           SizePX* size_out, int32_t* stride_out);
  bool ReadClipboardBitmap(std::shared_ptr<std::vector<uint8_t>>* storage_out,
                           SizePX* size_out, int32_t* stride_out, Error* err);
  Result<Id64> CreatePinWithBitmap(std::shared_ptr<std::vector<uint8_t>> storage,
                                   const SizePX& size_px, int32_t stride_bytes,
                                   const PointPX& pos_px);
  PointPX DefaultCenteredPos(const SizePX& size_px) const;

  void SetFocusedPin(const std::optional<Id64>& pin_id);
  Result<void> ClosePin(Id64 pin_id);
  Result<void> DestroyPin(Id64 pin_id);
  Result<void> DestroyAll();

  HINSTANCE instance_ = nullptr;
  HWND main_hwnd_ = nullptr;
  RuntimeState* runtime_state_ = nullptr;

  uint64_t next_pin_id_ = 1;
  std::unordered_map<uint64_t, PinEntry> pins_;
  std::optional<Id64> focused_pin_id_;
};

} // namespace snappin

