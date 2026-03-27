#include "ashpaw/engine/net/HandshakeProtocol.hpp"

#include <nlohmann/json.hpp>

namespace ashpaw::engine::net {

std::string BuildTemporaryJoinRequest(const HandshakeRequest& request) {
    return "join:" + request.playerName;
}

JoinRequest ParseTemporaryJoinRequest(std::string_view payload) {
    constexpr std::string_view joinPrefix = "join:";
    if (!payload.starts_with(joinPrefix)) {
        return {
            .decision = JoinRequestDecision::Invalid,
            .playerName = {},
            .detail = "Missing join prefix"
        };
    }

    const auto playerName = std::string(payload.substr(joinPrefix.size()));
    if (playerName.empty()) {
        return {
            .decision = JoinRequestDecision::Invalid,
            .playerName = {},
            .detail = "Player name is required"
        };
    }

    return {
        .decision = JoinRequestDecision::Accepted,
        .playerName = playerName,
        .detail = "Join request parsed"
    };
}

std::string BuildTemporaryHandshakeResponse(const HandshakeResponse& response) {
    if (response.decision == HandshakeDecision::Accepted) {
        if (!response.sessionInit.has_value()) {
            return "join_accepted";
        }

        const nlohmann::json sessionJson {
            {"player_entity_id", response.sessionInit->playerEntityId},
            {"player_name", response.sessionInit->playerName},
            {"map_name", response.sessionInit->mapName},
            {"spawn_x", response.sessionInit->spawnX},
            {"spawn_y", response.sessionInit->spawnY}
        };
        return "join_accepted:" + sessionJson.dump();
    }
    if (response.decision == HandshakeDecision::Rejected) {
        return "join_rejected:" + response.detail;
    }
    return "join_rejected:invalid_handshake";
}

HandshakeResponse ParseTemporaryHandshakeResponse(std::string_view payload) {
    if (payload == "join_accepted") {
        return {
            .decision = HandshakeDecision::Accepted,
            .detail = "Join accepted",
            .sessionInit = std::nullopt
        };
    }

    constexpr std::string_view acceptPrefix = "join_accepted:";
    if (payload.starts_with(acceptPrefix)) {
        const auto body = payload.substr(acceptPrefix.size());
        try {
            const auto json = nlohmann::json::parse(body);
            return {
                .decision = HandshakeDecision::Accepted,
                .detail = "Join accepted",
                .sessionInit = SessionInitData {
                    .playerEntityId = json.value("player_entity_id", 0ULL),
                    .playerName = json.value("player_name", ""),
                    .mapName = json.value("map_name", ""),
                    .spawnX = json.value("spawn_x", 0.0F),
                    .spawnY = json.value("spawn_y", 0.0F)
                }
            };
        } catch (...) {
            return {
                .decision = HandshakeDecision::Invalid,
                .detail = "Malformed accepted handshake payload",
                .sessionInit = std::nullopt
            };
        }
    }

    constexpr std::string_view rejectPrefix = "join_rejected:";
    if (payload.starts_with(rejectPrefix)) {
        return {
            .decision = HandshakeDecision::Rejected,
            .detail = std::string(payload.substr(rejectPrefix.size())),
            .sessionInit = std::nullopt
        };
    }

    return {
        .decision = HandshakeDecision::Invalid,
        .detail = "Unrecognized handshake response",
        .sessionInit = std::nullopt
    };
}

}  // namespace ashpaw::engine::net
