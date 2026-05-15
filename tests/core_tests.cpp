#include "Types.h"
#include "ActionRegistry.h"
#include "AnnotationEffects.h"
#include "AnnotateLayout.h"
#include "OcrRegion.h"
#include "OverlayWindow.h"
#include "AnnotateWindow.h"
#include "OcrResultWindow.h"
#include "OcrResultEvent.h"
#include "PinWindow.h"
#include "TrayIcon.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {

struct ButtonSearch {
  const wchar_t* expected = nullptr;
  bool found = false;
  HWND hwnd = nullptr;
};

BOOL CALLBACK FindButtonByText(HWND hwnd, LPARAM param) {
  ButtonSearch* search = reinterpret_cast<ButtonSearch*>(param);
  wchar_t text[128] = {};
  GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
  if (wcscmp(text, search->expected) == 0) {
    search->found = true;
    search->hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

HWND FindAnnotateButton(HWND parent, const wchar_t* label) {
  ButtonSearch search{label, false, nullptr};
  EnumChildWindows(parent, FindButtonByText, reinterpret_cast<LPARAM>(&search));
  return search.hwnd;
}

struct ClassSearch {
  const wchar_t* expected = nullptr;
  HWND hwnd = nullptr;
};

BOOL CALLBACK FindChildByClass(HWND hwnd, LPARAM param) {
  ClassSearch* search = reinterpret_cast<ClassSearch*>(param);
  wchar_t class_name[128] = {};
  GetClassNameW(hwnd, class_name, static_cast<int>(std::size(class_name)));
  if (lstrcmpiW(class_name, search->expected) == 0) {
    search->hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

HWND FindChildClass(HWND parent, const wchar_t* class_name) {
  ClassSearch search{class_name, nullptr};
  EnumChildWindows(parent, FindChildByClass, reinterpret_cast<LPARAM>(&search));
  return search.hwnd;
}

bool WithAnnotateWindow(std::function<bool(HWND)> check) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }
  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }
  const bool ok = check(hwnd);
  annotate.Destroy();
  return ok;
}

bool AnnotateWindowHasButton(const wchar_t* label) {
  return WithAnnotateWindow(
      [label](HWND hwnd) { return FindAnnotateButton(hwnd, label) != nullptr; });
}

bool AnnotateWindowSelectsTool(const wchar_t* label,
                               const wchar_t* selected_label) {
  return WithAnnotateWindow([label, selected_label](HWND hwnd) {
    HWND button = FindAnnotateButton(hwnd, label);
    if (!button) {
      return false;
    }
    SendMessageW(button, BM_CLICK, 0, 0);
    return FindAnnotateButton(hwnd, selected_label) != nullptr;
  });
}

std::shared_ptr<std::vector<uint8_t>> MakeAnnotateTestSource(int width,
                                                             int height,
                                                             int stride);

bool AnnotateWindowClientWidthStaysAtBitmapWidth() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  RECT rc = {};
  if (!GetClientRect(hwnd, &rc)) {
    annotate.Destroy();
    return false;
  }
  annotate.Destroy();
  return (rc.right - rc.left) == width;
}

enum class AnnotateCreateGesture {
  Drag,
  Click,
};

std::shared_ptr<std::vector<uint8_t>> MakeAnnotateTestSource(int width,
                                                             int height,
                                                             int stride) {
  auto source = std::make_shared<std::vector<uint8_t>>(
      static_cast<size_t>(stride) * static_cast<size_t>(height),
      static_cast<uint8_t>(0));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const size_t i = static_cast<size_t>(y) * stride +
                       static_cast<size_t>(x) * 4;
      (*source)[i + 0] = static_cast<uint8_t>((x * 7 + y * 3) & 0xFF);
      (*source)[i + 1] = static_cast<uint8_t>((x * 5 + y * 11) & 0xFF);
      (*source)[i + 2] = static_cast<uint8_t>((x * 13 + y * 2) & 0xFF);
      (*source)[i + 3] = 255;
    }
  }
  return source;
}

bool AnnotateWindowCopiesComposedToolPixels(
    const wchar_t* tool_label, AnnotateCreateGesture gesture) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  if (tool_label) {
    HWND tool_button = FindAnnotateButton(hwnd, tool_label);
    if (!tool_button) {
      annotate.Destroy();
      return false;
    }
    SendMessageW(tool_button, BM_CLICK, 0, 0);
  }

  constexpr int toolbar_height = 34;
  if (gesture == AnnotateCreateGesture::Drag) {
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
                 MAKELPARAM(4, toolbar_height + 4));
    SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
                 MAKELPARAM(24, toolbar_height + 24));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(24, toolbar_height + 24));
  } else {
    SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
                 MAKELPARAM(16, toolbar_height + 16));
    SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(16, toolbar_height + 16));
  }

  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!copy_button) {
    annotate.Destroy();
    return false;
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return *copied_pixels != *source;
}

bool AnnotateWindowEraserRemovesRectFromCopyPixels() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
               MAKELPARAM(24, toolbar_height + 24));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(24, toolbar_height + 24));

  HWND eraser_button = FindAnnotateButton(hwnd, L"Erase");
  if (!eraser_button) {
    annotate.Destroy();
    return false;
  }
  SendMessageW(eraser_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(12, toolbar_height + 12));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(12, toolbar_height + 12));

  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!copy_button) {
    annotate.Destroy();
    return false;
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return *copied_pixels == *source;
}

bool AnnotateWindowCopiesPolylinePixels() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  HWND tool_button = FindAnnotateButton(hwnd, L"Poly");
  if (!tool_button) {
    annotate.Destroy();
    return false;
  }
  SendMessageW(tool_button, BM_CLICK, 0, 0);

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 20));
  SendMessageW(hwnd, WM_RBUTTONDOWN, 0, MAKELPARAM(24, toolbar_height + 20));

  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!copy_button) {
    annotate.Destroy();
    return false;
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return *copied_pixels != *source;
}

bool AnnotateWindowPolylineNodeEditChangesCopiedPixels() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  HWND polyline_button = FindAnnotateButton(hwnd, L"Poly");
  HWND select_button = FindAnnotateButton(hwnd, L"Sel");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!polyline_button || !select_button || !copy_button) {
    annotate.Destroy();
    return false;
  }

  constexpr int toolbar_height = 34;
  SendMessageW(polyline_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 20));
  SendMessageW(hwnd, WM_RBUTTONDOWN, 0, MAKELPARAM(24, toolbar_height + 20));

  SendMessageW(copy_button, BM_CLICK, 0, 0);
  auto before_pixels = copied_pixels;

  SendMessageW(select_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(24, toolbar_height + 20));
  SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
               MAKELPARAM(28, toolbar_height + 28));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(28, toolbar_height + 28));
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !before_pixels || !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return *before_pixels != *copied_pixels;
}

bool PixelDiffersAt(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                    int stride, int x, int y) {
  const size_t i = static_cast<size_t>(y) * static_cast<size_t>(stride) +
                   static_cast<size_t>(x) * 4;
  return i + 3 < a.size() && i + 3 < b.size() &&
         (a[i + 0] != b[i + 0] || a[i + 1] != b[i + 1] ||
          a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]);
}

bool AnnotateWindowPolylineSegmentDoubleClickInsertsNode() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  HWND polyline_button = FindAnnotateButton(hwnd, L"Poly");
  HWND select_button = FindAnnotateButton(hwnd, L"Sel");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!polyline_button || !select_button || !copy_button) {
    annotate.Destroy();
    return false;
  }

  constexpr int toolbar_height = 34;
  SendMessageW(polyline_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(24, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(24, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 24));
  SendMessageW(hwnd, WM_RBUTTONDOWN, 0, MAKELPARAM(24, toolbar_height + 24));

  SendMessageW(select_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDBLCLK, MK_LBUTTON,
               MAKELPARAM(14, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(14, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
               MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return PixelDiffersAt(*source, *copied_pixels, stride, 4, 4) &&
         PixelDiffersAt(*source, *copied_pixels, stride, 24, 4) &&
         PixelDiffersAt(*source, *copied_pixels, stride, 14, 14);
}

bool AnnotateWindowPolylineDeleteRemovesSelectedNode() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  HWND polyline_button = FindAnnotateButton(hwnd, L"Poly");
  HWND select_button = FindAnnotateButton(hwnd, L"Sel");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!polyline_button || !select_button || !copy_button) {
    annotate.Destroy();
    return false;
  }

  constexpr int toolbar_height = 34;
  SendMessageW(polyline_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 4));
  SendMessageW(hwnd, WM_RBUTTONDOWN, 0, MAKELPARAM(24, toolbar_height + 4));

  SendMessageW(select_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(14, toolbar_height + 14));
  SendMessageW(hwnd, WM_KEYDOWN, VK_DELETE, 0);
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return PixelDiffersAt(*source, *copied_pixels, stride, 4, 4) &&
         PixelDiffersAt(*source, *copied_pixels, stride, 14, 4) &&
         PixelDiffersAt(*source, *copied_pixels, stride, 24, 4) &&
         !PixelDiffersAt(*source, *copied_pixels, stride, 14, 14);
}

bool AnnotateWindowEraserPartiallyErasesPolylinePixels() {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return false;
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return false;
  }

  HWND polyline_button = FindAnnotateButton(hwnd, L"Poly");
  HWND eraser_button = FindAnnotateButton(hwnd, L"Erase");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!polyline_button || !eraser_button || !copy_button) {
    annotate.Destroy();
    return false;
  }

  constexpr int toolbar_height = 34;
  SendMessageW(polyline_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(18, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, toolbar_height + 20));
  SendMessageW(hwnd, WM_RBUTTONDOWN, 0, MAKELPARAM(24, toolbar_height + 20));

  SendMessageW(eraser_button, BM_CLICK, 0, 0);
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(10, toolbar_height + 4));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(10, toolbar_height + 4));
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return false;
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return false;
  }
  return *copied_pixels != *source;
}

std::shared_ptr<std::vector<uint8_t>>
AnnotateWindowCopiesSerialPixelsAfterChars(const wchar_t* chars) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return {};
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return {};
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return {};
  }

  HWND serial_button = FindAnnotateButton(hwnd, L"No.");
  if (!serial_button) {
    annotate.Destroy();
    return {};
  }
  SendMessageW(serial_button, BM_CLICK, 0, 0);

  for (const wchar_t* ch = chars; ch && *ch; ++ch) {
    SendMessageW(hwnd, WM_CHAR, static_cast<WPARAM>(*ch), 0);
  }

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(16, toolbar_height + 16));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(16, toolbar_height + 16));

  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!copy_button) {
    annotate.Destroy();
    return {};
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return {};
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return {};
  }
  return copied_pixels;
}

bool AnnotateWindowSerialDirectEntryChangesCopiedPixels() {
  auto default_pixels = AnnotateWindowCopiesSerialPixelsAfterChars(L"");
  auto direct_pixels = AnnotateWindowCopiesSerialPixelsAfterChars(L"42");
  return default_pixels && direct_pixels && *default_pixels != *direct_pixels;
}

std::shared_ptr<std::vector<uint8_t>>
AnnotateWindowCopiesToolPixelsAfterWheel(const wchar_t* tool_label,
                                         int wheel_steps) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return {};
  }

  constexpr int width = 32;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return {};
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return {};
  }

  HWND tool_button = FindAnnotateButton(hwnd, tool_label);
  if (!tool_button) {
    annotate.Destroy();
    return {};
  }
  SendMessageW(tool_button, BM_CLICK, 0, 0);

  for (int i = 0; i < wheel_steps; ++i) {
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
  }

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 4));
  SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
               MAKELPARAM(24, toolbar_height + 24));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(24, toolbar_height + 24));

  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!copy_button) {
    annotate.Destroy();
    return {};
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return {};
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return {};
  }
  return copied_pixels;
}

bool AnnotateWindowEffectWheelChangesCopiedPixels(const wchar_t* tool_label) {
  auto default_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(tool_label, 0);
  auto strong_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(tool_label, 3);
  return default_pixels && strong_pixels && *default_pixels != *strong_pixels;
}

bool AnnotateWindowSpotlightWheelChangesDimPixels() {
  auto default_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(L"Spot", 0);
  auto strong_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(L"Spot", 3);
  return default_pixels && strong_pixels &&
         PixelDiffersAt(*default_pixels, *strong_pixels, 32 * 4, 2, 2);
}

bool AnnotateWindowHighlighterWheelChangesCenterOpacityPixels() {
  auto default_pixels =
      AnnotateWindowCopiesToolPixelsAfterWheel(L"HL", 0);
  auto strong_pixels =
      AnnotateWindowCopiesToolPixelsAfterWheel(L"HL", 3);
  return default_pixels && strong_pixels &&
         PixelDiffersAt(*default_pixels, *strong_pixels, 32 * 4, 16, 16);
}

bool AnnotateWindowMagnifierWheelChangesZoomPixels() {
  auto default_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(L"Zoom", 0);
  auto strong_pixels = AnnotateWindowCopiesToolPixelsAfterWheel(L"Zoom", 3);
  return default_pixels && strong_pixels &&
         PixelDiffersAt(*default_pixels, *strong_pixels, 32 * 4, 16, 10);
}

std::shared_ptr<std::vector<uint8_t>>
AnnotateWindowCopiesWatermarkPixelsAfterChars(const wchar_t* chars) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return {};
  }

  constexpr int width = 48;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return {};
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return {};
  }

  HWND watermark_button = FindAnnotateButton(hwnd, L"WM");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!watermark_button || !copy_button) {
    annotate.Destroy();
    return {};
  }
  SendMessageW(watermark_button, BM_CLICK, 0, 0);

  for (const wchar_t* ch = chars; ch && *ch; ++ch) {
    SendMessageW(hwnd, WM_CHAR, static_cast<WPARAM>(*ch), 0);
  }

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(4, toolbar_height + 6));
  SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON,
               MAKELPARAM(44, toolbar_height + 26));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(44, toolbar_height + 26));
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return {};
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return {};
  }
  return copied_pixels;
}

bool AnnotateWindowWatermarkDirectTextChangesCopiedPixels() {
  auto default_pixels = AnnotateWindowCopiesWatermarkPixelsAfterChars(L"");
  auto custom_pixels = AnnotateWindowCopiesWatermarkPixelsAfterChars(L"CODEX");
  return default_pixels && custom_pixels &&
         *default_pixels != *custom_pixels;
}

bool AnnotateWindowWatermarkWheelChangesOpacityPixels() {
  auto default_pixels =
      AnnotateWindowCopiesToolPixelsAfterWheel(L"WM", 0);
  auto strong_pixels =
      AnnotateWindowCopiesToolPixelsAfterWheel(L"WM", 3);
  return default_pixels && strong_pixels && *default_pixels != *strong_pixels;
}

std::shared_ptr<std::vector<uint8_t>>
AnnotateWindowCopiesTextPixelsWithBackground(bool background_enabled,
                                             int background_color_cycles = 0) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return {};
  }

  constexpr int width = 48;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return {};
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return {};
  }

  HWND text_bg_button = FindAnnotateButton(hwnd, L"BG");
  HWND text_bg_color_button = FindAnnotateButton(hwnd, L"Clr");
  HWND text_button = FindAnnotateButton(hwnd, L"Text");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!text_bg_button || !text_button || !copy_button ||
      (background_color_cycles > 0 && !text_bg_color_button)) {
    annotate.Destroy();
    return {};
  }
  if (background_enabled) {
    SendMessageW(text_bg_button, BM_CLICK, 0, 0);
  }
  for (int i = 0; i < background_color_cycles; ++i) {
    SendMessageW(text_bg_color_button, BM_CLICK, 0, 0);
  }
  SendMessageW(text_button, BM_CLICK, 0, 0);

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(8, toolbar_height + 12));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(8, toolbar_height + 12));
  SendMessageW(hwnd, WM_CHAR, static_cast<WPARAM>(L'A'), 0);
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return {};
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return {};
  }
  return copied_pixels;
}

std::shared_ptr<std::vector<uint8_t>>
AnnotateWindowCopiesSelectedTextPixelsWithBackgroundColor(int color_cycles) {
  snappin::AnnotateWindow annotate;
  if (!annotate.Create(GetModuleHandleW(nullptr))) {
    return {};
  }

  constexpr int width = 48;
  constexpr int height = 32;
  constexpr int stride = width * 4;
  auto source = MakeAnnotateTestSource(width, height, stride);

  bool callback_seen = false;
  snappin::AnnotateWindow::Command seen_command =
      snappin::AnnotateWindow::Command::Close;
  std::shared_ptr<std::vector<uint8_t>> copied_pixels;
  snappin::SizePX copied_size = {};
  int32_t copied_stride = 0;
  annotate.SetCommandCallback(
      [&](snappin::AnnotateWindow::Command command,
          std::shared_ptr<std::vector<uint8_t>> pixels,
          const snappin::SizePX& size_px, int32_t stride_bytes) {
        callback_seen = true;
        seen_command = command;
        copied_pixels = std::move(pixels);
        copied_size = size_px;
        copied_stride = stride_bytes;
      });

  if (!annotate.BeginSession({100, 100, width, height}, source,
                             {width, height}, stride)) {
    annotate.Destroy();
    return {};
  }

  HWND hwnd = FindWindowW(L"SnapPinAnnotateWindow", L"SnapPin Mark");
  if (!hwnd) {
    annotate.Destroy();
    return {};
  }

  HWND text_bg_button = FindAnnotateButton(hwnd, L"BG");
  HWND text_bg_color_button = FindAnnotateButton(hwnd, L"Clr");
  HWND text_button = FindAnnotateButton(hwnd, L"Text");
  HWND copy_button = FindAnnotateButton(hwnd, L"Copy");
  if (!text_bg_button || !text_bg_color_button || !text_button || !copy_button) {
    annotate.Destroy();
    return {};
  }

  SendMessageW(text_bg_button, BM_CLICK, 0, 0);
  SendMessageW(text_button, BM_CLICK, 0, 0);

  constexpr int toolbar_height = 34;
  SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON,
               MAKELPARAM(8, toolbar_height + 12));
  SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(8, toolbar_height + 12));
  SendMessageW(hwnd, WM_CHAR, static_cast<WPARAM>(L'A'), 0);
  for (int i = 0; i < color_cycles; ++i) {
    SendMessageW(text_bg_color_button, BM_CLICK, 0, 0);
  }
  SendMessageW(copy_button, BM_CLICK, 0, 0);
  annotate.Destroy();

  if (!callback_seen ||
      seen_command != snappin::AnnotateWindow::Command::Copy ||
      !copied_pixels) {
    return {};
  }
  if (copied_size.w != width || copied_size.h != height ||
      copied_stride != stride ||
      copied_pixels->size() != source->size()) {
    return {};
  }
  return copied_pixels;
}

bool AnnotateWindowTextBackgroundButtonTogglesState() {
  return WithAnnotateWindow([](HWND hwnd) {
    HWND button = FindAnnotateButton(hwnd, L"BG");
    if (!button) {
      return false;
    }
    SendMessageW(button, BM_CLICK, 0, 0);
    return FindAnnotateButton(hwnd, L"*BG") != nullptr;
  });
}

bool AnnotateWindowTextBackgroundChangesCopiedPixels() {
  auto plain_pixels = AnnotateWindowCopiesTextPixelsWithBackground(false);
  auto bg_pixels = AnnotateWindowCopiesTextPixelsWithBackground(true);
  return plain_pixels && bg_pixels && *plain_pixels != *bg_pixels;
}

bool AnnotateWindowTextBackgroundColorChangesCopiedPixels() {
  auto default_pixels = AnnotateWindowCopiesTextPixelsWithBackground(true);
  auto color_pixels = AnnotateWindowCopiesTextPixelsWithBackground(true, 1);
  return default_pixels && color_pixels && *default_pixels != *color_pixels;
}

bool AnnotateWindowSelectedTextBackgroundColorChangesCopiedPixels() {
  auto default_pixels = AnnotateWindowCopiesSelectedTextPixelsWithBackgroundColor(0);
  auto color_pixels = AnnotateWindowCopiesSelectedTextPixelsWithBackgroundColor(1);
  return default_pixels && color_pixels && *default_pixels != *color_pixels;
}

bool OcrResultWindowShowsSelectableText() {
  snappin::OcrResultWindow window;
  if (!window.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  const wchar_t expected[] = L"Recognized OCR text";
  window.ShowText(expected);

  HWND hwnd = FindWindowW(L"SnapPinOcrResultWindow", L"SnapPin OCR Result");
  if (!hwnd || !window.IsVisible()) {
    window.Destroy();
    return false;
  }

  HWND edit = FindChildClass(hwnd, L"EDIT");
  if (!edit) {
    window.Destroy();
    return false;
  }

  wchar_t text[256] = {};
  GetWindowTextW(edit, text, static_cast<int>(std::size(text)));
  const LONG_PTR style = GetWindowLongPtrW(edit, GWL_STYLE);
  const bool selectable_readonly =
      (style & ES_MULTILINE) != 0 && (style & ES_READONLY) != 0 &&
      (style & WS_VSCROLL) != 0;
  window.Destroy();
  return selectable_readonly && wcscmp(text, expected) == 0;
}

bool OcrResultWindowSelectsTextOnShow() {
  snappin::OcrResultWindow window;
  if (!window.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  const wchar_t expected[] = L"Selected OCR text";
  window.ShowText(expected);

  HWND hwnd = FindWindowW(L"SnapPinOcrResultWindow", L"SnapPin OCR Result");
  if (!hwnd || !window.IsVisible()) {
    window.Destroy();
    return false;
  }

  HWND edit = FindChildClass(hwnd, L"EDIT");
  if (!edit) {
    window.Destroy();
    return false;
  }

  DWORD start = 0;
  DWORD end = 0;
  SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start),
               reinterpret_cast<LPARAM>(&end));
  window.Destroy();
  return start == 0 && end == wcslen(expected);
}

bool OcrResultWindowCopyButtonInvokesCallback() {
  snappin::OcrResultWindow window;
  if (!window.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  const std::wstring expected = L"Copyable OCR text";
  std::wstring copied;
  window.SetCopyCallback(
      [&](const std::wstring& text) { copied = text; });
  window.ShowText(expected);

  HWND hwnd = FindWindowW(L"SnapPinOcrResultWindow", L"SnapPin OCR Result");
  if (!hwnd || !window.IsVisible()) {
    window.Destroy();
    return false;
  }

  HWND copy = FindAnnotateButton(hwnd, L"Copy");
  if (!copy) {
    window.Destroy();
    return false;
  }
  SendMessageW(copy, BM_CLICK, 0, 0);
  window.Destroy();
  return copied == expected;
}

bool OcrResultWindowCopyButtonTracksTextAvailability() {
  snappin::OcrResultWindow window;
  if (!window.Create(GetModuleHandleW(nullptr))) {
    return false;
  }

  window.ShowText(L"");
  HWND hwnd = FindWindowW(L"SnapPinOcrResultWindow", L"SnapPin OCR Result");
  if (!hwnd || !window.IsVisible()) {
    window.Destroy();
    return false;
  }

  HWND copy = FindAnnotateButton(hwnd, L"Copy");
  if (!copy) {
    window.Destroy();
    return false;
  }
  const bool disabled_for_empty = !IsWindowEnabled(copy);

  window.ShowText(L"Non-empty OCR text");
  const bool enabled_for_text = IsWindowEnabled(copy);
  window.Destroy();
  return disabled_for_empty && enabled_for_text;
}

bool OcrTextProgressEventCarriesUtf8Text() {
  const std::wstring expected = L"Recognized OCR text - \x6587\x672c";
  snappin::ActionEvent event =
      snappin::MakeOcrTextProgressEvent(snappin::Id64{77}, expected);
  if (event.action_id != "ocr.start" ||
      event.correlation_id.value != 77 ||
      event.type != snappin::ActionEvent::Type::Progress ||
      event.message != "ocr.text" || event.output_ref.empty()) {
    return false;
  }
  std::optional<std::wstring> decoded = snappin::OcrTextFromProgressEvent(event);
  return decoded.has_value() && *decoded == expected;
}

bool RectEquals(snappin::RectPX a, snappin::RectPX b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool HasContext(const snappin::ActionDescriptor& action,
                snappin::ActionContext context) {
  for (snappin::ActionContext existing : action.contexts) {
    if (existing == context) {
      return true;
    }
  }
  return false;
}

bool HasParam(const snappin::ActionDescriptor& action, const char* name,
              const char* type, bool required) {
  for (const snappin::ActionParamDef& param : action.params) {
    if (param.name == name && param.type == type &&
        param.required == required) {
      return true;
    }
  }
  return false;
}

} // namespace

int main() {
  snappin::RectPX r{};
  if (!(r.w == 0 && r.h == 0)) {
    return 1;
  }

  if (!snappin::OverlayWindow::ShouldUseSelectionHole(
          true, true, false, false)) {
    return 2;
  }

  if (snappin::OverlayWindow::ShouldUseSelectionHole(
          false, false, true, false)) {
    return 3;
  }

  if (snappin::OverlayWindow::ShouldUseSelectionHole(
          true, false, true, true)) {
    return 4;
  }

  if (!AnnotateWindowHasButton(L"Oval")) {
    return 5;
  }

  if (!AnnotateWindowHasButton(L"No.")) {
    return 6;
  }

  if (!AnnotateWindowHasButton(L"Mos")) {
    return 7;
  }

  if (!AnnotateWindowHasButton(L"Blur")) {
    return 56;
  }

  if (!AnnotateWindowHasButton(L"Poly")) {
    return 59;
  }

  if (!AnnotateWindowHasButton(L"Erase")) {
    return 25;
  }

  if (!AnnotateWindowHasButton(L"HL")) {
    return 27;
  }

  if (!AnnotateWindowHasButton(L"Spot")) {
    return 36;
  }

  if (!AnnotateWindowHasButton(L"WM")) {
    return 62;
  }

  if (!AnnotateWindowHasButton(L"Zoom")) {
    return 65;
  }

  if (!AnnotateWindowHasButton(L"BG")) {
    return 78;
  }

  if (!AnnotateWindowHasButton(L"Clr")) {
    return 86;
  }

  if (!AnnotateWindowSelectsTool(L"Oval", L"*Oval")) {
    return 8;
  }

  if (!AnnotateWindowSelectsTool(L"No.", L"*No.")) {
    return 9;
  }

  if (!AnnotateWindowSelectsTool(L"Mos", L"*Mos")) {
    return 10;
  }

  if (!AnnotateWindowSelectsTool(L"Blur", L"*Blur")) {
    return 57;
  }

  if (!AnnotateWindowSelectsTool(L"Poly", L"*Poly")) {
    return 60;
  }

  if (!AnnotateWindowSelectsTool(L"Erase", L"*Erase")) {
    return 26;
  }

  if (!AnnotateWindowSelectsTool(L"HL", L"*HL")) {
    return 28;
  }

  if (!AnnotateWindowSelectsTool(L"Spot", L"*Spot")) {
    return 37;
  }

  if (!AnnotateWindowSelectsTool(L"WM", L"*WM")) {
    return 63;
  }

  if (!AnnotateWindowSelectsTool(L"Zoom", L"*Zoom")) {
    return 66;
  }

  if (!AnnotateWindowTextBackgroundButtonTogglesState()) {
    return 79;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(nullptr,
                                             AnnotateCreateGesture::Drag)) {
    return 38;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Oval",
                                             AnnotateCreateGesture::Drag)) {
    return 39;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Mos",
                                             AnnotateCreateGesture::Drag)) {
    return 40;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Blur",
                                             AnnotateCreateGesture::Drag)) {
    return 58;
  }

  if (!AnnotateWindowEffectWheelChangesCopiedPixels(L"Mos")) {
    return 70;
  }

  if (!AnnotateWindowEffectWheelChangesCopiedPixels(L"Blur")) {
    return 71;
  }

  if (!AnnotateWindowCopiesPolylinePixels()) {
    return 61;
  }

  if (!AnnotateWindowPolylineNodeEditChangesCopiedPixels()) {
    return 69;
  }

  if (!AnnotateWindowPolylineSegmentDoubleClickInsertsNode()) {
    return 73;
  }

  if (!AnnotateWindowPolylineDeleteRemovesSelectedNode()) {
    return 74;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"HL",
                                             AnnotateCreateGesture::Drag)) {
    return 41;
  }

  if (!AnnotateWindowHighlighterWheelChangesCenterOpacityPixels()) {
    return 83;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Spot",
                                             AnnotateCreateGesture::Drag)) {
    return 42;
  }

  if (!AnnotateWindowSpotlightWheelChangesDimPixels()) {
    return 77;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"WM",
                                             AnnotateCreateGesture::Drag)) {
    return 64;
  }

  if (!AnnotateWindowWatermarkDirectTextChangesCopiedPixels()) {
    return 75;
  }

  if (!AnnotateWindowWatermarkWheelChangesOpacityPixels()) {
    return 84;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Zoom",
                                             AnnotateCreateGesture::Drag)) {
    return 67;
  }

  if (!AnnotateWindowMagnifierWheelChangesZoomPixels()) {
    return 76;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Line",
                                             AnnotateCreateGesture::Drag)) {
    return 43;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Arrow",
                                             AnnotateCreateGesture::Drag)) {
    return 44;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"No.",
                                             AnnotateCreateGesture::Click)) {
    return 45;
  }

  if (!AnnotateWindowSerialDirectEntryChangesCopiedPixels()) {
    return 68;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Pen",
                                             AnnotateCreateGesture::Drag)) {
    return 46;
  }

  if (!AnnotateWindowCopiesComposedToolPixels(L"Text",
                                             AnnotateCreateGesture::Click)) {
    return 47;
  }

  if (!AnnotateWindowTextBackgroundChangesCopiedPixels()) {
    return 80;
  }

  if (!AnnotateWindowTextBackgroundColorChangesCopiedPixels()) {
    return 87;
  }

  if (!AnnotateWindowSelectedTextBackgroundColorChangesCopiedPixels()) {
    return 88;
  }

  if (!AnnotateWindowEraserRemovesRectFromCopyPixels()) {
    return 48;
  }

  if (!AnnotateWindowEraserPartiallyErasesPolylinePixels()) {
    return 72;
  }

  if (!OcrResultWindowShowsSelectableText()) {
    return 54;
  }

  if (!OcrResultWindowSelectsTextOnShow()) {
    return 82;
  }

  if (!OcrResultWindowCopyButtonInvokesCallback()) {
    return 81;
  }

  if (!OcrResultWindowCopyButtonTracksTextAvailability()) {
    return 85;
  }

  if (!OcrTextProgressEventCarriesUtf8Text()) {
    return 55;
  }

  snappin::TrayIcon tray;
  if (tray.ShowNotification(L"SnapPin", L"OCR copied", false)) {
    return 11;
  }

  if (snappin::MosaicBlockSize(0, 0) != 0) {
    return 12;
  }

  if (snappin::MosaicBlockSize(10, 10) != 6) {
    return 13;
  }

  if (snappin::MosaicBlockSize(180, 90) != 10) {
    return 14;
  }

  if (snappin::MosaicBlockSize(400, 120) != 18) {
    return 15;
  }

  if (snappin::AdjustedSerialValue(1, -1) != 1) {
    return 33;
  }

  if (snappin::AdjustedSerialValue(3, -2) != 1) {
    return 34;
  }

  if (snappin::AdjustedSerialValue(3, 2) != 5) {
    return 35;
  }

  if (snappin::AnnotateToolbarMinWidth(12, 5, 72, 3, 4) != 1280) {
    return 29;
  }

  if (snappin::AnnotateToolbarMinWidth(0, 0, 72, 3, 4) != 8) {
    return 30;
  }

  if (!AnnotateWindowClientWidthStaysAtBitmapWidth()) {
    return 89;
  }

  snappin::RectPX clamped =
      snappin::ClampWindowRectToBounds({50, 50, 1280, 400},
                                       {0, 0, 1024, 768});
  if (!RectEquals(clamped, {0, 50, 1024, 400})) {
    return 31;
  }

  clamped = snappin::ClampWindowRectToBounds({900, 100, 200, 100},
                                             {0, 0, 1024, 768});
  if (!RectEquals(clamped, {824, 100, 200, 100})) {
    return 32;
  }

  auto crop = snappin::ResolveOcrCropRect({100, 200, 320, 240}, {320, 240},
                                          {120, 230, 50, 60});
  if (!crop.has_value() || !RectEquals(*crop, {20, 30, 50, 60})) {
    return 16;
  }

  crop = snappin::ResolveOcrCropRect({100, 200, 320, 240}, {320, 240},
                                     {90, 190, 40, 50});
  if (!crop.has_value() || !RectEquals(*crop, {0, 0, 30, 40})) {
    return 17;
  }

  crop = snappin::ResolveOcrCropRect({100, 200, 320, 240}, {320, 240},
                                     {500, 500, 10, 10});
  if (crop.has_value()) {
    return 18;
  }

  crop = snappin::ResolveOcrCropRect({0, 0, 0, 0}, {100, 80},
                                     {10, 20, 30, 40});
  if (!crop.has_value() || !RectEquals(*crop, {10, 20, 30, 40})) {
    return 19;
  }

  snappin::ActionRegistry registry;
  std::optional<snappin::ActionDescriptor> ocr_action =
      registry.Find("ocr.start");
  if (!ocr_action.has_value()) {
    return 20;
  }
  if (!HasContext(*ocr_action, snappin::ActionContext::PIN_FOCUSED)) {
    return 21;
  }
  if (!HasParam(*ocr_action, "source", "string", false)) {
    return 49;
  }
  if (!HasParam(*ocr_action, "x", "int32", false)) {
    return 50;
  }
  if (!HasParam(*ocr_action, "y", "int32", false)) {
    return 51;
  }
  if (!HasParam(*ocr_action, "w", "int32", false)) {
    return 52;
  }
  if (!HasParam(*ocr_action, "h", "int32", false)) {
    return 53;
  }

  if (!snappin::PinWindow::SupportsCommandForContent(
          snappin::PinWindow::ContentKind::Image,
          snappin::PinWindow::Command::OcrSelf)) {
    return 22;
  }

  if (snappin::PinWindow::SupportsCommandForContent(
          snappin::PinWindow::ContentKind::Text,
          snappin::PinWindow::Command::OcrSelf)) {
    return 23;
  }

  if (snappin::PinWindow::SupportsCommandForContent(
          snappin::PinWindow::ContentKind::Latex,
          snappin::PinWindow::Command::OcrSelf)) {
    return 24;
  }

  return 0;
}
