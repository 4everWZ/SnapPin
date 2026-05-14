#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace snappin {

class OcrResultWindow {
public:
  OcrResultWindow() = default;
  ~OcrResultWindow();

  bool Create(HINSTANCE instance);
  void Destroy();

  void ShowText(const std::wstring& text);
  void Hide();
  bool IsVisible() const;

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void EnsureControls();
  void LayoutControls();

  HWND hwnd_ = nullptr;
  HINSTANCE instance_ = nullptr;
  bool visible_ = false;
  std::wstring text_;

  HWND edit_text_ = nullptr;
  HWND btn_close_ = nullptr;
};

} // namespace snappin
