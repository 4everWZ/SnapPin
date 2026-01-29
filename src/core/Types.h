#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace snappin {

// ---------- IDs ----------
struct Id64 { uint64_t value = 0; };

// ---------- Geometry (Screen PX) ----------
struct PointPX { int32_t x = 0; int32_t y = 0; };
struct SizePX  { int32_t w = 0; int32_t h = 0; };
struct RectPX  { int32_t x = 0; int32_t y = 0; int32_t w = 0; int32_t h = 0; };

// ---------- Time ----------
struct TimeStamp { uint64_t mono_ms = 0; };

// ---------- Color / Style ----------
struct ColorRGBA { uint8_t r = 0; uint8_t g = 0; uint8_t b = 0; uint8_t a = 255; };

struct StrokeStyle {
  float width = 2.0f;
  ColorRGBA color{};
  float opacity = 1.0f;
};

struct FillStyle {
  bool enabled = false;
  ColorRGBA color{};
  float opacity = 0.0f;
};

struct TextStyle {
  std::wstring font_family = L"Segoe UI";
  float size = 16.0f;
  ColorRGBA color{};
  float opacity = 1.0f;
  bool bold = false;
};

// ---------- Error / Result ----------
struct Error {
  std::string code;
  std::string message;
  bool retryable = false;
  std::string detail;
};

template <class T>
struct Result {
  bool ok = false;
  T value{};
  Error error{};

  static Result<T> Ok(T v) {
    Result<T> r;
    r.ok = true;
    r.value = std::move(v);
    return r;
  }

  static Result<T> Fail(Error e) {
    Result<T> r;
    r.ok = false;
    r.error = std::move(e);
    return r;
  }
};

template <>
struct Result<void> {
  bool ok = false;
  Error error{};

  static Result<void> Ok() {
    Result<void> r;
    r.ok = true;
    return r;
  }

  static Result<void> Fail(Error e) {
    Result<void> r;
    r.ok = false;
    r.error = std::move(e);
    return r;
  }
};

// ---------- Thread policy (for Actions) ----------
enum class ThreadPolicy { UI_ONLY, ANY, BACKGROUND_OK };

// ---------- Opaque handles ----------
struct GpuFrameHandle { uint64_t h = 0; };
struct CpuDataRef { void* p = nullptr; };

// ---------- Bitmap ----------
enum class PixelFormat { RGBA8 };

struct CpuBitmap {
  PixelFormat format = PixelFormat::RGBA8;
  SizePX size_px{};
  int32_t stride_bytes = 0;
  CpuDataRef data{};
};

} // namespace snappin
