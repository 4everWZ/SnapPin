#pragma once
#include "Types.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace snappin {

enum class ActionContext {
  GLOBAL,
  OVERLAY,
  ARTIFACT_ACTIVE,
  PIN_FOCUSED,
  SCROLL_SESSION,
  RECORD_SESSION,
  ANNOTATE_SESSION,
};

struct RuntimeState {
  bool overlay_visible = false;
  std::optional<Id64> active_artifact_id;
  std::optional<Id64> focused_pin_id;
  bool scroll_running = false;
  bool record_running = false;
  bool annotate_running = false;
};

struct ActionParamDef {
  std::string name;
  std::string type;
  std::string default_value;
  bool required = false;
};

struct ActionDescriptor {
  std::string id;
  std::string title;
  std::string description;
  std::vector<ActionContext> contexts;
  ThreadPolicy thread_policy = ThreadPolicy::ANY;
  std::vector<ActionParamDef> params;
};

struct ActionInvoke {
  std::string id;
  std::vector<std::pair<std::string, std::string>> kv;
};

struct ActionEvent {
  std::string action_id;
  Id64 correlation_id;
  enum class Type { Started, Progress, Succeeded, Failed } type;
  float progress_0_1 = 0.0f;
  std::string message;
  std::string output_ref;
  std::optional<Error> error;
};

class IActionRegistry {
public:
  virtual ~IActionRegistry() = default;
  virtual std::vector<ActionDescriptor> ListAll() = 0;
  virtual std::optional<ActionDescriptor> Find(const std::string& id) = 0;
};

class IActionDispatcher {
public:
  virtual ~IActionDispatcher() = default;
  virtual bool IsEnabled(const std::string& action_id, const RuntimeState& state) = 0;
  virtual Result<Id64> Invoke(const ActionInvoke& req) = 0;
  virtual void Subscribe(std::function<void(const ActionEvent&)>) = 0;
};

} // namespace snappin
