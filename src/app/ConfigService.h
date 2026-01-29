#pragma once
#include "Types.h"

#include <string>

namespace snappin {

class ConfigService {
public:
  ConfigService() = default;

  Result<void> Initialize();
  Result<void> Reload();

  const std::string& RawJson() const;
  const std::wstring& RootDir() const;
  const std::wstring& ConfigDir() const;
  const std::wstring& ConfigPath() const;
  bool CaptureAutoCopyToClipboard(bool default_value = true) const;
  bool CaptureAutoShowToolbar(bool default_value = true) const;
  std::wstring ExportSaveDir() const;
  std::string ExportNamingPattern() const;
  bool ExportOpenFolderAfterSave(bool default_value = false) const;
  bool DebugEnabled(bool default_value = false) const;

private:
  bool EnsureConfigExists(Error* err);
  bool Load(Error* err);

  static std::string DefaultConfigJson();
  static std::wstring GetRootDir();
  static std::wstring GetExeDir();
  static std::wstring JoinPath(const std::wstring& a, const std::wstring& b);

  std::wstring root_dir_;
  std::wstring config_dir_;
  std::wstring config_path_;
  std::string json_;
};

} // namespace snappin
