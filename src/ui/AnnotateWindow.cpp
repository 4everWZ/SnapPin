#include "AnnotateWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace snappin {
namespace {

const wchar_t kAnnotateClassName[] = L"SnapPinAnnotateWindow";
const int kToolbarHeight = 34;
const int kToolbarPadding = 4;
const int kButtonWidth = 72;
const int kButtonHeight = 24;
const int kButtonGap = 3;
const int kHandleSize = 8;
const int kHitTolerance = 8;
const int kMinShapeSize = 1;

const int kCmdSelect = 5201;
const int kCmdRect = 5202;
const int kCmdLine = 5203;
const int kCmdArrow = 5204;
const int kCmdPencil = 5205;
const int kCmdText = 5206;
const int kCmdReselect = 5207;
const int kCmdUndo = 5208;
const int kCmdRedo = 5209;
const int kCmdCopy = 5210;
const int kCmdSave = 5211;
const int kCmdClose = 5212;

int ClampInt(int value, int lo, int hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

double DistanceSq(POINT a, POINT b) {
  const double dx = static_cast<double>(a.x - b.x);
  const double dy = static_cast<double>(a.y - b.y);
  return dx * dx + dy * dy;
}

double DistanceToSegmentSq(POINT p, POINT a, POINT b) {
  const double vx = static_cast<double>(b.x - a.x);
  const double vy = static_cast<double>(b.y - a.y);
  const double wx = static_cast<double>(p.x - a.x);
  const double wy = static_cast<double>(p.y - a.y);
  const double len_sq = vx * vx + vy * vy;
  if (len_sq <= 1e-6) {
    return DistanceSq(p, a);
  }
  double t = (wx * vx + wy * vy) / len_sq;
  if (t < 0.0) {
    t = 0.0;
  } else if (t > 1.0) {
    t = 1.0;
  }
  const double px = a.x + t * vx;
  const double py = a.y + t * vy;
  const double dx = static_cast<double>(p.x) - px;
  const double dy = static_cast<double>(p.y) - py;
  return dx * dx + dy * dy;
}

RECT ClampRectToWorkArea(const RECT& desired) {
  RECT out = desired;
  HMONITOR monitor = MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfoW(monitor, &mi)) {
    const RECT work = mi.rcWork;
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;
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

RECT ClampRectToBounds(const RECT& desired, const RECT& bounds) {
  RECT out = desired;
  const int width = desired.right - desired.left;
  const int height = desired.bottom - desired.top;
  if (out.left < bounds.left) {
    out.left = bounds.left;
    out.right = out.left + width;
  }
  if (out.right > bounds.right) {
    out.right = bounds.right;
    out.left = out.right - width;
  }
  if (out.top < bounds.top) {
    out.top = bounds.top;
    out.bottom = out.top + height;
  }
  if (out.bottom > bounds.bottom) {
    out.bottom = bounds.bottom;
    out.top = out.bottom - height;
  }
  return out;
}

bool PointsEqual(const POINT& a, const POINT& b) {
  return a.x == b.x && a.y == b.y;
}

POINT SnapPoint45(const POINT& anchor, const POINT& pt) {
  const int dx = pt.x - anchor.x;
  const int dy = pt.y - anchor.y;
  const int adx = std::abs(dx);
  const int ady = std::abs(dy);
  POINT out = pt;
  if (adx >= ady * 2) {
    out.y = anchor.y;
    return out;
  }
  if (ady >= adx * 2) {
    out.x = anchor.x;
    return out;
  }
  const int d = std::max(adx, ady);
  out.x = anchor.x + (dx >= 0 ? d : -d);
  out.y = anchor.y + (dy >= 0 ? d : -d);
  return out;
}

} // namespace

AnnotateWindow::~AnnotateWindow() { Destroy(); }

bool AnnotateWindow::Create(HINSTANCE instance, HWND parent) {
  if (hwnd_) {
    return true;
  }
  instance_ = instance;
  parent_hwnd_ = parent;

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &AnnotateWindow::WndProc;
  wc.hInstance = instance_;
  wc.lpszClassName = kAnnotateClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  DWORD ex_style = WS_EX_TOOLWINDOW;
  DWORD style = WS_POPUP | WS_BORDER;
  HWND parent_handle = nullptr;
  if (parent_hwnd_) {
    ex_style = 0;
    style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_CLIPSIBLINGS;
    parent_handle = parent_hwnd_;
  } else {
    ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    style = WS_POPUP | WS_BORDER;
  }

  hwnd_ = CreateWindowExW(ex_style, kAnnotateClassName, L"SnapPin Mark", style, 0, 0,
                          0, 0, parent_handle, nullptr, instance_, this);
  if (!hwnd_) {
    return false;
  }
  EnsureControls();
  return true;
}

void AnnotateWindow::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  visible_ = false;
  dragging_ = false;
  text_editing_ = false;
  source_pixels_.reset();
}

bool AnnotateWindow::BeginSession(const RectPX& screen_rect,
                                  std::shared_ptr<std::vector<uint8_t>> source_pixels,
                                  const SizePX& size_px, int32_t stride_bytes) {
  if (!hwnd_ || !source_pixels || size_px.w <= 0 || size_px.h <= 0 ||
      stride_bytes < size_px.w * 4) {
    return false;
  }
  const size_t expected_size =
      static_cast<size_t>(stride_bytes) * static_cast<size_t>(size_px.h);
  if (source_pixels->size() < expected_size) {
    return false;
  }

  screen_rect_px_ = screen_rect;
  bitmap_size_px_ = size_px;
  stride_bytes_ = stride_bytes;
  source_pixels_ = std::move(source_pixels);
  annotations_.clear();
  history_.clear();
  history_.push_back(annotations_);
  history_index_ = 0;
  selected_index_ = -1;
  drag_index_ = -1;
  drag_mode_ = DragMode::None;
  dragging_ = false;
  text_editing_ = false;
  text_edit_index_ = -1;
  tool_ = Tool::Rect;
  color_ = RGB(255, 80, 64);
  thickness_ = 2;

  const int min_toolbar_width = kToolbarPadding * 2 + (12 * kButtonWidth) +
                                (11 * kButtonGap);
  const int window_w = std::max(size_px.w, min_toolbar_width);
  const int window_h = size_px.h + kToolbarHeight;
  RECT desired = {};
  desired.left = screen_rect.x - (window_w - size_px.w) / 2;
  desired.top = screen_rect.y - kToolbarHeight;
  desired.right = desired.left + window_w;
  desired.bottom = desired.top + window_h;
  RECT clamped = desired;
  if (parent_hwnd_) {
    RECT parent_rect = {};
    RECT parent_client = {};
    if (GetWindowRect(parent_hwnd_, &parent_rect) &&
        GetClientRect(parent_hwnd_, &parent_client)) {
      desired.left -= parent_rect.left;
      desired.right -= parent_rect.left;
      desired.top -= parent_rect.top;
      desired.bottom -= parent_rect.top;
      clamped = ClampRectToBounds(desired, parent_client);
    }
  } else {
    clamped = ClampRectToWorkArea(desired);
  }

  UINT flags = SWP_SHOWWINDOW;
  HWND insert_after = HWND_TOPMOST;
  if (parent_hwnd_) {
    insert_after = nullptr;
    flags |= SWP_NOZORDER;
  }
  SetWindowPos(hwnd_, insert_after, clamped.left, clamped.top, window_w, window_h,
               flags);
  ShowWindow(hwnd_, SW_SHOWNORMAL);
  if (!parent_hwnd_) {
    SetForegroundWindow(hwnd_);
  }
  SetFocus(hwnd_);
  visible_ = true;
  LayoutControls();
  UpdateToolButtons();
  Invalidate();
  return true;
}

void AnnotateWindow::EndSession() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
  dragging_ = false;
  text_editing_ = false;
}

bool AnnotateWindow::IsVisible() const { return visible_; }

void AnnotateWindow::SetCommandCallback(CommandCallback on_command) {
  on_command_ = std::move(on_command);
}

LRESULT CALLBACK AnnotateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam) {
  AnnotateWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    self = reinterpret_cast<AnnotateWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self =
        reinterpret_cast<AnnotateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self) {
    return self->HandleMessage(msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT AnnotateWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_SIZE:
      LayoutControls();
      return 0;
    case WM_COMMAND: {
      const int cmd = LOWORD(wparam);
      switch (cmd) {
        case kCmdSelect:
          SetTool(Tool::Select);
          return 0;
        case kCmdRect:
          SetTool(Tool::Rect);
          return 0;
        case kCmdLine:
          SetTool(Tool::Line);
          return 0;
        case kCmdArrow:
          SetTool(Tool::Arrow);
          return 0;
        case kCmdPencil:
          SetTool(Tool::Pencil);
          return 0;
        case kCmdText:
          SetTool(Tool::Text);
          return 0;
        case kCmdReselect:
          EmitCommand(Command::Reselect);
          return 0;
        case kCmdUndo:
          Undo();
          return 0;
        case kCmdRedo:
          Redo();
          return 0;
        case kCmdCopy:
          EmitCommand(Command::Copy);
          return 0;
        case kCmdSave:
          EmitCommand(Command::Save);
          return 0;
        case kCmdClose:
          EmitCommand(Command::Close);
          return 0;
        default:
          break;
      }
      break;
    }
    case WM_LBUTTONDOWN: {
      POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      POINT pt_canvas = {};
      if (!ToCanvasPoint(pt_client, &pt_canvas)) {
        return 0;
      }
      BeginDrag(pt_canvas);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (!dragging_) {
        return 0;
      }
      POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      POINT pt_canvas = {};
      if (!ToCanvasPoint(pt_client, &pt_canvas)) {
        pt_canvas = ClampToCanvas(pt_client);
      }
      UpdateDrag(pt_canvas);
      return 0;
    }
    case WM_LBUTTONUP: {
      if (!dragging_) {
        return 0;
      }
      POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      POINT pt_canvas = {};
      if (!ToCanvasPoint(pt_client, &pt_canvas)) {
        pt_canvas = ClampToCanvas(pt_client);
      }
      EndDrag(pt_canvas);
      return 0;
    }
    case WM_CONTEXTMENU: {
      POINT pt = {};
      pt.x = GET_X_LPARAM(lparam);
      pt.y = GET_Y_LPARAM(lparam);
      if (pt.x == -1 && pt.y == -1) {
        RECT wr = {};
        GetWindowRect(hwnd_, &wr);
        pt.x = wr.left + 16;
        pt.y = wr.top + 16;
      }
      ShowContextMenu(pt);
      return 0;
    }
    case WM_KEYDOWN: {
      const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
      const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      if (wparam == VK_ESCAPE) {
        if (text_editing_) {
          text_editing_ = false;
          selected_index_ = -1;
          text_edit_index_ = -1;
          Invalidate();
          return 0;
        }
        if (selected_index_ >= 0) {
          selected_index_ = -1;
          text_editing_ = false;
          text_edit_index_ = -1;
          Invalidate();
          return 0;
        }
        EmitCommand(Command::Close);
        return 0;
      }
      if (ctrl && wparam == 'C') {
        EmitCommand(Command::Copy);
        return 0;
      }
      if (ctrl && wparam == 'S') {
        EmitCommand(Command::Save);
        return 0;
      }
      if (ctrl && wparam == 'Z') {
        Undo();
        return 0;
      }
      if (ctrl && wparam == 'Y') {
        Redo();
        return 0;
      }
      if (wparam == VK_DELETE) {
        DeleteSelection();
        return 0;
      }
      if (shift && wparam == '1') {
        SetTool(Tool::Rect);
        return 0;
      }
      if (shift && wparam == '2') {
        SetTool(Tool::Line);
        return 0;
      }
      if (shift && wparam == '3') {
        SetTool(Tool::Arrow);
        return 0;
      }
      if (shift && wparam == '5') {
        SetTool(Tool::Pencil);
        return 0;
      }
      if (shift && wparam == '8') {
        SetTool(Tool::Text);
        return 0;
      }
      if (wparam == 'V') {
        SetTool(Tool::Select);
        return 0;
      }
      if (!ctrl && !shift && wparam == 'R') {
        EmitCommand(Command::Reselect);
        return 0;
      }
      if (wparam == VK_OEM_4) {
        thickness_ = std::max(1, thickness_ - 1);
        return 0;
      }
      if (wparam == VK_OEM_6) {
        thickness_ = std::min(10, thickness_ + 1);
        return 0;
      }
      break;
    }
    case WM_MOUSEWHEEL: {
      const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
      if (delta > 0) {
        thickness_ = std::min(10, thickness_ + 1);
      } else if (delta < 0) {
        thickness_ = std::max(1, thickness_ - 1);
      }
      return 0;
    }
    case WM_CHAR: {
      if (!text_editing_ || text_edit_index_ < 0 ||
          text_edit_index_ >= static_cast<int>(annotations_.size())) {
        break;
      }
      Annotation& ann = annotations_[static_cast<size_t>(text_edit_index_)];
      if (wparam == VK_RETURN) {
        text_editing_ = false;
        PushHistory();
        Invalidate();
        return 0;
      }
      if (wparam == VK_BACK) {
        if (!ann.text.empty()) {
          ann.text.pop_back();
          PushHistory();
          Invalidate();
        }
        return 0;
      }
      if (wparam >= 32) {
        ann.text.push_back(static_cast<wchar_t>(wparam));
        PushHistory();
        Invalidate();
      }
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps = {};
      HDC hdc = BeginPaint(hwnd_, &ps);
      if (hdc) {
        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bmp = nullptr;
        HGDIOBJ old_bmp = nullptr;
        if (mem_dc && w > 0 && h > 0) {
          mem_bmp = CreateCompatibleBitmap(hdc, w, h);
          if (mem_bmp) {
            old_bmp = SelectObject(mem_dc, mem_bmp);
          }
        }
        HDC draw_dc = (mem_dc && mem_bmp) ? mem_dc : hdc;

        HBRUSH bg = CreateSolidBrush(RGB(24, 24, 24));
        FillRect(draw_dc, &rc, bg);
        DeleteObject(bg);

        RECT toolbar = rc;
        toolbar.bottom = std::min(rc.bottom, static_cast<LONG>(kToolbarHeight));
        HBRUSH tb_bg = CreateSolidBrush(RGB(38, 38, 38));
        FillRect(draw_dc, &toolbar, tb_bg);
        DeleteObject(tb_bg);

        RECT canvas = CanvasRectClient();
        if (source_pixels_ && !source_pixels_->empty() && bitmap_size_px_.w > 0 &&
            bitmap_size_px_.h > 0) {
          BITMAPINFO bmi = {};
          bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          bmi.bmiHeader.biWidth = bitmap_size_px_.w;
          bmi.bmiHeader.biHeight = -bitmap_size_px_.h;
          bmi.bmiHeader.biPlanes = 1;
          bmi.bmiHeader.biBitCount = 32;
          bmi.bmiHeader.biCompression = BI_RGB;
          StretchDIBits(draw_dc, canvas.left, canvas.top, bitmap_size_px_.w,
                        bitmap_size_px_.h, 0, 0, bitmap_size_px_.w,
                        bitmap_size_px_.h, source_pixels_->data(), &bmi,
                        DIB_RGB_COLORS, SRCCOPY);
        }

        int saved = SaveDC(draw_dc);
        SetViewportOrgEx(draw_dc, canvas.left, canvas.top, nullptr);
        for (size_t i = 0; i < annotations_.size(); ++i) {
          DrawAnnotation(draw_dc, annotations_[i],
                         selected_index_ == static_cast<int>(i));
        }
        DrawOverlay(draw_dc);
        if (selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(annotations_.size())) {
          DrawSelectionHandles(draw_dc,
                               annotations_[static_cast<size_t>(selected_index_)]);
        }
        RestoreDC(draw_dc, saved);

        if (draw_dc != hdc) {
          BitBlt(hdc, 0, 0, w, h, draw_dc, 0, 0, SRCCOPY);
          SelectObject(mem_dc, old_bmp);
          DeleteObject(mem_bmp);
          DeleteDC(mem_dc);
        } else if (mem_dc) {
          DeleteDC(mem_dc);
        }
      }
      EndPaint(hwnd_, &ps);
      return 0;
    }
    case WM_CLOSE:
      EmitCommand(Command::Close);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wparam, lparam);
}

void AnnotateWindow::EnsureControls() {
  if (btn_select_) {
    return;
  }
  btn_select_ = CreateWindowW(
      L"BUTTON", L"Select", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdSelect),
      instance_, nullptr);
  btn_rect_ = CreateWindowW(L"BUTTON", L"Rect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdRect), instance_, nullptr);
  btn_line_ = CreateWindowW(L"BUTTON", L"Line", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdLine), instance_, nullptr);
  btn_arrow_ = CreateWindowW(L"BUTTON", L"Arrow",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
                             kButtonWidth, kButtonHeight, hwnd_,
                             reinterpret_cast<HMENU>(kCmdArrow), instance_, nullptr);
  btn_pencil_ = CreateWindowW(
      L"BUTTON", L"Pencil", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdPencil),
      instance_, nullptr);
  btn_text_ = CreateWindowW(L"BUTTON", L"Text", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdText), instance_, nullptr);
  btn_reselect_ = CreateWindowW(
      L"BUTTON", L"Range", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdReselect),
      instance_, nullptr);
  btn_undo_ = CreateWindowW(L"BUTTON", L"Undo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdUndo), instance_, nullptr);
  btn_redo_ = CreateWindowW(L"BUTTON", L"Redo", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdRedo), instance_, nullptr);
  btn_copy_ = CreateWindowW(L"BUTTON", L"Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdCopy), instance_, nullptr);
  btn_save_ = CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdSave), instance_, nullptr);
  btn_close_ = CreateWindowW(L"BUTTON", L"Close",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
                             kButtonWidth, kButtonHeight, hwnd_,
                             reinterpret_cast<HMENU>(kCmdClose), instance_, nullptr);
  LayoutControls();
}

void AnnotateWindow::LayoutControls() {
  if (!hwnd_ || !btn_select_) {
    return;
  }
  const int y = (kToolbarHeight - kButtonHeight) / 2;

  RECT rc = {};
  GetClientRect(hwnd_, &rc);

  int x_left = kToolbarPadding;
  HWND left_buttons[] = {btn_select_, btn_rect_, btn_line_, btn_arrow_,
                         btn_pencil_, btn_text_, btn_reselect_};
  for (HWND btn : left_buttons) {
    if (btn) {
      SetWindowPos(btn, nullptr, x_left, y, kButtonWidth, kButtonHeight,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      x_left += kButtonWidth + kButtonGap;
    }
  }

  int x_right = rc.right - kToolbarPadding - kButtonWidth;
  HWND right_buttons[] = {btn_copy_, btn_save_, btn_close_, btn_redo_, btn_undo_};
  for (HWND btn : right_buttons) {
    if (btn) {
      SetWindowPos(btn, nullptr, x_right, y, kButtonWidth, kButtonHeight,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      x_right -= (kButtonWidth + kButtonGap);
    }
  }
}

void AnnotateWindow::UpdateToolButtons() {
  if (!btn_select_) {
    return;
  }
  SetWindowTextW(btn_select_, tool_ == Tool::Select ? L"[Select]" : L"Select");
  SetWindowTextW(btn_rect_, tool_ == Tool::Rect ? L"[Rect]" : L"Rect");
  SetWindowTextW(btn_line_, tool_ == Tool::Line ? L"[Line]" : L"Line");
  SetWindowTextW(btn_arrow_, tool_ == Tool::Arrow ? L"[Arrow]" : L"Arrow");
  SetWindowTextW(btn_pencil_, tool_ == Tool::Pencil ? L"[Pencil]" : L"Pencil");
  SetWindowTextW(btn_text_, tool_ == Tool::Text ? L"[Text]" : L"Text");
}

void AnnotateWindow::SetTool(Tool tool) {
  tool_ = tool;
  text_editing_ = false;
  text_edit_index_ = -1;
  if (hwnd_) {
    SetFocus(hwnd_);
  }
  UpdateToolButtons();
  Invalidate();
}

void AnnotateWindow::Invalidate() {
  if (!hwnd_) {
    return;
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
}

RECT AnnotateWindow::CanvasRectClient() const {
  RECT rc = {};
  rc.left = 0;
  rc.top = kToolbarHeight;
  rc.right = bitmap_size_px_.w;
  rc.bottom = kToolbarHeight + bitmap_size_px_.h;
  return rc;
}

bool AnnotateWindow::ToCanvasPoint(POINT client_pt, POINT* out_canvas) const {
  RECT canvas = CanvasRectClient();
  if (client_pt.x < canvas.left || client_pt.y < canvas.top ||
      client_pt.x >= canvas.right || client_pt.y >= canvas.bottom) {
    return false;
  }
  if (out_canvas) {
    out_canvas->x = client_pt.x - canvas.left;
    out_canvas->y = client_pt.y - canvas.top;
  }
  return true;
}

POINT AnnotateWindow::ClampToCanvas(POINT client_pt) const {
  RECT canvas = CanvasRectClient();
  POINT out = {};
  out.x = ClampInt(client_pt.x - canvas.left, 0, std::max(0, bitmap_size_px_.w - 1));
  out.y = ClampInt(client_pt.y - canvas.top, 0, std::max(0, bitmap_size_px_.h - 1));
  return out;
}

void AnnotateWindow::BeginDrag(POINT canvas_pt) {
  if (hwnd_) {
    SetFocus(hwnd_);
  }
  SetCapture(hwnd_);
  dragging_ = true;
  drag_start_ = canvas_pt;
  drag_current_ = canvas_pt;
  drag_mode_ = DragMode::None;
  drag_index_ = -1;
  drag_offset_ = {};

  if (text_editing_ && tool_ != Tool::Text) {
    text_editing_ = false;
    text_edit_index_ = -1;
  }

  if (tool_ == Tool::Text) {
    DragMode text_hit_mode = DragMode::None;
    const int text_hit = HitTestAnnotation(canvas_pt, &text_hit_mode);
    if (text_hit >= 0 &&
        annotations_[static_cast<size_t>(text_hit)].type == AnnotationType::Text) {
      selected_index_ = text_hit;
      drag_index_ = text_hit;
      drag_seed_ = annotations_[static_cast<size_t>(text_hit)];
      drag_mode_ = DragMode::MoveText;
      text_editing_ = false;
      text_edit_index_ = -1;
      Invalidate();
      return;
    }
    Annotation text;
    text.type = AnnotationType::Text;
    text.color = color_;
    text.text_size = 22;
    text.p1 = canvas_pt;
    text.p2 = canvas_pt;
    annotations_.push_back(text);
    selected_index_ = static_cast<int>(annotations_.size() - 1);
    text_editing_ = true;
    text_edit_index_ = selected_index_;
    PushHistory();
    dragging_ = false;
    drag_mode_ = DragMode::None;
    ReleaseCapture();
    SetFocus(hwnd_);
    Invalidate();
    return;
  }

  DragMode hit_mode = DragMode::None;
  const int hit_index = HitTestAnnotation(canvas_pt, &hit_mode);
  if (hit_index >= 0 && AnnotationEditable(annotations_[hit_index].type)) {
    selected_index_ = hit_index;
    drag_index_ = hit_index;
    drag_seed_ = annotations_[hit_index];
    drag_mode_ = hit_mode;
    Invalidate();
    return;
  }

  selected_index_ = -1;
  drag_seed_ = {};
  drag_seed_.color = color_;
  drag_seed_.thickness = thickness_;
  drag_seed_.p1 = canvas_pt;
  drag_seed_.p2 = canvas_pt;
  switch (tool_) {
    case Tool::Rect:
      drag_mode_ = DragMode::CreateRect;
      drag_seed_.type = AnnotationType::Rect;
      break;
    case Tool::Line:
      drag_mode_ = DragMode::CreateLine;
      drag_seed_.type = AnnotationType::Line;
      break;
    case Tool::Arrow:
      drag_mode_ = DragMode::CreateArrow;
      drag_seed_.type = AnnotationType::Arrow;
      break;
    case Tool::Pencil:
      drag_mode_ = DragMode::CreatePencil;
      drag_seed_.type = AnnotationType::Pencil;
      drag_seed_.points.clear();
      drag_seed_.points.push_back(canvas_pt);
      break;
    case Tool::Select:
      drag_mode_ = DragMode::None;
      dragging_ = false;
      ReleaseCapture();
      break;
    case Tool::Text:
      break;
  }
  Invalidate();
}

void AnnotateWindow::UpdateDrag(POINT canvas_pt) {
  if (!dragging_) {
    return;
  }
  POINT adjusted = canvas_pt;
  const bool shift_locked = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  if (shift_locked) {
    if (drag_mode_ == DragMode::CreateLine || drag_mode_ == DragMode::CreateArrow) {
      adjusted = SnapPoint45(drag_start_, canvas_pt);
    } else if (drag_mode_ == DragMode::MoveLineStart &&
               drag_index_ >= 0 &&
               drag_index_ < static_cast<int>(annotations_.size())) {
      adjusted = SnapPoint45(drag_seed_.p2, canvas_pt);
    } else if (drag_mode_ == DragMode::MoveLineEnd &&
               drag_index_ >= 0 &&
               drag_index_ < static_cast<int>(annotations_.size())) {
      adjusted = SnapPoint45(drag_seed_.p1, canvas_pt);
    }
  }
  drag_current_ = adjusted;
  if (drag_mode_ == DragMode::CreatePencil) {
    if (drag_seed_.points.empty() || !PointsEqual(drag_seed_.points.back(), adjusted)) {
      drag_seed_.points.push_back(adjusted);
    }
    Invalidate();
    return;
  }
  if (drag_index_ >= 0 && drag_index_ < static_cast<int>(annotations_.size())) {
    Annotation& ann = annotations_[static_cast<size_t>(drag_index_)];
    const int dx = adjusted.x - drag_start_.x;
    const int dy = adjusted.y - drag_start_.y;
    switch (drag_mode_) {
      case DragMode::MoveRect:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        ann.p2.x = drag_seed_.p2.x + dx;
        ann.p2.y = drag_seed_.p2.y + dy;
        break;
      case DragMode::ResizeRectTL:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        break;
      case DragMode::ResizeRectTR:
        ann.p2.x = drag_seed_.p2.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        break;
      case DragMode::ResizeRectBL:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p2.y = drag_seed_.p2.y + dy;
        break;
      case DragMode::ResizeRectBR:
        ann.p2.x = drag_seed_.p2.x + dx;
        ann.p2.y = drag_seed_.p2.y + dy;
        break;
      case DragMode::MoveLine:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        ann.p2.x = drag_seed_.p2.x + dx;
        ann.p2.y = drag_seed_.p2.y + dy;
        break;
      case DragMode::MoveLineStart:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        break;
      case DragMode::MoveLineEnd:
        ann.p2.x = drag_seed_.p2.x + dx;
        ann.p2.y = drag_seed_.p2.y + dy;
        break;
      case DragMode::MoveText:
        ann.p1.x = drag_seed_.p1.x + dx;
        ann.p1.y = drag_seed_.p1.y + dy;
        ann.p2 = ann.p1;
        break;
      default:
        break;
    }
  }
  Invalidate();
}

void AnnotateWindow::EndDrag(POINT canvas_pt) {
  if (!dragging_) {
    return;
  }
  ReleaseCapture();
  dragging_ = false;
  POINT adjusted = canvas_pt;
  const bool shift_locked = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  if (shift_locked) {
    if (drag_mode_ == DragMode::CreateLine || drag_mode_ == DragMode::CreateArrow) {
      adjusted = SnapPoint45(drag_start_, canvas_pt);
    } else if (drag_mode_ == DragMode::MoveLineStart && drag_index_ >= 0 &&
               drag_index_ < static_cast<int>(annotations_.size())) {
      adjusted = SnapPoint45(drag_seed_.p2, canvas_pt);
    } else if (drag_mode_ == DragMode::MoveLineEnd && drag_index_ >= 0 &&
               drag_index_ < static_cast<int>(annotations_.size())) {
      adjusted = SnapPoint45(drag_seed_.p1, canvas_pt);
    }
  }
  drag_current_ = adjusted;
  bool changed = false;

  switch (drag_mode_) {
    case DragMode::CreateRect: {
      RectPX r = NormalizeRect(RectFromPoints(drag_start_, drag_current_));
      if (r.w >= kMinShapeSize && r.h >= kMinShapeSize) {
        Annotation ann = drag_seed_;
        ann.p1.x = r.x;
        ann.p1.y = r.y;
        ann.p2.x = r.x + r.w;
        ann.p2.y = r.y + r.h;
        annotations_.push_back(std::move(ann));
        selected_index_ = static_cast<int>(annotations_.size() - 1);
        changed = true;
      }
      break;
    }
    case DragMode::CreateLine:
    case DragMode::CreateArrow: {
      if (DistanceSq(drag_start_, drag_current_) >=
          static_cast<double>(kMinShapeSize * kMinShapeSize)) {
        Annotation ann = drag_seed_;
        ann.p1 = drag_start_;
        ann.p2 = drag_current_;
        annotations_.push_back(std::move(ann));
        selected_index_ = static_cast<int>(annotations_.size() - 1);
        changed = true;
      }
      break;
    }
    case DragMode::CreatePencil:
      if (drag_seed_.points.size() > 1) {
        annotations_.push_back(std::move(drag_seed_));
        selected_index_ = static_cast<int>(annotations_.size() - 1);
        changed = true;
      }
      break;
    case DragMode::MoveRect:
    case DragMode::ResizeRectTL:
    case DragMode::ResizeRectTR:
    case DragMode::ResizeRectBL:
    case DragMode::ResizeRectBR:
    case DragMode::MoveLine:
    case DragMode::MoveLineStart:
    case DragMode::MoveLineEnd:
    case DragMode::MoveText:
      if (drag_index_ >= 0 && drag_index_ < static_cast<int>(annotations_.size())) {
        const Annotation& current = annotations_[static_cast<size_t>(drag_index_)];
        changed = !PointsEqual(current.p1, drag_seed_.p1) ||
                  !PointsEqual(current.p2, drag_seed_.p2);
      }
      break;
    default:
      break;
  }

  if (changed) {
    PushHistory();
  }
  drag_mode_ = DragMode::None;
  drag_index_ = -1;
  Invalidate();
}

int AnnotateWindow::HitTestAnnotation(POINT canvas_pt, DragMode* mode_out) const {
  if (mode_out) {
    *mode_out = DragMode::None;
  }
  const double tol_sq = static_cast<double>(kHitTolerance * kHitTolerance);
  for (int i = static_cast<int>(annotations_.size()) - 1; i >= 0; --i) {
    const Annotation& ann = annotations_[static_cast<size_t>(i)];
    if (!AnnotationTypeAllowedByTool(ann.type) || !AnnotationEditable(ann.type)) {
      continue;
    }
    if (ann.type == AnnotationType::Rect) {
      RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
      POINT tl = {r.x, r.y};
      POINT tr = {r.x + r.w, r.y};
      POINT bl = {r.x, r.y + r.h};
      POINT br = {r.x + r.w, r.y + r.h};
      if (DistanceSq(canvas_pt, tl) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::ResizeRectTL;
        }
        return i;
      }
      if (DistanceSq(canvas_pt, tr) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::ResizeRectTR;
        }
        return i;
      }
      if (DistanceSq(canvas_pt, bl) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::ResizeRectBL;
        }
        return i;
      }
      if (DistanceSq(canvas_pt, br) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::ResizeRectBR;
        }
        return i;
      }
      if (canvas_pt.x >= r.x && canvas_pt.y >= r.y && canvas_pt.x <= r.x + r.w &&
          canvas_pt.y <= r.y + r.h) {
        if (mode_out) {
          *mode_out = DragMode::MoveRect;
        }
        return i;
      }
      continue;
    }
    if (ann.type == AnnotationType::Line || ann.type == AnnotationType::Arrow) {
      if (DistanceSq(canvas_pt, ann.p1) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::MoveLineStart;
        }
        return i;
      }
      if (DistanceSq(canvas_pt, ann.p2) <= tol_sq) {
        if (mode_out) {
          *mode_out = DragMode::MoveLineEnd;
        }
        return i;
      }
      const int seg_tol = std::max(kHitTolerance, ann.thickness + 2);
      if (DistanceToSegmentSq(canvas_pt, ann.p1, ann.p2) <=
          static_cast<double>(seg_tol * seg_tol)) {
        if (mode_out) {
          *mode_out = DragMode::MoveLine;
        }
        return i;
      }
      continue;
    }
    if (ann.type == AnnotationType::Text) {
      RectPX r = RectBoundsForAnnotation(ann);
      if (canvas_pt.x >= r.x && canvas_pt.y >= r.y && canvas_pt.x <= r.x + r.w &&
          canvas_pt.y <= r.y + r.h) {
        if (mode_out) {
          *mode_out = DragMode::MoveText;
        }
        return i;
      }
    }
  }
  return -1;
}

bool AnnotateWindow::AnnotationTypeAllowedByTool(AnnotationType type) const {
  if (tool_ == Tool::Select) {
    return true;
  }
  if (tool_ == Tool::Rect) {
    return type == AnnotationType::Rect;
  }
  if (tool_ == Tool::Line) {
    return type == AnnotationType::Line;
  }
  if (tool_ == Tool::Arrow) {
    return type == AnnotationType::Arrow;
  }
  if (tool_ == Tool::Text) {
    return type == AnnotationType::Text;
  }
  if (tool_ == Tool::Pencil) {
    return type == AnnotationType::Pencil;
  }
  return true;
}

bool AnnotateWindow::AnnotationEditable(AnnotationType type) const {
  return type != AnnotationType::Pencil;
}

RectPX AnnotateWindow::RectFromPoints(POINT a, POINT b) const {
  RectPX r = {};
  r.x = std::min(a.x, b.x);
  r.y = std::min(a.y, b.y);
  r.w = std::abs(b.x - a.x);
  r.h = std::abs(b.y - a.y);
  return r;
}

RectPX AnnotateWindow::RectBoundsForAnnotation(const Annotation& ann) const {
  if (ann.type == AnnotationType::Rect) {
    return NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  }
  if (ann.type == AnnotationType::Line || ann.type == AnnotationType::Arrow) {
    RectPX r = {};
    r.x = std::min(ann.p1.x, ann.p2.x);
    r.y = std::min(ann.p1.y, ann.p2.y);
    r.w = std::abs(ann.p2.x - ann.p1.x);
    r.h = std::abs(ann.p2.y - ann.p1.y);
    return r;
  }
  if (ann.type == AnnotationType::Pencil) {
    if (ann.points.empty()) {
      return {};
    }
    int min_x = ann.points[0].x;
    int max_x = ann.points[0].x;
    int min_y = ann.points[0].y;
    int max_y = ann.points[0].y;
    for (const POINT& p : ann.points) {
      min_x = std::min(min_x, static_cast<int>(p.x));
      max_x = std::max(max_x, static_cast<int>(p.x));
      min_y = std::min(min_y, static_cast<int>(p.y));
      max_y = std::max(max_y, static_cast<int>(p.y));
    }
    RectPX r = {};
    r.x = min_x;
    r.y = min_y;
    r.w = max_x - min_x;
    r.h = max_y - min_y;
    return r;
  }
  if (ann.type == AnnotationType::Text) {
    RectPX r = {};
    const int char_w = std::max(8, ann.text_size / 2);
    const int w =
        std::max(char_w * 2, static_cast<int>(ann.text.size()) * char_w);
    const int h = ann.text_size + 10;
    r.x = ann.p1.x;
    r.y = ann.p1.y;
    r.w = w;
    r.h = h;
    return r;
  }
  return {};
}

RectPX AnnotateWindow::NormalizeRect(RectPX rect) const {
  if (rect.w < 0) {
    rect.x += rect.w;
    rect.w = -rect.w;
  }
  if (rect.h < 0) {
    rect.y += rect.h;
    rect.h = -rect.h;
  }
  rect.x = ClampInt(rect.x, 0, std::max(0, bitmap_size_px_.w - 1));
  rect.y = ClampInt(rect.y, 0, std::max(0, bitmap_size_px_.h - 1));
  rect.w = ClampInt(rect.w, 0, std::max(0, bitmap_size_px_.w - rect.x));
  rect.h = ClampInt(rect.h, 0, std::max(0, bitmap_size_px_.h - rect.y));
  return rect;
}

void AnnotateWindow::DrawOverlay(HDC hdc) const {
  if (drag_mode_ == DragMode::CreateRect || drag_mode_ == DragMode::CreateLine ||
      drag_mode_ == DragMode::CreateArrow) {
    Annotation preview = drag_seed_;
    preview.p1 = drag_start_;
    preview.p2 = drag_current_;
    DrawAnnotation(hdc, preview, false);
    return;
  }
  if (drag_mode_ == DragMode::CreatePencil && drag_seed_.points.size() > 1) {
    DrawAnnotation(hdc, drag_seed_, false);
    return;
  }
}

void AnnotateWindow::DrawAnnotation(HDC hdc, const Annotation& ann,
                                    bool selected) const {
  HPEN pen = CreatePen(PS_SOLID, std::max(1, ann.thickness), ann.color);
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

  switch (ann.type) {
    case AnnotationType::Rect: {
      RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
      Rectangle(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
      break;
    }
    case AnnotationType::Line:
      MoveToEx(hdc, ann.p1.x, ann.p1.y, nullptr);
      LineTo(hdc, ann.p2.x, ann.p2.y);
      break;
    case AnnotationType::Arrow:
      MoveToEx(hdc, ann.p1.x, ann.p1.y, nullptr);
      LineTo(hdc, ann.p2.x, ann.p2.y);
      DrawArrowHead(hdc, ann.p1, ann.p2, ann.color, ann.thickness);
      break;
    case AnnotationType::Pencil:
      if (ann.points.size() > 1) {
        Polyline(hdc, ann.points.data(), static_cast<int>(ann.points.size()));
      }
      break;
    case AnnotationType::Text: {
      HFONT font = CreateFontW(
          ann.text_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
      HGDIOBJ old_font = SelectObject(hdc, font);
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, ann.color);
      const std::wstring draw = ann.text.empty() ? L"Text" : ann.text;
      TextOutW(hdc, ann.p1.x, ann.p1.y, draw.c_str(),
               static_cast<int>(draw.size()));
      SelectObject(hdc, old_font);
      DeleteObject(font);
      break;
    }
    default:
      break;
  }

  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(pen);

  if (selected && ann.type == AnnotationType::Text && text_editing_ &&
      selected_index_ >= 0 &&
      annotations_[static_cast<size_t>(selected_index_)].type ==
          AnnotationType::Text) {
    RectPX r = RectBoundsForAnnotation(ann);
    HPEN caret_pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ old_caret_pen = SelectObject(hdc, caret_pen);
    MoveToEx(hdc, r.x + r.w + 2, r.y, nullptr);
    LineTo(hdc, r.x + r.w + 2, r.y + r.h);
    SelectObject(hdc, old_caret_pen);
    DeleteObject(caret_pen);
  }
}

void AnnotateWindow::DrawArrowHead(HDC hdc, POINT start, POINT end, COLORREF color,
                                   int thickness) const {
  const double dx = static_cast<double>(end.x - start.x);
  const double dy = static_cast<double>(end.y - start.y);
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0) {
    return;
  }
  const double ux = dx / len;
  const double uy = dy / len;
  const double head_len = std::max(8.0, static_cast<double>(thickness * 4));
  const double wing = std::max(5.0, static_cast<double>(thickness * 2));
  POINT p0 = end;
  POINT p1 = {static_cast<int>(std::lround(end.x - ux * head_len - uy * wing)),
              static_cast<int>(std::lround(end.y - uy * head_len + ux * wing))};
  POINT p2 = {static_cast<int>(std::lround(end.x - ux * head_len + uy * wing)),
              static_cast<int>(std::lround(end.y - uy * head_len - ux * wing))};
  POINT tri[3] = {p0, p1, p2};
  HBRUSH brush = CreateSolidBrush(color);
  HGDIOBJ old_brush = SelectObject(hdc, brush);
  Polygon(hdc, tri, 3);
  SelectObject(hdc, old_brush);
  DeleteObject(brush);
}

void AnnotateWindow::DrawSelectionHandles(HDC hdc, const Annotation& ann) const {
  if (!AnnotationEditable(ann.type)) {
    return;
  }
  RectPX r = RectBoundsForAnnotation(ann);
  if (r.w <= 0 && r.h <= 0) {
    return;
  }
  const int hs = kHandleSize;
  RECT handles[4] = {
      {r.x - hs / 2, r.y - hs / 2, r.x + hs / 2, r.y + hs / 2},
      {r.x + r.w - hs / 2, r.y - hs / 2, r.x + r.w + hs / 2, r.y + hs / 2},
      {r.x - hs / 2, r.y + r.h - hs / 2, r.x + hs / 2, r.y + r.h + hs / 2},
      {r.x + r.w - hs / 2, r.y + r.h - hs / 2, r.x + r.w + hs / 2,
       r.y + r.h + hs / 2},
  };
  HBRUSH fill = CreateSolidBrush(RGB(255, 255, 255));
  HPEN border = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  HGDIOBJ old_brush = SelectObject(hdc, fill);
  HGDIOBJ old_pen = SelectObject(hdc, border);
  for (RECT handle : handles) {
    Rectangle(hdc, handle.left, handle.top, handle.right, handle.bottom);
  }
  SelectObject(hdc, old_pen);
  SelectObject(hdc, old_brush);
  DeleteObject(border);
  DeleteObject(fill);
}

bool AnnotateWindow::BuildComposedPixels(
    std::shared_ptr<std::vector<uint8_t>>* out_pixels, SizePX* out_size,
    int32_t* out_stride) const {
  if (!out_pixels || !out_size || !out_stride || !source_pixels_ ||
      bitmap_size_px_.w <= 0 || bitmap_size_px_.h <= 0 ||
      stride_bytes_ < bitmap_size_px_.w * 4) {
    return false;
  }

  const int width = bitmap_size_px_.w;
  const int height = bitmap_size_px_.h;
  const int stride = width * 4;
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  HDC screen = GetDC(nullptr);
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    return false;
  }

  uint8_t* dst = reinterpret_cast<uint8_t*>(bits);
  const uint8_t* src = source_pixels_->data();
  for (int y = 0; y < height; ++y) {
    std::memcpy(dst + static_cast<size_t>(y) * stride,
                src + static_cast<size_t>(y) * stride_bytes_,
                static_cast<size_t>(stride));
  }

  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(dib);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  for (size_t i = 0; i < annotations_.size(); ++i) {
    DrawAnnotation(mem, annotations_[i], false);
  }
  SelectObject(mem, old);
  DeleteDC(mem);

  auto pixels = std::make_shared<std::vector<uint8_t>>();
  pixels->resize(static_cast<size_t>(stride) * static_cast<size_t>(height));
  std::memcpy(pixels->data(), bits, pixels->size());

  DeleteObject(dib);
  *out_pixels = std::move(pixels);
  *out_size = SizePX{width, height};
  *out_stride = stride;
  return true;
}

void AnnotateWindow::PushHistory() {
  if (history_index_ + 1 < history_.size()) {
    history_.resize(history_index_ + 1);
  }
  history_.push_back(annotations_);
  history_index_ = history_.size() - 1;
}

bool AnnotateWindow::Undo() {
  if (history_index_ == 0 || history_.empty()) {
    return false;
  }
  --history_index_;
  annotations_ = history_[history_index_];
  selected_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  Invalidate();
  return true;
}

bool AnnotateWindow::Redo() {
  if (history_.empty() || history_index_ + 1 >= history_.size()) {
    return false;
  }
  ++history_index_;
  annotations_ = history_[history_index_];
  selected_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  Invalidate();
  return true;
}

void AnnotateWindow::DeleteSelection() {
  if (selected_index_ < 0 || selected_index_ >= static_cast<int>(annotations_.size())) {
    return;
  }
  if (!AnnotationEditable(annotations_[static_cast<size_t>(selected_index_)].type)) {
    return;
  }
  annotations_.erase(annotations_.begin() + selected_index_);
  selected_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  PushHistory();
  Invalidate();
}

void AnnotateWindow::ShowContextMenu(POINT screen_pt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  AppendMenuW(menu, MF_STRING, kCmdSelect, L"Tool: Select");
  AppendMenuW(menu, MF_STRING, kCmdRect, L"Tool: Rect");
  AppendMenuW(menu, MF_STRING, kCmdLine, L"Tool: Line");
  AppendMenuW(menu, MF_STRING, kCmdArrow, L"Tool: Arrow");
  AppendMenuW(menu, MF_STRING, kCmdPencil, L"Tool: Pencil");
  AppendMenuW(menu, MF_STRING, kCmdText, L"Tool: Text");
  AppendMenuW(menu, MF_STRING, kCmdReselect, L"Reselect Range (R)");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCmdUndo, L"Undo");
  AppendMenuW(menu, MF_STRING, kCmdRedo, L"Redo");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCmdCopy, L"Copy");
  AppendMenuW(menu, MF_STRING, kCmdSave, L"Save");
  AppendMenuW(menu, MF_STRING, kCmdClose, L"Close");

  SetForegroundWindow(hwnd_);
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                  screen_pt.x, screen_pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
  if (cmd != 0) {
    PostMessageW(hwnd_, WM_COMMAND, static_cast<WPARAM>(cmd), 0);
  }
}

void AnnotateWindow::EmitCommand(Command cmd) {
  if (!on_command_) {
    return;
  }
  std::shared_ptr<std::vector<uint8_t>> pixels;
  SizePX size = {};
  int32_t stride = 0;
  BuildComposedPixels(&pixels, &size, &stride);
  on_command_(cmd, std::move(pixels), size, stride);
}

} // namespace snappin
