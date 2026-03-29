#include "ashpaw/engine/net/Protocol.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>

namespace ashpaw::engine::net {

namespace {

void WriteU8(std::vector<std::uint8_t>& buffer, std::uint8_t value) {
    buffer.push_back(value);
}

void WriteU16(std::vector<std::uint8_t>& buffer, std::uint16_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void WriteU32(std::vector<std::uint8_t>& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    buffer.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void WriteI8(std::vector<std::uint8_t>& buffer, std::int8_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value));
}

void WriteF32(std::vector<std::uint8_t>& buffer, float value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(float));
}

bool WriteString(std::vector<std::uint8_t>& buffer, std::string_view value) {
    if (value.size() > 255U) {
        return false;
    }
    WriteU8(buffer, static_cast<std::uint8_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
    return true;
}

bool IsPacketSizeValid(const std::vector<std::uint8_t>& buffer) {
    return buffer.size() <= kMaxPacketSizeBytes;
}

bool ReadU8(std::string_view payload, std::size_t& offset, std::uint8_t& out) {
    if (offset + 1U > payload.size()) {
        return false;
    }
    out = static_cast<std::uint8_t>(payload[offset]);
    ++offset;
    return true;
}

bool ReadU16(std::string_view payload, std::size_t& offset, std::uint16_t& out) {
    if (offset + 2U > payload.size()) {
        return false;
    }
    out = static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(payload[offset]) |
        (static_cast<std::uint16_t>(static_cast<std::uint8_t>(payload[offset + 1U])) << 8U)
    );
    offset += 2U;
    return true;
}

bool ReadU32(std::string_view payload, std::size_t& offset, std::uint32_t& out) {
    if (offset + 4U > payload.size()) {
        return false;
    }
    out = static_cast<std::uint32_t>(
        static_cast<std::uint8_t>(payload[offset]) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(payload[offset + 1U])) << 8U) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(payload[offset + 2U])) << 16U) |
        (static_cast<std::uint32_t>(static_cast<std::uint8_t>(payload[offset + 3U])) << 24U)
    );
    offset += 4U;
    return true;
}

bool ReadF32(std::string_view payload, std::size_t& offset, float& out) {
    if (offset + sizeof(float) > payload.size()) {
        return false;
    }
    std::memcpy(&out, payload.data() + static_cast<std::ptrdiff_t>(offset), sizeof(float));
    offset += sizeof(float);
    return true;
}

bool ReadString(std::string_view payload, std::size_t& offset, std::string& out) {
    std::uint8_t length = 0;
    if (!ReadU8(payload, offset, length)) {
        return false;
    }
    if (offset + length > payload.size()) {
        return false;
    }
    out.assign(payload.substr(offset, length));
    offset += length;
    return true;
}

std::int8_t QuantizeAxis(float value) {
    if (value > 0.25F) {
        return 1;
    }
    if (value < -0.25F) {
        return -1;
    }
    return 0;
}

const char* OpcodeName(Opcode opcode) {
    switch (opcode) {
    case Opcode::ClientHello:
        return "client_hello";
    case Opcode::ServerHello:
        return "server_hello";
    case Opcode::JoinAccepted:
        return "join_accepted";
    case Opcode::JoinRejected:
        return "join_rejected";
    case Opcode::MovementInput:
        return "movement_input";
    case Opcode::PlayerSpawn:
        return "player_spawn";
    case Opcode::PlayerDespawn:
        return "player_despawn";
    case Opcode::TransformSnapshot:
        return "transform_snapshot";
    case Opcode::InteractionRequest:
        return "interaction_request";
    case Opcode::InteractionResult:
        return "interaction_result";
    case Opcode::ObjectStateUpdate:
        return "object_state_update";
    case Opcode::ChatSend:
        return "chat_send";
    case Opcode::ChatBroadcast:
        return "chat_broadcast";
    case Opcode::IdentityUpdate:
        return "identity_update";
    }
    return "unknown";
}

std::string TrimCopy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

ParsedPacket InvalidPacket(std::string detail) {
    ParsedPacket packet;
    packet.kind = PacketKind::Invalid;
    packet.detail = std::move(detail);
    return packet;
}

}  // namespace

std::string SanitizeDisplayName(std::string_view rawName) {
    std::string sanitized;
    sanitized.reserve(rawName.size());
    for (const auto character : rawName) {
        const auto unsignedCharacter = static_cast<unsigned char>(character);
        if (std::isalnum(unsignedCharacter) != 0 || character == ' ' || character == '-' || character == '_') {
            sanitized.push_back(character);
        }
    }
    sanitized = TrimCopy(std::move(sanitized));
    if (sanitized.size() > kMaxDisplayNameLength) {
        sanitized.resize(kMaxDisplayNameLength);
    }
    if (sanitized.empty()) {
        sanitized = "player";
    }
    return sanitized;
}

std::optional<std::vector<std::uint8_t>> BuildClientHelloPacket(std::string_view displayName, const PackageMetadata& packageMetadata) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::ClientHello));
    WriteU16(buffer, kProtocolVersion);
    if (!WriteString(buffer, displayName) ||
        !WriteString(buffer, packageMetadata.mapId) ||
        !WriteString(buffer, packageMetadata.packageVersion) ||
        !WriteString(buffer, packageMetadata.contentHash) ||
        !IsPacketSizeValid(buffer)) {
        return std::nullopt;
    }
    return buffer;
}

std::optional<std::vector<std::uint8_t>> BuildMovementInputPacket(const MovementIntent& intent) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::MovementInput));
    WriteI8(buffer, QuantizeAxis(intent.x));
    WriteI8(buffer, QuantizeAxis(intent.y));
    return buffer;
}

std::optional<std::vector<std::uint8_t>> BuildInteractionRequestPacket(const InteractionRequest& request) {
    if (request.targetId.empty()) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::InteractionRequest));
    if (!WriteString(buffer, request.targetId) || !IsPacketSizeValid(buffer)) {
        return std::nullopt;
    }
    return buffer;
}

std::optional<std::vector<std::uint8_t>> BuildChatSendPacket(std::string_view message) {
    if (message.empty() || message.size() > kMaxChatMessageLength) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::ChatSend));
    if (!WriteString(buffer, message) || !IsPacketSizeValid(buffer)) {
        return std::nullopt;
    }
    return buffer;
}

std::vector<std::uint8_t> BuildServerHelloPacket(const ServerHelloData& hello) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::ServerHello));
    WriteU16(buffer, hello.protocolVersion);
    WriteU16(buffer, hello.tickRate);
    WriteString(buffer, hello.mapId);
    WriteString(buffer, hello.packageVersion);
    WriteString(buffer, hello.contentHash);
    WriteU8(buffer, hello.packageDownloadRequired ? 1U : 0U);
    return buffer;
}

std::vector<std::uint8_t> BuildJoinAcceptedPacket(const JoinAcceptedData& accepted) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::JoinAccepted));
    WriteU32(buffer, accepted.sessionId);
    WriteU32(buffer, accepted.entityId);
    WriteF32(buffer, accepted.spawnX);
    WriteF32(buffer, accepted.spawnY);
    WriteU32(buffer, static_cast<std::uint32_t>(accepted.spawnZ));
    return buffer;
}

std::vector<std::uint8_t> BuildJoinRejectedPacket(const JoinRejectedData& rejected) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::JoinRejected));
    WriteU8(buffer, static_cast<std::uint8_t>(rejected.reasonCode));
    WriteString(buffer, rejected.message);
    return buffer;
}

std::vector<std::uint8_t> BuildPlayerSpawnPacket(const EntityTransformState& entity) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::PlayerSpawn));
    WriteU32(buffer, static_cast<std::uint32_t>(entity.entityId));
    WriteF32(buffer, entity.x);
    WriteF32(buffer, entity.y);
    WriteU32(buffer, static_cast<std::uint32_t>(entity.z));
    return buffer;
}

std::vector<std::uint8_t> BuildPlayerDespawnPacket(std::uint32_t entityId) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::PlayerDespawn));
    WriteU32(buffer, entityId);
    return buffer;
}

std::vector<std::uint8_t> BuildTransformSnapshotPacket(const std::vector<EntityTransformState>& entities) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::TransformSnapshot));
    WriteU16(buffer, static_cast<std::uint16_t>(entities.size()));
    for (const auto& entity : entities) {
        WriteU32(buffer, static_cast<std::uint32_t>(entity.entityId));
        WriteF32(buffer, entity.x);
        WriteF32(buffer, entity.y);
        WriteU32(buffer, static_cast<std::uint32_t>(entity.z));
    }
    return buffer;
}

std::vector<std::uint8_t> BuildInteractionResultPacket(const InteractionResult& result) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::InteractionResult));
    WriteU8(buffer, static_cast<std::uint8_t>(result.status));
    WriteString(buffer, result.targetId);
    WriteString(buffer, result.message);
    return buffer;
}

std::vector<std::uint8_t> BuildObjectStateUpdatePacket(const ObjectStateUpdate& update) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::ObjectStateUpdate));
    WriteString(buffer, update.targetId);
    WriteU8(buffer, update.isOpen ? 1U : 0U);
    WriteU32(buffer, static_cast<std::uint32_t>(update.occupantEntityId));
    return buffer;
}

std::vector<std::uint8_t> BuildChatBroadcastPacket(const ChatBroadcast& broadcast) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::ChatBroadcast));
    WriteU32(buffer, static_cast<std::uint32_t>(broadcast.entityId));
    WriteString(buffer, broadcast.displayName);
    WriteString(buffer, broadcast.message);
    return buffer;
}

std::vector<std::uint8_t> BuildIdentityUpdatePacket(const IdentityUpdate& update) {
    std::vector<std::uint8_t> buffer;
    WriteU8(buffer, static_cast<std::uint8_t>(Opcode::IdentityUpdate));
    WriteU32(buffer, static_cast<std::uint32_t>(update.entityId));
    WriteString(buffer, update.displayName);
    return buffer;
}

ParsedPacket ParsePacket(std::string_view payload) {
    if (payload.empty()) {
        return InvalidPacket("Empty packet");
    }

    std::size_t offset = 0;
    std::uint8_t opcodeByte = 0;
    if (!ReadU8(payload, offset, opcodeByte)) {
        return InvalidPacket("Missing opcode");
    }

    const auto opcode = static_cast<Opcode>(opcodeByte);
    switch (opcode) {
    case Opcode::ServerHello: {
        ServerHelloData hello;
        std::uint8_t packageDownloadRequired = 0;
        if (!ReadU16(payload, offset, hello.protocolVersion) ||
            !ReadU16(payload, offset, hello.tickRate) ||
            !ReadString(payload, offset, hello.mapId) ||
            !ReadString(payload, offset, hello.packageVersion) ||
            !ReadString(payload, offset, hello.contentHash) ||
            !ReadU8(payload, offset, packageDownloadRequired)) {
            return InvalidPacket("Malformed server_hello");
        }
        hello.packageDownloadRequired = packageDownloadRequired != 0U;
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in server_hello");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::ServerHello;
        packet.serverHello = hello;
        return packet;
    }
    case Opcode::JoinAccepted: {
        JoinAcceptedData accepted;
        if (!ReadU32(payload, offset, accepted.sessionId) ||
            !ReadU32(payload, offset, accepted.entityId) ||
            !ReadF32(payload, offset, accepted.spawnX) ||
            !ReadF32(payload, offset, accepted.spawnY)) {
            return InvalidPacket("Malformed join_accepted");
        }
        std::uint32_t spawnZ = 0;
        if (!ReadU32(payload, offset, spawnZ)) {
            return InvalidPacket("Malformed join_accepted");
        }
        accepted.spawnZ = static_cast<std::int32_t>(spawnZ);
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in join_accepted");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::JoinAccepted;
        packet.joinAccepted = accepted;
        return packet;
    }
    case Opcode::JoinRejected: {
        std::uint8_t reasonCode = 0;
        JoinRejectedData rejected;
        if (!ReadU8(payload, offset, reasonCode) || !ReadString(payload, offset, rejected.message)) {
            return InvalidPacket("Malformed join_rejected");
        }
        rejected.reasonCode = static_cast<JoinRejectReasonCode>(reasonCode);
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in join_rejected");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::JoinRejected;
        packet.joinRejected = rejected;
        return packet;
    }
    case Opcode::PlayerSpawn: {
        EntityTransformState entity;
        std::uint32_t entityId = 0;
        std::uint32_t z = 0;
        if (!ReadU32(payload, offset, entityId) || !ReadF32(payload, offset, entity.x) || !ReadF32(payload, offset, entity.y) ||
            !ReadU32(payload, offset, z)) {
            return InvalidPacket("Malformed player_spawn");
        }
        entity.entityId = entityId;
        entity.z = static_cast<std::int32_t>(z);
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in player_spawn");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::PlayerSpawn;
        packet.entity = entity;
        return packet;
    }
    case Opcode::PlayerDespawn: {
        std::uint32_t entityId = 0;
        if (!ReadU32(payload, offset, entityId)) {
            return InvalidPacket("Malformed player_despawn");
        }
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in player_despawn");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::PlayerDespawn;
        packet.entityId = entityId;
        return packet;
    }
    case Opcode::TransformSnapshot: {
        std::uint16_t entityCount = 0;
        if (!ReadU16(payload, offset, entityCount)) {
            return InvalidPacket("Malformed transform_snapshot");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::TransformSnapshot;
        packet.snapshotEntities.reserve(entityCount);
        for (std::uint16_t index = 0; index < entityCount; ++index) {
            EntityTransformState entity;
            std::uint32_t entityId = 0;
            std::uint32_t z = 0;
            if (!ReadU32(payload, offset, entityId) || !ReadF32(payload, offset, entity.x) || !ReadF32(payload, offset, entity.y) ||
                !ReadU32(payload, offset, z)) {
                return InvalidPacket("Malformed transform_snapshot entity");
            }
            entity.entityId = entityId;
            entity.z = static_cast<std::int32_t>(z);
            packet.snapshotEntities.push_back(entity);
        }
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in transform_snapshot");
        }
        return packet;
    }
    case Opcode::InteractionResult: {
        std::uint8_t status = 0;
        InteractionResult result;
        if (!ReadU8(payload, offset, status) ||
            !ReadString(payload, offset, result.targetId) ||
            !ReadString(payload, offset, result.message)) {
            return InvalidPacket("Malformed interaction_result");
        }
        result.status = static_cast<InteractionStatus>(status);
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in interaction_result");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::InteractionResult;
        packet.interactionResult = result;
        return packet;
    }
    case Opcode::ObjectStateUpdate: {
        ObjectStateUpdate update;
        std::uint8_t isOpen = 0;
        std::uint32_t occupantEntityId = 0;
        if (!ReadString(payload, offset, update.targetId) ||
            !ReadU8(payload, offset, isOpen) ||
            !ReadU32(payload, offset, occupantEntityId)) {
            return InvalidPacket("Malformed object_state_update");
        }
        update.isOpen = isOpen != 0U;
        update.occupantEntityId = occupantEntityId;
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in object_state_update");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::ObjectStateUpdate;
        packet.objectStateUpdate = update;
        return packet;
    }
    case Opcode::ChatBroadcast: {
        ChatBroadcast broadcast;
        std::uint32_t entityId = 0;
        if (!ReadU32(payload, offset, entityId) ||
            !ReadString(payload, offset, broadcast.displayName) ||
            !ReadString(payload, offset, broadcast.message)) {
            return InvalidPacket("Malformed chat_broadcast");
        }
        broadcast.entityId = entityId;
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in chat_broadcast");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::ChatBroadcast;
        packet.chatBroadcast = broadcast;
        return packet;
    }
    case Opcode::IdentityUpdate: {
        IdentityUpdate update;
        std::uint32_t entityId = 0;
        if (!ReadU32(payload, offset, entityId) || !ReadString(payload, offset, update.displayName)) {
            return InvalidPacket("Malformed identity_update");
        }
        update.entityId = entityId;
        if (offset != payload.size()) {
            return InvalidPacket("Unexpected trailing bytes in identity_update");
        }
        ParsedPacket packet;
        packet.kind = PacketKind::IdentityUpdate;
        packet.identityUpdate = update;
        return packet;
    }
    default:
        return InvalidPacket("Unknown or unsupported packet opcode");
    }
}

std::optional<ClientHelloData> ParseClientHelloPacket(std::string_view payload) {
    if (payload.empty() || static_cast<Opcode>(static_cast<std::uint8_t>(payload.front())) != Opcode::ClientHello) {
        return std::nullopt;
    }

    std::size_t offset = 1;
    ClientHelloData hello;
    if (!ReadU16(payload, offset, hello.protocolVersion) ||
        !ReadString(payload, offset, hello.displayName) ||
        !ReadString(payload, offset, hello.localMapId) ||
        !ReadString(payload, offset, hello.localPackageVersion) ||
        !ReadString(payload, offset, hello.localContentHash)) {
        return std::nullopt;
    }
    if (offset != payload.size()) {
        return std::nullopt;
    }
    return hello;
}

std::optional<MovementIntent> ParseMovementInputPacket(std::string_view payload) {
    if (payload.size() != 3U || static_cast<Opcode>(static_cast<std::uint8_t>(payload.front())) != Opcode::MovementInput) {
        return std::nullopt;
    }
    const auto x = static_cast<std::int8_t>(static_cast<std::uint8_t>(payload[1]));
    const auto y = static_cast<std::int8_t>(static_cast<std::uint8_t>(payload[2]));
    return MovementIntent {
        .x = static_cast<float>(x),
        .y = static_cast<float>(y)
    };
}

std::optional<InteractionRequest> ParseInteractionRequestPacket(std::string_view payload) {
    if (payload.empty() || static_cast<Opcode>(static_cast<std::uint8_t>(payload.front())) != Opcode::InteractionRequest) {
        return std::nullopt;
    }
    std::size_t offset = 1;
    InteractionRequest request;
    if (!ReadString(payload, offset, request.targetId) || request.targetId.empty() || offset != payload.size()) {
        return std::nullopt;
    }
    return request;
}

std::optional<std::string> ParseChatSendPacket(std::string_view payload) {
    if (payload.empty() || static_cast<Opcode>(static_cast<std::uint8_t>(payload.front())) != Opcode::ChatSend) {
        return std::nullopt;
    }
    std::size_t offset = 1;
    std::string message;
    if (!ReadString(payload, offset, message) || message.empty() || offset != payload.size()) {
        return std::nullopt;
    }
    return message;
}

std::string DescribePacket(std::string_view payload) {
    if (payload.empty()) {
        return "empty";
    }
    const auto opcode = static_cast<Opcode>(static_cast<std::uint8_t>(payload.front()));
    return OpcodeName(opcode);
}

}  // namespace ashpaw::engine::net
