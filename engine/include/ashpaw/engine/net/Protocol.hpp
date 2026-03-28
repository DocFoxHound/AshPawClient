#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ashpaw::engine::net {

constexpr std::uint16_t kProtocolVersion = 1;
constexpr std::size_t kMaxPacketSizeBytes = 512;
constexpr std::size_t kMaxDisplayNameLength = 24;
constexpr std::size_t kMaxChatMessageLength = 120;

enum class Opcode : std::uint8_t {
    ClientHello = 1,
    ServerHello = 2,
    JoinAccepted = 3,
    JoinRejected = 4,
    MovementInput = 5,
    PlayerSpawn = 6,
    PlayerDespawn = 7,
    TransformSnapshot = 8,
    InteractionRequest = 9,
    InteractionResult = 10,
    ObjectStateUpdate = 11,
    ChatSend = 12,
    ChatBroadcast = 13,
    IdentityUpdate = 14
};

enum class JoinRejectReasonCode : std::uint8_t {
    InvalidProtocol = 1,
    ServerFull = 2,
    MalformedPacket = 3,
    Unknown = 255
};

enum class InteractionStatus : std::uint8_t {
    Success = 0,
    NotFound = 1,
    OutOfRange = 2,
    Blocked = 3,
    InvalidTarget = 4
};

struct MovementIntent {
    float x {0.0F};
    float y {0.0F};
};

struct InteractionRequest {
    std::string targetId;
};

struct ServerHelloData {
    std::uint16_t protocolVersion {0};
    std::uint16_t tickRate {0};
};

struct ClientHelloData {
    std::uint16_t protocolVersion {0};
    std::string displayName;
};

struct JoinAcceptedData {
    std::uint32_t sessionId {0};
    std::uint32_t entityId {0};
    float spawnX {0.0F};
    float spawnY {0.0F};
};

struct JoinRejectedData {
    JoinRejectReasonCode reasonCode {JoinRejectReasonCode::Unknown};
    std::string message;
};

struct EntityTransformState {
    std::uint64_t entityId {0};
    float x {0.0F};
    float y {0.0F};
};

struct InteractionResult {
    InteractionStatus status {InteractionStatus::InvalidTarget};
    std::string targetId;
    std::string message;
};

struct ObjectStateUpdate {
    std::string targetId;
    bool isOpen {false};
    std::uint64_t occupantEntityId {0};
};

struct IdentityUpdate {
    std::uint64_t entityId {0};
    std::string displayName;
};

struct ChatBroadcast {
    std::uint64_t entityId {0};
    std::string displayName;
    std::string message;
};

enum class PacketKind {
    Invalid,
    ServerHello,
    JoinAccepted,
    JoinRejected,
    PlayerSpawn,
    PlayerDespawn,
    TransformSnapshot,
    InteractionResult,
    ObjectStateUpdate,
    ChatBroadcast,
    IdentityUpdate
};

struct ParsedPacket {
    PacketKind kind {PacketKind::Invalid};
    std::optional<ServerHelloData> serverHello;
    std::optional<JoinAcceptedData> joinAccepted;
    std::optional<JoinRejectedData> joinRejected;
    std::optional<EntityTransformState> entity;
    std::vector<EntityTransformState> snapshotEntities;
    std::optional<InteractionResult> interactionResult;
    std::optional<ObjectStateUpdate> objectStateUpdate;
    std::optional<ChatBroadcast> chatBroadcast;
    std::optional<IdentityUpdate> identityUpdate;
    std::uint64_t entityId {0};
    std::string detail;
};

enum class ServerMessageKind {
    Invalid,
    Spawn,
    Snapshot,
    Despawn,
    InteractionResult,
    ChatBroadcast,
    IdentityUpdate,
    ObjectStateUpdate
};

struct ServerMessage {
    ServerMessageKind kind {ServerMessageKind::Invalid};
    std::optional<EntityTransformState> entity;
    std::vector<EntityTransformState> snapshotEntities;
    std::optional<InteractionResult> interactionResult;
    std::optional<ObjectStateUpdate> objectStateUpdate;
    std::optional<ChatBroadcast> chatBroadcast;
    std::optional<IdentityUpdate> identityUpdate;
    std::uint64_t entityId {0};
    std::string detail;
};

std::string SanitizeDisplayName(std::string_view rawName);

std::optional<std::vector<std::uint8_t>> BuildClientHelloPacket(std::string_view displayName);
std::optional<std::vector<std::uint8_t>> BuildMovementInputPacket(const MovementIntent& intent);
std::optional<std::vector<std::uint8_t>> BuildInteractionRequestPacket(const InteractionRequest& request);
std::optional<std::vector<std::uint8_t>> BuildChatSendPacket(std::string_view message);

std::vector<std::uint8_t> BuildServerHelloPacket(const ServerHelloData& hello);
std::vector<std::uint8_t> BuildJoinAcceptedPacket(const JoinAcceptedData& accepted);
std::vector<std::uint8_t> BuildJoinRejectedPacket(const JoinRejectedData& rejected);
std::vector<std::uint8_t> BuildPlayerSpawnPacket(const EntityTransformState& entity);
std::vector<std::uint8_t> BuildPlayerDespawnPacket(std::uint32_t entityId);
std::vector<std::uint8_t> BuildTransformSnapshotPacket(const std::vector<EntityTransformState>& entities);
std::vector<std::uint8_t> BuildInteractionResultPacket(const InteractionResult& result);
std::vector<std::uint8_t> BuildObjectStateUpdatePacket(const ObjectStateUpdate& update);
std::vector<std::uint8_t> BuildChatBroadcastPacket(const ChatBroadcast& broadcast);
std::vector<std::uint8_t> BuildIdentityUpdatePacket(const IdentityUpdate& update);

ParsedPacket ParsePacket(std::string_view payload);
std::optional<ClientHelloData> ParseClientHelloPacket(std::string_view payload);
std::optional<MovementIntent> ParseMovementInputPacket(std::string_view payload);
std::optional<InteractionRequest> ParseInteractionRequestPacket(std::string_view payload);
std::optional<std::string> ParseChatSendPacket(std::string_view payload);
std::string DescribePacket(std::string_view payload);

}  // namespace ashpaw::engine::net
