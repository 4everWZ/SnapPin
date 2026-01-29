#include "TrayIcon.h"

#include <shellapi.h>

namespace snappin {
namespace {
const wchar_t kTooltipText[] = L"SnapPin";
}

TrayIcon::~TrayIcon() { Cleanup(); }

bool TrayIcon::Init(HWND hwnd, UINT callback_message, UINT icon_id) {
  hwnd_ = hwnd;
  callback_message_ = callback_message;
  icon_id_ = icon_id;

  ZeroMemory(&nid_, sizeof(nid_));
  nid_.cbSize = sizeof(nid_);
  nid_.hWnd = hwnd_;
  nid_.uID = icon_id_;
  nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid_.uCallbackMessage = callback_message_;
  nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  lstrcpynW(nid_.szTip, kTooltipText, ARRAYSIZE(nid_.szTip));

  return AddIcon();
}

void TrayIcon::Cleanup() { RemoveIcon(); }

void TrayIcon::OnTaskbarCreated() {
  if (hwnd_) {
    AddIcon();
  }
}

void TrayIcon::ShowContextMenu(const POINT& pt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }

  AppendMenuW(menu, MF_STRING, kTrayMenuCaptureId, L"Capture");
  AppendMenuW(menu, MF_STRING, kTrayMenuSettingsId, L"Settings");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayMenuExitId, L"Exit");

  SetForegroundWindow(hwnd_);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                 pt.x, pt.y, 0, hwnd_, nullptr);
  PostMessageW(hwnd_, WM_NULL, 0, 0);

  DestroyMenu(menu);
}

bool TrayIcon::AddIcon() {
  if (!hwnd_) {
    return false;
  }

  if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
    visible_ = false;
    return false;
  }

  nid_.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &nid_);
  visible_ = true;
  return true;
}

void TrayIcon::RemoveIcon() {
  if (!visible_) { 
    return;
  }
  Shell_NotifyIconW(NIM_DELETE, &nid_);
  visible_ = false;
}

} // namespace snappin
