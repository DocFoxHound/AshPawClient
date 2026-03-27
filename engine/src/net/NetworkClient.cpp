#include "ashpaw/engine/net/NetworkClient.hpp"

#include <SDL.h>

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ashpaw::engine::net {

namespace {
std::mutex g_enetMutex;
std::uint32_t g_enetUsers = 0;
}

NetworkClient::~NetworkClient() {
    Shutdown();
}

bool NetworkClient::Initialize() {
    if (initialized_) {
        return true;
    }

    if (!AcquireEnet()) {
        status_.lastError = "Failed to initialize ENet";
        status_.detailMessage = status_.lastError;
        return false;
    }
    enetInitialized_ = true;

    clientHost_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (clientHost_ == nullptr) {
        status_.lastError = "Failed to create ENet client host";
        status_.detailMessage = status_.lastError;
        ReleaseEnet();
        enetInitialized_ = false;
        return false;
    }

    initialized_ = true;
    SetState(ConnectionState::Disconnected, "Client network ready");
    return true;
}

void NetworkClient::Shutdown() {
    Disconnect();
    if (clientHost_ != nullptr) {
        enet_host_destroy(clientHost_);
        clientHost_ = nullptr;
    }
    if (enetInitialized_) {
        ReleaseEnet();
        enetInitialized_ = false;
    }
    initialized_ = false;
}

bool NetworkClient::Connect(const ConnectionConfig& config) {
    if (!Initialize()) {
        return false;
    }

    if (status_.state != ConnectionState::Disconnected) {
        status_.lastError = "Connect requested while not disconnected";
        return false;
    }

    config_ = config;
    status_.targetHost = config.host;
    status_.targetPort = config.port;
    status_.playerName = config.playerName;
    status_.lastError.clear();
    status_.sessionInit.reset();

    ENetAddress address {};
    address.port = config.port;
    if (enet_address_set_host(&address, config.host.c_str()) != 0) {
        SetState(ConnectionState::Disconnected, "Unable to resolve server host");
        status_.lastError = "Host resolution failed";
        return false;
    }

    serverPeer_ = enet_host_connect(clientHost_, &address, 2, 0);
    if (serverPeer_ == nullptr) {
        SetState(ConnectionState::Disconnected, "Unable to create outbound peer");
        status_.lastError = "Peer creation failed";
        return false;
    }

    connectStartTicks_ = SDL_GetTicks();
    SetState(ConnectionState::Connecting, "Attempting connection");
    return true;
}

void NetworkClient::Disconnect() {
    if (!initialized_) {
        return;
    }

    if (serverPeer_ != nullptr) {
        SetState(ConnectionState::Disconnecting, "Disconnecting");
        enet_peer_disconnect_now(serverPeer_, 0);
        serverPeer_ = nullptr;
    }
    SetState(ConnectionState::Disconnected, "Disconnected");
    std::queue<ServerMessage> empty;
    serverMessages_.swap(empty);
}

bool NetworkClient::Reconnect() {
    const auto reconnectConfig = config_;
    Disconnect();
    status_.lastError.clear();
    status_.sessionInit.reset();
    return Connect(reconnectConfig);
}

void NetworkClient::Tick(std::uint32_t serviceTimeoutMs) {
    if (!initialized_ || clientHost_ == nullptr) {
        return;
    }

    ENetEvent event {};
    while (enet_host_service(clientHost_, &event, serviceTimeoutMs) > 0) {
        serviceTimeoutMs = 0;
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            HandleConnected();
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            HandlePacket(*event.packet);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            serverPeer_ = nullptr;
            if (status_.state == ConnectionState::Disconnecting) {
                SetState(ConnectionState::Disconnected, "Disconnected");
            } else {
                SetState(ConnectionState::Disconnected, "Server disconnected");
                if (status_.lastError.empty()) {
                    status_.lastError = "Connection closed by server";
                }
            }
            break;
        case ENET_EVENT_TYPE_NONE:
            break;
        }
    }

    const auto now = SDL_GetTicks();
    if (status_.state == ConnectionState::Connecting &&
        now - connectStartTicks_ > config_.connectTimeoutMs) {
        status_.lastError = "Timed out connecting to server";
        ResetPeer();
        SetState(ConnectionState::Disconnected, "Connection timed out");
    }
    if (status_.state == ConnectionState::Handshaking &&
        now - handshakeStartTicks_ > config_.handshakeTimeoutMs) {
        status_.lastError = "Timed out waiting for handshake";
        ResetPeer();
        SetState(ConnectionState::Disconnected, "Handshake timed out");
    }
}

ConnectionState NetworkClient::State() const {
    return status_.state;
}

ConnectionStatus NetworkClient::Status() const {
    return status_;
}

bool NetworkClient::SessionActive() const {
    return status_.state == ConnectionState::Active;
}

std::optional<SessionInitData> NetworkClient::ConsumeSessionInit() {
    auto sessionInit = status_.sessionInit;
    status_.sessionInit.reset();
    return sessionInit;
}

void NetworkClient::SendMovementIntent(const MovementIntent& intent) {
    if (status_.state != ConnectionState::Active || serverPeer_ == nullptr || clientHost_ == nullptr) {
        return;
    }

    const auto payload = BuildMovementIntentMessage(intent);
    auto* packet = enet_packet_create(payload.data(), payload.size(), 0);
    enet_peer_send(serverPeer_, 0, packet);
}

std::vector<ServerMessage> NetworkClient::ConsumeServerMessages() {
    std::vector<ServerMessage> messages;
    while (!serverMessages_.empty()) {
        messages.push_back(std::move(serverMessages_.front()));
        serverMessages_.pop();
    }
    return messages;
}

void NetworkClient::SetState(ConnectionState state, std::string detailMessage) {
    status_.state = state;
    status_.detailMessage = std::move(detailMessage);
}

void NetworkClient::HandleConnected() {
    handshakeStartTicks_ = SDL_GetTicks();
    SetState(ConnectionState::Handshaking, "Connected. Waiting for handshake response");

    const auto request = BuildTemporaryJoinRequest({.playerName = config_.playerName});
    auto* packet = enet_packet_create(request.data(), request.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverPeer_, 0, packet);
    enet_host_flush(clientHost_);
}

void NetworkClient::HandlePacket(const ENetPacket& packet) {
    const auto payload = std::string_view(
        reinterpret_cast<const char*>(packet.data),
        static_cast<std::size_t>(packet.dataLength)
    );
    const auto response = ParseTemporaryHandshakeResponse(payload);
    if (response.decision == HandshakeDecision::Accepted) {
        status_.lastError.clear();
        status_.sessionInit = response.sessionInit;
        if (response.sessionInit.has_value()) {
            status_.playerName = response.sessionInit->playerName;
        }
        SetState(ConnectionState::Active, response.detail);
        return;
    }
    if (response.decision == HandshakeDecision::Rejected) {
        status_.lastError = response.detail.empty() ? "Join rejected" : response.detail;
        ResetPeer();
        SetState(ConnectionState::Disconnected, "Handshake rejected");
        return;
    }

    if (status_.state == ConnectionState::Active) {
        const auto message = ParseServerMessage(payload);
        if (message.kind == ServerMessageKind::Invalid) {
            status_.lastError = message.detail;
            return;
        }
        serverMessages_.push(message);
        return;
    }

    status_.lastError = response.detail;
    ResetPeer();
    SetState(ConnectionState::Disconnected, "Invalid handshake response");
}

void NetworkClient::ResetPeer() {
    if (serverPeer_ != nullptr) {
        enet_peer_reset(serverPeer_);
        serverPeer_ = nullptr;
    }
}

bool NetworkClient::AcquireEnet() {
    std::scoped_lock lock(g_enetMutex);
    if (g_enetUsers == 0 && enet_initialize() != 0) {
        return false;
    }
    ++g_enetUsers;
    return true;
}

void NetworkClient::ReleaseEnet() {
    std::scoped_lock lock(g_enetMutex);
    if (g_enetUsers == 0) {
        return;
    }
    --g_enetUsers;
    if (g_enetUsers == 0) {
        enet_deinitialize();
    }
}

}  // namespace ashpaw::engine::net
