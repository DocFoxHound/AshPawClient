#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ashpaw::engine::net {

struct HandshakeRequest {
    std::string playerName;
};

enum class JoinRequestDecision {
    Accepted,
    Invalid
};

struct JoinRequest {
    JoinRequestDecision decision {JoinRequestDecision::Invalid};
    std::string playerName;
    std::string detail;
};

enum class HandshakeDecision {
    Accepted,
    Rejected,
    Invalid
};

struct SessionInitData {
    std::uint64_t playerEntityId {0};
    std::string playerName;
    std::string mapName;
    float spawnX {0.0F};
    float spawnY {0.0F};
};

struct HandshakeResponse {
    HandshakeDecision decision {HandshakeDecision::Invalid};
    std::string detail;
    std::optional<SessionInitData> sessionInit;
};

std::string BuildTemporaryJoinRequest(const HandshakeRequest& request);
JoinRequest ParseTemporaryJoinRequest(std::string_view payload);
std::string BuildTemporaryHandshakeResponse(const HandshakeResponse& response);
HandshakeResponse ParseTemporaryHandshakeResponse(std::string_view payload);

}  // namespace ashpaw::engine::net
