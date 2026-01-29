#pragma once
#include <string>

namespace snappin {

class SingleInstanceGuard {
public:
  explicit SingleInstanceGuard(const std::wstring& name);
  ~SingleInstanceGuard();

  bool IsPrimary() const;
  unsigned long LastError() const;

private:
  void* mutex_ = nullptr;
  bool is_primary_ = false;
  unsigned long last_error_ = 0;
};

std::wstring BuildInstanceMutexName();

} // namespace snappin
