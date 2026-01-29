#include "ConfigService.h"

#include "ErrorCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>

#include <cctype>
#include <string>
#include <vector>

namespace snappin {
namespace {

bool ReadBoolField(const std::string& json, const std::string& key, bool* out) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  if (json.compare(pos, 4, "true") == 0) {
    *out = true;
    return true;
  }
  if (json.compare(pos, 5, "false") == 0) {
    *out = false;
    return true;
  }
  return false;
}

bool ReadStringField(const std::string& json, const std::string& key, std::string* out) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return false;
  }
  ++pos;
  std::string value;
  bool escape = false;
  for (; pos < json.size(); ++pos) {
    char c = json[pos];
    if (escape) {
      value.push_back(c);
      escape = false;
      continue;
    }
    if (c == '\\') {
      escape = true;
      continue;
    }
    if (c == '"') {
      *out = value;
      return true;
    }
    value.push_back(c);
  }
  return false;
}

bool FindObjectSection(const std::string& json, const std::string& key, size_t* start,
                       size_t* end) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('{', pos + needle.size());
  if (pos == std::string::npos) {
    return false;
  }

  size_t i = pos + 1;
  int depth = 1;
  bool in_string = false;
  bool escape = false;
  for (; i < json.size(); ++i) {
    char c = json[i];
    if (in_string) {
      if (escape) {
        escape = false;
      } else if (c == '\\') {
        escape = true;
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    if (c == '"') {
      in_string = true;
      continue;
    }
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        *start = pos + 1;
        *end = i;
        return true;
      }
    }
  }
  return false;
}

void FillWin32Error(Error* err, const char* code, const char* message, DWORD last_error) {
  if (!err) {
    return;
  }
  err->code = code;
  err->message = message;
  err->retryable = true;
  err->detail = std::to_string(static_cast<unsigned long long>(last_error));
}

bool EnsureDir(const std::wstring& path, Error* err) {
  if (path.empty()) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Invalid config path", ERROR_INVALID_PARAMETER);
    return false;
  }
  if (CreateDirectoryW(path.c_str(), nullptr)) {
    return true;
  }
  DWORD last = GetLastError();
  if (last == ERROR_ALREADY_EXISTS) {
    return true;
  }
  FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Config path not writable", last);
  return false;
}

bool ReadFileToString(const std::wstring& path, std::string* out, Error* err) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to open config", GetLastError());
    return false;
  }

  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to read config", GetLastError());
    CloseHandle(file);
    return false;
  }

  if (size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Config size invalid", ERROR_BAD_LENGTH);
    CloseHandle(file);
    return false;
  }

  std::string buffer;
  buffer.resize(static_cast<size_t>(size.QuadPart));
  DWORD read = 0;
  DWORD expected = static_cast<DWORD>(buffer.size());
  if (!ReadFile(file, buffer.data(), expected, &read, nullptr) || read != expected) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to read config", GetLastError());
    CloseHandle(file);
    return false;
  }

  CloseHandle(file);
  *out = std::move(buffer);
  return true;
}

bool WriteFileAtomic(const std::wstring& path, const std::string& data, Error* err) {
  std::wstring temp_path = path + L".tmp";
  HANDLE file = CreateFileW(temp_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Config path not writable", GetLastError());
    return false;
  }

  DWORD written = 0;
  DWORD expected = static_cast<DWORD>(data.size());
  if (!WriteFile(file, data.data(), expected, &written, nullptr) || written != expected) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to write config", GetLastError());
    CloseHandle(file);
    DeleteFileW(temp_path.c_str());
    return false;
  }

  if (!FlushFileBuffers(file)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to flush config", GetLastError());
    CloseHandle(file);
    DeleteFileW(temp_path.c_str());
    return false;
  }

  CloseHandle(file);
  if (!MoveFileExW(temp_path.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to replace config", GetLastError());
    DeleteFileW(temp_path.c_str());
    return false;
  }

  return true;
}

} // namespace

Result<void> ConfigService::Initialize() {
  root_dir_ = GetRootDir();
  if (root_dir_.empty()) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Failed to resolve config root";
    err.retryable = true;
    err.detail = "root_dir_empty";
    return Result<void>::Fail(err);
  }

  config_dir_ = JoinPath(root_dir_, L"config");
  config_path_ = JoinPath(config_dir_, L"config.json");

  Error err;
  if (!EnsureConfigExists(&err)) {
    return Result<void>::Fail(err);
  }
  if (!Load(&err)) {
    return Result<void>::Fail(err);
  }
  return Result<void>::Ok();
}

Result<void> ConfigService::Reload() {
  if (config_path_.empty()) {
    return Initialize();
  }

  DWORD attrs = GetFileAttributesW(config_path_.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) {
    Error err;
    if (!EnsureConfigExists(&err)) {
      return Result<void>::Fail(err);
    }
  }

  Error err;
  if (!Load(&err)) {
    return Result<void>::Fail(err);
  }
  OutputDebugStringA("Config reloaded\n");
  return Result<void>::Ok();
}

const std::string& ConfigService::RawJson() const { return json_; }

const std::wstring& ConfigService::RootDir() const { return root_dir_; }

const std::wstring& ConfigService::ConfigDir() const { return config_dir_; }

const std::wstring& ConfigService::ConfigPath() const { return config_path_; }

bool ConfigService::CaptureAutoCopyToClipboard(bool default_value) const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "capture", &start, &end)) {
    return default_value;
  }
  std::string section = json_.substr(start, end - start);
  bool enabled = default_value;
  if (ReadBoolField(section, "auto_copy_to_clipboard", &enabled)) {
    return enabled;
  }
  return default_value;
}

bool ConfigService::CaptureAutoShowToolbar(bool default_value) const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "capture", &start, &end)) {
    return default_value;
  }
  std::string section = json_.substr(start, end - start);
  bool enabled = default_value;
  if (ReadBoolField(section, "auto_show_toolbar", &enabled)) {
    return enabled;
  }
  return default_value;
}

std::wstring ConfigService::ExportSaveDir() const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "export", &start, &end)) {
    return L"";
  }
  std::string section = json_.substr(start, end - start);
  std::string value;
  if (!ReadStringField(section, "save_dir", &value)) {
    return L"";
  }
  return std::wstring(value.begin(), value.end());
}

std::string ConfigService::ExportNamingPattern() const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "export", &start, &end)) {
    return "";
  }
  std::string section = json_.substr(start, end - start);
  std::string value;
  if (!ReadStringField(section, "naming_pattern", &value)) {
    return "";
  }
  return value;
}

bool ConfigService::ExportOpenFolderAfterSave(bool default_value) const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "export", &start, &end)) {
    return default_value;
  }
  std::string section = json_.substr(start, end - start);
  bool enabled = default_value;
  if (ReadBoolField(section, "open_folder_after_save", &enabled)) {
    return enabled;
  }
  return default_value;
}

bool ConfigService::DebugEnabled(bool default_value) const {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json_, "debug", &start, &end)) {
    return default_value;
  }
  std::string section = json_.substr(start, end - start);
  bool enabled = default_value;
  if (ReadBoolField(section, "enabled", &enabled)) {
    return enabled;
  }
  return default_value;
}

bool ConfigService::EnsureConfigExists(Error* err) {
  if (!EnsureDir(root_dir_, err)) {
    return false;
  }
  if (!EnsureDir(config_dir_, err)) {
    return false;
  }

  DWORD attrs = GetFileAttributesW(config_path_.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    return true;
  }

  std::string defaults = DefaultConfigJson();
  return WriteFileAtomic(config_path_, defaults, err);
}

bool ConfigService::Load(Error* err) {
  return ReadFileToString(config_path_, &json_, err);
}

std::string ConfigService::DefaultConfigJson() {
  return R"json({
  "config_version": 1,
  "app": {
    "language": "auto",
    "start_on_boot": false,
    "single_instance": true,
    "theme": "system"
  },
  "privacy": {
    "allow_network_features": false,
    "log_redaction_level": "strict",
    "first_time_network_prompt_shown": false
  },
  "hotkeys": {
    "enabled": true,
    "conflict_policy": "warn"
  },
  "capture": {
    "detect_mode_default": "elements",
    "backend_prefer": "auto",
    "include_cursor": false,
    "overlay_min_rect_px": 5,
    "overlay_show_hint": true,
    "multi_monitor_behavior": "current_monitor",
    "auto_copy_to_clipboard": true,
    "auto_show_toolbar": true,
    "copy_priority": "image"
  },
  "export": {
    "default_format": "png",
    "jpeg_quality_0_100": 90,
    "webp_quality_0_100": 90,
    "save_dir": "",
    "naming_pattern": "SnapPin_{yyyyMMdd_HHmmss}_{rand4}",
    "open_folder_after_save": false,
    "clipboard_retry_ms": 200,
    "clipboard_retry_count": 5
  },
  "annotate": {
    "default_tool": "rect",
    "stroke_width": 2.0,
    "stroke_color": "#FF3B30",
    "text_font": "Segoe UI",
    "text_size": 16.0,
    "auto_save_temp": true,
    "confirm_on_close_if_dirty": true
  },
  "pin": {
    "always_on_top_default": true,
    "opacity_step": 0.05,
    "scale_step": 0.05,
    "scale_step_fine": 0.01,
    "min_opacity_0_1": 0.2,
    "max_scale": 5.0,
    "min_scale": 0.1,
    "double_click_action": "none",
    "lock_disables_annotate": true,
    "clipboard_prefer": "image_first",
    "from_clipboard_fail_toast": true
  },
  "text_render": {
    "enabled": true,
    "font_family": "Segoe UI",
    "font_size": 16.0,
    "text_color": "#1E1E1E",
    "bg_color": "#FFFFFF",
    "padding_px": 12,
    "line_spacing": 1.25,
    "max_width_px": 720,
    "max_height_px": 2000,
    "trim_trailing_blank_lines": true,
    "tab_to_spaces": 2,
    "corner_radius_px": 10,
    "shadow_enabled": false
  },
  "ocr": {
    "enabled": true,
    "engine": "system",
    "auto_ocr_on_pin": false,
    "language_hint": "",
    "copy_fulltext_after_recognize": false,
    "selection_mode": "rect",
    "hover_highlight": true
  },
  "scroll": {
    "enabled": true,
    "max_frames": 300,
    "downscale": 0.5,
    "low_fps_hint": 10,
    "match_fail_policy": "prompt",
    "overlap_search_px": 200
  },
  "record": {
    "enabled": true,
    "container_default": "mp4",
    "fps": 30,
    "bitrate_kbps": 8000,
    "countdown_seconds": 3,
    "include_cursor": true,
    "max_queue_frames": 60,
    "drop_policy": "drop_oldest",
    "output_dir": "",
    "filename_pattern": "SnapPinRec_{yyyyMMdd_HHmmss}_{rand4}"
  },
  "history": {
    "enabled": true,
    "max_items": 50,
    "max_total_mb": 500,
    "keep_days": 0,
    "thumb_max_edge_px": 320,
    "thumb_cache_items": 20,
    "auto_cleanup_on_start": true,
    "index_file_name": "index.jsonl"
  },
  "advanced": {
    "lazy_release_seconds": 10,
    "memory_pressure_release": true,
    "max_gpu_staging_mb": 256,
    "max_cpu_bitmap_cache_mb": 128,
    "ipc_channel": "named_pipe"
  },
  "debug": {
    "enabled": false,
    "show_stats_panel": false,
    "log_level": "info",
    "save_frames_for_diagnostics": false
  }
})json";
}

std::wstring ConfigService::GetRootDir() {
  std::wstring exe_dir = GetExeDir();
  std::wstring portable_flag = JoinPath(exe_dir, L"portable.flag");
  DWORD attrs = GetFileAttributesW(portable_flag.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    return JoinPath(exe_dir, L"SnapPinData");
  }

  PWSTR local_app = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &local_app);
  if (FAILED(hr) || !local_app) {
    if (local_app) {
      CoTaskMemFree(local_app);
    }
    return L"";
  }

  std::wstring root = JoinPath(local_app, L"SnapPin");
  CoTaskMemFree(local_app);
  return root;
}

std::wstring ConfigService::GetExeDir() {
  wchar_t buffer[MAX_PATH] = {};
  const DWORD cap = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
  DWORD len = GetModuleFileNameW(nullptr, buffer, cap);
  if (len == 0 || len == cap) {
    return L".";
  }
  std::wstring path(buffer, len);
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return L".";
  }
  return path.substr(0, pos);
}

std::wstring ConfigService::JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) {
    return b;
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return a + b;
  }
  return a + L"\\" + b;
}

} // namespace snappin
