#include "PinWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>

namespace snappin {
namespace {

const wchar_t kPinWindowClassName[] = L"SnapPinPinWindow";

const int kMenuCopy = 4101;
const int kMenuSave = 4102;
const int kMenuClose = 4103;
const int kMenuDestroy = 4104;
const int kMenuCloseAll = 4105;
const int kMenuDestroyAll = 4106;
const int kMenuToggleLock = 4107;

constexpr float kScaleMin = 0.10f;
constexpr float kScaleMax = 5.0f;
constexpr float kScaleStep = 0.05f;
constexpr float kOpacityMin = 0.20f;
constexpr float kOpacityMax = 1.00f;
constexpr float kOpacityStep = 0.05f;

int WidthFromScale(int width, float scale) {
  const int v = static_cast<int>(std::round(width * scale));
  return std::max(16, v);
}

int HeightFromScale(int height, float scale) {
  const int v = static_cast<int>(std::round(height * scale));
  return std::max(16, v);
}

} // namespace

PinWindow::~PinWindow() { Destroy(); }

bool PinWindow::Create(HINSTANCE instance, Id64 pin_id,
                       std::shared_ptr<std::vector<uint8_t>> pixels,
                       const SizePX& size_px, int32_t stride_bytes,
                       const PointPX& pos_px) {
  if (hwnd_ || !pixels || size_px.w <= 0 || size_px.h <= 0 ||
      stride_bytes < size_px.w * 4) {
    return false;
  }

  instance_ = instance;
  pin_id_ = pin_id;
  pixels_ = std::move(pixels);
  bitmap_size_px_ = size_px;
  stride_bytes_ = stride_bytes;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &PinWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kPinWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  const int w = WidthFromScale(bitmap_size_px_.w, scale_);
  const int h = HeightFromScale(bitmap_size_px_.h, scale_);
  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                          kPinWindowClassName, L"SnapPin Pin",
                          WS_POPUP | WS_BORDER, pos_px.x, pos_px.y, w, h,
                          nullptr, nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }

  UpdateAlpha();
  Show();
  return true;
}

void PinWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  visible_ = false;
  dragging_ = false;
}

void PinWindow::Show() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  visible_ = true;
  NotifyFocus();
}

void PinWindow::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
  dragging_ = false;
}

bool PinWindow::IsVisible() const { return visible_; }

Id64 PinWindow::pin_id() const { return pin_id_; }

bool PinWindow::is_locked() const { return locked_; }

void PinWindow::SetCallbacks(FocusCallback on_focus, CommandCallback on_command) {
  on_focus_ = std::move(on_focus);
  on_command_ = std::move(on_command);
}

LRESULT CALLBACK PinWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
  PinWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<PinWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<PinWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT PinWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_SETFOCUS:
      NotifyFocus();
      return 0;
    case WM_LBUTTONDOWN: {
      NotifyFocus();
      if (locked_) {
        return 0;
      }
      SetCapture(hwnd_);
      dragging_ = true;

      POINT cur = {};
      GetCursorPos(&cur);
      RECT wr = {};
      GetWindowRect(hwnd_, &wr);

      drag_start_cursor_.x = cur.x;
      drag_start_cursor_.y = cur.y;
      drag_start_window_.x = wr.left;
      drag_start_window_.y = wr.top;
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!dragging_) {
        return 0;
      }
      POINT cur = {};
      GetCursorPos(&cur);
      const int dx = cur.x - drag_start_cursor_.x;
      const int dy = cur.y - drag_start_cursor_.y;
      SetWindowPos(hwnd_, HWND_TOPMOST, drag_start_window_.x + dx,
                   drag_start_window_.y + dy, 0, 0,
                   SWP_NOSIZE | SWP_NOACTIVATE);
      return 0;
    }
    case WM_LBUTTONUP:
      if (dragging_) {
        dragging_ = false;
        ReleaseCapture();
      }
      return 0;
    case WM_MOUSEWHEEL:
      if (locked_) {
        return 0;
      }
      if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        ApplyOpacity(GET_WHEEL_DELTA_WPARAM(wparam));
      } else {
        ApplyScale(GET_WHEEL_DELTA_WPARAM(wparam));
      }
      return 0;
    case WM_MBUTTONUP:
      if (!locked_) {
        ResetScaleOpacity();
      }
      return 0;
    case WM_CONTEXTMENU: {
      POINT pt = {};
      pt.x = GET_X_LPARAM(lparam);
      pt.y = GET_Y_LPARAM(lparam);
      if (pt.x == -1 && pt.y == -1) {
        RECT wr = {};
        GetWindowRect(hwnd_, &wr);
        pt.x = wr.left + 8;
        pt.y = wr.top + 8;
      }
      ShowContextMenu(pt);
      return 0;
    }
    case WM_KEYDOWN: {
      const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      if (wparam == 'L') {
        locked_ = !locked_;
        return 0;
      }
      if (ctrl && wparam == 'C') {
        if (on_command_) {
          on_command_(pin_id_, Command::CopySelf);
        }
        return 0;
      }
      if (ctrl && wparam == 'S') {
        if (on_command_) {
          on_command_(pin_id_, Command::SaveSelf);
        }
        return 0;
      }
      if (ctrl && wparam == 'D') {
        if (on_command_) {
          on_command_(pin_id_, Command::DestroySelf);
        }
        return 0;
      }
      if (ctrl && shift && wparam == 'W') {
        if (on_command_) {
          on_command_(pin_id_, Command::CloseAll);
        }
        return 0;
      }
      if (ctrl && wparam == 'W') {
        if (on_command_) {
          on_command_(pin_id_, Command::CloseSelf);
        }
        return 0;
      }
      break;
    }
    case WM_CLOSE:
      if (on_command_) {
        on_command_(pin_id_, Command::CloseSelf);
      } else {
        Hide();
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd_, &ps);
      if (hdc) {
        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        const int dst_w = rc.right - rc.left;
        const int dst_h = rc.bottom - rc.top;

        if (pixels_ && !pixels_->empty()) {
          BITMAPINFO bmi = {};
          bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          bmi.bmiHeader.biWidth = bitmap_size_px_.w;
          bmi.bmiHeader.biHeight = -bitmap_size_px_.h;
          bmi.bmiHeader.biPlanes = 1;
          bmi.bmiHeader.biBitCount = 32;
          bmi.bmiHeader.biCompression = BI_RGB;

          SetStretchBltMode(hdc, HALFTONE);
          StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0, bitmap_size_px_.w,
                        bitmap_size_px_.h, pixels_->data(), &bmi,
                        DIB_RGB_COLORS, SRCCOPY);
        } else {
          HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
          FillRect(hdc, &rc, bg);
          DeleteObject(bg);
        }
      }
      EndPaint(hwnd_, &ps);
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void PinWindow::Invalidate() {
  if (!hwnd_) {
    return;
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void PinWindow::UpdateAlpha() {
  if (!hwnd_) {
    return;
  }
  const BYTE alpha =
      static_cast<BYTE>(std::round(std::clamp(opacity_, kOpacityMin, kOpacityMax) * 255.0f));
  SetLayeredWindowAttributes(hwnd_, 0, alpha, LWA_ALPHA);
}

void PinWindow::ResetScaleOpacity() {
  scale_ = 1.0f;
  opacity_ = 1.0f;
  if (hwnd_) {
    RECT wr = {};
    GetWindowRect(hwnd_, &wr);
    const int w = WidthFromScale(bitmap_size_px_.w, scale_);
    const int h = HeightFromScale(bitmap_size_px_.h, scale_);
    SetWindowPos(hwnd_, HWND_TOPMOST, wr.left, wr.top, w, h,
                 SWP_NOACTIVATE);
  }
  UpdateAlpha();
  Invalidate();
}

void PinWindow::ApplyScale(int wheel_delta) {
  if (wheel_delta == 0) {
    return;
  }
  const float dir = wheel_delta > 0 ? 1.0f : -1.0f;
  scale_ = std::clamp(scale_ + dir * kScaleStep, kScaleMin, kScaleMax);

  RECT wr = {};
  GetWindowRect(hwnd_, &wr);
  const int w = WidthFromScale(bitmap_size_px_.w, scale_);
  const int h = HeightFromScale(bitmap_size_px_.h, scale_);
  SetWindowPos(hwnd_, HWND_TOPMOST, wr.left, wr.top, w, h, SWP_NOACTIVATE);
  Invalidate();
}

void PinWindow::ApplyOpacity(int wheel_delta) {
  if (wheel_delta == 0) {
    return;
  }
  const float dir = wheel_delta > 0 ? 1.0f : -1.0f;
  opacity_ = std::clamp(opacity_ + dir * kOpacityStep, kOpacityMin, kOpacityMax);
  UpdateAlpha();
}

void PinWindow::ShowContextMenu(POINT screen_pt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  AppendMenuW(menu, MF_STRING, kMenuCopy, L"Copy");
  AppendMenuW(menu, MF_STRING, kMenuSave, L"Save");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuClose, L"Close");
  AppendMenuW(menu, MF_STRING, kMenuDestroy, L"Destroy");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuCloseAll, L"Close All");
  AppendMenuW(menu, MF_STRING, kMenuDestroyAll, L"Destroy All");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuToggleLock, locked_ ? L"Unlock" : L"Lock");

  SetForegroundWindow(hwnd_);
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  screen_pt.x, screen_pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  if (cmd == 0) {
    return;
  }
  if (cmd == kMenuToggleLock) {
    locked_ = !locked_;
    return;
  }
  if (!on_command_) {
    return;
  }
  if (cmd == kMenuCopy) {
    on_command_(pin_id_, Command::CopySelf);
    return;
  }
  if (cmd == kMenuSave) {
    on_command_(pin_id_, Command::SaveSelf);
    return;
  }
  if (cmd == kMenuClose) {
    on_command_(pin_id_, Command::CloseSelf);
    return;
  }
  if (cmd == kMenuDestroy) {
    on_command_(pin_id_, Command::DestroySelf);
    return;
  }
  if (cmd == kMenuCloseAll) {
    on_command_(pin_id_, Command::CloseAll);
    return;
  }
  if (cmd == kMenuDestroyAll) {
    on_command_(pin_id_, Command::DestroyAll);
    return;
  }
}

void PinWindow::NotifyFocus() {
  if (on_focus_) {
    on_focus_(pin_id_);
  }
}

} // namespace snappin



