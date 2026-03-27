#include "ashpaw/engine/net/TemporaryProtocol.hpp"

#include <nlohmann/json.hpp>

namespace ashpaw::engine::net {

namespace {

const char* ToString(EntityKind kind) {
    switch (kind) {
    case EntityKind::Player:
        return "player";
    case EntityKind::Npc:
        return "npc";
    }
    return "player";
}

EntityKind ParseEntityKind(const std::string& kind) {
    if (kind == "npc") {
        return EntityKind::Npc;
    }
    return EntityKind::Player;
}

nlohmann::json ToEntityJson(const ReplicatedEntityState& entity) {
    return {
        {"entity_id", entity.entityId},
        {"display_name", entity.displayName},
        {"x", entity.x},
        {"y", entity.y},
        {"width", entity.width},
        {"height", entity.height},
        {"local_controlled", entity.localControlled},
        {"kind", ToString(entity.kind)}
    };
}

std::optional<ReplicatedEntityState> ParseEntityJson(std::string_view body) {
    try {
        const auto json = nlohmann::json::parse(body);
        return ReplicatedEntityState {
            .entityId = json.value("entity_id", 0ULL),
            .displayName = json.value("display_name", ""),
            .x = json.value("x", 0.0F),
            .y = json.value("y", 0.0F),
            .width = json.value("width", 32.0F),
            .height = json.value("height", 32.0F),
            .localControlled = json.value("local_controlled", false),
            .kind = ParseEntityKind(json.value("kind", "player"))
        };
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

std::string BuildMovementIntentMessage(const MovementIntent& intent) {
    const nlohmann::json json {
        {"x", intent.x},
        {"y", intent.y}
    };
    return "input:" + json.dump();
}

std::optional<MovementIntent> ParseMovementIntentMessage(std::string_view payload) {
    constexpr std::string_view prefix = "input:";
    if (!payload.starts_with(prefix)) {
        return std::nullopt;
    }
    try {
        const auto json = nlohmann::json::parse(payload.substr(prefix.size()));
        return MovementIntent {
            .x = json.value("x", 0.0F),
            .y = json.value("y", 0.0F)
        };
    } catch (...) {
        return std::nullopt;
    }
}

std::string BuildSpawnMessage(const ReplicatedEntityState& entity) {
    return "spawn:" + ToEntityJson(entity).dump();
}

std::string BuildSnapshotMessage(const ReplicatedEntityState& entity) {
    return "snapshot:" + ToEntityJson(entity).dump();
}

std::string BuildDespawnMessage(std::uint64_t entityId) {
    const nlohmann::json json {{"entity_id", entityId}};
    return "despawn:" + json.dump();
}

ServerMessage ParseServerMessage(std::string_view payload) {
    constexpr std::string_view spawnPrefix = "spawn:";
    constexpr std::string_view snapshotPrefix = "snapshot:";
    constexpr std::string_view despawnPrefix = "despawn:";

    if (payload.starts_with(spawnPrefix)) {
        if (const auto entity = ParseEntityJson(payload.substr(spawnPrefix.size())); entity.has_value()) {
            return {
                .kind = ServerMessageKind::Spawn,
                .entity = entity,
                .entityId = 0,
                .detail = {}
            };
        }
        return {
            .kind = ServerMessageKind::Invalid,
            .entity = std::nullopt,
            .entityId = 0,
            .detail = "Malformed spawn payload"
        };
    }

    if (payload.starts_with(snapshotPrefix)) {
        if (const auto entity = ParseEntityJson(payload.substr(snapshotPrefix.size())); entity.has_value()) {
            return {
                .kind = ServerMessageKind::Snapshot,
                .entity = entity,
                .entityId = 0,
                .detail = {}
            };
        }
        return {
            .kind = ServerMessageKind::Invalid,
            .entity = std::nullopt,
            .entityId = 0,
            .detail = "Malformed snapshot payload"
        };
    }

    if (payload.starts_with(despawnPrefix)) {
        try {
            const auto json = nlohmann::json::parse(payload.substr(despawnPrefix.size()));
            return {
                .kind = ServerMessageKind::Despawn,
                .entity = std::nullopt,
                .entityId = json.value("entity_id", 0ULL),
                .detail = {}
            };
        } catch (...) {
            return {
                .kind = ServerMessageKind::Invalid,
                .entity = std::nullopt,
                .entityId = 0,
                .detail = "Malformed despawn payload"
            };
        }
    }

    return {
        .kind = ServerMessageKind::Invalid,
        .entity = std::nullopt,
        .entityId = 0,
        .detail = "Unknown server message"
    };
}

}  // namespace ashpaw::engine::net
