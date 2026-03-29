#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ashpaw/engine/math/Vector2.hpp"

namespace ashpaw::engine::world {

using EntityId = std::uint64_t;

enum class RenderLayer {
    Grounded = 0,
    Actors = 1,
    Overlay = 2
};

struct WorldSceneInfo {
    std::string mapName;
    math::Vector2 worldSize {};
    std::int32_t activeZ {0};
};

struct EntityPresentation {
    EntityId id {0};
    math::Vector2 position {};
    std::int32_t z {0};
    math::Vector2 size {32.0F, 32.0F};
    math::Color color {};
    std::string displayName;
    bool authoritative {false};
    RenderLayer renderLayer {RenderLayer::Actors};
    math::Vector2 labelOffset {0.0F, -18.0F};
};

struct IdentityRecord {
    EntityId entityId {0};
    std::string displayName;
};

struct InteractableRecord {
    std::string targetId;
    bool isOpen {false};
    EntityId occupantEntityId {0};
};

class ClientWorld {
public:
    void SetSceneInfo(const WorldSceneInfo& sceneInfo);
    [[nodiscard]] WorldSceneInfo SceneInfo() const;
    void ClearEntities();
    void ClearAuthorityState();
    void UpsertEntity(const EntityPresentation& entity);
    void RemoveEntity(EntityId id);
    [[nodiscard]] std::optional<EntityPresentation> FindEntity(EntityId id) const;
    [[nodiscard]] std::vector<EntityPresentation> SortedEntities() const;
    [[nodiscard]] std::size_t EntityCount() const;
    void UpsertIdentity(const IdentityRecord& identity);
    void RemoveIdentity(EntityId id);
    [[nodiscard]] std::optional<IdentityRecord> FindIdentity(EntityId id) const;
    void UpsertInteractable(const InteractableRecord& interactable);
    [[nodiscard]] std::optional<InteractableRecord> FindInteractable(std::string_view targetId) const;
    [[nodiscard]] std::vector<InteractableRecord> Interactables() const;

    void SetLocalPlayerId(EntityId id);
    [[nodiscard]] EntityId LocalPlayerId() const;

private:
    WorldSceneInfo sceneInfo_ {};
    std::unordered_map<EntityId, EntityPresentation> entities_;
    std::unordered_map<EntityId, IdentityRecord> identities_;
    std::unordered_map<std::string, InteractableRecord> interactables_;
    EntityId localPlayerId_ {0};
};

}  // namespace ashpaw::engine::world
