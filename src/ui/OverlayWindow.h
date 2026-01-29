#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>

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
  void Hide();
  bool IsVisible() const;

  void SetCallbacks(SelectCallback on_select, CancelCallback on_cancel);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void UpdateDpi(UINT dpi);
  void BeginDrag(POINT pt_client);
  void UpdateDrag(POINT pt_client);
  void EndDrag(POINT pt_client);
  void Cancel();
  RectPX CurrentRectPx() const;
  RectPX ActiveRectPx() const;
  void UpdateMaskRegion();
  void Invalidate();
  void SetClickThrough(bool enabled);

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;
  bool dragging_ = false;
  bool has_selection_ = false;

  POINT monitor_origin_ = {};
  SIZE monitor_size_ = {};

  PointPX start_px_{};
  PointPX current_px_{};
  RectPX selected_rect_px_{};
  float dpi_scale_ = 1.0f;

  SelectCallback on_select_;
  CancelCallback on_cancel_;
};

} // namespace snappin
