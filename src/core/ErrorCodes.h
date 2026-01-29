#pragma once

namespace snappin {

// Common
inline constexpr const char* ERR_UNSUPPORTED_OS = "UNSUPPORTED_OS";
inline constexpr const char* ERR_PERMISSION_DENIED = "PERMISSION_DENIED";
inline constexpr const char* ERR_TARGET_INVALID = "TARGET_INVALID";
inline constexpr const char* ERR_DEVICE_LOST = "DEVICE_LOST";
inline constexpr const char* ERR_OUT_OF_MEMORY = "OUT_OF_MEMORY";
inline constexpr const char* ERR_OPERATION_ABORTED = "OPERATION_ABORTED";
inline constexpr const char* ERR_INTERNAL_ERROR = "INTERNAL_ERROR";

// Capture
inline constexpr const char* ERR_CAPTURE_BACKEND_UNAVAILABLE = "CAPTURE_BACKEND_UNAVAILABLE";
inline constexpr const char* ERR_CAPTURE_FAILED = "CAPTURE_FAILED";

// Clipboard / Export
inline constexpr const char* ERR_CLIPBOARD_BUSY = "CLIPBOARD_BUSY";
inline constexpr const char* ERR_CLIPBOARD_EMPTY = "CLIPBOARD_EMPTY";
inline constexpr const char* ERR_DISK_FULL = "DISK_FULL";
inline constexpr const char* ERR_PATH_NOT_WRITABLE = "PATH_NOT_WRITABLE";
inline constexpr const char* ERR_ENCODE_IMAGE_FAILED = "ENCODE_IMAGE_FAILED";

} // namespace snappin
