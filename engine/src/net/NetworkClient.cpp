#include "ashpaw/engine/net/NetworkClient.hpp"

#include <SDL.h>

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
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
    status_.playerName = SanitizeDisplayName(config.playerName);
    status_.lastError.clear();
    status_.sessionInit.reset();
    status_.serverPackage = {};
    status_.packageDownloadRequired = false;
    status_.serverTickRate = 0;
    status_.sessionId = 0;
    status_.controlledEntityId = 0;
    status_.pingMs = 0;
    status_.packetsSent = 0;
    status_.packetsReceived = 0;
    packetLog_.clear();

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
            ++status_.packetsReceived;
            RecordPacket("recv", event.channelID, {
                reinterpret_cast<const char*>(event.packet->data),
                static_cast<std::size_t>(event.packet->dataLength)
            });
            HandlePacket(*event.packet, event.channelID);
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
    status_.pingMs = serverPeer_ != nullptr ? serverPeer_->roundTripTime : 0U;
    if (status_.state == ConnectionState::Connecting &&
        now - connectStartTicks_ > config_.connectTimeoutMs) {
        status_.lastError = "Timed out connecting to server";
        ResetPeer();
        SetState(ConnectionState::Disconnected, "Connection timed out");
    }
    if ((status_.state == ConnectionState::WaitingForServerHello ||
         status_.state == ConnectionState::WaitingForJoinAccepted) &&
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

std::optional<JoinAcceptedData> NetworkClient::ConsumeSessionInit() {
    auto sessionInit = status_.sessionInit;
    status_.sessionInit.reset();
    return sessionInit;
}

void NetworkClient::SendMovementIntent(const MovementIntent& intent) {
    if (status_.state != ConnectionState::Active || serverPeer_ == nullptr || clientHost_ == nullptr) {
        return;
    }

    const auto payload = BuildMovementInputPacket(intent);
    if (!payload.has_value()) {
        return;
    }
    auto* packet = enet_packet_create(payload->data(), payload->size(), 0);
    enet_peer_send(serverPeer_, 1, packet);
    ++status_.packetsSent;
    RecordPacket("send", 1, {
        reinterpret_cast<const char*>(payload->data()),
        payload->size()
    });
}

void NetworkClient::SendInteractionRequest(const InteractionRequest& request) {
    if (status_.state != ConnectionState::Active || serverPeer_ == nullptr || clientHost_ == nullptr) {
        return;
    }

    const auto payload = BuildInteractionRequestPacket(request);
    if (!payload.has_value()) {
        return;
    }
    auto* packet = enet_packet_create(payload->data(), payload->size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverPeer_, 0, packet);
    ++status_.packetsSent;
    RecordPacket("send", 0, {
        reinterpret_cast<const char*>(payload->data()),
        payload->size()
    });
}

void NetworkClient::SendChatMessage(std::string_view body) {
    if (status_.state != ConnectionState::Active || serverPeer_ == nullptr || clientHost_ == nullptr) {
        return;
    }

    const auto payload = BuildChatSendPacket(body);
    if (!payload.has_value()) {
        return;
    }
    auto* packet = enet_packet_create(payload->data(), payload->size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverPeer_, 0, packet);
    ++status_.packetsSent;
    RecordPacket("send", 0, {
        reinterpret_cast<const char*>(payload->data()),
        payload->size()
    });
}

std::vector<std::string> NetworkClient::PacketLog() const {
    return {packetLog_.begin(), packetLog_.end()};
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
    SetState(ConnectionState::WaitingForServerHello, "Connected. Waiting for server hello");

    const auto request = BuildClientHelloPacket(status_.playerName, config_.localPackage);
    if (!request.has_value()) {
        status_.lastError = "Failed to build client hello";
        ResetPeer();
        SetState(ConnectionState::Disconnected, "Handshake setup failed");
        return;
    }
    auto* packet = enet_packet_create(request->data(), request->size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(serverPeer_, 0, packet);
    ++status_.packetsSent;
    RecordPacket("send", 0, {
        reinterpret_cast<const char*>(request->data()),
        request->size()
    });
    enet_host_flush(clientHost_);
}

void NetworkClient::HandlePacket(const ENetPacket& packet, std::uint8_t channelId) {
    const auto payload = std::string_view(
        reinterpret_cast<const char*>(packet.data),
        static_cast<std::size_t>(packet.dataLength)
    );
    const auto parsed = ParsePacket(payload);
    if (parsed.kind == PacketKind::Invalid) {
        status_.lastError = parsed.detail;
        if (status_.state == ConnectionState::WaitingForServerHello ||
            status_.state == ConnectionState::WaitingForJoinAccepted) {
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Invalid handshake packet");
        }
        return;
    }

    if (status_.state == ConnectionState::WaitingForServerHello) {
        if (parsed.kind == PacketKind::JoinRejected && parsed.joinRejected.has_value()) {
            status_.lastError = parsed.joinRejected->message.empty() ? "Join rejected" : parsed.joinRejected->message;
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Handshake rejected");
            return;
        }
        if (parsed.kind != PacketKind::ServerHello || !parsed.serverHello.has_value()) {
            status_.lastError = "Expected server_hello";
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Unexpected handshake packet");
            return;
        }
        if (parsed.serverHello->protocolVersion != kProtocolVersion) {
            status_.lastError = "Unsupported protocol version";
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Protocol mismatch");
            return;
        }
        status_.serverTickRate = parsed.serverHello->tickRate;
        status_.serverPackage = PackageMetadata {
            .mapId = parsed.serverHello->mapId,
            .packageVersion = parsed.serverHello->packageVersion,
            .contentHash = parsed.serverHello->contentHash
        };
        status_.packageDownloadRequired = parsed.serverHello->packageDownloadRequired;
        if (parsed.serverHello->packageDownloadRequired) {
            status_.lastError = "World package download required";
        }
        SetState(ConnectionState::WaitingForJoinAccepted, "Server hello received. Waiting for join acceptance");
        return;
    }

    if (status_.state == ConnectionState::WaitingForJoinAccepted) {
        if (parsed.kind == PacketKind::JoinRejected && parsed.joinRejected.has_value()) {
            status_.lastError = parsed.joinRejected->message.empty() ? "Join rejected" : parsed.joinRejected->message;
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Handshake rejected");
            return;
        }
        if (parsed.kind != PacketKind::JoinAccepted || !parsed.joinAccepted.has_value()) {
            status_.lastError = "Expected join_accepted";
            ResetPeer();
            SetState(ConnectionState::Disconnected, "Unexpected handshake packet");
            return;
        }
        status_.sessionInit = parsed.joinAccepted;
        status_.sessionId = parsed.joinAccepted->sessionId;
        status_.controlledEntityId = parsed.joinAccepted->entityId;
        status_.lastError.clear();
        SetState(ConnectionState::Active, "Session active");
        return;
    }

    if (status_.state != ConnectionState::Active) {
        return;
    }

    switch (parsed.kind) {
    case PacketKind::PlayerSpawn:
        QueueServerMessage({
            .kind = ServerMessageKind::Spawn,
            .entity = parsed.entity,
            .snapshotEntities = {},
            .interactionResult = std::nullopt,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = std::nullopt,
            .identityUpdate = std::nullopt,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::PlayerDespawn:
        QueueServerMessage({
            .kind = ServerMessageKind::Despawn,
            .entity = std::nullopt,
            .snapshotEntities = {},
            .interactionResult = std::nullopt,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = std::nullopt,
            .identityUpdate = std::nullopt,
            .entityId = parsed.entityId,
            .detail = {}
        });
        break;
    case PacketKind::TransformSnapshot:
        QueueServerMessage({
            .kind = ServerMessageKind::Snapshot,
            .entity = std::nullopt,
            .snapshotEntities = parsed.snapshotEntities,
            .interactionResult = std::nullopt,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = std::nullopt,
            .identityUpdate = std::nullopt,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::InteractionResult:
        QueueServerMessage({
            .kind = ServerMessageKind::InteractionResult,
            .entity = std::nullopt,
            .snapshotEntities = {},
            .interactionResult = parsed.interactionResult,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = std::nullopt,
            .identityUpdate = std::nullopt,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::ObjectStateUpdate:
        QueueServerMessage({
            .kind = ServerMessageKind::ObjectStateUpdate,
            .entity = std::nullopt,
            .snapshotEntities = {},
            .interactionResult = std::nullopt,
            .objectStateUpdate = parsed.objectStateUpdate,
            .chatBroadcast = std::nullopt,
            .identityUpdate = std::nullopt,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::ChatBroadcast:
        QueueServerMessage({
            .kind = ServerMessageKind::ChatBroadcast,
            .entity = std::nullopt,
            .snapshotEntities = {},
            .interactionResult = std::nullopt,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = parsed.chatBroadcast,
            .identityUpdate = std::nullopt,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::IdentityUpdate:
        QueueServerMessage({
            .kind = ServerMessageKind::IdentityUpdate,
            .entity = std::nullopt,
            .snapshotEntities = {},
            .interactionResult = std::nullopt,
            .objectStateUpdate = std::nullopt,
            .chatBroadcast = std::nullopt,
            .identityUpdate = parsed.identityUpdate,
            .entityId = 0,
            .detail = {}
        });
        break;
    case PacketKind::ServerHello:
    case PacketKind::JoinAccepted:
    case PacketKind::JoinRejected:
    case PacketKind::Invalid:
        status_.lastError = "Unexpected packet while active";
        static_cast<void>(channelId);
        break;
    }
}

void NetworkClient::ResetPeer() {
    if (serverPeer_ != nullptr) {
        enet_peer_reset(serverPeer_);
        serverPeer_ = nullptr;
    }
}

void NetworkClient::RecordPacket(std::string_view direction, std::uint8_t channelId, std::string_view payload) {
    constexpr std::size_t maxEntries = 40;
    std::ostringstream preview;
    preview << DescribePacket(payload) << " (" << payload.size() << " bytes)";
    packetLog_.push_back(
        std::string(direction) + " ch" + std::to_string(channelId) + " " + preview.str()
    );
    while (packetLog_.size() > maxEntries) {
        packetLog_.pop_front();
    }
}

void NetworkClient::QueueServerMessage(ServerMessage message) {
    serverMessages_.push(std::move(message));
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
