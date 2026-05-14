#include "AnnotateWindow.h"
#include "AnnotationEffects.h"
#include "AnnotateLayout.h"

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
const int kLeftToolbarButtonCount = 18;
const int kRightToolbarButtonCount = 5;
const int kHandleSize = 8;
const int kHitTolerance = 8;
const int kMinShapeSize = 1;

const INT_PTR kCmdSelect = 5201;
const INT_PTR kCmdRect = 5202;
const INT_PTR kCmdEllipse = 5203;
const INT_PTR kCmdLine = 5204;
const INT_PTR kCmdArrow = 5205;
const INT_PTR kCmdSerial = 5206;
const INT_PTR kCmdMosaic = 5207;
const INT_PTR kCmdEraser = 5208;
const INT_PTR kCmdPencil = 5209;
const INT_PTR kCmdText = 5210;
const INT_PTR kCmdReselect = 5211;
const INT_PTR kCmdUndo = 5212;
const INT_PTR kCmdRedo = 5213;
const INT_PTR kCmdCopy = 5214;
const INT_PTR kCmdSave = 5215;
const INT_PTR kCmdClose = 5216;
const INT_PTR kCmdHighlighter = 5217;
const INT_PTR kCmdSpotlight = 5218;
const INT_PTR kCmdBlur = 5219;
const INT_PTR kCmdPolyline = 5220;
const INT_PTR kCmdWatermark = 5221;
const INT_PTR kCmdMagnifier = 5222;
const INT_PTR kCmdTextBackground = 5223;

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
  const int width = desired.right - desired.left;
  const int height = desired.bottom - desired.top;
  RectPX desired_px{desired.left, desired.top, width, height};
  RectPX bounds_px{bounds.left, bounds.top, bounds.right - bounds.left,
                   bounds.bottom - bounds.top};
  RectPX clamped = ClampWindowRectToBounds(desired_px, bounds_px);
  RECT out = {clamped.x, clamped.y, clamped.x + clamped.w,
              clamped.y + clamped.h};
  return out;
}

bool PointsEqual(const POINT& a, const POINT& b) {
  return a.x == b.x && a.y == b.y;
}

bool PointVectorsEqual(const std::vector<POINT>& a,
                       const std::vector<POINT>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (!PointsEqual(a[i], b[i])) {
      return false;
    }
  }
  return true;
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

bool PointInsideEllipse(const POINT& point, const RectPX& bounds) {
  if (bounds.w <= 0 || bounds.h <= 0) {
    return false;
  }
  const double rx = static_cast<double>(bounds.w) / 2.0;
  const double ry = static_cast<double>(bounds.h) / 2.0;
  const double cx = static_cast<double>(bounds.x) + rx;
  const double cy = static_cast<double>(bounds.y) + ry;
  const double nx = (static_cast<double>(point.x) - cx) / rx;
  const double ny = (static_cast<double>(point.y) - cy) / ry;
  return nx * nx + ny * ny <= 1.0;
}

int SerialEntryValue(const std::wstring& digits) {
  int value = 0;
  for (wchar_t ch : digits) {
    if (ch < L'0' || ch > L'9') {
      continue;
    }
    if (value > 999999) {
      value = 9999999;
      break;
    }
    value = value * 10 + static_cast<int>(ch - L'0');
  }
  return std::max(1, value);
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
  wc.style = CS_DBLCLKS;
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
  selected_point_index_ = -1;
  drag_point_index_ = -1;
  text_editing_ = false;
  polyline_drawing_ = false;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
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
  selected_point_index_ = -1;
  drag_index_ = -1;
  drag_point_index_ = -1;
  drag_mode_ = DragMode::None;
  dragging_ = false;
  drag_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  polyline_drawing_ = false;
  tool_ = Tool::Rect;
  color_ = RGB(255, 80, 64);
  thickness_ = 2;
  next_serial_value_ = 1;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
  next_watermark_text_.clear();
  next_text_background_ = false;

  const int min_toolbar_width = AnnotateToolbarMinWidth(
      kLeftToolbarButtonCount, kRightToolbarButtonCount, kButtonWidth,
      kButtonGap, kToolbarPadding);
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
  selected_point_index_ = -1;
  text_editing_ = false;
  polyline_drawing_ = false;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
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
        case kCmdEllipse:
          SetTool(Tool::Ellipse);
          return 0;
        case kCmdLine:
          SetTool(Tool::Line);
          return 0;
        case kCmdPolyline:
          SetTool(Tool::Polyline);
          return 0;
        case kCmdArrow:
          SetTool(Tool::Arrow);
          return 0;
        case kCmdSerial:
          SetTool(Tool::Serial);
          return 0;
        case kCmdMosaic:
          SetTool(Tool::Mosaic);
          return 0;
        case kCmdBlur:
          SetTool(Tool::Blur);
          return 0;
        case kCmdEraser:
          SetTool(Tool::Eraser);
          return 0;
        case kCmdHighlighter:
          SetTool(Tool::Highlighter);
          return 0;
        case kCmdSpotlight:
          SetTool(Tool::Spotlight);
          return 0;
        case kCmdWatermark:
          SetTool(Tool::Watermark);
          return 0;
        case kCmdMagnifier:
          SetTool(Tool::Magnifier);
          return 0;
        case kCmdPencil:
          SetTool(Tool::Pencil);
          return 0;
        case kCmdText:
          SetTool(Tool::Text);
          return 0;
        case kCmdTextBackground:
          ToggleTextBackground();
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
      if (polyline_drawing_) {
        POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        POINT pt_canvas = {};
        if (!ToCanvasPoint(pt_client, &pt_canvas)) {
          pt_canvas = ClampToCanvas(pt_client);
        }
        UpdatePolylinePreview(pt_canvas);
        return 0;
      }
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
    case WM_RBUTTONDOWN: {
      if (!polyline_drawing_) {
        break;
      }
      POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      POINT pt_canvas = {};
      if (!ToCanvasPoint(pt_client, &pt_canvas)) {
        pt_canvas = ClampToCanvas(pt_client);
      }
      FinishPolyline(pt_canvas);
      return 0;
    }
    case WM_LBUTTONDBLCLK: {
      if (!polyline_drawing_) {
        if (tool_ == Tool::Select) {
          POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
          POINT pt_canvas = {};
          if (ToCanvasPoint(pt_client, &pt_canvas) &&
              InsertPolylinePointAt(pt_canvas)) {
            return 0;
          }
        }
        break;
      }
      POINT pt_client = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      POINT pt_canvas = {};
      if (!ToCanvasPoint(pt_client, &pt_canvas)) {
        pt_canvas = ClampToCanvas(pt_client);
      }
      FinishPolyline(pt_canvas);
      return 0;
    }
    case WM_CONTEXTMENU: {
      if (polyline_drawing_) {
        return 0;
      }
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
        if (polyline_drawing_) {
          polyline_drawing_ = false;
          drag_seed_ = {};
          Invalidate();
          return 0;
        }
        if (text_editing_) {
          text_editing_ = false;
          selected_index_ = -1;
          selected_point_index_ = -1;
          text_edit_index_ = -1;
          Invalidate();
          return 0;
        }
        if (selected_index_ >= 0) {
          selected_index_ = -1;
          selected_point_index_ = -1;
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
      const bool plus_key = wparam == VK_OEM_PLUS || wparam == VK_ADD;
      const bool minus_key = wparam == VK_OEM_MINUS || wparam == VK_SUBTRACT;
      if (!ctrl && tool_ == Tool::Serial && (plus_key || minus_key)) {
        serial_entry_text_.clear();
        serial_entry_target_index_ = -2;
        const int delta = plus_key ? 1 : -1;
        if (selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(annotations_.size()) &&
            annotations_[static_cast<size_t>(selected_index_)].type ==
                AnnotationType::Serial) {
          Annotation& ann = annotations_[static_cast<size_t>(selected_index_)];
          const int next_value = AdjustedSerialValue(ann.serial_value, delta);
          if (next_value != ann.serial_value) {
            ann.serial_value = next_value;
            PushHistory();
            Invalidate();
          }
        } else {
          next_serial_value_ = AdjustedSerialValue(next_serial_value_, delta);
        }
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
      if (shift && wparam == '4') {
        SetTool(Tool::Ellipse);
        return 0;
      }
      if (shift && wparam == '5') {
        SetTool(Tool::Pencil);
        return 0;
      }
      if (shift && wparam == '6') {
        SetTool(Tool::Serial);
        return 0;
      }
      if (shift && wparam == '7') {
        SetTool(Tool::Mosaic);
        return 0;
      }
      if (shift && wparam == '8') {
        SetTool(Tool::Text);
        return 0;
      }
      if (shift && wparam == '9') {
        SetTool(Tool::Eraser);
        return 0;
      }
      if (shift && wparam == '0') {
        SetTool(Tool::Highlighter);
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
      if (ApplySerialEntryChar(static_cast<wchar_t>(wparam))) {
        return 0;
      }
      if (ApplyWatermarkEntryChar(static_cast<wchar_t>(wparam))) {
        return 0;
      }
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
  btn_ellipse_ = CreateWindowW(
      L"BUTTON", L"Ellipse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdEllipse),
      instance_, nullptr);
  btn_line_ = CreateWindowW(L"BUTTON", L"Line", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdLine), instance_, nullptr);
  btn_polyline_ = CreateWindowW(
      L"BUTTON", L"Polyline", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdPolyline),
      instance_, nullptr);
  btn_arrow_ = CreateWindowW(L"BUTTON", L"Arrow",
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
                             kButtonWidth, kButtonHeight, hwnd_,
                             reinterpret_cast<HMENU>(kCmdArrow), instance_, nullptr);
  btn_serial_ = CreateWindowW(
      L"BUTTON", L"Serial", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdSerial),
      instance_, nullptr);
  btn_mosaic_ = CreateWindowW(
      L"BUTTON", L"Mosaic", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdMosaic),
      instance_, nullptr);
  btn_blur_ = CreateWindowW(
      L"BUTTON", L"Blur", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdBlur),
      instance_, nullptr);
  btn_eraser_ = CreateWindowW(
      L"BUTTON", L"Eraser", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdEraser),
      instance_, nullptr);
  btn_highlighter_ = CreateWindowW(
      L"BUTTON", L"Highlighter", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(kCmdHighlighter), instance_, nullptr);
  btn_spotlight_ = CreateWindowW(
      L"BUTTON", L"Spotlight", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(kCmdSpotlight), instance_, nullptr);
  btn_watermark_ = CreateWindowW(
      L"BUTTON", L"Watermark", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(kCmdWatermark), instance_, nullptr);
  btn_magnifier_ = CreateWindowW(
      L"BUTTON", L"Magnifier", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(kCmdMagnifier), instance_, nullptr);
  btn_pencil_ = CreateWindowW(
      L"BUTTON", L"Pencil", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_, reinterpret_cast<HMENU>(kCmdPencil),
      instance_, nullptr);
  btn_text_ = CreateWindowW(L"BUTTON", L"Text", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                            0, 0, kButtonWidth, kButtonHeight, hwnd_,
                            reinterpret_cast<HMENU>(kCmdText), instance_, nullptr);
  btn_text_bg_ = CreateWindowW(
      L"BUTTON", L"Text BG", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0,
      kButtonWidth, kButtonHeight, hwnd_,
      reinterpret_cast<HMENU>(kCmdTextBackground), instance_, nullptr);
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
  HWND left_buttons[] = {btn_select_, btn_rect_, btn_ellipse_, btn_line_,
                         btn_polyline_, btn_arrow_, btn_serial_, btn_mosaic_,
                         btn_blur_, btn_eraser_, btn_highlighter_,
                         btn_spotlight_, btn_watermark_, btn_magnifier_,
                         btn_pencil_, btn_text_, btn_text_bg_, btn_reselect_};
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
  SetWindowTextW(btn_ellipse_,
                 tool_ == Tool::Ellipse ? L"[Ellipse]" : L"Ellipse");
  SetWindowTextW(btn_line_, tool_ == Tool::Line ? L"[Line]" : L"Line");
  SetWindowTextW(btn_polyline_,
                 tool_ == Tool::Polyline ? L"[Polyline]" : L"Polyline");
  SetWindowTextW(btn_arrow_, tool_ == Tool::Arrow ? L"[Arrow]" : L"Arrow");
  SetWindowTextW(btn_serial_,
                 tool_ == Tool::Serial ? L"[Serial]" : L"Serial");
  SetWindowTextW(btn_mosaic_,
                 tool_ == Tool::Mosaic ? L"[Mosaic]" : L"Mosaic");
  SetWindowTextW(btn_blur_, tool_ == Tool::Blur ? L"[Blur]" : L"Blur");
  SetWindowTextW(btn_eraser_,
                 tool_ == Tool::Eraser ? L"[Eraser]" : L"Eraser");
  SetWindowTextW(btn_highlighter_,
                 tool_ == Tool::Highlighter ? L"[Highlighter]" : L"Highlighter");
  SetWindowTextW(btn_spotlight_,
                 tool_ == Tool::Spotlight ? L"[Spotlight]" : L"Spotlight");
  SetWindowTextW(btn_watermark_,
                 tool_ == Tool::Watermark ? L"[Watermark]" : L"Watermark");
  SetWindowTextW(btn_magnifier_,
                 tool_ == Tool::Magnifier ? L"[Magnifier]" : L"Magnifier");
  SetWindowTextW(btn_pencil_, tool_ == Tool::Pencil ? L"[Pencil]" : L"Pencil");
  SetWindowTextW(btn_text_, tool_ == Tool::Text ? L"[Text]" : L"Text");
  bool text_bg_active = next_text_background_;
  if (selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(annotations_.size()) &&
      annotations_[static_cast<size_t>(selected_index_)].type ==
          AnnotationType::Text) {
    text_bg_active =
        annotations_[static_cast<size_t>(selected_index_)].text_background;
  }
  SetWindowTextW(btn_text_bg_, text_bg_active ? L"[Text BG]" : L"Text BG");
}

void AnnotateWindow::SetTool(Tool tool) {
  polyline_drawing_ = false;
  tool_ = tool;
  selected_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
  if (hwnd_) {
    SetFocus(hwnd_);
  }
  UpdateToolButtons();
  Invalidate();
}

void AnnotateWindow::ToggleTextBackground() {
  if (selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(annotations_.size()) &&
      annotations_[static_cast<size_t>(selected_index_)].type ==
          AnnotationType::Text) {
    Annotation& ann = annotations_[static_cast<size_t>(selected_index_)];
    ann.text_background = !ann.text_background;
    PushHistory();
  } else {
    next_text_background_ = !next_text_background_;
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

bool AnnotateWindow::ApplySerialEntryChar(wchar_t ch) {
  if (tool_ != Tool::Serial) {
    return false;
  }

  const bool is_digit = ch >= L'0' && ch <= L'9';
  const bool is_backspace = ch == L'\b';
  const bool is_commit = ch == L'\r' || ch == L'\n';
  if (!is_digit && !is_backspace && !is_commit) {
    return false;
  }

  int target_index = -1;
  if (selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(annotations_.size()) &&
      annotations_[static_cast<size_t>(selected_index_)].type ==
          AnnotationType::Serial) {
    target_index = selected_index_;
  }
  if (serial_entry_target_index_ != target_index) {
    serial_entry_text_.clear();
    serial_entry_target_index_ = target_index;
  }

  if (is_commit) {
    serial_entry_text_.clear();
    serial_entry_target_index_ = -2;
    return true;
  }

  if (is_backspace) {
    if (!serial_entry_text_.empty()) {
      serial_entry_text_.pop_back();
    }
  } else if (serial_entry_text_.size() < 7) {
    serial_entry_text_.push_back(ch);
  }

  const int value = SerialEntryValue(serial_entry_text_);
  if (target_index >= 0) {
    Annotation& ann = annotations_[static_cast<size_t>(target_index)];
    if (ann.serial_value != value) {
      ann.serial_value = value;
      PushHistory();
    }
  } else {
    next_serial_value_ = value;
  }
  Invalidate();
  return true;
}

bool AnnotateWindow::ApplyWatermarkEntryChar(wchar_t ch) {
  if (tool_ != Tool::Watermark) {
    return false;
  }

  const bool is_backspace = ch == L'\b';
  const bool is_commit = ch == L'\r' || ch == L'\n';
  const bool is_printable = ch >= 32;
  if (!is_backspace && !is_commit && !is_printable) {
    return false;
  }
  if (is_commit) {
    return true;
  }

  std::wstring* target = &next_watermark_text_;
  bool updates_annotation = false;
  if (selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(annotations_.size()) &&
      annotations_[static_cast<size_t>(selected_index_)].type ==
          AnnotationType::Watermark) {
    target = &annotations_[static_cast<size_t>(selected_index_)].text;
    updates_annotation = true;
  }

  const std::wstring before = *target;
  if (is_backspace) {
    if (!target->empty()) {
      target->pop_back();
    }
  } else if (target->size() < 64) {
    target->push_back(ch);
  }
  if (*target != before && updates_annotation) {
    PushHistory();
  }
  Invalidate();
  return true;
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
  drag_point_index_ = -1;
  drag_offset_ = {};

  if (text_editing_ && tool_ != Tool::Text) {
    text_editing_ = false;
    text_edit_index_ = -1;
  }

  if (tool_ == Tool::Polyline) {
    if (!polyline_drawing_) {
      polyline_drawing_ = true;
      selected_index_ = -1;
      selected_point_index_ = -1;
      drag_seed_ = {};
      drag_seed_.type = AnnotationType::Polyline;
      drag_seed_.color = color_;
      drag_seed_.thickness = thickness_;
      drag_seed_.points = {canvas_pt, canvas_pt};
    } else if (!drag_seed_.points.empty()) {
      drag_seed_.points.back() = canvas_pt;
      drag_seed_.points.push_back(canvas_pt);
    }
    dragging_ = false;
    drag_mode_ = DragMode::None;
    ReleaseCapture();
    Invalidate();
    return;
  }

  if (tool_ == Tool::Serial) {
    DragMode serial_hit_mode = DragMode::None;
    const int serial_hit = HitTestAnnotation(canvas_pt, &serial_hit_mode);
    if (serial_hit >= 0 &&
        annotations_[static_cast<size_t>(serial_hit)].type ==
            AnnotationType::Serial) {
      selected_index_ = serial_hit;
      selected_point_index_ = -1;
      drag_index_ = serial_hit;
      drag_seed_ = annotations_[static_cast<size_t>(serial_hit)];
      drag_mode_ = DragMode::MoveText;
      Invalidate();
      return;
    }
    Annotation serial;
    serial.type = AnnotationType::Serial;
    serial.color = color_;
    serial.thickness = thickness_;
    serial.text_size = 18;
    serial.p1 = canvas_pt;
    serial.p2 = canvas_pt;
    serial.serial_value = next_serial_value_++;
    annotations_.push_back(serial);
    selected_index_ = static_cast<int>(annotations_.size() - 1);
    selected_point_index_ = -1;
    serial_entry_text_.clear();
    serial_entry_target_index_ = -2;
    PushHistory();
    dragging_ = false;
    drag_mode_ = DragMode::None;
    ReleaseCapture();
    Invalidate();
    return;
  }

  if (tool_ == Tool::Eraser) {
    DragMode erase_hit_mode = DragMode::None;
    int erase_segment_end_index = -1;
    const int erase_hit =
        HitTestAnnotation(canvas_pt, &erase_hit_mode, &erase_segment_end_index);
    if (erase_hit >= 0) {
      if (!ErasePathSegment(erase_hit, erase_segment_end_index)) {
        annotations_.erase(annotations_.begin() + erase_hit);
      }
      selected_index_ = -1;
      selected_point_index_ = -1;
      PushHistory();
      Invalidate();
    }
    dragging_ = false;
    drag_mode_ = DragMode::None;
    ReleaseCapture();
    return;
  }

  if (tool_ == Tool::Text) {
    DragMode text_hit_mode = DragMode::None;
    const int text_hit = HitTestAnnotation(canvas_pt, &text_hit_mode);
    if (text_hit >= 0 &&
        annotations_[static_cast<size_t>(text_hit)].type == AnnotationType::Text) {
      selected_index_ = text_hit;
      selected_point_index_ = -1;
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
    text.text_background = next_text_background_;
    text.p1 = canvas_pt;
    text.p2 = canvas_pt;
    annotations_.push_back(text);
    selected_index_ = static_cast<int>(annotations_.size() - 1);
    selected_point_index_ = -1;
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
  int hit_point_index = -1;
  const int hit_index =
      HitTestAnnotation(canvas_pt, &hit_mode, &hit_point_index);
  if (hit_index >= 0 && AnnotationEditable(annotations_[hit_index].type)) {
    selected_index_ = hit_index;
    selected_point_index_ =
        hit_mode == DragMode::MovePolylinePoint ? hit_point_index : -1;
    drag_index_ = hit_index;
    drag_point_index_ = hit_point_index;
    drag_seed_ = annotations_[hit_index];
    drag_mode_ = hit_mode;
    Invalidate();
    return;
  }

  selected_index_ = -1;
  selected_point_index_ = -1;
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
    case Tool::Ellipse:
      drag_mode_ = DragMode::CreateEllipse;
      drag_seed_.type = AnnotationType::Ellipse;
      break;
    case Tool::Line:
      drag_mode_ = DragMode::CreateLine;
      drag_seed_.type = AnnotationType::Line;
      break;
    case Tool::Polyline:
      drag_mode_ = DragMode::CreatePolyline;
      drag_seed_.type = AnnotationType::Polyline;
      break;
    case Tool::Arrow:
      drag_mode_ = DragMode::CreateArrow;
      drag_seed_.type = AnnotationType::Arrow;
      break;
    case Tool::Serial:
      break;
    case Tool::Mosaic:
      drag_mode_ = DragMode::CreateMosaic;
      drag_seed_.type = AnnotationType::Mosaic;
      break;
    case Tool::Blur:
      drag_mode_ = DragMode::CreateBlur;
      drag_seed_.type = AnnotationType::Blur;
      break;
    case Tool::Eraser:
      break;
    case Tool::Spotlight:
      drag_mode_ = DragMode::CreateSpotlight;
      drag_seed_.type = AnnotationType::Spotlight;
      break;
    case Tool::Watermark:
      drag_mode_ = DragMode::CreateWatermark;
      drag_seed_.type = AnnotationType::Watermark;
      drag_seed_.color = RGB(255, 255, 255);
      drag_seed_.text = next_watermark_text_;
      break;
    case Tool::Magnifier:
      drag_mode_ = DragMode::CreateMagnifier;
      drag_seed_.type = AnnotationType::Magnifier;
      break;
    case Tool::Highlighter:
      drag_mode_ = DragMode::CreatePencil;
      drag_seed_.type = AnnotationType::Highlighter;
      drag_seed_.color = RGB(255, 232, 80);
      drag_seed_.thickness = std::max(10, thickness_ * 5);
      drag_seed_.points.clear();
      drag_seed_.points.push_back(canvas_pt);
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
      case DragMode::MovePolyline:
        ann.points = drag_seed_.points;
        for (POINT& point : ann.points) {
          point.x += dx;
          point.y += dy;
        }
        break;
      case DragMode::MovePolylinePoint:
        if (drag_point_index_ >= 0 &&
            drag_point_index_ < static_cast<int>(ann.points.size())) {
          ann.points[static_cast<size_t>(drag_point_index_)] = adjusted;
        }
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
    case DragMode::CreateEllipse: {
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
    case DragMode::CreateMosaic: {
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
    case DragMode::CreateBlur: {
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
    case DragMode::CreateSpotlight: {
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
    case DragMode::CreateWatermark: {
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
    case DragMode::CreateMagnifier: {
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
    case DragMode::MovePolyline:
    case DragMode::MovePolylinePoint:
    case DragMode::MoveText:
      if (drag_index_ >= 0 && drag_index_ < static_cast<int>(annotations_.size())) {
        const Annotation& current = annotations_[static_cast<size_t>(drag_index_)];
        if (drag_mode_ == DragMode::MovePolyline ||
            drag_mode_ == DragMode::MovePolylinePoint) {
          changed = !PointVectorsEqual(current.points, drag_seed_.points);
        } else {
          changed = !PointsEqual(current.p1, drag_seed_.p1) ||
                    !PointsEqual(current.p2, drag_seed_.p2);
        }
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
  drag_point_index_ = -1;
  Invalidate();
}

void AnnotateWindow::UpdatePolylinePreview(POINT canvas_pt) {
  if (!polyline_drawing_ || drag_seed_.points.empty()) {
    return;
  }
  POINT adjusted = canvas_pt;
  const bool shift_locked = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  if (shift_locked && drag_seed_.points.size() >= 2) {
    adjusted = SnapPoint45(drag_seed_.points[drag_seed_.points.size() - 2],
                           canvas_pt);
  }
  drag_seed_.points.back() = adjusted;
  Invalidate();
}

void AnnotateWindow::FinishPolyline(POINT canvas_pt) {
  if (!polyline_drawing_) {
    return;
  }
  UpdatePolylinePreview(canvas_pt);

  std::vector<POINT> points;
  points.reserve(drag_seed_.points.size());
  for (POINT point : drag_seed_.points) {
    if (points.empty() || !PointsEqual(points.back(), point)) {
      points.push_back(point);
    }
  }

  bool has_segment = false;
  for (size_t i = 1; i < points.size(); ++i) {
    if (DistanceSq(points[i - 1], points[i]) >=
        static_cast<double>(kMinShapeSize * kMinShapeSize)) {
      has_segment = true;
      break;
    }
  }

  if (has_segment) {
    Annotation ann = drag_seed_;
    ann.points = std::move(points);
    annotations_.push_back(std::move(ann));
    selected_index_ = static_cast<int>(annotations_.size() - 1);
    PushHistory();
  }

  polyline_drawing_ = false;
  drag_seed_ = {};
  drag_mode_ = DragMode::None;
  Invalidate();
}

bool AnnotateWindow::InsertPolylinePointAt(POINT canvas_pt) {
  const int path_tol = std::max(kHitTolerance, thickness_ + 2);
  const double path_tol_sq = static_cast<double>(path_tol * path_tol);

  for (int i = static_cast<int>(annotations_.size()) - 1; i >= 0; --i) {
    Annotation& ann = annotations_[static_cast<size_t>(i)];
    if (ann.type != AnnotationType::Polyline || ann.points.size() < 2) {
      continue;
    }

    for (size_t p = 0; p < ann.points.size(); ++p) {
      if (DistanceSq(canvas_pt, ann.points[p]) <= path_tol_sq) {
        selected_index_ = i;
        selected_point_index_ = static_cast<int>(p);
        Invalidate();
        return true;
      }
    }

    int insert_index = -1;
    double best_distance = path_tol_sq;
    for (size_t p = 1; p < ann.points.size(); ++p) {
      const double distance =
          DistanceToSegmentSq(canvas_pt, ann.points[p - 1], ann.points[p]);
      if (distance <= best_distance) {
        best_distance = distance;
        insert_index = static_cast<int>(p);
      }
    }
    if (insert_index < 0) {
      continue;
    }

    ann.points.insert(ann.points.begin() + insert_index, canvas_pt);
    selected_index_ = i;
    selected_point_index_ = insert_index;
    PushHistory();
    Invalidate();
    return true;
  }

  return false;
}

int AnnotateWindow::HitTestAnnotation(POINT canvas_pt, DragMode* mode_out,
                                      int* point_index_out) const {
  if (mode_out) {
    *mode_out = DragMode::None;
  }
  if (point_index_out) {
    *point_index_out = -1;
  }
  const double tol_sq = static_cast<double>(kHitTolerance * kHitTolerance);
  for (int i = static_cast<int>(annotations_.size()) - 1; i >= 0; --i) {
    const Annotation& ann = annotations_[static_cast<size_t>(i)];
    const bool erasing = tool_ == Tool::Eraser;
    if (!AnnotationTypeAllowedByTool(ann.type) ||
        (!erasing && !AnnotationEditable(ann.type))) {
      continue;
    }
    if (ann.type == AnnotationType::Rect || ann.type == AnnotationType::Ellipse ||
        ann.type == AnnotationType::Mosaic || ann.type == AnnotationType::Blur ||
        ann.type == AnnotationType::Spotlight ||
        ann.type == AnnotationType::Watermark ||
        ann.type == AnnotationType::Magnifier) {
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
      const bool body_hit =
          ann.type == AnnotationType::Rect
              ? (canvas_pt.x >= r.x && canvas_pt.y >= r.y &&
                 canvas_pt.x <= r.x + r.w && canvas_pt.y <= r.y + r.h)
          : ann.type == AnnotationType::Ellipse
              ? PointInsideEllipse(canvas_pt, r)
              : (canvas_pt.x >= r.x && canvas_pt.y >= r.y &&
                 canvas_pt.x <= r.x + r.w && canvas_pt.y <= r.y + r.h);
      if (body_hit) {
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
    if (ann.type == AnnotationType::Polyline && ann.points.size() > 1) {
      const int path_tol = std::max(kHitTolerance, ann.thickness + 2);
      const double path_tol_sq = static_cast<double>(path_tol * path_tol);
      if (!erasing && tool_ == Tool::Select) {
        for (size_t p = 0; p < ann.points.size(); ++p) {
          if (DistanceSq(canvas_pt, ann.points[p]) <= path_tol_sq) {
            if (mode_out) {
              *mode_out = DragMode::MovePolylinePoint;
            }
            if (point_index_out) {
              *point_index_out = static_cast<int>(p);
            }
            return i;
          }
        }
      }
      for (size_t p = 1; p < ann.points.size(); ++p) {
        if (DistanceToSegmentSq(canvas_pt, ann.points[p - 1], ann.points[p]) <=
            path_tol_sq) {
          if (mode_out) {
            *mode_out = erasing ? DragMode::None : DragMode::MovePolyline;
          }
          if (point_index_out) {
            *point_index_out = static_cast<int>(p);
          }
          return i;
        }
      }
    }
    if (ann.type == AnnotationType::Text || ann.type == AnnotationType::Serial) {
      RectPX r = RectBoundsForAnnotation(ann);
      if (canvas_pt.x >= r.x && canvas_pt.y >= r.y && canvas_pt.x <= r.x + r.w &&
          canvas_pt.y <= r.y + r.h) {
        if (mode_out) {
          *mode_out = DragMode::MoveText;
        }
        return i;
      }
    }
    if (erasing &&
        (ann.type == AnnotationType::Pencil ||
         ann.type == AnnotationType::Highlighter) &&
        ann.points.size() > 1) {
      const int path_tol = std::max(kHitTolerance, ann.thickness + 2);
      const double path_tol_sq = static_cast<double>(path_tol * path_tol);
      for (size_t p = 1; p < ann.points.size(); ++p) {
        if (DistanceToSegmentSq(canvas_pt, ann.points[p - 1], ann.points[p]) <=
            path_tol_sq) {
          if (point_index_out) {
            *point_index_out = static_cast<int>(p);
          }
          return i;
        }
      }
    }
  }
  return -1;
}

bool AnnotateWindow::AnnotationTypeAllowedByTool(AnnotationType type) const {
  if (tool_ == Tool::Select || tool_ == Tool::Eraser) {
    return true;
  }
  if (tool_ == Tool::Rect) {
    return type == AnnotationType::Rect;
  }
  if (tool_ == Tool::Ellipse) {
    return type == AnnotationType::Ellipse;
  }
  if (tool_ == Tool::Line) {
    return type == AnnotationType::Line;
  }
  if (tool_ == Tool::Polyline) {
    return type == AnnotationType::Polyline;
  }
  if (tool_ == Tool::Arrow) {
    return type == AnnotationType::Arrow;
  }
  if (tool_ == Tool::Serial) {
    return type == AnnotationType::Serial;
  }
  if (tool_ == Tool::Mosaic) {
    return type == AnnotationType::Mosaic;
  }
  if (tool_ == Tool::Blur) {
    return type == AnnotationType::Blur;
  }
  if (tool_ == Tool::Spotlight) {
    return type == AnnotationType::Spotlight;
  }
  if (tool_ == Tool::Watermark) {
    return type == AnnotationType::Watermark;
  }
  if (tool_ == Tool::Magnifier) {
    return type == AnnotationType::Magnifier;
  }
  if (tool_ == Tool::Highlighter) {
    return type == AnnotationType::Highlighter;
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
  return type != AnnotationType::Pencil && type != AnnotationType::Highlighter;
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
  if (ann.type == AnnotationType::Rect || ann.type == AnnotationType::Ellipse ||
      ann.type == AnnotationType::Mosaic || ann.type == AnnotationType::Blur ||
      ann.type == AnnotationType::Spotlight ||
      ann.type == AnnotationType::Watermark ||
      ann.type == AnnotationType::Magnifier) {
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
  if (ann.type == AnnotationType::Pencil ||
      ann.type == AnnotationType::Polyline ||
      ann.type == AnnotationType::Highlighter) {
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
  if (ann.type == AnnotationType::Serial) {
    const int size = std::max(24, ann.text_size + 10);
    RectPX r = {};
    r.x = ann.p1.x - size / 2;
    r.y = ann.p1.y - size / 2;
    r.w = size;
    r.h = size;
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
  if (polyline_drawing_ && drag_seed_.points.size() > 1) {
    DrawAnnotation(hdc, drag_seed_, false);
    return;
  }
  if (drag_mode_ == DragMode::CreateRect ||
      drag_mode_ == DragMode::CreateEllipse ||
      drag_mode_ == DragMode::CreateMosaic ||
      drag_mode_ == DragMode::CreateBlur ||
      drag_mode_ == DragMode::CreateSpotlight ||
      drag_mode_ == DragMode::CreateWatermark ||
      drag_mode_ == DragMode::CreateMagnifier ||
      drag_mode_ == DragMode::CreateLine ||
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
    case AnnotationType::Ellipse: {
      RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
      Ellipse(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
      break;
    }
    case AnnotationType::Mosaic:
      DrawMosaic(hdc, ann);
      break;
    case AnnotationType::Blur:
      DrawBlur(hdc, ann);
      break;
    case AnnotationType::Spotlight:
      DrawSpotlight(hdc, ann);
      break;
    case AnnotationType::Watermark:
      DrawWatermark(hdc, ann);
      break;
    case AnnotationType::Magnifier:
      DrawMagnifier(hdc, ann);
      break;
    case AnnotationType::Line:
      MoveToEx(hdc, ann.p1.x, ann.p1.y, nullptr);
      LineTo(hdc, ann.p2.x, ann.p2.y);
      break;
    case AnnotationType::Polyline:
      if (ann.points.size() > 1) {
        Polyline(hdc, ann.points.data(), static_cast<int>(ann.points.size()));
      }
      break;
    case AnnotationType::Arrow:
      MoveToEx(hdc, ann.p1.x, ann.p1.y, nullptr);
      LineTo(hdc, ann.p2.x, ann.p2.y);
      DrawArrowHead(hdc, ann.p1, ann.p2, ann.color, ann.thickness);
      break;
    case AnnotationType::Serial: {
      RectPX r = RectBoundsForAnnotation(ann);
      HBRUSH fill = CreateSolidBrush(ann.color);
      HGDIOBJ serial_old_brush = SelectObject(hdc, fill);
      Ellipse(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
      SelectObject(hdc, serial_old_brush);
      DeleteObject(fill);

      HFONT font = CreateFontW(
          ann.text_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
      HGDIOBJ old_font = SelectObject(hdc, font);
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(255, 255, 255));
      std::wstring number = std::to_wstring(std::max(1, ann.serial_value));
      RECT text_rect = {r.x, r.y, r.x + r.w, r.y + r.h};
      DrawTextW(hdc, number.c_str(), static_cast<int>(number.size()),
                &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      SelectObject(hdc, old_font);
      DeleteObject(font);
      break;
    }
    case AnnotationType::Pencil:
      if (ann.points.size() > 1) {
        Polyline(hdc, ann.points.data(), static_cast<int>(ann.points.size()));
      }
      break;
    case AnnotationType::Highlighter:
      DrawHighlighter(hdc, ann);
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
      if (ann.text_background) {
        SIZE extent = {};
        GetTextExtentPoint32W(hdc, draw.c_str(), static_cast<int>(draw.size()),
                              &extent);
        const int text_w = static_cast<int>(extent.cx);
        const int text_h = static_cast<int>(extent.cy);
        RECT bg = {ann.p1.x - 3, ann.p1.y - 2,
                   ann.p1.x + std::max(12, text_w) + 6,
                   ann.p1.y + std::max(ann.text_size, text_h) + 4};
        HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 210));
        FillRect(hdc, &bg, bg_brush);
        DeleteObject(bg_brush);
      }
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

void AnnotateWindow::DrawMosaic(HDC hdc, const Annotation& ann) const {
  RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  if (r.w <= 1 || r.h <= 1) {
    return;
  }

  const int base_block = MosaicBlockSize(r.w, r.h);
  if (base_block <= 0) {
    return;
  }
  const int strength = ClampInt(ann.thickness, 1, 10);
  const int block = ClampInt(base_block + (strength - 2) * 2, 3, 32);
  const int small_w = std::max(1, r.w / block);
  const int small_h = std::max(1, r.h / block);

  HDC small_dc = CreateCompatibleDC(hdc);
  if (!small_dc) {
    return;
  }
  HBITMAP small_bitmap = CreateCompatibleBitmap(hdc, small_w, small_h);
  if (!small_bitmap) {
    DeleteDC(small_dc);
    return;
  }

  HGDIOBJ old_bitmap = SelectObject(small_dc, small_bitmap);
  const int old_small_mode = SetStretchBltMode(small_dc, COLORONCOLOR);
  StretchBlt(small_dc, 0, 0, small_w, small_h, hdc, r.x, r.y, r.w, r.h,
             SRCCOPY);
  const int old_target_mode = SetStretchBltMode(hdc, COLORONCOLOR);
  StretchBlt(hdc, r.x, r.y, r.w, r.h, small_dc, 0, 0, small_w, small_h,
             SRCCOPY);
  SetStretchBltMode(hdc, old_target_mode);
  SetStretchBltMode(small_dc, old_small_mode);

  SelectObject(small_dc, old_bitmap);
  DeleteObject(small_bitmap);
  DeleteDC(small_dc);
}

void AnnotateWindow::DrawBlur(HDC hdc, const Annotation& ann) const {
  RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  if (r.w <= 2 || r.h <= 2) {
    return;
  }

  const int strength = ClampInt(ann.thickness, 1, 10);
  const int block =
      ClampInt(std::max(3, MosaicBlockSize(r.w, r.h) / 2) + (strength - 2),
               2, 24);
  const int small_w = std::max(1, r.w / block);
  const int small_h = std::max(1, r.h / block);

  HDC small_dc = CreateCompatibleDC(hdc);
  if (!small_dc) {
    return;
  }
  HBITMAP small_bitmap = CreateCompatibleBitmap(hdc, small_w, small_h);
  if (!small_bitmap) {
    DeleteDC(small_dc);
    return;
  }

  HGDIOBJ old_bitmap = SelectObject(small_dc, small_bitmap);
  const int old_small_mode = SetStretchBltMode(small_dc, HALFTONE);
  SetBrushOrgEx(small_dc, 0, 0, nullptr);
  StretchBlt(small_dc, 0, 0, small_w, small_h, hdc, r.x, r.y, r.w, r.h,
             SRCCOPY);
  const int old_target_mode = SetStretchBltMode(hdc, HALFTONE);
  SetBrushOrgEx(hdc, 0, 0, nullptr);
  StretchBlt(hdc, r.x, r.y, r.w, r.h, small_dc, 0, 0, small_w, small_h,
             SRCCOPY);
  SetStretchBltMode(hdc, old_target_mode);
  SetStretchBltMode(small_dc, old_small_mode);

  SelectObject(small_dc, old_bitmap);
  DeleteObject(small_bitmap);
  DeleteDC(small_dc);
}

void AnnotateWindow::DrawHighlighter(HDC hdc, const Annotation& ann) const {
  if (ann.points.size() <= 1 || bitmap_size_px_.w <= 0 || bitmap_size_px_.h <= 0) {
    return;
  }

  RectPX raw = RectBoundsForAnnotation(ann);
  const int pad = std::max(ann.thickness + 4, 12);
  const int left = ClampInt(raw.x - pad, 0, bitmap_size_px_.w);
  const int top = ClampInt(raw.y - pad, 0, bitmap_size_px_.h);
  const int right = ClampInt(raw.x + raw.w + pad, 0, bitmap_size_px_.w);
  const int bottom = ClampInt(raw.y + raw.h + pad, 0, bitmap_size_px_.h);
  const int w = right - left;
  const int h = bottom - top;
  if (w <= 0 || h <= 0) {
    return;
  }

  HDC blend_dc = CreateCompatibleDC(hdc);
  if (!blend_dc) {
    return;
  }
  HBITMAP blend_bitmap = CreateCompatibleBitmap(hdc, w, h);
  if (!blend_bitmap) {
    DeleteDC(blend_dc);
    return;
  }

  HGDIOBJ old_bitmap = SelectObject(blend_dc, blend_bitmap);
  BitBlt(blend_dc, 0, 0, w, h, hdc, left, top, SRCCOPY);

  std::vector<POINT> local_points;
  local_points.reserve(ann.points.size());
  for (POINT p : ann.points) {
    p.x -= left;
    p.y -= top;
    local_points.push_back(p);
  }

  HPEN highlighter_pen =
      CreatePen(PS_SOLID, std::max(1, ann.thickness), ann.color);
  HGDIOBJ old_pen = SelectObject(blend_dc, highlighter_pen);
  HGDIOBJ old_brush = SelectObject(blend_dc, GetStockObject(HOLLOW_BRUSH));
  Polyline(blend_dc, local_points.data(), static_cast<int>(local_points.size()));
  SelectObject(blend_dc, old_brush);
  SelectObject(blend_dc, old_pen);
  DeleteObject(highlighter_pen);

  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 110;
  AlphaBlend(hdc, left, top, w, h, blend_dc, 0, 0, w, h, blend);

  SelectObject(blend_dc, old_bitmap);
  DeleteObject(blend_bitmap);
  DeleteDC(blend_dc);
}

void AnnotateWindow::DrawSpotlight(HDC hdc, const Annotation& ann) const {
  if (bitmap_size_px_.w <= 0 || bitmap_size_px_.h <= 0) {
    return;
  }
  RectPX focus = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  if (focus.w <= 1 || focus.h <= 1) {
    return;
  }

  HDC shade_dc = CreateCompatibleDC(hdc);
  if (!shade_dc) {
    return;
  }
  HBITMAP shade_bitmap =
      CreateCompatibleBitmap(hdc, bitmap_size_px_.w, bitmap_size_px_.h);
  if (!shade_bitmap) {
    DeleteDC(shade_dc);
    return;
  }

  HGDIOBJ old_bitmap = SelectObject(shade_dc, shade_bitmap);
  BitBlt(shade_dc, 0, 0, bitmap_size_px_.w, bitmap_size_px_.h, hdc, 0, 0,
         SRCCOPY);

  HRGN full_region = CreateRectRgn(0, 0, bitmap_size_px_.w, bitmap_size_px_.h);
  HRGN focus_region =
      CreateRectRgn(focus.x, focus.y, focus.x + focus.w, focus.y + focus.h);
  if (full_region && focus_region &&
      CombineRgn(full_region, full_region, focus_region, RGN_DIFF) != ERROR) {
    SelectClipRgn(shade_dc, full_region);
    RECT canvas = {0, 0, bitmap_size_px_.w, bitmap_size_px_.h};
    HBRUSH dim = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(shade_dc, &canvas, dim);
    DeleteObject(dim);
    SelectClipRgn(shade_dc, nullptr);
  }
  if (focus_region) {
    DeleteObject(focus_region);
  }
  if (full_region) {
    DeleteObject(full_region);
  }

  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  const int strength = ClampInt(ann.thickness, 1, 10);
  blend.SourceConstantAlpha = static_cast<BYTE>(ClampInt(85 + strength * 15,
                                                         80, 220));
  AlphaBlend(hdc, 0, 0, bitmap_size_px_.w, bitmap_size_px_.h, shade_dc, 0, 0,
             bitmap_size_px_.w, bitmap_size_px_.h, blend);

  SelectObject(shade_dc, old_bitmap);
  DeleteObject(shade_bitmap);
  DeleteDC(shade_dc);
}

void AnnotateWindow::DrawWatermark(HDC hdc, const Annotation& ann) const {
  RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  if (r.w <= 2 || r.h <= 2) {
    return;
  }

  HDC blend_dc = CreateCompatibleDC(hdc);
  if (!blend_dc) {
    return;
  }
  HBITMAP blend_bitmap = CreateCompatibleBitmap(hdc, r.w, r.h);
  if (!blend_bitmap) {
    DeleteDC(blend_dc);
    return;
  }

  HGDIOBJ old_bitmap = SelectObject(blend_dc, blend_bitmap);
  BitBlt(blend_dc, 0, 0, r.w, r.h, hdc, r.x, r.y, SRCCOPY);

  const int font_size = std::max(10, std::min(28, r.h / 2));
  HFONT font = CreateFontW(
      font_size, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
  HGDIOBJ old_font = font ? SelectObject(blend_dc, font) : nullptr;
  SetBkMode(blend_dc, TRANSPARENT);
  SetTextColor(blend_dc, ann.color);
  const std::wstring text = ann.text.empty() ? L"SnapPin" : ann.text;
  RECT text_rect = {0, 0, r.w, r.h};
  DrawTextW(blend_dc, text.c_str(), static_cast<int>(text.size()), &text_rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  if (old_font) {
    SelectObject(blend_dc, old_font);
  }
  if (font) {
    DeleteObject(font);
  }

  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 125;
  AlphaBlend(hdc, r.x, r.y, r.w, r.h, blend_dc, 0, 0, r.w, r.h, blend);

  SelectObject(blend_dc, old_bitmap);
  DeleteObject(blend_bitmap);
  DeleteDC(blend_dc);
}

void AnnotateWindow::DrawMagnifier(HDC hdc, const Annotation& ann) const {
  if (bitmap_size_px_.w <= 0 || bitmap_size_px_.h <= 0) {
    return;
  }
  RectPX r = NormalizeRect(RectFromPoints(ann.p1, ann.p2));
  if (r.w <= 2 || r.h <= 2) {
    return;
  }

  const int zoom = ClampInt(ann.thickness, 1, 8);
  const int src_w = std::max(1, r.w / zoom);
  const int src_h = std::max(1, r.h / zoom);
  const int center_x = r.x + r.w / 2;
  const int center_y = r.y + r.h / 2;
  const int src_x =
      ClampInt(center_x - src_w / 2, 0, std::max(0, bitmap_size_px_.w - src_w));
  const int src_y =
      ClampInt(center_y - src_h / 2, 0, std::max(0, bitmap_size_px_.h - src_h));

  HDC source_dc = CreateCompatibleDC(hdc);
  if (!source_dc) {
    return;
  }
  HBITMAP source_bitmap = CreateCompatibleBitmap(hdc, src_w, src_h);
  if (!source_bitmap) {
    DeleteDC(source_dc);
    return;
  }

  HGDIOBJ old_source_bitmap = SelectObject(source_dc, source_bitmap);
  BitBlt(source_dc, 0, 0, src_w, src_h, hdc, src_x, src_y, SRCCOPY);

  const int old_stretch_mode = SetStretchBltMode(hdc, HALFTONE);
  SetBrushOrgEx(hdc, 0, 0, nullptr);
  StretchBlt(hdc, r.x, r.y, r.w, r.h, source_dc, 0, 0, src_w, src_h, SRCCOPY);
  SetStretchBltMode(hdc, old_stretch_mode);

  HPEN border_pen = CreatePen(PS_SOLID, std::max(1, ann.thickness),
                              RGB(255, 255, 255));
  HGDIOBJ old_pen = SelectObject(hdc, border_pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
  Rectangle(hdc, r.x, r.y, r.x + r.w, r.y + r.h);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(border_pen);

  SelectObject(source_dc, old_source_bitmap);
  DeleteObject(source_bitmap);
  DeleteDC(source_dc);
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
  selected_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
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
  selected_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
  Invalidate();
  return true;
}

void AnnotateWindow::DeleteSelection() {
  if (selected_index_ < 0 || selected_index_ >= static_cast<int>(annotations_.size())) {
    return;
  }
  Annotation& selected = annotations_[static_cast<size_t>(selected_index_)];
  if (!AnnotationEditable(selected.type)) {
    return;
  }
  if (selected.type == AnnotationType::Polyline && selected_point_index_ >= 0 &&
      selected_point_index_ < static_cast<int>(selected.points.size()) &&
      selected.points.size() > 2) {
    selected.points.erase(selected.points.begin() + selected_point_index_);
    selected_point_index_ = -1;
    PushHistory();
    Invalidate();
    return;
  }
  annotations_.erase(annotations_.begin() + selected_index_);
  selected_index_ = -1;
  selected_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  serial_entry_text_.clear();
  serial_entry_target_index_ = -2;
  PushHistory();
  Invalidate();
}

bool AnnotateWindow::ErasePathSegment(int annotation_index,
                                      int segment_end_index) {
  if (annotation_index < 0 ||
      annotation_index >= static_cast<int>(annotations_.size()) ||
      segment_end_index <= 0) {
    return false;
  }
  const Annotation& source = annotations_[static_cast<size_t>(annotation_index)];
  if (source.type != AnnotationType::Polyline &&
      source.type != AnnotationType::Pencil &&
      source.type != AnnotationType::Highlighter) {
    return false;
  }
  if (segment_end_index >= static_cast<int>(source.points.size())) {
    return false;
  }

  std::vector<Annotation> replacements;
  if (segment_end_index > 1) {
    Annotation left = source;
    left.points.assign(source.points.begin(),
                       source.points.begin() + segment_end_index);
    if (left.points.size() > 1) {
      replacements.push_back(std::move(left));
    }
  }
  if (segment_end_index + 1 < static_cast<int>(source.points.size())) {
    Annotation right = source;
    right.points.assign(source.points.begin() + segment_end_index,
                        source.points.end());
    if (right.points.size() > 1) {
      replacements.push_back(std::move(right));
    }
  }

  if (replacements.empty()) {
    return false;
  }

  auto insert_pos = annotations_.erase(annotations_.begin() + annotation_index);
  annotations_.insert(insert_pos, replacements.begin(), replacements.end());
  selected_index_ = -1;
  selected_point_index_ = -1;
  drag_index_ = -1;
  drag_point_index_ = -1;
  text_editing_ = false;
  text_edit_index_ = -1;
  return true;
}

void AnnotateWindow::ShowContextMenu(POINT screen_pt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  AppendMenuW(menu, MF_STRING, kCmdSelect, L"Tool: Select");
  AppendMenuW(menu, MF_STRING, kCmdRect, L"Tool: Rect");
  AppendMenuW(menu, MF_STRING, kCmdEllipse, L"Tool: Ellipse");
  AppendMenuW(menu, MF_STRING, kCmdLine, L"Tool: Line");
  AppendMenuW(menu, MF_STRING, kCmdPolyline, L"Tool: Polyline");
  AppendMenuW(menu, MF_STRING, kCmdArrow, L"Tool: Arrow");
  AppendMenuW(menu, MF_STRING, kCmdSerial, L"Tool: Serial");
  AppendMenuW(menu, MF_STRING, kCmdMosaic, L"Tool: Mosaic");
  AppendMenuW(menu, MF_STRING, kCmdBlur, L"Tool: Blur");
  AppendMenuW(menu, MF_STRING, kCmdEraser, L"Tool: Eraser");
  AppendMenuW(menu, MF_STRING, kCmdHighlighter, L"Tool: Highlighter");
  AppendMenuW(menu, MF_STRING, kCmdSpotlight, L"Tool: Spotlight");
  AppendMenuW(menu, MF_STRING, kCmdWatermark, L"Tool: Watermark");
  AppendMenuW(menu, MF_STRING, kCmdMagnifier, L"Tool: Magnifier");
  AppendMenuW(menu, MF_STRING, kCmdPencil, L"Tool: Pencil");
  AppendMenuW(menu, MF_STRING, kCmdText, L"Tool: Text");
  AppendMenuW(menu, MF_STRING, kCmdTextBackground, L"Text Background");
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
