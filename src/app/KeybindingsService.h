#pragma once
#include "Action.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace snappin {

class ConfigService;

class KeybindingsService {
public:
  KeybindingsService() = default;

  Result<void> Initialize(const ConfigService& config, IActionRegistry& registry, HWND hwnd);
  void Shutdown();

  std::optional<std::string> ActionForHotkeyId(WPARAM hotkey_id) const;

private:
  struct Binding {
    std::string id;
    std::string keys;
    std::string scope;
    bool enabled = true;
    bool runtime_disabled = false;
    UINT hotkey_id = 0;
    UINT modifiers = 0;
    UINT vk = 0;
  };

  enum class ConflictPolicy { Warn, Override, Ignore };

  bool EnsureKeybindingsExists(Error* err);
  bool LoadBindings(Error* err);
  bool ParseBindings(const std::string& json, Error* err);
  bool RegisterBindings(ConflictPolicy policy, Error* err);

  static ConflictPolicy ParseConflictPolicy(const std::string& json);
  static bool ParseHotkeysEnabled(const std::string& json, bool* enabled_out);
  static std::string DefaultKeybindingsJson();
  static std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
  static bool ReadFileToString(const std::wstring& path, std::string* out, Error* err);
  static bool WriteFileAtomic(const std::wstring& path, const std::string& data, Error* err);
  static bool ParseKeyCombo(const std::string& keys, UINT* modifiers, UINT* vk,
                            std::string* normalized);
  static bool IsReservedGlobal(UINT modifiers, UINT vk);

  std::wstring config_dir_;
  std::wstring keybindings_path_;
  std::string json_;

  IActionRegistry* registry_ = nullptr;
  HWND hwnd_ = nullptr;
  UINT next_hotkey_id_ = 1;
  std::vector<Binding> bindings_;
  std::unordered_map<UINT, std::string> hotkey_to_action_;
};

} // namespace snappin
