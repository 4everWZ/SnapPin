#include "OcrResultWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>

namespace snappin {
namespace {

const wchar_t kOcrResultClassName[] = L"SnapPinOcrResultWindow";
const int kWindowWidth = 560;
const int kWindowHeight = 360;
const int kPadding = 12;
const int kButtonWidth = 90;
const int kButtonHeight = 28;

const INT_PTR kCmdClose = 6101;

} // namespace

OcrResultWindow::~OcrResultWindow() { Destroy(); }

bool OcrResultWindow::Create(HINSTANCE instance) {
  if (hwnd_) {
    return true;
  }
  instance_ = instance;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &OcrResultWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kOcrResultClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, kOcrResultClassName,
                          L"SnapPin OCR Result",
                          WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                              WS_THICKFRAME,
                          CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth,
                          kWindowHeight, nullptr, nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }

  EnsureControls();
  return true;
}

void OcrResultWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  edit_text_ = nullptr;
  btn_close_ = nullptr;
  visible_ = false;
}

void OcrResultWindow::ShowText(const std::wstring& text) {
  text_ = text;
  if (!hwnd_) {
    return;
  }
  EnsureControls();
  if (edit_text_) {
    SetWindowTextW(edit_text_, text_.c_str());
  }
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  SetForegroundWindow(hwnd_);
  visible_ = true;
}

void OcrResultWindow::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

bool OcrResultWindow::IsVisible() const { return visible_; }

LRESULT CALLBACK OcrResultWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                          LPARAM lparam) {
  OcrResultWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<OcrResultWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self =
        reinterpret_cast<OcrResultWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT OcrResultWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_SIZE:
      LayoutControls();
      return 0;
    case WM_COMMAND: {
      const int cmd = LOWORD(wparam);
      if (cmd == kCmdClose) {
        Hide();
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

void OcrResultWindow::EnsureControls() {
  if (edit_text_) {
    return;
  }
  edit_text_ = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", text_.c_str(),
      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL |
          ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
      0, 0, 0, 0, hwnd_, nullptr, instance_, nullptr);
  btn_close_ = CreateWindowW(L"BUTTON", L"Close",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
                             kButtonWidth, kButtonHeight, hwnd_,
                             reinterpret_cast<HMENU>(kCmdClose), instance_,
                             nullptr);
  LayoutControls();
}

void OcrResultWindow::LayoutControls() {
  if (!hwnd_ || !edit_text_) {
    return;
  }
  RECT rc = {};
  GetClientRect(hwnd_, &rc);
  const int width = std::max(0, static_cast<int>(rc.right - rc.left));
  const int height = std::max(0, static_cast<int>(rc.bottom - rc.top));
  const int button_y = std::max(kPadding, height - kPadding - kButtonHeight);
  const int edit_h = std::max(0, button_y - kPadding * 2);
  SetWindowPos(edit_text_, nullptr, kPadding, kPadding,
               std::max(0, width - kPadding * 2), edit_h,
               SWP_NOZORDER | SWP_NOACTIVATE);
  if (btn_close_) {
    SetWindowPos(btn_close_, nullptr,
                 std::max(kPadding, width - kPadding - kButtonWidth),
                 button_y, kButtonWidth, kButtonHeight,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }
}

} // namespace snappin
