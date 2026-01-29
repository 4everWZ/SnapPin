#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>

namespace snappin {

class SettingsWindow {
public:
  using ActionCallback = std::function<void()>;

  SettingsWindow() = default;
  ~SettingsWindow();

  bool Create(HINSTANCE instance);
  void Destroy();

  void Show();
  void Hide();
  bool IsVisible() const;

  void SetCallbacks(ActionCallback on_capture, ActionCallback on_reload,
                    ActionCallback on_open_config, ActionCallback on_exit);

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void EnsureControls();

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;

  HWND btn_capture_ = nullptr;
  HWND btn_reload_ = nullptr;
  HWND btn_open_config_ = nullptr;
  HWND btn_exit_ = nullptr;

  ActionCallback on_capture_;
  ActionCallback on_reload_;
  ActionCallback on_open_config_;
  ActionCallback on_exit_;
};

} // namespace snappin
