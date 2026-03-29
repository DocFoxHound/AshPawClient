#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <queue>
#include <string>

#include "ashpaw/engine/net/Protocol.hpp"

#include <enet/enet.h>

namespace ashpaw::engine::net {

enum class ConnectionState {
    Disconnected,
    Connecting,
    WaitingForServerHello,
    WaitingForJoinAccepted,
    Active,
    Disconnecting
};

struct ConnectionConfig {
    std::string host {"127.0.0.1"};
    std::uint16_t port {7777};
    std::string playerName {"Local Prowler"};
    PackageMetadata localPackage {};
    std::uint32_t connectTimeoutMs {2500};
    std::uint32_t handshakeTimeoutMs {2500};
};

struct ConnectionStatus {
    ConnectionState state {ConnectionState::Disconnected};
    std::string targetHost {"127.0.0.1"};
    std::uint16_t targetPort {7777};
    std::string playerName {"Local Prowler"};
    std::string detailMessage {"Idle"};
    std::string lastError;
    std::optional<JoinAcceptedData> sessionInit;
    PackageMetadata serverPackage {};
    bool packageDownloadRequired {false};
    std::uint16_t serverTickRate {0};
    std::uint32_t sessionId {0};
    std::uint32_t controlledEntityId {0};
    std::uint32_t pingMs {0};
    std::uint64_t packetsSent {0};
    std::uint64_t packetsReceived {0};
};

class NetworkClient {
public:
    NetworkClient() = default;
    ~NetworkClient();

    bool Initialize();
    void Shutdown();
    bool Connect(const ConnectionConfig& config);
    void Disconnect();
    bool Reconnect();
    void Tick(std::uint32_t serviceTimeoutMs = 0);
    [[nodiscard]] ConnectionState State() const;
    [[nodiscard]] ConnectionStatus Status() const;
    [[nodiscard]] bool SessionActive() const;
    [[nodiscard]] std::optional<JoinAcceptedData> ConsumeSessionInit();
    void SendMovementIntent(const MovementIntent& intent);
    void SendInteractionRequest(const InteractionRequest& request);
    void SendChatMessage(std::string_view body);
    [[nodiscard]] std::vector<std::string> PacketLog() const;
    [[nodiscard]] std::vector<ServerMessage> ConsumeServerMessages();

private:
    static bool AcquireEnet();
    static void ReleaseEnet();
    void SetState(ConnectionState state, std::string detailMessage);
    void HandleConnected();
    void HandlePacket(const ENetPacket& packet, std::uint8_t channelId);
    void RecordPacket(std::string_view direction, std::uint8_t channelId, std::string_view payload);
    void QueueServerMessage(ServerMessage message);
    void ResetPeer();

    ENetHost* clientHost_ {nullptr};
    ENetPeer* serverPeer_ {nullptr};
    ConnectionConfig config_ {};
    ConnectionStatus status_ {};
    std::queue<ServerMessage> serverMessages_ {};
    std::deque<std::string> packetLog_ {};
    bool initialized_ {false};
    bool enetInitialized_ {false};
    std::uint32_t connectStartTicks_ {0};
    std::uint32_t handshakeStartTicks_ {0};
};

}  // namespace ashpaw::engine::net
