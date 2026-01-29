#include "CaptureService.h"

#include "ErrorCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>
#include <string>

namespace snappin {
namespace {

enum class BackendKind { WGC, DXGI, GDI };

Result<CaptureFrame> MakeBackendUnavailable(const char* detail) {
  Error err;
  err.code = ERR_CAPTURE_BACKEND_UNAVAILABLE;
  err.message = "Capture backend unavailable";
  err.retryable = true;
  err.detail = detail;
  return Result<CaptureFrame>::Fail(err);
}

Result<CaptureFrame> CaptureGdi(const CaptureTarget& target) {
  if (target.type != CaptureTargetType::REGION || !target.region_px.has_value()) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Invalid capture target";
    err.retryable = true;
    err.detail = "target_not_region";
    return Result<CaptureFrame>::Fail(err);
  }

  RectPX rect = target.region_px.value();
  if (rect.w <= 0 || rect.h <= 0) {
    Error err;
    err.code = ERR_TARGET_INVALID;
    err.message = "Invalid capture size";
    err.retryable = true;
    err.detail = "rect_empty";
    return Result<CaptureFrame>::Fail(err);
  }

  HDC screen = GetDC(nullptr);
  if (!screen) {
    Error err;
    err.code = ERR_CAPTURE_FAILED;
    err.message = "Capture failed";
    err.retryable = true;
    err.detail = "GetDC";
    return Result<CaptureFrame>::Fail(err);
  }

  HDC mem = CreateCompatibleDC(screen);
  if (!mem) {
    ReleaseDC(nullptr, screen);
    Error err;
    err.code = ERR_CAPTURE_FAILED;
    err.message = "Capture failed";
    err.retryable = true;
    err.detail = "CreateCompatibleDC";
    return Result<CaptureFrame>::Fail(err);
  }

  HBITMAP bmp = CreateCompatibleBitmap(screen, rect.w, rect.h);
  if (!bmp) {
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    Error err;
    err.code = ERR_CAPTURE_FAILED;
    err.message = "Capture failed";
    err.retryable = true;
    err.detail = "CreateCompatibleBitmap";
    return Result<CaptureFrame>::Fail(err);
  }

  HGDIOBJ old = SelectObject(mem, bmp);
  BOOL ok = BitBlt(mem, 0, 0, rect.w, rect.h, screen, rect.x, rect.y,
                   SRCCOPY | CAPTUREBLT);
  SelectObject(mem, old);

  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (!ok) {
    Error err;
    err.code = ERR_CAPTURE_FAILED;
    err.message = "Capture failed";
    err.retryable = true;
    err.detail = "BitBlt";
    return Result<CaptureFrame>::Fail(err);
  }

  CaptureFrame frame;
  frame.size_px = SizePX{rect.w, rect.h};
  frame.screen_rect_px = rect;
  frame.timestamp = TimeStamp{GetTickCount64()};
  frame.dpi_scale = 1.0f;
  return Result<CaptureFrame>::Ok(frame);
}

Result<CaptureFrame> CaptureWgc(const CaptureTarget&) {
#if defined(SNAPPIN_ENABLE_WGC)
  return MakeBackendUnavailable("wgc_not_implemented");
#else
  return MakeBackendUnavailable("wgc_disabled");
#endif
}

Result<CaptureFrame> CaptureDxgi(const CaptureTarget&) {
#if defined(SNAPPIN_ENABLE_DXGI_DUP)
  return MakeBackendUnavailable("dxgi_not_implemented");
#else
  return MakeBackendUnavailable("dxgi_disabled");
#endif
}

class CaptureServiceImpl final : public ICaptureService {
public:
  Result<CaptureFrame> CaptureOnce(const CaptureTarget& target,
                                   const CaptureOptions& options) override {
    if (options.prefer_backend == CaptureBackend::WGC) {
      return CaptureWgc(target);
    }
    if (options.prefer_backend == CaptureBackend::DXGI) {
      return CaptureDxgi(target);
    }

    Result<CaptureFrame> err = MakeBackendUnavailable("auto_start");
    Result<CaptureFrame> res = CaptureWgc(target);
    if (res.ok) {
      return res;
    }
    err = res;

    res = CaptureDxgi(target);
    if (res.ok) {
      return res;
    }
    err = res;

    res = CaptureGdi(target);
    if (res.ok) {
      return res;
    }
    err = res;
    return err;
  }

  Result<StreamId> StartFrameStream(const CaptureTarget&, const CaptureOptions&, int32_t,
                                    std::function<void(const CaptureFrame&)>) override {
    Error err;
    err.code = ERR_CAPTURE_FAILED;
    err.message = "Stream not supported";
    err.retryable = false;
    err.detail = "stream_not_supported";
    return Result<StreamId>::Fail(err);
  }

  void StopFrameStream(StreamId) override {}

  FrameStreamStats GetStreamStats(StreamId) override { return {}; }
};

} // namespace

std::unique_ptr<ICaptureService> CreateCaptureService() {
  return std::make_unique<CaptureServiceImpl>();
}

} // namespace snappin
