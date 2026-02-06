#pragma once
#include "Action.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace snappin {

class ConfigService;
class OverlayWindow;
class IArtifactStore;
class IExportService;
class ToolbarWindow;
class SettingsWindow;
class PinManager;

class ActionDispatcher final : public IActionDispatcher {
public:
  ActionDispatcher(IActionRegistry& registry, RuntimeState* state, HWND hwnd,
                   ConfigService* config_service, OverlayWindow* overlay,
                   IArtifactStore* artifacts, IExportService* exporter,
                   ToolbarWindow* toolbar, SettingsWindow* settings,
                   PinManager* pin_manager);

  bool IsEnabled(const std::string& action_id, const RuntimeState& state) override;
  Result<Id64> Invoke(const ActionInvoke& req) override;
  void Subscribe(std::function<void(const ActionEvent&)>) override;

private:
  bool ContextSatisfied(ActionContext ctx, const RuntimeState& state) const;
  bool IsContextAllowed(const ActionDescriptor& desc, const RuntimeState& state) const;
  void EmitEvent(const ActionEvent& ev);
  Result<void> ExecuteAction(const ActionInvoke& req, Id64 correlation_id);

  IActionRegistry& registry_;
  RuntimeState* state_ = nullptr;
  HWND hwnd_ = nullptr;
  ConfigService* config_service_ = nullptr;
  OverlayWindow* overlay_ = nullptr;
  IArtifactStore* artifacts_ = nullptr;
  IExportService* exporter_ = nullptr;
  ToolbarWindow* toolbar_ = nullptr;
  SettingsWindow* settings_ = nullptr;
  PinManager* pin_manager_ = nullptr;
  std::atomic<uint64_t> next_correlation_{1};
  std::mutex subs_mu_;
  std::vector<std::function<void(const ActionEvent&)>> subscribers_;
};

} // namespace snappin
