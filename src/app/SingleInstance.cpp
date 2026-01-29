#include "SingleInstance.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>

#include <vector>

namespace snappin {
namespace {

std::wstring GetUserSidString() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    return L"unknown";
  }

  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    CloseHandle(token);
    return L"unknown";
  }

  std::vector<unsigned char> buffer(size);
  if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
    CloseHandle(token);
    return L"unknown";
  }

  const TOKEN_USER* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
  LPWSTR sid_str = nullptr;
  if (!ConvertSidToStringSidW(user->User.Sid, &sid_str)) {
    CloseHandle(token);
    return L"unknown";
  }

  std::wstring sid(sid_str);
  LocalFree(sid_str);
  CloseHandle(token);
  return sid;
}

} // namespace

std::wstring BuildInstanceMutexName() {
  std::wstring sid = GetUserSidString();
  return L"Local\\SnapPin." + sid + L".Mutex";
}

SingleInstanceGuard::SingleInstanceGuard(const std::wstring& name) {
  HANDLE m = CreateMutexW(nullptr, TRUE, name.c_str());
  last_error_ = GetLastError();
  mutex_ = m;
  if (m && last_error_ != ERROR_ALREADY_EXISTS) {
    is_primary_ = true;
  } else {
    is_primary_ = false;
  }
}

SingleInstanceGuard::~SingleInstanceGuard() {
  if (mutex_) {
    if (is_primary_) {
      ReleaseMutex(static_cast<HANDLE>(mutex_));
    }
    CloseHandle(static_cast<HANDLE>(mutex_));
    mutex_ = nullptr;
  }
}

bool SingleInstanceGuard::IsPrimary() const { return is_primary_; }

unsigned long SingleInstanceGuard::LastError() const { return last_error_; }

} // namespace snappin
