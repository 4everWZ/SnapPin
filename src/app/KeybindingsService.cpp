#include "KeybindingsService.h"

#include "ConfigService.h"
#include "ErrorCodes.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>

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

bool EnsureDir(const std::wstring& path, Error* err) {
  if (path.empty()) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Invalid keybindings path",
                   ERROR_INVALID_PARAMETER);
    return false;
  }
  if (CreateDirectoryW(path.c_str(), nullptr)) {
    return true;
  }
  DWORD last = GetLastError();
  if (last == ERROR_ALREADY_EXISTS) {
    return true;
  }
  FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Keybindings path not writable", last);
  return false;
}

void Trim(std::string* s) {
  if (!s) {
    return;
  }
  size_t start = 0;
  while (start < s->size() && std::isspace(static_cast<unsigned char>((*s)[start]))) {
    ++start;
  }
  size_t end = s->size();
  while (end > start && std::isspace(static_cast<unsigned char>((*s)[end - 1]))) {
    --end;
  }
  *s = s->substr(start, end - start);
}

std::string ToUpper(std::string s) {
  for (char& ch : s) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return s;
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

bool FindArraySection(const std::string& json, const std::string& key, size_t* start,
                      size_t* end) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  pos = json.find('[', pos + needle.size());
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
    if (c == '[') {
      ++depth;
    } else if (c == ']') {
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

std::vector<std::string> Split(const std::string& s, char sep) {
  std::vector<std::string> parts;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      parts.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  parts.push_back(cur);
  return parts;
}

bool ExtractObjects(const std::string& json, size_t start, size_t end,
                    std::vector<std::string>* objects) {
  size_t i = start;
  while (i < end) {
    while (i < end && std::isspace(static_cast<unsigned char>(json[i]))) {
      ++i;
    }
    if (i >= end) {
      break;
    }
    if (json[i] == ',') {
      ++i;
      continue;
    }
    if (json[i] != '{') {
      ++i;
      continue;
    }
    size_t obj_start = i;
    ++i;
    int depth = 1;
    bool in_string = false;
    bool escape = false;
    for (; i < end; ++i) {
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
          size_t obj_end = i + 1;
          objects->push_back(json.substr(obj_start, obj_end - obj_start));
          ++i;
          break;
        }
      }
    }
  }
  return true;
}

} // namespace

Result<void> KeybindingsService::Initialize(const ConfigService& config,
                                            IActionRegistry& registry, HWND hwnd) {
  registry_ = &registry;
  hwnd_ = hwnd;
  config_dir_ = config.ConfigDir();
  keybindings_path_ = JoinPath(config_dir_, L"keybindings.json");

  Error err;
  if (!EnsureKeybindingsExists(&err)) {
    return Result<void>::Fail(err);
  }
  if (!LoadBindings(&err)) {
    return Result<void>::Fail(err);
  }

  bool enabled = true;
  ParseHotkeysEnabled(config.RawJson(), &enabled);
  if (!enabled) {
    OutputDebugStringA("Hotkeys disabled by config\n");
    return Result<void>::Ok();
  }

  ConflictPolicy policy = ParseConflictPolicy(config.RawJson());
  if (!RegisterBindings(policy, &err)) {
    return Result<void>::Fail(err);
  }

  return Result<void>::Ok();
}

void KeybindingsService::Shutdown() {
  for (const auto& entry : hotkey_to_action_) {
    UnregisterHotKey(hwnd_, entry.first);
  }
  hotkey_to_action_.clear();
  bindings_.clear();
  next_hotkey_id_ = 1;
}

std::optional<std::string> KeybindingsService::ActionForHotkeyId(WPARAM hotkey_id) const {
  auto it = hotkey_to_action_.find(static_cast<UINT>(hotkey_id));
  if (it == hotkey_to_action_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool KeybindingsService::EnsureKeybindingsExists(Error* err) {
  if (!EnsureDir(config_dir_, err)) {
    return false;
  }
  DWORD attrs = GetFileAttributesW(keybindings_path_.c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    return true;
  }
  std::string defaults = DefaultKeybindingsJson();
  return WriteFileAtomic(keybindings_path_, defaults, err);
}

bool KeybindingsService::LoadBindings(Error* err) {
  if (!ReadFileToString(keybindings_path_, &json_, err)) {
    return false;
  }
  return ParseBindings(json_, err);
}

bool KeybindingsService::ParseBindings(const std::string& json, Error* err) {
  bindings_.clear();

  size_t start = 0;
  size_t end = 0;
  if (!FindArraySection(json, "bindings", &start, &end)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Invalid keybindings", ERROR_BAD_FORMAT);
    return false;
  }

  std::vector<std::string> objects;
  ExtractObjects(json, start, end, &objects);
  for (const std::string& obj : objects) {
    Binding binding;
    if (!ReadStringField(obj, "id", &binding.id)) {
      continue;
    }
    if (!ReadStringField(obj, "keys", &binding.keys)) {
      continue;
    }
    if (!ReadStringField(obj, "scope", &binding.scope)) {
      binding.scope = "global";
    }
    bool enabled = true;
    ReadBoolField(obj, "enabled", &enabled);
    binding.enabled = enabled;
    bindings_.push_back(binding);
  }
  return true;
}

bool KeybindingsService::RegisterBindings(ConflictPolicy policy, Error* err) {
  std::unordered_map<std::string, size_t> used;

  for (size_t idx = 0; idx < bindings_.size(); ++idx) {
    Binding& binding = bindings_[idx];
    if (!binding.enabled) {
      continue;
    }
    if (!registry_ || !registry_->Find(binding.id).has_value()) {
      continue;
    }
    std::string scope_upper = ToUpper(binding.scope);
    if (scope_upper != "GLOBAL") {
      continue;
    }

    std::string normalized;
    if (!ParseKeyCombo(binding.keys, &binding.modifiers, &binding.vk, &normalized)) {
      binding.runtime_disabled = true;
      continue;
    }
    if (IsReservedGlobal(binding.modifiers, binding.vk)) {
      binding.runtime_disabled = true;
      continue;
    }

    auto it = used.find(normalized);
    if (it != used.end()) {
      if (policy == ConflictPolicy::Override) {
        Binding& prev = bindings_[it->second];
        if (prev.hotkey_id != 0) {
          UnregisterHotKey(hwnd_, prev.hotkey_id);
          hotkey_to_action_.erase(prev.hotkey_id);
        }
        prev.runtime_disabled = true;
        used[normalized] = idx;
      } else {
        binding.runtime_disabled = true;
        continue;
      }
    } else {
      used.emplace(normalized, idx);
    }

    binding.hotkey_id = next_hotkey_id_++;
    if (!RegisterHotKey(hwnd_, binding.hotkey_id, binding.modifiers | MOD_NOREPEAT,
                        binding.vk)) {
      binding.runtime_disabled = true;
      continue;
    }
    hotkey_to_action_[binding.hotkey_id] = binding.id;
  }

  if (hotkey_to_action_.empty()) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "No hotkeys registered", ERROR_NOT_SUPPORTED);
    return false;
  }
  return true;
}

KeybindingsService::ConflictPolicy KeybindingsService::ParseConflictPolicy(
    const std::string& json) {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json, "hotkeys", &start, &end)) {
    return ConflictPolicy::Warn;
  }
  std::string section = json.substr(start, end - start);
  std::string value;
  if (!ReadStringField(section, "conflict_policy", &value)) {
    return ConflictPolicy::Warn;
  }
  value = ToUpper(value);
  if (value == "OVERRIDE") {
    return ConflictPolicy::Override;
  }
  if (value == "IGNORE") {
    return ConflictPolicy::Ignore;
  }
  return ConflictPolicy::Warn;
}

bool KeybindingsService::ParseHotkeysEnabled(const std::string& json, bool* enabled_out) {
  size_t start = 0;
  size_t end = 0;
  if (!FindObjectSection(json, "hotkeys", &start, &end)) {
    return false;
  }
  std::string section = json.substr(start, end - start);
  return ReadBoolField(section, "enabled", enabled_out);
}

std::string KeybindingsService::DefaultKeybindingsJson() {
  return R"json({
  "keybindings_version": 1,
  "bindings": [
    { "id": "capture.start", "keys": "Ctrl+1", "scope": "global" },
    { "id": "pin.create_from_clipboard", "keys": "Ctrl+2", "scope": "global" }
  ]
})json";
}

std::wstring KeybindingsService::JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) {
    return b;
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return a + b;
  }
  return a + L"\\" + b;
}

bool KeybindingsService::ReadFileToString(const std::wstring& path, std::string* out,
                                          Error* err) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to open keybindings",
                   GetLastError());
    return false;
  }

  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to read keybindings",
                   GetLastError());
    CloseHandle(file);
    return false;
  }
  if (size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Keybindings size invalid",
                   ERROR_BAD_LENGTH);
    CloseHandle(file);
    return false;
  }

  std::string buffer;
  buffer.resize(static_cast<size_t>(size.QuadPart));
  DWORD read = 0;
  DWORD expected = static_cast<DWORD>(buffer.size());
  if (!ReadFile(file, buffer.data(), expected, &read, nullptr) || read != expected) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to read keybindings",
                   GetLastError());
    CloseHandle(file);
    return false;
  }
  CloseHandle(file);
  *out = std::move(buffer);
  return true;
}

bool KeybindingsService::WriteFileAtomic(const std::wstring& path,
                                         const std::string& data, Error* err) {
  std::wstring temp_path = path + L".tmp";
  HANDLE file = CreateFileW(temp_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    FillWin32Error(err, ERR_PATH_NOT_WRITABLE, "Keybindings path not writable",
                   GetLastError());
    return false;
  }

  DWORD written = 0;
  DWORD expected = static_cast<DWORD>(data.size());
  if (!WriteFile(file, data.data(), expected, &written, nullptr) || written != expected) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to write keybindings",
                   GetLastError());
    CloseHandle(file);
    DeleteFileW(temp_path.c_str());
    return false;
  }

  if (!FlushFileBuffers(file)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to flush keybindings",
                   GetLastError());
    CloseHandle(file);
    DeleteFileW(temp_path.c_str());
    return false;
  }
  CloseHandle(file);

  if (!MoveFileExW(temp_path.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    FillWin32Error(err, ERR_INTERNAL_ERROR, "Failed to replace keybindings",
                   GetLastError());
    DeleteFileW(temp_path.c_str());
    return false;
  }
  return true;
}

bool KeybindingsService::ParseKeyCombo(const std::string& keys, UINT* modifiers, UINT* vk,
                                       std::string* normalized) {
  std::vector<std::string> tokens = Split(keys, '+');
  UINT mods = 0;
  UINT key = 0;
  std::string key_name;

  for (std::string token : tokens) {
    Trim(&token);
    if (token.empty()) {
      continue;
    }
    std::string upper = ToUpper(token);
    if (upper == "CTRL" || upper == "CONTROL") {
      mods |= MOD_CONTROL;
      continue;
    }
    if (upper == "ALT") {
      mods |= MOD_ALT;
      continue;
    }
    if (upper == "SHIFT") {
      mods |= MOD_SHIFT;
      continue;
    }
    if (upper == "WIN" || upper == "WINDOWS") {
      mods |= MOD_WIN;
      continue;
    }

    if (key != 0) {
      return false;
    }

    if (upper.size() == 1 && std::isalnum(static_cast<unsigned char>(upper[0]))) {
      key = static_cast<UINT>(upper[0]);
      key_name = upper;
      continue;
    }

    if (upper.size() >= 2 && upper[0] == 'F') {
      int num = std::atoi(upper.c_str() + 1);
      if (num >= 1 && num <= 24) {
        key = VK_F1 + (num - 1);
        key_name = "F" + std::to_string(num);
        continue;
      }
    }

    if (upper == "ESC") {
      key = VK_ESCAPE;
      key_name = "Esc";
    } else if (upper == "ENTER") {
      key = VK_RETURN;
      key_name = "Enter";
    } else if (upper == "SPACE") {
      key = VK_SPACE;
      key_name = "Space";
    } else if (upper == "TAB") {
      key = VK_TAB;
      key_name = "Tab";
    } else if (upper == "BACKSPACE") {
      key = VK_BACK;
      key_name = "Backspace";
    } else if (upper == "DELETE") {
      key = VK_DELETE;
      key_name = "Delete";
    } else if (upper == "INSERT") {
      key = VK_INSERT;
      key_name = "Insert";
    } else if (upper == "LEFT") {
      key = VK_LEFT;
      key_name = "Left";
    } else if (upper == "RIGHT") {
      key = VK_RIGHT;
      key_name = "Right";
    } else if (upper == "UP") {
      key = VK_UP;
      key_name = "Up";
    } else if (upper == "DOWN") {
      key = VK_DOWN;
      key_name = "Down";
    } else if (upper == "HOME") {
      key = VK_HOME;
      key_name = "Home";
    } else if (upper == "END") {
      key = VK_END;
      key_name = "End";
    } else if (upper == "PAGEUP") {
      key = VK_PRIOR;
      key_name = "PageUp";
    } else if (upper == "PAGEDOWN") {
      key = VK_NEXT;
      key_name = "PageDown";
    } else {
      return false;
    }
  }

  if (key == 0 || mods == 0) {
    return false;
  }

  std::string norm;
  if (mods & MOD_CONTROL) {
    norm += "Ctrl+";
  }
  if (mods & MOD_ALT) {
    norm += "Alt+";
  }
  if (mods & MOD_SHIFT) {
    norm += "Shift+";
  }
  if (mods & MOD_WIN) {
    norm += "Win+";
  }
  norm += key_name;

  *modifiers = mods;
  *vk = key;
  if (normalized) {
    *normalized = norm;
  }
  return true;
}

bool KeybindingsService::IsReservedGlobal(UINT modifiers, UINT vk) {
  if ((modifiers & MOD_CONTROL) && !(modifiers & (MOD_ALT | MOD_SHIFT | MOD_WIN))) {
    switch (vk) {
      case 'C':
      case 'V':
      case 'X':
      case 'Z':
      case 'Y':
      case 'A':
        return true;
      default:
        break;
    }
  }
  if ((modifiers & MOD_ALT) && !(modifiers & (MOD_CONTROL | MOD_SHIFT | MOD_WIN)) &&
      vk == VK_TAB) {
    return true;
  }
  if ((modifiers & MOD_WIN) && !(modifiers & (MOD_CONTROL | MOD_SHIFT | MOD_ALT))) {
    if (vk == 'L' || vk == 'D' || vk == 'R' || vk == 'E') {
      return true;
    }
  }
  if ((modifiers & MOD_CONTROL) && (modifiers & MOD_ALT) && vk == VK_DELETE) {
    return true;
  }
  return false;
}

} // namespace snappin
