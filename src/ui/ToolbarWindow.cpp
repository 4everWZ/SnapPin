#include "ToolbarWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>

namespace snappin {
namespace {

const wchar_t kToolbarClassName[] = L"SnapPinToolbar";
const int kToolbarWidth = 366;
const int kToolbarHeight = 34;
const int kButtonWidth = 55;
const int kButtonHeight = 24;
const int kPadding = 6;
const int kGap = 4;

enum class ToolbarCommand {
  Copy = 2001,
  Save = 2002,
  Pin = 2003,
  Annotate = 2004,
  Ocr = 2005,
  Close = 2006
};

RECT ClampRectToWorkArea(const RECT& desired) {
  RECT out = desired;
  HMONITOR monitor = MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(monitor, &mi)) {
    RECT work = mi.rcWork;
    int width = desired.right - desired.left;
    int height = desired.bottom - desired.top;
    if (out.left < work.left) {
      out.left = work.left;
      out.right = out.left + width;
    }
    if (out.right > work.right) {
      out.right = work.right;
      out.left = out.right - width;
    }
    if (out.top < work.top) {
      out.top = work.top;
      out.bottom = out.top + height;
    }
    if (out.bottom > work.bottom) {
      out.bottom = work.bottom;
      out.top = out.bottom - height;
    }
  }
  return out;
}

} // namespace

ToolbarWindow::~ToolbarWindow() { Destroy(); }

bool ToolbarWindow::Create(HINSTANCE instance) {
  if (hwnd_) {
    return true;
  }
  instance_ = instance;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &ToolbarWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kToolbarClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kToolbarClassName, L"SnapPin Toolbar",
      WS_POPUP | WS_BORDER,
      0, 0, kToolbarWidth, kToolbarHeight, nullptr, nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }
  EnsureButtons();
  return true;
}

void ToolbarWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  visible_ = false;
}

void ToolbarWindow::ShowAtRect(const RectPX& rect) {
  if (!hwnd_) {
    return;
  }

  RECT desired = {};
  desired.left = rect.x + rect.w - kToolbarWidth;
  desired.top = rect.y + rect.h + 8;
  desired.right = desired.left + kToolbarWidth;
  desired.bottom = desired.top + kToolbarHeight;

  RECT clamp = ClampRectToWorkArea(desired);
  if (clamp.bottom > desired.bottom) {
    // If clamped vertically, try above selection.
    RECT above = {};
    above.left = rect.x + rect.w - kToolbarWidth;
    above.top = rect.y - kToolbarHeight - 8;
    above.right = above.left + kToolbarWidth;
    above.bottom = above.top + kToolbarHeight;
    clamp = ClampRectToWorkArea(above);
  }

  SetWindowPos(hwnd_, HWND_TOPMOST, clamp.left, clamp.top, kToolbarWidth,
               kToolbarHeight, SWP_SHOWWINDOW);
  ShowWindow(hwnd_, SW_SHOWNA);
  visible_ = true;
}

void ToolbarWindow::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

bool ToolbarWindow::IsVisible() const { return visible_; }

void ToolbarWindow::SetCallbacks(ActionCallback on_copy, ActionCallback on_save,
                                 ActionCallback on_pin,
                                 ActionCallback on_annotate,
                                 ActionCallback on_ocr,
                                 ActionCallback on_close) {
  on_copy_ = std::move(on_copy);
  on_save_ = std::move(on_save);
  on_pin_ = std::move(on_pin);
  on_annotate_ = std::move(on_annotate);
  on_ocr_ = std::move(on_ocr);
  on_close_ = std::move(on_close);
}

LRESULT CALLBACK ToolbarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam) {
  ToolbarWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<ToolbarWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<ToolbarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT ToolbarWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_COMMAND: {
      int cmd = LOWORD(wparam);
      if (cmd == static_cast<int>(ToolbarCommand::Copy)) {
        if (on_copy_) {
          on_copy_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(ToolbarCommand::Save)) {
        if (on_save_) {
          on_save_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(ToolbarCommand::Pin)) {
        if (on_pin_) {
          on_pin_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(ToolbarCommand::Annotate)) {
        if (on_annotate_) {
          on_annotate_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(ToolbarCommand::Ocr)) {
        if (on_ocr_) {
          on_ocr_();
        }
        return 0;
      }
      if (cmd == static_cast<int>(ToolbarCommand::Close)) {
        if (on_close_) {
          on_close_();
        }
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      if (on_close_) {
        on_close_();
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void ToolbarWindow::EnsureButtons() {
  if (btn_copy_) {
    return;
  }
  btn_annotate_ = CreateWindowW(
      L"BUTTON", L"Mark", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding, 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Annotate), instance_, nullptr);
  btn_ocr_ = CreateWindowW(
      L"BUTTON", L"OCR", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding + (kButtonWidth + kGap), 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Ocr), instance_, nullptr);
  btn_close_ = CreateWindowW(
      L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding + (kButtonWidth + kGap) * 2, 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Close), instance_, nullptr);
  btn_pin_ = CreateWindowW(
      L"BUTTON", L"Pin", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding + (kButtonWidth + kGap) * 3, 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Pin), instance_, nullptr);
  btn_save_ = CreateWindowW(
      L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding + (kButtonWidth + kGap) * 4, 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Save), instance_, nullptr);
  btn_copy_ = CreateWindowW(
      L"BUTTON", L"Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      kPadding + (kButtonWidth + kGap) * 5, 5, kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(ToolbarCommand::Copy), instance_, nullptr);
}

} // namespace snappin
