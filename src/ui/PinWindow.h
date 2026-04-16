#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace snappin {

class PinWindow {
public:
  enum class ContentKind {
    Image = 1,
    Text = 2,
    Latex = 3,
  };

  enum class Command {
    CopySelf = 1,
    SaveSelf = 2,
    CloseSelf = 3,
    DestroySelf = 4,
    CloseAll = 5,
    DestroyAll = 6,
  };

  using FocusCallback = std::function<void(Id64)>;
  using CommandCallback = std::function<void(Id64, Command)>;

  PinWindow() = default;
  ~PinWindow();

  bool Create(HINSTANCE instance, Id64 pin_id,
              std::shared_ptr<std::vector<uint8_t>> pixels,
              const SizePX& size_px, int32_t stride_bytes, const PointPX& pos_px,
              ContentKind content_kind = ContentKind::Image,
              const std::wstring& text_payload = L"");
  void Destroy();

  void Show();
  void Hide();
  bool IsVisible() const;

  Id64 pin_id() const;
  bool is_locked() const;
  ContentKind content_kind() const;

  void SetCallbacks(FocusCallback on_focus, CommandCallback on_command);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void Invalidate();
  void UpdateAlpha();
  void UpdateTopMost();
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
  bool always_on_top_ = true;

  PointPX drag_start_cursor_{};
  PointPX drag_start_window_{};

  ContentKind content_kind_ = ContentKind::Image;
  std::wstring text_payload_;
  std::shared_ptr<std::vector<uint8_t>> pixels_;
  SizePX bitmap_size_px_{};
  int32_t stride_bytes_ = 0;

  float scale_ = 1.0f;
  float opacity_ = 1.0f;

  FocusCallback on_focus_;
  CommandCallback on_command_;
};

} // namespace snappin


