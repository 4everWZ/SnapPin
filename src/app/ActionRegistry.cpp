#include "ActionRegistry.h"

namespace snappin {
namespace {

ActionDescriptor MakeAction(const char* id, const char* title, const char* desc,
                            std::vector<ActionContext> contexts,
                            ThreadPolicy policy) {
  ActionDescriptor d;
  d.id = id;
  d.title = title;
  d.description = desc;
  d.contexts = std::move(contexts);
  d.thread_policy = policy;
  return d;
}

} // namespace

ActionRegistry::ActionRegistry() {
  actions_.push_back(MakeAction("app.exit", "Exit", "Exit SnapPin",
                                {ActionContext::GLOBAL}, ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("capture.start", "Capture", "Start capture overlay",
                                {ActionContext::GLOBAL}, ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("pin.create_from_clipboard", "Pin Clipboard",
                                "Create pin from clipboard",
                                {ActionContext::GLOBAL}, ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("export.copy_image", "Copy Image",
                                "Copy active artifact to clipboard",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::BACKGROUND_OK));
  actions_.push_back(MakeAction("export.save_image", "Save Image",
                                "Save active artifact to file",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::BACKGROUND_OK));
  actions_.push_back(MakeAction("pin.create_from_artifact", "Pin",
                                "Create pin from active artifact",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("pin.close_focused", "Close Focused Pin",
                                "Close currently focused pin",
                                {ActionContext::PIN_FOCUSED},
                                ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("pin.close_all", "Close All Pins",
                                "Close all pin windows",
                                {ActionContext::GLOBAL},
                                ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("annotate.open", "Annotate",
                                "Open annotation editor for active artifact",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("ocr.start", "OCR",
                                "Run OCR for active artifact",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::BACKGROUND_OK));
  actions_.push_back(MakeAction("artifact.dismiss", "Close Toolbar",
                                "Dismiss active artifact",
                                {ActionContext::ARTIFACT_ACTIVE},
                                ThreadPolicy::UI_ONLY));
  actions_.push_back(MakeAction("settings.reload", "Reload Settings", "Reload config",
                                {ActionContext::GLOBAL}, ThreadPolicy::BACKGROUND_OK));
  actions_.push_back(MakeAction("settings.open", "Open Settings", "Open settings window",
                                {ActionContext::GLOBAL}, ThreadPolicy::UI_ONLY));
}

std::vector<ActionDescriptor> ActionRegistry::ListAll() { return actions_; }

std::optional<ActionDescriptor> ActionRegistry::Find(const std::string& id) {
  for (const auto& action : actions_) {
    if (action.id == id) {
      return action;
    }
  }
  return std::nullopt;
}

} // namespace snappin
