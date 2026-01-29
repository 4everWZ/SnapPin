#pragma once
#include "Types.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>

namespace snappin {

class ToolbarWindow {
public:
  using ActionCallback = std::function<void()>;

  ToolbarWindow() = default;
  ~ToolbarWindow();

  bool Create(HINSTANCE instance);
  void Destroy();

  void ShowAtRect(const RectPX& rect);
  void Hide();
  bool IsVisible() const;

  void SetCallbacks(ActionCallback on_copy, ActionCallback on_save, ActionCallback on_close);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void EnsureButtons();

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;

  HWND btn_copy_ = nullptr;
  HWND btn_save_ = nullptr;
  HWND btn_close_ = nullptr;

  ActionCallback on_copy_;
  ActionCallback on_save_;
  ActionCallback on_close_;
};

} // namespace snappin
