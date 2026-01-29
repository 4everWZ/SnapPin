#pragma once
#include "Artifact.h"

#include <unordered_map>

namespace snappin {

class ArtifactStore final : public IArtifactStore {
public:
  ArtifactStore() = default;

  std::optional<Artifact> Get(Id64 id) override;
  void Put(const Artifact& artifact) override;
  void ClearActive() override;

  std::optional<Id64> ActiveId() const;
  Id64 NextId();

private:
  std::unordered_map<uint64_t, Artifact> items_;
  std::optional<Id64> active_id_;
  uint64_t next_id_ = 1;
};

} // namespace snappin
