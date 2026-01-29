#pragma once
#include "Action.h"

#include <vector>

namespace snappin {

class ActionRegistry final : public IActionRegistry {
public:
  ActionRegistry();

  std::vector<ActionDescriptor> ListAll() override;
  std::optional<ActionDescriptor> Find(const std::string& id) override;

private:
  std::vector<ActionDescriptor> actions_;
};

} // namespace snappin
