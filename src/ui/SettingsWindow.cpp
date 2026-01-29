#include "SettingsWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace snappin {
namespace {

const wchar_t kSettingsClassName[] = L"SnapPinSettings";
const int kWindowWidth = 360;
const int kWindowHeight = 220;

enum class SettingsCommand {
  Capture = 3001,
  Reload = 3002,
  OpenConfig = 3003,
  ExitApp = 3004
};

} // namespace

SettingsWindow::~SettingsWindow() { Destroy(); }

bool SettingsWindow::Create(HINSTANCE instance) {
  if (hwnd_) {
    return true;
  }
  instance_ = instance;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &SettingsWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kSettingsClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      0, kSettingsClassName, L"SnapPin Settings",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
      nullptr, nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }

  EnsureControls();
  return true;
}

void SettingsWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  visible_ = false;
}

void SettingsWindow::Show() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  SetForegroundWindow(hwnd_);
  visible_ = true;
}

void SettingsWindow::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

bool SettingsWindow::IsVisible() const { return visible_; }

void SettingsWindow::SetCallbacks(ActionCallback on_capture, ActionCallback on_reload,
                                  ActionCallback on_open_config, ActionCallback on_exit) {
  on_capture_ = std::move(on_capture);
  on_reload_ = std::move(on_reload);
  on_open_config_ = std::move(on_open_config);
  on_exit_ = std::move(on_exit);
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam) {
  SettingsWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<SettingsWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT SettingsWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_COMMAND: {
      int cmd = LOWORD(wparam);
      if (cmd == static_cast<int>(SettingsCommand::Capture)) {
        if (on_capture_) {
          on_capture_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(SettingsCommand::Reload)) {
        if (on_reload_) {
          on_reload_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(SettingsCommand::OpenConfig)) {
        if (on_open_config_) {
          on_open_config_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(SettingsCommand::ExitApp)) {
        if (on_exit_) {
          on_exit_();
        }
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      Hide();
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void SettingsWindow::EnsureControls() {
  if (btn_capture_) {
    return;
  }
  int left = 20;
  int top = 20;
  int width = 140;
  int height = 28;
  int gap = 10;

  btn_capture_ = CreateWindowW(L"BUTTON", L"Capture",
                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               left, top, width, height, hwnd_,
                               reinterpret_cast<HMENU>(SettingsCommand::Capture),
                               instance_, nullptr);
  btn_reload_ = CreateWindowW(L"BUTTON", L"Reload Config",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              left, top + height + gap, width, height, hwnd_,
                              reinterpret_cast<HMENU>(SettingsCommand::Reload),
                              instance_, nullptr);
  btn_open_config_ = CreateWindowW(L"BUTTON", L"Open Config Folder",
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   left, top + (height + gap) * 2, width + 40,
                                   height, hwnd_,
                                   reinterpret_cast<HMENU>(SettingsCommand::OpenConfig),
                                   instance_, nullptr);
  btn_exit_ = CreateWindowW(L"BUTTON", L"Exit",
                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            left, top + (height + gap) * 3, width, height, hwnd_,
                            reinterpret_cast<HMENU>(SettingsCommand::ExitApp),
                            instance_, nullptr);
}

} // namespace snappin
