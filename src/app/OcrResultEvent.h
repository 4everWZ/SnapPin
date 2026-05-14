#pragma once
#include "Action.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <optional>
#include <string>

namespace snappin {

inline std::string WideToUtf8(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  const int required = WideCharToMultiByte(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
      nullptr, nullptr);
  if (required <= 0) {
    return {};
  }
  std::string out(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      out.data(), required, nullptr, nullptr);
  return out;
}

inline std::optional<std::wstring> Utf8ToWide(const std::string& text) {
  if (text.empty()) {
    return std::wstring{};
  }
  const int required = MultiByteToWideChar(
      CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
      nullptr, 0);
  if (required <= 0) {
    return std::nullopt;
  }
  std::wstring out(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                      static_cast<int>(text.size()), out.data(), required);
  return out;
}

inline ActionEvent MakeOcrTextProgressEvent(Id64 correlation_id,
                                            const std::wstring& text) {
  ActionEvent event;
  event.action_id = "ocr.start";
  event.correlation_id = correlation_id;
  event.type = ActionEvent::Type::Progress;
  event.message = "ocr.text";
  event.output_ref = WideToUtf8(text);
  return event;
}

inline std::optional<std::wstring> OcrTextFromProgressEvent(
    const ActionEvent& event) {
  if (event.action_id != "ocr.start" ||
      event.type != ActionEvent::Type::Progress ||
      event.message != "ocr.text") {
    return std::nullopt;
  }
  return Utf8ToWide(event.output_ref);
}

} // namespace snappin
