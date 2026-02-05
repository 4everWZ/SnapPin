#include "CaptureFreeze.h"

#include "ErrorCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstring>
#include <string>

namespace snappin {
namespace {

std::optional<FrozenFrame> g_frozen_frame;

void FillWin32Error(Error* err, const char* code, const char* message, DWORD last_error) {
  if (!err) {
    return;
  }
  err->code = code;
  err->message = message;
  err->retryable = true;
  err->detail = std::to_string(static_cast<unsigned long long>(last_error));
}

struct DIBSection {
  HBITMAP bitmap = nullptr;
  void* bits = nullptr;
  int32_t stride = 0;
};

DIBSection CreateDib(int32_t width, int32_t height) {
  DIBSection out;
  BITMAPV5HEADER bi = {};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = width;
  bi.bV5Height = -height; // top-down
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  HDC screen = GetDC(nullptr);
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&bi),
                                 DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);

  if (bmp && bits) {
    out.bitmap = bmp;
    out.bits = bits;
    out.stride = width * 4;
  }
  return out;
}

bool CaptureRectToDib(const RectPX& rect, DIBSection* out, Error* err) {
  if (!out) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Invalid DIB target",
                   ERROR_INVALID_PARAMETER);
    return false;
  }
  if (rect.w <= 0 || rect.h <= 0) {
    FillWin32Error(err, ERR_TARGET_INVALID, "Invalid capture size",
                   ERROR_INVALID_PARAMETER);
    return false;
  }

  DIBSection dib = CreateDib(rect.w, rect.h);
  if (!dib.bitmap) {
    FillWin32Error(err, ERR_OUT_OF_MEMORY, "Failed to allocate bitmap",
                   GetLastError());
    return false;
  }

  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  HGDIOBJ old = SelectObject(mem, dib.bitmap);
  BOOL ok = BitBlt(mem, 0, 0, rect.w, rect.h, screen, rect.x, rect.y,
                   SRCCOPY | CAPTUREBLT);
  SelectObject(mem, old);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (!ok) {
    DeleteObject(dib.bitmap);
    FillWin32Error(err, ERR_CAPTURE_FAILED, "Capture failed", GetLastError());
    return false;
  }

  *out = dib;
  return true;
}

Result<FrozenFrame> CaptureFrozenFrameForMonitorRect(const RectPX& rect) {
  Error err;
  DIBSection dib;
  if (!CaptureRectToDib(rect, &dib, &err)) {
    return Result<FrozenFrame>::Fail(err);
  }

  size_t bytes = static_cast<size_t>(dib.stride) * rect.h;
  auto storage = std::make_shared<std::vector<uint8_t>>();
  storage->resize(bytes);
  std::memcpy(storage->data(), dib.bits, bytes);
  DeleteObject(dib.bitmap);

  FrozenFrame frame;
  frame.screen_rect_px = rect;
  frame.size_px = SizePX{rect.w, rect.h};
  frame.stride_bytes = dib.stride;
  frame.format = PixelFormat::BGRA8;
  frame.pixels = std::move(storage);
  return Result<FrozenFrame>::Ok(frame);
}

RectPX ResolveMonitorRectPx(HMONITOR monitor) {
  MONITORINFOEXW mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) {
    return RectPX{};
  }

  int32_t logical_w = mi.rcMonitor.right - mi.rcMonitor.left;
  int32_t logical_h = mi.rcMonitor.bottom - mi.rcMonitor.top;
  if (logical_w <= 0 || logical_h <= 0) {
    return RectPX{};
  }

  float scale = 1.0f;
  DEVMODEW dm = {};
  dm.dmSize = sizeof(dm);
  if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
    if (dm.dmPelsWidth > 0 && dm.dmPelsHeight > 0) {
      float sx = static_cast<float>(dm.dmPelsWidth) / logical_w;
      float sy = static_cast<float>(dm.dmPelsHeight) / logical_h;
      float diff = std::fabs(sx - sy);
      if (diff < 0.05f && (sx > 1.05f || sx < 0.95f)) {
        scale = sx;
      }
    }
  }

  RectPX rect;
  rect.x = static_cast<int32_t>(std::lround(mi.rcMonitor.left * scale));
  rect.y = static_cast<int32_t>(std::lround(mi.rcMonitor.top * scale));
  rect.w = static_cast<int32_t>(std::lround(logical_w * scale));
  rect.h = static_cast<int32_t>(std::lround(logical_h * scale));
  return rect;
}

} // namespace

Result<void> PrepareFrozenFrameForCursorMonitor() {
  POINT cursor = {};
  if (!GetCursorPos(&cursor)) {
    Error err;
    FillWin32Error(&err, ERR_CAPTURE_FAILED, "Capture failed", GetLastError());
    return Result<void>::Fail(err);
  }

  HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
  RectPX rect = ResolveMonitorRectPx(monitor);
  if (rect.w <= 0 || rect.h <= 0) {
    Error err;
    FillWin32Error(&err, ERR_CAPTURE_FAILED, "Capture failed", GetLastError());
    return Result<void>::Fail(err);
  }

  Result<FrozenFrame> frame = CaptureFrozenFrameForMonitorRect(rect);
  if (!frame.ok) {
    return Result<void>::Fail(frame.error);
  }

  g_frozen_frame = std::move(frame.value);
  return Result<void>::Ok();
}

std::optional<FrozenFrame> ConsumeFrozenFrame() {
  if (!g_frozen_frame.has_value()) {
    return std::nullopt;
  }
  std::optional<FrozenFrame> out = std::move(g_frozen_frame);
  g_frozen_frame.reset();
  return out;
}

const FrozenFrame* PeekFrozenFrame() {
  if (!g_frozen_frame.has_value()) {
    return nullptr;
  }
  return &g_frozen_frame.value();
}

void ClearFrozenFrame() { g_frozen_frame.reset(); }

} // namespace snappin
