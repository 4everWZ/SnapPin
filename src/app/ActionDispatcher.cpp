#include "ActionDispatcher.h"

#include "CaptureFreeze.h"
#include "ConfigService.h"
#include "ErrorCodes.h"
#include "OverlayWindow.h"
#include "Artifact.h"
#include "ExportService.h"
#include "ToolbarWindow.h"
#include "SettingsWindow.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include <cctype>

namespace snappin {
namespace {

std::optional<std::string> FindParam(const ActionInvoke& req, const char* key) {
  for (const auto& kv : req.kv) {
    if (kv.first == key) {
      return kv.second;
    }
  }
  return std::nullopt;
}

std::wstring WidenUtf8(const std::string& value) {
  if (value.empty()) {
    return L"";
  }
  int needed =
      MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                          nullptr, 0);
  if (needed <= 0) {
    return L"";
  }
  std::wstring out;
  out.resize(static_cast<size_t>(needed));
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      out.data(), needed);
  return out;
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
  if (a.empty()) {
    return b;
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return a + b;
  }
  return a + L"\\" + b;
}

std::wstring DirName(const std::wstring& path) {
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) {
    return L"";
  }
  return path.substr(0, pos);
}

std::wstring GetDesktopDir() {
  PWSTR desktop = nullptr;
  HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr,
                                    &desktop);
  if (FAILED(hr) || !desktop) {
    if (desktop) {
      CoTaskMemFree(desktop);
    }
    return L"";
  }
  std::wstring out(desktop);
  CoTaskMemFree(desktop);
  return out;
}

std::wstring SanitizeFileName(const std::wstring& name) {
  if (name.empty()) {
    return L"";
  }
  std::wstring out = name;
  for (wchar_t& ch : out) {
    if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
        ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') {
      ch = L'_';
    }
  }
  while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) {
    out.pop_back();
  }
  return out;
}

std::wstring BuildAutoSavePath(const std::wstring& dir, const std::wstring& name) {
  if (dir.empty() || name.empty()) {
    return L"";
  }
  return JoinPath(dir, name + L".png");
}

bool EnsureDir(const std::wstring& path) {
  if (path.empty()) {
    return false;
  }
  int res = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
  if (res == ERROR_SUCCESS || res == ERROR_ALREADY_EXISTS) {
    return true;
  }
  return false;
}

void ReplaceAll(std::string* s, const std::string& from, const std::string& to) {
  if (!s || from.empty()) {
    return;
  }
  size_t pos = 0;
  while ((pos = s->find(from, pos)) != std::string::npos) {
    s->replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string ExpandPattern(const std::string& pattern) {
  SYSTEMTIME st = {};
  GetLocalTime(&st);
  char datetime[32] = {};
  _snprintf_s(datetime, sizeof(datetime), _TRUNCATE, "%04d%02d%02d_%02d%02d%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

  unsigned int tick = static_cast<unsigned int>(GetTickCount());
  char rand4[8] = {};
  _snprintf_s(rand4, sizeof(rand4), _TRUNCATE, "%04X", tick & 0xFFFF);

  std::string out = pattern;
  ReplaceAll(&out, "{yyyyMMdd_HHmmss}", datetime);
  ReplaceAll(&out, "{rand4}", rand4);
  return out;
}

} // namespace

ActionDispatcher::ActionDispatcher(IActionRegistry& registry, RuntimeState* state, HWND hwnd,
                                   ConfigService* config_service, OverlayWindow* overlay,
                                   IArtifactStore* artifacts, IExportService* exporter,
                                   ToolbarWindow* toolbar, SettingsWindow* settings)
    : registry_(registry),
      state_(state),
      hwnd_(hwnd),
      config_service_(config_service),
      overlay_(overlay),
      artifacts_(artifacts),
      exporter_(exporter),
      toolbar_(toolbar),
      settings_(settings) {}

bool ActionDispatcher::IsEnabled(const std::string& action_id, const RuntimeState& state) {
  auto desc = registry_.Find(action_id);
  if (!desc.has_value()) {
    return false;
  }
  return IsContextAllowed(desc.value(), state);
}

Result<Id64> ActionDispatcher::Invoke(const ActionInvoke& req) {
  auto desc = registry_.Find(req.id);
  if (!desc.has_value()) {
    Error err;
    err.code = ERR_INTERNAL_ERROR;
    err.message = "Unknown action";
    err.retryable = false;
    err.detail = req.id;
    return Result<Id64>::Fail(err);
  }

  const RuntimeState state_snapshot = state_ ? *state_ : RuntimeState{};
  if (!IsContextAllowed(desc.value(), state_snapshot)) {
    Error err;
    err.code = ERR_OPERATION_ABORTED;
    err.message = "Action not enabled";
    err.retryable = true;
    err.detail = req.id;
    return Result<Id64>::Fail(err);
  }

  Id64 correlation_id{next_correlation_.fetch_add(1)};

  ActionEvent started{};
  started.action_id = req.id;
  started.correlation_id = correlation_id;
  started.type = ActionEvent::Type::Started;
  EmitEvent(started);

  Result<void> exec = ExecuteAction(req, correlation_id);
  if (!exec.ok) {
    ActionEvent failed{};
    failed.action_id = req.id;
    failed.correlation_id = correlation_id;
    failed.type = ActionEvent::Type::Failed;
    failed.error = exec.error;
    EmitEvent(failed);
    return Result<Id64>::Ok(correlation_id);
  }

  ActionEvent done{};
  done.action_id = req.id;
  done.correlation_id = correlation_id;
  done.type = ActionEvent::Type::Succeeded;
  EmitEvent(done);

  return Result<Id64>::Ok(correlation_id);
}

void ActionDispatcher::Subscribe(std::function<void(const ActionEvent&)> cb) {
  std::lock_guard<std::mutex> lock(subs_mu_);
  subscribers_.push_back(std::move(cb));
}

bool ActionDispatcher::ContextSatisfied(ActionContext ctx, const RuntimeState& state) const {
  switch (ctx) {
    case ActionContext::GLOBAL:
      return true;
    case ActionContext::OVERLAY:
      return state.overlay_visible;
    case ActionContext::ARTIFACT_ACTIVE:
      return state.active_artifact_id.has_value();
    case ActionContext::PIN_FOCUSED:
      return state.focused_pin_id.has_value();
    case ActionContext::SCROLL_SESSION:
      return state.scroll_running;
    case ActionContext::RECORD_SESSION:
      return state.record_running;
    case ActionContext::ANNOTATE_SESSION:
      return state.annotate_running;
    default:
      return false;
  }
}

bool ActionDispatcher::IsContextAllowed(const ActionDescriptor& desc,
                                        const RuntimeState& state) const {
  if (desc.contexts.empty()) {
    return true;
  }
  for (ActionContext ctx : desc.contexts) {
    if (ContextSatisfied(ctx, state)) {
      return true;
    }
  }
  return false;
}

void ActionDispatcher::EmitEvent(const ActionEvent& ev) {
  std::vector<std::function<void(const ActionEvent&)>> subs_copy;
  {
    std::lock_guard<std::mutex> lock(subs_mu_);
    subs_copy = subscribers_;
  }
  for (auto& cb : subs_copy) {
    cb(ev);
  }
}

Result<void> ActionDispatcher::ExecuteAction(const ActionInvoke& req, Id64) {
  if (req.id == "app.exit") {
    if (hwnd_) {
      PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
    return Result<void>::Ok();
  }
  if (req.id == "capture.start") {
    if (!overlay_) {
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Overlay unavailable";
      err.retryable = true;
      err.detail = "overlay_null";
      return Result<void>::Fail(err);
    }
    Result<void> freeze = PrepareFrozenFrameForCursorMonitor();
    if (!freeze.ok) {
      OutputDebugStringA("Capture freeze failed\n");
      ClearFrozenFrame();
    }
    const FrozenFrame* frozen = PeekFrozenFrame();
    if (overlay_) {
      if (frozen && frozen->pixels) {
        overlay_->SetFrozenFrame(frozen->pixels, frozen->size_px,
                                 frozen->stride_bytes);
        overlay_->ShowForRect(frozen->screen_rect_px);
      } else {
        overlay_->ClearFrozenFrame();
        overlay_->ShowForCurrentMonitor();
      }
    }
    if (state_) {
      state_->overlay_visible = overlay_->IsVisible();
    }
    if (!overlay_->IsVisible()) {
      ClearFrozenFrame();
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Overlay show failed";
      err.retryable = true;
      err.detail = "overlay_show_failed";
      return Result<void>::Fail(err);
    }
    return Result<void>::Ok();
  }
  if (req.id == "pin.create_from_clipboard") {
    // Placeholder for pin creation.
    return Result<void>::Ok();
  }
  if (req.id == "export.copy_image") {
    if (!artifacts_ || !exporter_) {
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Export unavailable";
      err.retryable = true;
      err.detail = "export_null";
      return Result<void>::Fail(err);
    }
    std::optional<Id64> active_id = state_ ? state_->active_artifact_id : std::nullopt;
    if (!active_id.has_value()) {
      Error err;
      err.code = ERR_TARGET_INVALID;
      err.message = "No active artifact";
      err.retryable = false;
      err.detail = "no_active_artifact";
      return Result<void>::Fail(err);
    }
    std::optional<Artifact> art = artifacts_->Get(active_id.value());
    if (!art.has_value()) {
      Error err;
      err.code = ERR_TARGET_INVALID;
      err.message = "Artifact missing";
      err.retryable = false;
      err.detail = "artifact_missing";
      return Result<void>::Fail(err);
    }
    return exporter_->CopyImageToClipboard(*art);
  }
  if (req.id == "export.save_image") {
    if (!artifacts_ || !exporter_ || !config_service_) {
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Export unavailable";
      err.retryable = true;
      err.detail = "export_save_null";
      return Result<void>::Fail(err);
    }
    std::optional<Id64> active_id = state_ ? state_->active_artifact_id : std::nullopt;
    if (!active_id.has_value()) {
      Error err;
      err.code = ERR_TARGET_INVALID;
      err.message = "No active artifact";
      err.retryable = false;
      err.detail = "no_active_artifact";
      return Result<void>::Fail(err);
    }
    std::optional<Artifact> art = artifacts_->Get(active_id.value());
    if (!art.has_value()) {
      Error err;
      err.code = ERR_TARGET_INVALID;
      err.message = "Artifact missing";
      err.retryable = false;
      err.detail = "artifact_missing";
      return Result<void>::Fail(err);
    }

    SaveImageOptions options;
    options.format = ImageFormat::PNG;

    std::optional<std::string> format = FindParam(req, "format");
    if (format.has_value()) {
      std::string upper = *format;
      for (char& ch : upper) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      }
      if (upper != "PNG") {
        Error err;
        err.code = ERR_ENCODE_IMAGE_FAILED;
        err.message = "Unsupported format";
        err.retryable = false;
        err.detail = "format";
        return Result<void>::Fail(err);
      }
    }

    std::wstring path;
    std::optional<std::string> path_param = FindParam(req, "path");
    if (path_param.has_value()) {
      path = WidenUtf8(*path_param);
    }
    if (path.empty()) {
      std::wstring dir = config_service_->ExportSaveDir();
      if (dir.empty()) {
        dir = GetDesktopDir();
      }
      if (dir.empty()) {
        dir = JoinPath(config_service_->RootDir(), L"exports");
      }
      if (!EnsureDir(dir)) {
        Error err;
        err.code = ERR_PATH_NOT_WRITABLE;
        err.message = "Save path not writable";
        err.retryable = false;
        err.detail = "export_dir";
        return Result<void>::Fail(err);
      }
      std::string pattern = config_service_->ExportNamingPattern();
      if (pattern.empty()) {
        pattern = "SnapPin_{yyyyMMdd_HHmmss}_{rand4}";
      }
      std::string filename = ExpandPattern(pattern);
      std::wstring safe = SanitizeFileName(WidenUtf8(filename));
      if (safe.empty()) {
        safe = L"SnapPin";
      }
      path = BuildAutoSavePath(dir, safe);
    }
    options.path = path;
    bool auto_path = !path_param.has_value();

    bool open_folder = config_service_->ExportOpenFolderAfterSave(false);
    std::optional<std::string> open_param = FindParam(req, "open_folder");
    if (open_param.has_value()) {
      std::string val = *open_param;
      for (char& ch : val) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      if (val == "true") {
        open_folder = true;
      } else if (val == "false") {
        open_folder = false;
      }
    }
    options.open_folder = open_folder;

    Result<std::wstring> saved = exporter_->SaveImage(*art, options);
    if (!saved.ok && auto_path && saved.error.code == ERR_PATH_NOT_WRITABLE) {
      std::wstring fallback_dir = GetDesktopDir();
      if (fallback_dir.empty()) {
        wchar_t temp_path[MAX_PATH] = {};
        DWORD len = GetTempPathW(MAX_PATH, temp_path);
        if (len > 0 && len < MAX_PATH) {
          fallback_dir.assign(temp_path, temp_path + len);
          if (!fallback_dir.empty() &&
              (fallback_dir.back() == L'\\' || fallback_dir.back() == L'/')) {
            fallback_dir.pop_back();
          }
        }
      }
      if (!fallback_dir.empty()) {
        std::wstring file_name = L"SnapPin";
        std::wstring name_only = path;
        size_t pos = name_only.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
          name_only = name_only.substr(pos + 1);
        }
        if (!name_only.empty()) {
          size_t dot = name_only.find_last_of(L'.');
          if (dot != std::wstring::npos) {
            name_only = name_only.substr(0, dot);
          }
          name_only = SanitizeFileName(name_only);
          if (!name_only.empty()) {
            file_name = name_only;
          }
        }
        options.path = BuildAutoSavePath(fallback_dir, file_name);
        saved = exporter_->SaveImage(*art, options);
      }
    }
    if (!saved.ok) {
      char buffer[256];
      _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                  "save failed code=%s detail=%s\n",
                  saved.error.code.c_str(), saved.error.detail.c_str());
      OutputDebugStringA(buffer);
      return Result<void>::Fail(saved.error);
    }

    if (options.open_folder) {
      std::wstring dir = DirName(saved.value);
      if (!dir.empty()) {
        ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
    }
    return Result<void>::Ok();
  }
  if (req.id == "artifact.dismiss") {
    if (artifacts_) {
      artifacts_->ClearActive();
    }
    if (state_) {
      state_->active_artifact_id.reset();
    }
    if (toolbar_) {
      toolbar_->Hide();
    }
    if (overlay_) {
      overlay_->Hide();
    }
    return Result<void>::Ok();
  }
  if (req.id == "pin.create_from_clipboard") {
    // Placeholder for clipboard pin.
    return Result<void>::Ok();
  }
  if (req.id == "settings.reload") {
    if (!config_service_) {
      Error err;
      err.code = ERR_INTERNAL_ERROR;
      err.message = "Config service unavailable";
      err.retryable = true;
      err.detail = "config_service_null";
      return Result<void>::Fail(err);
    }
    return config_service_->Reload();
  }
  if (req.id == "settings.open") {
    if (settings_) {
      settings_->Show();
    }
    return Result<void>::Ok();
  }

  Error err;
  err.code = ERR_INTERNAL_ERROR;
  err.message = "No handler";
  err.retryable = false;
  err.detail = req.id;
  return Result<void>::Fail(err);
}

} // namespace snappin
