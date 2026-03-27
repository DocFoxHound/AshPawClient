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
};

struct EntityPresentation {
    EntityId id {0};
    math::Vector2 position {};
    math::Vector2 size {32.0F, 32.0F};
    math::Color color {};
    std::string displayName;
    bool authoritative {false};
    RenderLayer renderLayer {RenderLayer::Actors};
    math::Vector2 labelOffset {0.0F, -18.0F};
};

class ClientWorld {
public:
    void SetSceneInfo(const WorldSceneInfo& sceneInfo);
    [[nodiscard]] WorldSceneInfo SceneInfo() const;
    void ClearEntities();
    void UpsertEntity(const EntityPresentation& entity);
    void RemoveEntity(EntityId id);
    [[nodiscard]] std::optional<EntityPresentation> FindEntity(EntityId id) const;
    [[nodiscard]] std::vector<EntityPresentation> SortedEntities() const;
    [[nodiscard]] std::size_t EntityCount() const;

    void SetLocalPlayerId(EntityId id);
    [[nodiscard]] EntityId LocalPlayerId() const;

private:
    WorldSceneInfo sceneInfo_ {};
    std::unordered_map<EntityId, EntityPresentation> entities_;
    EntityId localPlayerId_ {0};
};

}  // namespace ashpaw::engine::world
