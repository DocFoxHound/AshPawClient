#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <string>

#include "ashpaw/engine/net/HandshakeProtocol.hpp"
#include "ashpaw/engine/net/TemporaryProtocol.hpp"

#include <enet/enet.h>

namespace ashpaw::engine::net {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Handshaking,
    Active,
    Disconnecting
};

struct ConnectionConfig {
    std::string host {"127.0.0.1"};
    std::uint16_t port {7777};
    std::string playerName {"Local Prowler"};
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
    std::optional<SessionInitData> sessionInit;
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
    [[nodiscard]] std::optional<SessionInitData> ConsumeSessionInit();
    void SendMovementIntent(const MovementIntent& intent);
    [[nodiscard]] std::vector<ServerMessage> ConsumeServerMessages();

private:
    static bool AcquireEnet();
    static void ReleaseEnet();
    void SetState(ConnectionState state, std::string detailMessage);
    void HandleConnected();
    void HandlePacket(const ENetPacket& packet);
    void ResetPeer();

    ENetHost* clientHost_ {nullptr};
    ENetPeer* serverPeer_ {nullptr};
    ConnectionConfig config_ {};
    ConnectionStatus status_ {};
    std::queue<ServerMessage> serverMessages_ {};
    bool initialized_ {false};
    bool enetInitialized_ {false};
    std::uint32_t connectStartTicks_ {0};
    std::uint32_t handshakeStartTicks_ {0};
};

}  // namespace ashpaw::engine::net
