#include "ashpaw/engine/world/ClientWorld.hpp"

#include <algorithm>

namespace ashpaw::engine::world {

void ClientWorld::SetSceneInfo(const WorldSceneInfo& sceneInfo) {
    sceneInfo_ = sceneInfo;
}

WorldSceneInfo ClientWorld::SceneInfo() const {
    return sceneInfo_;
}

void ClientWorld::ClearEntities() {
    entities_.clear();
}

void ClientWorld::ClearAuthorityState() {
    identities_.clear();
    interactables_.clear();
}

void ClientWorld::UpsertEntity(const EntityPresentation& entity) {
    entities_[entity.id] = entity;
}

void ClientWorld::RemoveEntity(EntityId id) {
    entities_.erase(id);
}

std::optional<EntityPresentation> ClientWorld::FindEntity(EntityId id) const {
    const auto iterator = entities_.find(id);
    if (iterator == entities_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<EntityPresentation> ClientWorld::SortedEntities() const {
    std::vector<EntityPresentation> entities;
    entities.reserve(entities_.size());
    for (const auto& [id, entity] : entities_) {
        static_cast<void>(id);
        entities.push_back(entity);
    }
    std::sort(entities.begin(), entities.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.z != rhs.z) {
            return lhs.z < rhs.z;
        }
        if (lhs.renderLayer != rhs.renderLayer) {
            return static_cast<int>(lhs.renderLayer) < static_cast<int>(rhs.renderLayer);
        }
        const auto lhsBottom = lhs.position.y + lhs.size.y;
        const auto rhsBottom = rhs.position.y + rhs.size.y;
        if (lhsBottom == rhsBottom) {
            return lhs.id < rhs.id;
        }
        return lhsBottom < rhsBottom;
    });
    return entities;
}

std::size_t ClientWorld::EntityCount() const {
    return entities_.size();
}

void ClientWorld::UpsertIdentity(const IdentityRecord& identity) {
    identities_[identity.entityId] = identity;
}

void ClientWorld::RemoveIdentity(EntityId id) {
    identities_.erase(id);
}

std::optional<IdentityRecord> ClientWorld::FindIdentity(EntityId id) const {
    const auto iterator = identities_.find(id);
    if (iterator == identities_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

void ClientWorld::UpsertInteractable(const InteractableRecord& interactable) {
    interactables_[interactable.targetId] = interactable;
}

std::optional<InteractableRecord> ClientWorld::FindInteractable(std::string_view targetId) const {
    const auto iterator = interactables_.find(std::string(targetId));
    if (iterator == interactables_.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::vector<InteractableRecord> ClientWorld::Interactables() const {
    std::vector<InteractableRecord> interactables;
    interactables.reserve(interactables_.size());
    for (const auto& [targetId, interactable] : interactables_) {
        static_cast<void>(targetId);
        interactables.push_back(interactable);
    }
    return interactables;
}

void ClientWorld::SetLocalPlayerId(EntityId id) {
    localPlayerId_ = id;
}

EntityId ClientWorld::LocalPlayerId() const {
    return localPlayerId_;
}

}  // namespace ashpaw::engine::world
