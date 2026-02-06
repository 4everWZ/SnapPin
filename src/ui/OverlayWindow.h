#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <memory>
#include <vector>

namespace snappin {

class OverlayWindow {
public:
  using SelectCallback = std::function<void(const RectPX&)>;
  using CancelCallback = std::function<void()>;

  OverlayWindow() = default;
  ~OverlayWindow();

  bool Create(HINSTANCE instance);
  void Destroy();

  void ShowForCurrentMonitor();
  void ShowForRect(const RectPX& rect);
  void Hide();
  bool IsVisible() const;

  void SetCallbacks(SelectCallback on_select, CancelCallback on_cancel);
  void SetFrozenFrame(std::shared_ptr<std::vector<uint8_t>> pixels,
                      const SizePX& size_px, int32_t stride_bytes);
  void ClearFrozenFrame();

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void UpdateDpi(UINT dpi);
  void BeginDrag(POINT pt_client);
  void UpdateDrag(POINT pt_client);
  void EndDrag(POINT pt_client);
  void Cancel();
  RectPX CurrentRectPx() const;
  RectPX CurrentRectClient() const;
  RectPX ActiveRectPx() const;
  RectPX ActiveRectClient() const;
  void UpdateMaskRegion();
  void Invalidate();
  void SetClickThrough(bool enabled);
  void UpdateOverlayAlpha();
  void EnsureEscapeHotkey(bool enable);
  void UpdateHoverRect();

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;
  bool dragging_ = false;
  bool has_selection_ = false;

  POINT monitor_origin_ = {};
  SIZE monitor_size_ = {};

  PointPX start_px_{};
  PointPX current_px_{};
  PointPX start_client_px_{};
  PointPX current_client_px_{};
  RectPX selected_rect_px_{};
  RectPX selected_rect_client_px_{};
  RectPX hover_rect_px_{};
  float dpi_scale_ = 1.0f;

  std::shared_ptr<std::vector<uint8_t>> frozen_pixels_;
  std::shared_ptr<std::vector<uint8_t>> frozen_dimmed_;
  SizePX frozen_size_px_{};
  int32_t frozen_stride_ = 0;
  bool frozen_active_ = false;
  bool esc_hotkey_registered_ = false;

  SelectCallback on_select_;
  CancelCallback on_cancel_;
};

} // namespace snappin
