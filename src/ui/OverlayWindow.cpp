#include "OverlayWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace snappin {
namespace {

const wchar_t kOverlayClassName[] = L"SnapPinOverlay";
const BYTE kOverlayAlpha = 170;
const float kOverlayDimFactor = 0.55f;
const int kBorderPx = 2;
const int kEscapeHotkeyId = 42;

void DrawFrozenFrame(HDC hdc, const RECT& rc, const uint8_t* pixels, int32_t width,
                     int32_t height) {
  if (!pixels || width <= 0 || height <= 0) {
    return;
  }

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  int dst_w = rc.right - rc.left;
  int dst_h = rc.bottom - rc.top;
  SetStretchBltMode(hdc, HALFTONE);
  StretchDIBits(hdc, 0, 0, dst_w, dst_h, 0, 0, width, height, pixels, &bmi,
                DIB_RGB_COLORS, SRCCOPY);
}

RectPX RectFromScreenPoints(const PointPX& a, const POINT& b) {
  int32_t x1 = a.x;
  int32_t y1 = a.y;
  int32_t x2 = static_cast<int32_t>(b.x);
  int32_t y2 = static_cast<int32_t>(b.y);
  RectPX rect;
  rect.x = std::min(x1, x2);
  rect.y = std::min(y1, y2);
  rect.w = std::abs(x2 - x1);
  rect.h = std::abs(y2 - y1);
  return rect;
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

  UpdateOverlayAlpha();
  return true;
}

void OverlayWindow::Destroy() {
  if (hwnd_) {
    EnsureEscapeHotkey(false);
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

  RectPX rect;
  rect.x = mi.rcMonitor.left;
  rect.y = mi.rcMonitor.top;
  rect.w = mi.rcMonitor.right - mi.rcMonitor.left;
  rect.h = mi.rcMonitor.bottom - mi.rcMonitor.top;
  ShowForRect(rect);
}

void OverlayWindow::ShowForRect(const RectPX& rect) {
  if (!hwnd_) {
    return;
  }
  SetClickThrough(false);
  EnsureEscapeHotkey(true);
  monitor_origin_.x = rect.x;
  monitor_origin_.y = rect.y;
  monitor_size_.cx = rect.w;
  monitor_size_.cy = rect.h;

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
  EnsureEscapeHotkey(false);
  visible_ = false;
  dragging_ = false;
  has_selection_ = false;
  ClearFrozenFrame();
}

bool OverlayWindow::IsVisible() const { return visible_; }

void OverlayWindow::SetCallbacks(SelectCallback on_select, CancelCallback on_cancel) {
  on_select_ = std::move(on_select);
  on_cancel_ = std::move(on_cancel);
}

void OverlayWindow::SetFrozenFrame(std::shared_ptr<std::vector<uint8_t>> pixels,
                                   const SizePX& size_px, int32_t stride_bytes) {
  frozen_pixels_ = std::move(pixels);
  frozen_size_px_ = size_px;
  frozen_stride_ = stride_bytes;
  frozen_active_ = frozen_pixels_ && frozen_size_px_.w > 0 && frozen_size_px_.h > 0 &&
                   frozen_stride_ >= frozen_size_px_.w * 4;
  frozen_dimmed_.reset();
  if (frozen_active_) {
    const size_t total = static_cast<size_t>(frozen_stride_) *
                         static_cast<size_t>(frozen_size_px_.h);
    auto dimmed = std::make_shared<std::vector<uint8_t>>();
    dimmed->resize(total);
    const uint8_t* src = frozen_pixels_->data();
    uint8_t* dst = dimmed->data();
    for (size_t i = 0; i < total; i += 4) {
      dst[i + 0] = static_cast<uint8_t>(src[i + 0] * kOverlayDimFactor);
      dst[i + 1] = static_cast<uint8_t>(src[i + 1] * kOverlayDimFactor);
      dst[i + 2] = static_cast<uint8_t>(src[i + 2] * kOverlayDimFactor);
      dst[i + 3] = 0xFF;
    }
    frozen_dimmed_ = std::move(dimmed);
  }
  UpdateOverlayAlpha();
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::ClearFrozenFrame() {
  frozen_pixels_.reset();
  frozen_dimmed_.reset();
  frozen_size_px_ = {};
  frozen_stride_ = 0;
  frozen_active_ = false;
  UpdateOverlayAlpha();
  UpdateMaskRegion();
  Invalidate();
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
    case WM_HOTKEY: {
      if (wparam == kEscapeHotkeyId) {
        Cancel();
        return 0;
      }
      break;
    }
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
        const int rc_w = rc.right - rc.left;
        const int rc_h = rc.bottom - rc.top;
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bmp = nullptr;
        HGDIOBJ old_bmp = nullptr;
        if (mem_dc && rc_w > 0 && rc_h > 0) {
          mem_bmp = CreateCompatibleBitmap(hdc, rc_w, rc_h);
          if (mem_bmp) {
            old_bmp = SelectObject(mem_dc, mem_bmp);
          }
        }

        HDC draw_dc = (mem_dc && mem_bmp) ? mem_dc : hdc;

        RectPX rect_screen = ActiveRectPx();
        if (dragging_) {
          POINT cur = {};
          if (GetCursorPos(&cur)) {
            rect_screen = RectFromScreenPoints(start_px_, cur);
          }
        }
        RECT sel = {};
        bool show_sel = dragging_ || has_selection_;
        if (show_sel) {
          RECT win = {};
          GetWindowRect(hwnd_, &win);
          sel.left = rect_screen.x - win.left;
          sel.top = rect_screen.y - win.top;
          sel.right = sel.left + rect_screen.w;
          sel.bottom = sel.top + rect_screen.h;
        }

        if (frozen_active_ && frozen_pixels_ && frozen_dimmed_) {
          DrawFrozenFrame(draw_dc, rc, frozen_dimmed_->data(), frozen_size_px_.w,
                          frozen_size_px_.h);
          if (show_sel) {
            RECT bright = sel;
            if (bright.left < 0) {
              bright.left = 0;
            }
            if (bright.top < 0) {
              bright.top = 0;
            }
            if (bright.right > rc.right) {
              bright.right = rc.right;
            }
            if (bright.bottom > rc.bottom) {
              bright.bottom = rc.bottom;
            }
            int bw = bright.right - bright.left;
            int bh = bright.bottom - bright.top;
            if (bw > 0 && bh > 0) {
              HRGN clip = CreateRectRgn(bright.left, bright.top, bright.right,
                                        bright.bottom);
              if (clip) {
                SelectClipRgn(draw_dc, clip);
                DrawFrozenFrame(draw_dc, rc, frozen_pixels_->data(),
                                frozen_size_px_.w, frozen_size_px_.h);
                SelectClipRgn(draw_dc, nullptr);
                DeleteObject(clip);
              }
            }
          }
        } else {
          HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
          FillRect(draw_dc, &rc, bg);
          DeleteObject(bg);
        }

        if (show_sel) {
          HPEN pen = CreatePen(PS_SOLID, kBorderPx, RGB(255, 255, 255));
          HGDIOBJ old_pen = SelectObject(draw_dc, pen);
          HGDIOBJ old_brush = SelectObject(draw_dc, GetStockObject(HOLLOW_BRUSH));
          Rectangle(draw_dc, sel.left, sel.top, sel.right, sel.bottom);
          SelectObject(draw_dc, old_brush);
          SelectObject(draw_dc, old_pen);
          DeleteObject(pen);
        }

        if (draw_dc != hdc) {
          BitBlt(hdc, 0, 0, rc_w, rc_h, draw_dc, 0, 0, SRCCOPY);
          SelectObject(mem_dc, old_bmp);
          DeleteObject(mem_bmp);
          DeleteDC(mem_dc);
        } else if (mem_dc) {
          DeleteDC(mem_dc);
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
  POINT screen = {};
  if (GetCursorPos(&screen)) {
    POINT client = screen;
    ScreenToClient(hwnd_, &client);
    start_client_px_.x = client.x;
    start_client_px_.y = client.y;
    start_px_.x = screen.x;
    start_px_.y = screen.y;
  } else {
    start_client_px_.x = pt_client.x;
    start_client_px_.y = pt_client.y;
    start_px_.x = monitor_origin_.x + start_client_px_.x;
    start_px_.y = monitor_origin_.y + start_client_px_.y;
  }
  current_client_px_ = start_client_px_;
  current_px_ = start_px_;
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::UpdateDrag(POINT pt_client) {
  POINT screen = {};
  if (GetCursorPos(&screen)) {
    POINT client = screen;
    ScreenToClient(hwnd_, &client);
    current_client_px_.x = client.x;
    current_client_px_.y = client.y;
    current_px_.x = screen.x;
    current_px_.y = screen.y;
  } else {
    current_client_px_.x = pt_client.x;
    current_client_px_.y = pt_client.y;
    current_px_.x = monitor_origin_.x + current_client_px_.x;
    current_px_.y = monitor_origin_.y + current_client_px_.y;
  }
  UpdateMaskRegion();
  Invalidate();
}

void OverlayWindow::EndDrag(POINT pt_client) {
  ReleaseCapture();
  POINT screen = {};
  if (GetCursorPos(&screen)) {
    POINT client = screen;
    ScreenToClient(hwnd_, &client);
    current_client_px_.x = client.x;
    current_client_px_.y = client.y;
    current_px_.x = screen.x;
    current_px_.y = screen.y;
  } else {
    current_client_px_.x = pt_client.x;
    current_client_px_.y = pt_client.y;
    current_px_.x = monitor_origin_.x + current_client_px_.x;
    current_px_.y = monitor_origin_.y + current_client_px_.y;
  }
  RectPX rect = CurrentRectPx();
  RectPX rect_client = CurrentRectClient();
  selected_rect_px_ = rect;
  selected_rect_client_px_ = rect_client;
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

RectPX OverlayWindow::CurrentRectClient() const {
  int32_t x1 = start_client_px_.x;
  int32_t y1 = start_client_px_.y;
  int32_t x2 = current_client_px_.x;
  int32_t y2 = current_client_px_.y;

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

RectPX OverlayWindow::ActiveRectClient() const {
  if (dragging_) {
    return CurrentRectClient();
  }
  if (has_selection_) {
    return selected_rect_client_px_;
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

  if (frozen_active_) {
    SetWindowRgn(hwnd_, nullptr, TRUE);
    DeleteObject(base);
    return;
  }
  if (dragging_ || has_selection_) {
    RectPX rect = ActiveRectClient();
    RECT sel;
    sel.left = rect.x;
    sel.top = rect.y;
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

void OverlayWindow::UpdateOverlayAlpha() {
  if (!hwnd_) {
    return;
  }
  BYTE alpha = frozen_active_ ? 255 : kOverlayAlpha;
  SetLayeredWindowAttributes(hwnd_, 0, alpha, LWA_ALPHA);
}

void OverlayWindow::EnsureEscapeHotkey(bool enable) {
  if (!hwnd_) {
    return;
  }
  if (enable && !esc_hotkey_registered_) {
    if (RegisterHotKey(hwnd_, kEscapeHotkeyId, MOD_NOREPEAT, VK_ESCAPE)) {
      esc_hotkey_registered_ = true;
    }
    return;
  }
  if (!enable && esc_hotkey_registered_) {
    UnregisterHotKey(hwnd_, kEscapeHotkeyId);
    esc_hotkey_registered_ = false;
  }
}

} // namespace snappin
