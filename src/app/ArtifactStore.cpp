#include "ArtifactStore.h"

namespace snappin {

std::optional<Artifact> ArtifactStore::Get(Id64 id) {
  auto it = items_.find(id.value);
  if (it == items_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void ArtifactStore::Put(const Artifact& artifact) {
  items_[artifact.artifact_id.value] = artifact;
  active_id_ = artifact.artifact_id;
}

void ArtifactStore::ClearActive() { active_id_.reset(); }

std::optional<Id64> ArtifactStore::ActiveId() const { return active_id_; }

Id64 ArtifactStore::NextId() {
  Id64 id{next_id_++};
  return id;
}

} // namespace snappin
