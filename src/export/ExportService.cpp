#include "ExportService.h"

#include "ErrorCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <wincodec.h>

#include <shlwapi.h>

#include <cstring>
#include <string>
#include <vector>

namespace snappin {
namespace {

void FillWin32Error(Error* err, const char* code, const char* message, DWORD last_error) {
  if (!err) {
    return;
  }
  err->code = code;
  err->message = message;
  err->retryable = true;
  err->detail = std::to_string(static_cast<unsigned long long>(last_error));
}

bool OpenClipboardWithRetry(HWND hwnd, int retry_ms, int retry_count, Error* err) {
  for (int i = 0; i <= retry_count; ++i) {
    if (OpenClipboard(hwnd)) {
      return true;
    }
    Sleep(retry_ms);
  }
  FillWin32Error(err, ERR_CLIPBOARD_BUSY, "Clipboard busy", GetLastError());
  return false;
}

void SafeRelease(IUnknown* ptr) {
  if (ptr) {
    ptr->Release();
  }
}

struct DIBSection {
  HBITMAP bitmap = nullptr;
  void* bits = nullptr;
  int32_t stride = 0;
};

bool TryGetCpuBitmap(const Artifact& art, CpuBitmap* out) {
  if (!out) {
    return false;
  }
  if (!art.base_cpu.has_value()) {
    return false;
  }
  if (!art.base_cpu_storage || art.base_cpu_storage->empty()) {
    return false;
  }
  *out = art.base_cpu.value();
  out->data.p = art.base_cpu_storage->data();
  if (out->size_px.w <= 0 || out->size_px.h <= 0) {
    return false;
  }
  if (out->stride_bytes < out->size_px.w * 4) {
    return false;
  }
  return out->data.p != nullptr;
}

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

bool CaptureRegionToDib(const RectPX& rect, DIBSection* out, Error* err) {
  if (!out) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Invalid DIB target", ERROR_INVALID_PARAMETER);
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

HGLOBAL CreateDibV5Global(const DIBSection& dib, int32_t width, int32_t height) {
  BITMAPV5HEADER header = {};
  header.bV5Size = sizeof(header);
  header.bV5Width = width;
  header.bV5Height = -height;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  size_t image_bytes = static_cast<size_t>(dib.stride) * height;
  size_t total_bytes = sizeof(header) + image_bytes;

  HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, total_bytes);
  if (!hg) {
    return nullptr;
  }
  void* mem = GlobalLock(hg);
  if (!mem) {
    GlobalFree(hg);
    return nullptr;
  }

  memcpy(mem, &header, sizeof(header));
  BYTE* pixels = reinterpret_cast<BYTE*>(mem) + sizeof(header);
  memcpy(pixels, dib.bits, image_bytes);

  GlobalUnlock(hg);
  return hg;
}

HGLOBAL CreateDibV5GlobalFromPixels(const void* pixels, int32_t width, int32_t height,
                                    int32_t stride) {
  if (!pixels || width <= 0 || height <= 0) {
    return nullptr;
  }
  const size_t row_bytes = static_cast<size_t>(width) * 4;
  const size_t image_bytes = row_bytes * static_cast<size_t>(height);
  const size_t total_bytes = sizeof(BITMAPV5HEADER) + image_bytes;

  BITMAPV5HEADER header = {};
  header.bV5Size = sizeof(header);
  header.bV5Width = width;
  header.bV5Height = -height;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, total_bytes);
  if (!hg) {
    return nullptr;
  }
  void* mem = GlobalLock(hg);
  if (!mem) {
    GlobalFree(hg);
    return nullptr;
  }

  memcpy(mem, &header, sizeof(header));
  BYTE* dst = reinterpret_cast<BYTE*>(mem) + sizeof(header);
  const BYTE* src = reinterpret_cast<const BYTE*>(pixels);
  for (int32_t y = 0; y < height; ++y) {
    memcpy(dst + static_cast<size_t>(y) * row_bytes,
           src + static_cast<size_t>(y) * stride, row_bytes);
  }

  GlobalUnlock(hg);
  return hg;
}

bool EnsureDirForFile(const std::wstring& path, Error* err) {
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return true;
  }
  std::wstring dir = path.substr(0, pos);
  if (dir.empty()) {
    return true;
  }
  if (dir.size() <= 3 && dir.size() >= 2 && dir[1] == L':') {
    return true;
  }
  size_t start = 0;
  if (dir.size() >= 3 && dir[1] == L':' && (dir[2] == L'\\' || dir[2] == L'/')) {
    start = 3;
  }
  for (size_t i = start; i < dir.size(); ++i) {
    if (dir[i] == L'\\' || dir[i] == L'/') {
      std::wstring partial = dir.substr(0, i);
      if (!partial.empty()) {
        if (!CreateDirectoryW(partial.c_str(), nullptr)) {
          DWORD last = GetLastError();
          if (last != ERROR_ALREADY_EXISTS) {
            FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Save path not writable",
                           last);
            return false;
          }
        }
      }
    }
  }
  if (!CreateDirectoryW(dir.c_str(), nullptr)) {
    DWORD last = GetLastError();
    if (last != ERROR_ALREADY_EXISTS) {
      FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Save path not writable", last);
      return false;
    }
  }
  return true;
}

Result<std::wstring> SavePngFromPixels(const void* pixels, int32_t width, int32_t height,
                                       int32_t stride, const std::wstring& path) {
  Error err;
  if (!EnsureDirForFile(path, &err)) {
    return Result<std::wstring>::Fail(err);
  }

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool co_uninit = (hr == S_OK || hr == S_FALSE);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
    err.code = ERR_INTERNAL_ERROR;
    err.message = "COM init failed";
    err.retryable = true;
    err.detail = "CoInitializeEx";
    return Result<std::wstring>::Fail(err);
  }

  IWICImagingFactory* factory = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  IPropertyBag2* bag = nullptr;
  IWICBitmap* bitmap = nullptr;
  IStream* file_stream = nullptr;

  hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    err.code = ERR_INTERNAL_ERROR;
    err.message = "WIC unavailable";
    err.retryable = true;
    err.detail = "CoCreateInstance";
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = factory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA,
                                       stride, height * stride,
                                       reinterpret_cast<BYTE*>(const_cast<void*>(pixels)),
                                       &bitmap);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "CreateBitmapFromMemory";
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = SHCreateStreamOnFileEx(path.c_str(),
                              STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
                              FILE_ATTRIBUTE_NORMAL, TRUE, nullptr, &file_stream);
  if (FAILED(hr)) {
    DWORD last = GetLastError();
    if (HRESULT_FROM_WIN32(ERROR_DISK_FULL) == hr || last == ERROR_DISK_FULL) {
      err.code = ERR_DISK_FULL;
      err.message = "Disk full";
      err.retryable = false;
    } else {
      err.code = ERR_PATH_NOT_WRITABLE;
      err.message = "Save path not writable";
      err.retryable = false;
    }
    char buffer[128];
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "SHCreateStreamOnFileEx hr=0x%08X",
                static_cast<unsigned int>(hr));
    err.detail = buffer;
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "CreateEncoder";
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = encoder->Initialize(file_stream, WICBitmapEncoderNoCache);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "EncoderInitialize";
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = encoder->CreateNewFrame(&frame, &bag);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "CreateNewFrame";
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = frame->Initialize(bag);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "FrameInitialize";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = frame->SetSize(width, height);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "SetSize";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  hr = frame->SetPixelFormat(&format);
  if (FAILED(hr) || format != GUID_WICPixelFormat32bppBGRA) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "SetPixelFormat";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = frame->WriteSource(bitmap, nullptr);
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "WriteSource";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = frame->Commit();
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "FrameCommit";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  hr = encoder->Commit();
  if (FAILED(hr)) {
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Encode failed";
    err.retryable = true;
    err.detail = "EncoderCommit";
    SafeRelease(bag);
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(file_stream);
    SafeRelease(bitmap);
    SafeRelease(factory);
    if (co_uninit) {
      CoUninitialize();
    }
    return Result<std::wstring>::Fail(err);
  }

  SafeRelease(bag);
  SafeRelease(frame);
  SafeRelease(encoder);
  SafeRelease(file_stream);
  SafeRelease(bitmap);
  SafeRelease(factory);
  if (co_uninit) {
    CoUninitialize();
  }
  return Result<std::wstring>::Ok(path);
}

Result<std::wstring> SavePngFromDib(const DIBSection& dib, const RectPX& rect,
                                    const std::wstring& path) {
  return SavePngFromPixels(dib.bits, rect.w, rect.h, dib.stride, path);
}

} // namespace

Result<void> ExportService::CopyImageToClipboard(const Artifact& art) {
  Error err;
  CpuBitmap bmp;
  if (TryGetCpuBitmap(art, &bmp) && bmp.format == PixelFormat::BGRA8 &&
      bmp.size_px.w > 0 && bmp.size_px.h > 0) {
    HGLOBAL hmem = CreateDibV5GlobalFromPixels(
        bmp.data.p, bmp.size_px.w, bmp.size_px.h, bmp.stride_bytes);
    if (!hmem) {
      Error out;
      out.code = ERR_OUT_OF_MEMORY;
      out.message = "Clipboard image alloc failed";
      out.retryable = true;
      out.detail = "GlobalAlloc";
      return Result<void>::Fail(out);
    }

    if (!OpenClipboardWithRetry(nullptr, 200, 5, &err)) {
      GlobalFree(hmem);
      return Result<void>::Fail(err);
    }

    EmptyClipboard();
    if (!SetClipboardData(CF_DIBV5, hmem)) {
      CloseClipboard();
      GlobalFree(hmem);
      FillWin32Error(&err, ERR_INTERNAL_ERROR, "Clipboard write failed",
                     GetLastError());
      return Result<void>::Fail(err);
    }

    CloseClipboard();
    return Result<void>::Ok();
  }

  RectPX rect = art.screen_rect_px;
  if (rect.w <= 0 || rect.h <= 0) {
    err.code = ERR_TARGET_INVALID;
    err.message = "Invalid artifact";
    err.retryable = false;
    err.detail = "artifact_rect_empty";
    return Result<void>::Fail(err);
  }

  DIBSection dib;
  if (!CaptureRegionToDib(rect, &dib, &err)) {
    return Result<void>::Fail(err);
  }

  HGLOBAL hmem = CreateDibV5Global(dib, rect.w, rect.h);
  DeleteObject(dib.bitmap);
  if (!hmem) {
    Error out;
    out.code = ERR_OUT_OF_MEMORY;
    out.message = "Clipboard image alloc failed";
    out.retryable = true;
    out.detail = "GlobalAlloc";
    return Result<void>::Fail(out);
  }

  int retry_ms = 200;
  int retry_count = 5;
  if (!OpenClipboardWithRetry(nullptr, retry_ms, retry_count, &err)) {
    GlobalFree(hmem);
    return Result<void>::Fail(err);
  }

  EmptyClipboard();
  if (!SetClipboardData(CF_DIBV5, hmem)) {
    CloseClipboard();
    GlobalFree(hmem);
    FillWin32Error(&err, ERR_INTERNAL_ERROR, "Clipboard write failed",
                   GetLastError());
    return Result<void>::Fail(err);
  }

  CloseClipboard();
  return Result<void>::Ok();
}

Result<std::wstring> ExportService::SaveImage(const Artifact& art,
                                              const SaveImageOptions& options) {
  if (options.format != ImageFormat::PNG) {
    Error err;
    err.code = ERR_ENCODE_IMAGE_FAILED;
    err.message = "Unsupported format";
    err.retryable = false;
    err.detail = "format";
    return Result<std::wstring>::Fail(err);
  }
  if (options.path.empty()) {
    Error err;
    err.code = ERR_PATH_NOT_WRITABLE;
    err.message = "Save path not writable";
    err.retryable = false;
    err.detail = "path_empty";
    return Result<std::wstring>::Fail(err);
  }

  CpuBitmap bmp;
  if (TryGetCpuBitmap(art, &bmp) && bmp.format == PixelFormat::BGRA8 &&
      bmp.size_px.w > 0 && bmp.size_px.h > 0) {
    return SavePngFromPixels(bmp.data.p, bmp.size_px.w, bmp.size_px.h,
                             bmp.stride_bytes, options.path);
  }

  // Placeholder: recapture using GDI until GPU frames are wired.
  RectPX rect = art.screen_rect_px;
  if (rect.w <= 0 || rect.h <= 0) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Invalid artifact";
    err.retryable = false;
    err.detail = "artifact_rect_empty";
    return Result<std::wstring>::Fail(err);
  }

  Error err;
  DIBSection dib;
  if (!CaptureRegionToDib(rect, &dib, &err)) {
    return Result<std::wstring>::Fail(err);
  }

  Result<std::wstring> saved = SavePngFromDib(dib, rect, options.path);
  DeleteObject(dib.bitmap);
  return saved;
}

Result<void> ExportService::CopyTextToClipboard(const std::wstring& text) {
  Error err;
  if (!OpenClipboardWithRetry(nullptr, 200, 5, &err)) {
    return Result<void>::Fail(err);
  }

  EmptyClipboard();
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) {
    CloseClipboard();
    Error out;
    out.code = ERR_OUT_OF_MEMORY;
    out.message = "Clipboard alloc failed";
    out.retryable = true;
    out.detail = "GlobalAlloc";
    return Result<void>::Fail(out);
  }
  void* locked = GlobalLock(mem);
  memcpy(locked, text.c_str(), bytes);
  GlobalUnlock(mem);

  if (!SetClipboardData(CF_UNICODETEXT, mem)) {
    CloseClipboard();
    GlobalFree(mem);
    FillWin32Error(&err, ERR_INTERNAL_ERROR, "Clipboard write failed",
                   GetLastError());
    return Result<void>::Fail(err);
  }

  CloseClipboard();
  return Result<void>::Ok();
}

} // namespace snappin
