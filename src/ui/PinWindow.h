#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <memory>
#include <vector>

namespace snappin {

class PinWindow {
public:
  enum class Command {
    CloseSelf = 1,
    DestroySelf = 2,
    CloseAll = 3,
    DestroyAll = 4,
  };

  using FocusCallback = std::function<void(Id64)>;
  using CommandCallback = std::function<void(Id64, Command)>;

  PinWindow() = default;
  ~PinWindow();

  bool Create(HINSTANCE instance, Id64 pin_id,
              std::shared_ptr<std::vector<uint8_t>> pixels,
              const SizePX& size_px, int32_t stride_bytes, const PointPX& pos_px);
  void Destroy();

  void Show();
  void Hide();
  bool IsVisible() const;

  Id64 pin_id() const;
  bool is_locked() const;

  void SetCallbacks(FocusCallback on_focus, CommandCallback on_command);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void Invalidate();
  void UpdateAlpha();
  void ResetScaleOpacity();
  void ApplyScale(int wheel_delta);
  void ApplyOpacity(int wheel_delta);
  void ShowContextMenu(POINT screen_pt);
  void NotifyFocus();

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  Id64 pin_id_{};
  bool visible_ = false;
  bool locked_ = false;
  bool dragging_ = false;

  PointPX drag_start_cursor_{};
  PointPX drag_start_window_{};

  std::shared_ptr<std::vector<uint8_t>> pixels_;
  SizePX bitmap_size_px_{};
  int32_t stride_bytes_ = 0;

  float scale_ = 1.0f;
  float opacity_ = 1.0f;

  FocusCallback on_focus_;
  CommandCallback on_command_;
};

} // namespace snappin

