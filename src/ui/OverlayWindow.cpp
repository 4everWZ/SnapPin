#include "OverlayWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>

namespace snappin {
namespace {

const wchar_t kOverlayClassName[] = L"SnapPinOverlay";
const BYTE kOverlayAlpha = 170;
const int kBorderPx = 2;

POINT ClientToScreenPoint(HWND hwnd, POINT pt_client) {
  POINT pt = pt_client;
  ClientToScreen(hwnd, &pt);
  return pt;
}

} // namespace

OverlayWindow::~OverlayWindow() { Destroy(); }

bool OverlayWindow::Create(HINSTANCE instance) {
  if (hwnd_) {
    return true;
  }

  instance_ = instance;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &OverlayWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kOverlayClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);

  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
      kOverlayClassName, L"SnapPin Overlay", WS_POPUP, 0, 0, 0, 0, nullptr,
      nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }

  SetLayeredWindowAttributes(hwnd_, 0, kOverlayAlpha, LWA_ALPHA);
  return true;
}

void OverlayWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  visible_ = false;
}

void OverlayWindow::ShowForCurrentMonitor() {
  if (!hwnd_) {
    return;
  }

  SetClickThrough(false);
  POINT cursor = {};
  GetCursorPos(&cursor);
  HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) {
    return;
  }

  monitor_origin_.x = mi.rcMonitor.left;
  monitor_origin_.y = mi.rcMonitor.top;
  monitor_size_.cx = mi.rcMonitor.right - mi.rcMonitor.left;
  monitor_size_.cy = mi.rcMonitor.bottom - mi.rcMonitor.top;

  SetWindowPos(hwnd_, HWND_TOPMOST, monitor_origin_.x, monitor_origin_.y,
               monitor_size_.cx, monitor_size_.cy, SWP_SHOWWINDOW);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  SetFocus(hwnd_);
  UpdateDpi(GetDpiForWindow(hwnd_));
  visible_ = true;
  dragging_ = false;
  has_selection_ = false;
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
  dragging_ = false;
  has_selection_ = false;
}

bool OverlayWindow::IsVisible() const { return visible_; }

void OverlayWindow::SetCallbacks(SelectCallback on_select, CancelCallback on_cancel) {
  on_select_ = std::move(on_select);
  on_cancel_ = std::move(on_cancel);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                        LPARAM lparam) {
  OverlayWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<OverlayWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT OverlayWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_DPICHANGED: {
      RECT* suggested = reinterpret_cast<RECT*>(lparam);
      if (suggested) {
        SetWindowPos(hwnd_, HWND_TOPMOST, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOACTIVATE);
      }
      UpdateDpi(HIWORD(wparam));
      return 0;
    }
    case WM_LBUTTONDOWN: {
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      BeginDrag(pt);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!dragging_) {
        break;
      }
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      UpdateDrag(pt);
      return 0;
    }
    case WM_LBUTTONUP: {
      if (!dragging_) {
        break;
      }
      POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      EndDrag(pt);
      return 0;
    }
    case WM_KEYDOWN: {
      if (wparam == VK_ESCAPE) {
        Cancel();
        return 0;
      }
      break;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd_, &ps);
      if (hdc) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        if (dragging_ || has_selection_) {
          RectPX rect = ActiveRectPx();
          RECT sel;
          sel.left = rect.x - monitor_origin_.x;
          sel.top = rect.y - monitor_origin_.y;
          sel.right = sel.left + rect.w;
          sel.bottom = sel.top + rect.h;

          HPEN pen = CreatePen(PS_SOLID, kBorderPx, RGB(255, 255, 255));
          HGDIOBJ old_pen = SelectObject(hdc, pen);
          HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
          Rectangle(hdc, sel.left, sel.top, sel.right, sel.bottom);
          SelectObject(hdc, old_brush);
          SelectObject(hdc, old_pen);
          DeleteObject(pen);
        }
        EndPaint(hwnd_, &ps);
      }
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void OverlayWindow::UpdateDpi(UINT dpi) {
  dpi_scale_ = static_cast<float>(dpi) / 96.0f;
}

void OverlayWindow::BeginDrag(POINT pt_client) {
  SetCapture(hwnd_);
  dragging_ = true;
  has_selection_ = false;
  SetClickThrough(false);
  POINT screen = ClientToScreenPoint(hwnd_, pt_client);
  start_px_.x = screen.x;
  start_px_.y = screen.y;
  current_px_ = start_px_;
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::UpdateDrag(POINT pt_client) {
  POINT screen = ClientToScreenPoint(hwnd_, pt_client);
  current_px_.x = screen.x;
  current_px_.y = screen.y;
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::EndDrag(POINT pt_client) {
  ReleaseCapture();
  POINT screen = ClientToScreenPoint(hwnd_, pt_client);
  current_px_.x = screen.x;
  current_px_.y = screen.y;
  RectPX rect = CurrentRectPx();
  selected_rect_px_ = rect;
  has_selection_ = true;
  dragging_ = false;
  UpdateMaskRegion();
  Invalidate();
  SetClickThrough(true);
  UpdateMaskRegion();
  if (on_select_) {
    on_select_(rect);
  }
}

void OverlayWindow::Cancel() {
  if (dragging_) {
    ReleaseCapture();
  }
  dragging_ = false;
  has_selection_ = false;
  SetClickThrough(false);
  UpdateMaskRegion();
  Hide();
  UpdateMaskRegion();
  if (on_cancel_) {
    on_cancel_();
  }
}

RectPX OverlayWindow::CurrentRectPx() const {
  int32_t x1 = start_px_.x;
  int32_t y1 = start_px_.y;
  int32_t x2 = current_px_.x;
  int32_t y2 = current_px_.y;

  RectPX rect;
  rect.x = std::min(x1, x2);
  rect.y = std::min(y1, y2);
  rect.w = std::abs(x2 - x1);
  rect.h = std::abs(y2 - y1);
  return rect;
}

RectPX OverlayWindow::ActiveRectPx() const {
  if (dragging_) {
    return CurrentRectPx();
  }
  if (has_selection_) {
    return selected_rect_px_;
  }
  return RectPX{};
}

void OverlayWindow::Invalidate() {
  if (!hwnd_) {
    return;
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void OverlayWindow::UpdateMaskRegion() {
  if (!hwnd_) {
    return;
  }

  RECT full = {0, 0, monitor_size_.cx, monitor_size_.cy};
  HRGN base = CreateRectRgn(full.left, full.top, full.right, full.bottom);
  if (!base) {
    return;
  }

  if (dragging_ || has_selection_) {
    RectPX rect = ActiveRectPx();
    RECT sel;
    sel.left = rect.x - monitor_origin_.x;
    sel.top = rect.y - monitor_origin_.y;
    sel.right = sel.left + rect.w;
    sel.bottom = sel.top + rect.h;

    RECT inner = sel;
    if ((sel.right - sel.left) > kBorderPx * 2 &&
        (sel.bottom - sel.top) > kBorderPx * 2) {
      inner.left += kBorderPx;
      inner.top += kBorderPx;
      inner.right -= kBorderPx;
      inner.bottom -= kBorderPx;
    }
    HRGN hole = CreateRectRgn(inner.left, inner.top, inner.right, inner.bottom);
    if (hole) {
      CombineRgn(base, base, hole, RGN_DIFF);
      DeleteObject(hole);
    }
  }

  SetWindowRgn(hwnd_, base, TRUE);
  // base region ownership is transferred to the window; do not delete.
}

void OverlayWindow::SetClickThrough(bool enabled) {
  if (!hwnd_) {
    return;
  }
  LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
  if (enabled) {
    ex |= WS_EX_TRANSPARENT;
  } else {
    ex &= ~WS_EX_TRANSPARENT;
  }
  SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

} // namespace snappin
