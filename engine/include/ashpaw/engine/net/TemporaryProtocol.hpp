#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ashpaw::engine::net {

enum class EntityKind {
    Player,
    Npc
};

struct ReplicatedEntityState {
    std::uint64_t entityId {0};
    std::string displayName;
    float x {0.0F};
    float y {0.0F};
    float width {32.0F};
    float height {32.0F};
    bool localControlled {false};
    EntityKind kind {EntityKind::Player};
};

struct MovementIntent {
    float x {0.0F};
    float y {0.0F};
};

enum class ServerMessageKind {
    Invalid,
    Spawn,
    Snapshot,
    Despawn
};

struct ServerMessage {
    ServerMessageKind kind {ServerMessageKind::Invalid};
    std::optional<ReplicatedEntityState> entity;
    std::uint64_t entityId {0};
    std::string detail;
};

std::string BuildMovementIntentMessage(const MovementIntent& intent);
std::optional<MovementIntent> ParseMovementIntentMessage(std::string_view payload);

std::string BuildSpawnMessage(const ReplicatedEntityState& entity);
std::string BuildSnapshotMessage(const ReplicatedEntityState& entity);
std::string BuildDespawnMessage(std::uint64_t entityId);
ServerMessage ParseServerMessage(std::string_view payload);

}  // namespace ashpaw::engine::net
