#include "PinManager.h"

#include "Action.h"
#include "ConfigService.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace snappin {
namespace {

bool OpenClipboardWithRetry(int retry_ms, int retry_count) {
  for (int i = 0; i <= retry_count; ++i) {
    if (OpenClipboard(nullptr)) {
      return true;
    }
    Sleep(retry_ms);
  }
  return false;
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) {
    return b;
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return a + b;
  }
  return a + L"\\" + b;
}

std::wstring GetDesktopDir() {
  PWSTR desktop = nullptr;
  const HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT,
                                          nullptr, &desktop);
  if (FAILED(hr) || !desktop) {
    if (desktop) {
      CoTaskMemFree(desktop);
    }
    return L"";
  }
  std::wstring out(desktop);
  CoTaskMemFree(desktop);
  return out;
}

bool EnsureDir(const std::wstring& path) {
  if (path.empty()) {
    return false;
  }
  const int ret = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
  return ret == ERROR_SUCCESS || ret == ERROR_ALREADY_EXISTS;
}

} // namespace

PinManager::~PinManager() { Shutdown(); }

bool PinManager::Initialize(HINSTANCE instance, HWND main_hwnd,
                            RuntimeState* runtime_state,
                            ConfigService* config_service,
                            IExportService* exporter) {
  instance_ = instance;
  main_hwnd_ = main_hwnd;
  runtime_state_ = runtime_state;
  config_service_ = config_service;
  exporter_ = exporter;
  return instance_ != nullptr && main_hwnd_ != nullptr;
}

void PinManager::Shutdown() {
  DestroyAll();
  instance_ = nullptr;
  main_hwnd_ = nullptr;
  runtime_state_ = nullptr;
  config_service_ = nullptr;
  exporter_ = nullptr;
}

Result<Id64> PinManager::CreateFromArtifact(const Artifact& art) {
  std::shared_ptr<std::vector<uint8_t>> storage;
  SizePX size_px{};
  int32_t stride_bytes = 0;

  if (art.base_cpu.has_value() && art.base_cpu_storage &&
      !art.base_cpu_storage->empty() &&
      art.base_cpu->format == PixelFormat::BGRA8 && art.base_cpu->size_px.w > 0 &&
      art.base_cpu->size_px.h > 0 &&
      art.base_cpu->stride_bytes >= art.base_cpu->size_px.w * 4) {
    const size_t total = static_cast<size_t>(art.base_cpu->stride_bytes) *
                         static_cast<size_t>(art.base_cpu->size_px.h);
    if (art.base_cpu_storage->size() < total) {
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Artifact bitmap storage invalid";
      err.retryable = true;
      err.detail = "base_cpu_storage_size";
      return Result<Id64>::Fail(err);
    }
    auto copied = std::make_shared<std::vector<uint8_t>>();
    copied->resize(total);
    std::memcpy(copied->data(), art.base_cpu_storage->data(), total);
    storage = std::move(copied);
    size_px = art.base_cpu->size_px;
    stride_bytes = art.base_cpu->stride_bytes;
  } else {
    if (!CaptureRectToBitmap(art.screen_rect_px, &storage, &size_px, &stride_bytes)) {
      Error err;
      err.code = ERR_CAPTURE_FAILED;
      err.message = "Pin capture failed";
      err.retryable = true;
      err.detail = "capture_rect";
      return Result<Id64>::Fail(err);
    }
  }

  PointPX pos{};
  pos.x = art.screen_rect_px.x;
  pos.y = art.screen_rect_px.y;
  return CreatePinWithBitmap(std::move(storage), size_px, stride_bytes, pos);
}

Result<Id64> PinManager::CreateFromClipboard() {
  std::shared_ptr<std::vector<uint8_t>> storage;
  SizePX size_px{};
  int32_t stride_bytes = 0;
  Error err;
  if (!ReadClipboardBitmap(&storage, &size_px, &stride_bytes, &err)) {
    return Result<Id64>::Fail(err);
  }
  const PointPX pos = DefaultCenteredPos(size_px);
  return CreatePinWithBitmap(std::move(storage), size_px, stride_bytes, pos);
}

Result<void> PinManager::CloseFocused() {
  if (!focused_pin_id_.has_value()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "No focused pin";
    err.retryable = false;
    err.detail = "pin_focus_empty";
    return Result<void>::Fail(err);
  }
  return ClosePin(focused_pin_id_.value());
}

Result<void> PinManager::CloseAll() {
  for (auto& kv : pins_) {
    if (kv.second.window) {
      kv.second.window->Hide();
    }
  }
  SetFocusedPin(std::nullopt);
  return Result<void>::Ok();
}

Result<void> PinManager::CopyFocused() {
  if (!focused_pin_id_.has_value()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "No focused pin";
    err.retryable = false;
    err.detail = "pin_focus_empty";
    return Result<void>::Fail(err);
  }
  return CopyPin(*focused_pin_id_);
}

Result<void> PinManager::SaveFocused() {
  if (!focused_pin_id_.has_value()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "No focused pin";
    err.retryable = false;
    err.detail = "pin_focus_empty";
    return Result<void>::Fail(err);
  }
  return SavePin(*focused_pin_id_);
}

bool PinManager::HandleWindowCommand(WPARAM wparam, LPARAM lparam) {
  const Id64 pin_id{static_cast<uint64_t>(wparam)};
  const PinWindow::Command cmd = static_cast<PinWindow::Command>(lparam);

  Result<void> res = Result<void>::Ok();
  switch (cmd) {
    case PinWindow::Command::CopySelf:
      res = CopyPin(pin_id);
      break;
    case PinWindow::Command::SaveSelf:
      res = SavePin(pin_id);
      break;
    case PinWindow::Command::CloseSelf:
      res = ClosePin(pin_id);
      break;
    case PinWindow::Command::DestroySelf:
      res = DestroyPin(pin_id);
      break;
    case PinWindow::Command::CloseAll:
      res = CloseAll();
      break;
    case PinWindow::Command::DestroyAll:
      res = DestroyAll();
      break;
    default:
      return false;
  }

  if (!res.ok) {
    char buffer[256] = {};
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                "pin cmd failed code=%s detail=%s\n", res.error.code.c_str(),
                res.error.detail.c_str());
    OutputDebugStringA(buffer);
  }
  return true;
}

bool PinManager::CaptureRectToBitmap(
    const RectPX& rect, std::shared_ptr<std::vector<uint8_t>>* storage_out,
    SizePX* size_out, int32_t* stride_out) {
  if (!storage_out || !size_out || !stride_out || rect.w <= 0 || rect.h <= 0) {
    return false;
  }

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = rect.w;
  bi.bmiHeader.biHeight = -rect.h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HBITMAP dib =
      CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    ReleaseDC(nullptr, screen);
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  HGDIOBJ old = SelectObject(mem, dib);
  BOOL ok = BitBlt(mem, 0, 0, rect.w, rect.h, screen, rect.x, rect.y,
                   SRCCOPY | CAPTUREBLT);
  SelectObject(mem, old);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  if (!ok) {
    DeleteObject(dib);
    return false;
  }

  const int32_t stride = rect.w * 4;
  const size_t total = static_cast<size_t>(stride) * static_cast<size_t>(rect.h);
  auto storage = std::make_shared<std::vector<uint8_t>>();
  storage->resize(total);
  std::memcpy(storage->data(), bits, total);
  DeleteObject(dib);

  *storage_out = std::move(storage);
  *size_out = SizePX{rect.w, rect.h};
  *stride_out = stride;
  return true;
}

bool PinManager::ReadClipboardBitmap(
    std::shared_ptr<std::vector<uint8_t>>* storage_out, SizePX* size_out,
    int32_t* stride_out, Error* err) {
  if (!storage_out || !size_out || !stride_out || !err) {
    return false;
  }

  if (!OpenClipboardWithRetry(100, 5)) {
    err->code = ERR_CLIPBOARD_BUSY;
    err->message = "Clipboard busy";
    err->retryable = true;
    err->detail = "OpenClipboard";
    return false;
  }

  if (!IsClipboardFormatAvailable(CF_BITMAP)) {
    CloseClipboard();
    err->code = ERR_CLIPBOARD_EMPTY;
    err->message = "Clipboard has no image";
    err->retryable = false;
    err->detail = "CF_BITMAP";
    return false;
  }

  HBITMAP bmp = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
  if (!bmp) {
    CloseClipboard();
    err->code = ERR_CLIPBOARD_EMPTY;
    err->message = "Clipboard image unavailable";
    err->retryable = false;
    err->detail = "GetClipboardData";
    return false;
  }

  BITMAP bm = {};
  if (!GetObjectW(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
    CloseClipboard();
    err->code = ERR_INTERNAL_ERROR;
    err->message = "Clipboard bitmap invalid";
    err->retryable = true;
    err->detail = "GetObjectW";
    return false;
  }

  const int32_t w = bm.bmWidth;
  const int32_t h = bm.bmHeight;
  const int32_t stride = w * 4;
  auto storage = std::make_shared<std::vector<uint8_t>>();
  storage->resize(static_cast<size_t>(stride) * static_cast<size_t>(h));

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  HDC screen = GetDC(nullptr);
  const int lines =
      GetDIBits(screen, bmp, 0, static_cast<UINT>(h), storage->data(), &bi,
                DIB_RGB_COLORS);
  ReleaseDC(nullptr, screen);
  CloseClipboard();

  if (lines <= 0) {
    err->code = ERR_INTERNAL_ERROR;
    err->message = "Clipboard decode failed";
    err->retryable = true;
    err->detail = "GetDIBits";
    return false;
  }

  *storage_out = std::move(storage);
  *size_out = SizePX{w, h};
  *stride_out = stride;
  return true;
}

Result<Id64> PinManager::CreatePinWithBitmap(
    std::shared_ptr<std::vector<uint8_t>> storage, const SizePX& size_px,
    int32_t stride_bytes, const PointPX& pos_px) {
  if (!instance_ || !main_hwnd_ || !storage || size_px.w <= 0 || size_px.h <= 0) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Pin manager not initialized";
    err.retryable = true;
    err.detail = "init";
    return Result<Id64>::Fail(err);
  }

  Id64 pin_id{next_pin_id_++};
  auto window = std::make_unique<PinWindow>();
  window->SetCallbacks(
      [this](Id64 focused_id) { SetFocusedPin(focused_id); },
      [this](Id64 source_id, PinWindow::Command command) {
        if (main_hwnd_) {
          PostMessageW(main_hwnd_, kWindowCommandMessage,
                       static_cast<WPARAM>(source_id.value),
                       static_cast<LPARAM>(command));
        }
      });

  if (!window->Create(instance_, pin_id, storage, size_px, stride_bytes, pos_px)) {
    Error err;
    err.code = ERR_OUT_OF_MEMORY;
    err.message = "Pin window create failed";
    err.retryable = true;
    err.detail = "CreateWindowExW";
    return Result<Id64>::Fail(err);
  }

  PinEntry entry;
  entry.storage = std::move(storage);
  entry.size_px = size_px;
  entry.stride_bytes = stride_bytes;
  entry.window = std::move(window);
  pins_[pin_id.value] = std::move(entry);

  SetFocusedPin(pin_id);
  return Result<Id64>::Ok(pin_id);
}

PointPX PinManager::DefaultCenteredPos(const SizePX& size_px) const {
  POINT cursor = {};
  GetCursorPos(&cursor);
  HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (!GetMonitorInfoW(monitor, &mi)) {
    return PointPX{cursor.x, cursor.y};
  }
  const RECT work = mi.rcWork;
  PointPX pos;
  const int offset_x = (static_cast<int>(work.right - work.left) - size_px.w) / 2;
  const int offset_y = (static_cast<int>(work.bottom - work.top) - size_px.h) / 2;
  pos.x = static_cast<int32_t>(work.left) + std::max<int>(0, offset_x);
  pos.y = static_cast<int32_t>(work.top) + std::max<int>(0, offset_y);
  return pos;
}

void PinManager::SetFocusedPin(const std::optional<Id64>& pin_id) {
  focused_pin_id_ = pin_id;
  if (runtime_state_) {
    runtime_state_->focused_pin_id = pin_id;
  }
}

Result<void> PinManager::BuildArtifactFromPin(Id64 pin_id, Artifact* out_artifact) {
  if (!out_artifact) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Invalid output artifact";
    err.retryable = true;
    err.detail = "artifact_null";
    return Result<void>::Fail(err);
  }

  auto it = pins_.find(pin_id.value);
  if (it == pins_.end() || !it->second.storage || it->second.storage->empty()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Pin not found";
    err.retryable = false;
    err.detail = "pin_id";
    return Result<void>::Fail(err);
  }

  Artifact art;
  art.artifact_id = pin_id;
  art.kind = ArtifactKind::CAPTURE;
  art.base_cpu_storage = it->second.storage;
  CpuBitmap bmp;
  bmp.format = PixelFormat::BGRA8;
  bmp.size_px = it->second.size_px;
  bmp.stride_bytes = it->second.stride_bytes;
  bmp.data.p = it->second.storage->data();
  art.base_cpu = bmp;
  art.screen_rect_px = RectPX{0, 0, it->second.size_px.w, it->second.size_px.h};
  art.dpi_scale = 1.0f;

  *out_artifact = std::move(art);
  return Result<void>::Ok();
}

Result<void> PinManager::CopyPin(Id64 pin_id) {
  if (!exporter_) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Exporter unavailable";
    err.retryable = true;
    err.detail = "exporter_null";
    return Result<void>::Fail(err);
  }

  Artifact art;
  Result<void> built = BuildArtifactFromPin(pin_id, &art);
  if (!built.ok) {
    return built;
  }
  return exporter_->CopyImageToClipboard(art);
}

Result<void> PinManager::SavePin(Id64 pin_id) {
  if (!exporter_ || !config_service_) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Save service unavailable";
    err.retryable = true;
    err.detail = "save_service_null";
    return Result<void>::Fail(err);
  }

  Artifact art;
  Result<void> built = BuildArtifactFromPin(pin_id, &art);
  if (!built.ok) {
    return built;
  }

  std::wstring dir = config_service_->ExportSaveDir();
  if (dir.empty()) {
    dir = GetDesktopDir();
  }
  if (dir.empty()) {
    Error err;
    err.code = ERR_PATH_NOT_WRITABLE;
    err.message = "Save path unavailable";
    err.retryable = true;
    err.detail = "save_dir_empty";
    return Result<void>::Fail(err);
  }
  if (!EnsureDir(dir)) {
    Error err;
    err.code = ERR_PATH_NOT_WRITABLE;
    err.message = "Save path not writable";
    err.retryable = false;
    err.detail = "save_dir_unwritable";
    return Result<void>::Fail(err);
  }

  SYSTEMTIME st = {};
  GetLocalTime(&st);
  wchar_t file_name[128] = {};
  _snwprintf_s(file_name, _countof(file_name), _TRUNCATE,
               L"SnapPin_Pin_%04d%02d%02d_%02d%02d%02d_%llu.png", st.wYear,
               st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
               static_cast<unsigned long long>(pin_id.value));

  SaveImageOptions opts;
  opts.format = ImageFormat::PNG;
  opts.path = JoinPath(dir, file_name);

  Result<std::wstring> saved = exporter_->SaveImage(art, opts);
  if (!saved.ok) {
    return Result<void>::Fail(saved.error);
  }
  return Result<void>::Ok();
}

Result<void> PinManager::ClosePin(Id64 pin_id) {
  auto it = pins_.find(pin_id.value);
  if (it == pins_.end() || !it->second.window) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Pin not found";
    err.retryable = false;
    err.detail = "pin_id";
    return Result<void>::Fail(err);
  }
  it->second.window->Hide();
  if (focused_pin_id_.has_value() && focused_pin_id_->value == pin_id.value) {
    SetFocusedPin(std::nullopt);
  }
  return Result<void>::Ok();
}

Result<void> PinManager::DestroyPin(Id64 pin_id) {
  auto it = pins_.find(pin_id.value);
  if (it == pins_.end()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Pin not found";
    err.retryable = false;
    err.detail = "pin_id";
    return Result<void>::Fail(err);
  }
  if (it->second.window) {
    it->second.window->Destroy();
  }
  pins_.erase(it);
  if (focused_pin_id_.has_value() && focused_pin_id_->value == pin_id.value) {
    SetFocusedPin(std::nullopt);
  }
  return Result<void>::Ok();
}

Result<void> PinManager::DestroyAll() {
  for (auto& kv : pins_) {
    if (kv.second.window) {
      kv.second.window->Destroy();
    }
  }
  pins_.clear();
  SetFocusedPin(std::nullopt);
  return Result<void>::Ok();
}

} // namespace snappin

