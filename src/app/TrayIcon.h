#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace snappin {

constexpr int kTrayMenuCaptureId = 1000;
constexpr int kTrayMenuSettingsId = 1002;
constexpr int kTrayMenuExitId = 1001;

class TrayIcon {
public:
  TrayIcon() = default;
  ~TrayIcon();

  bool Init(HWND hwnd, UINT callback_message, UINT icon_id);
  void Cleanup();
  void ShowContextMenu(const POINT& pt);
  void OnTaskbarCreated();

private:
  bool AddIcon();
  void RemoveIcon();

  HWND hwnd_ = nullptr;
  UINT callback_message_ = 0;
  UINT icon_id_ = 0;
  bool visible_ = false;
  NOTIFYICONDATAW nid_ = {};
};

} // namespace snappin
