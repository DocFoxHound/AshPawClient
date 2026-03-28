#include <chrono>
#include <fstream>
#include <thread>
#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ashpaw/engine/assets/AssetManager.hpp"
#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/config/Config.hpp"
#include "ashpaw/engine/input/InputSystem.hpp"
#include "ashpaw/engine/net/NetworkClient.hpp"
#include "ashpaw/engine/net/Protocol.hpp"
#include "ashpaw/engine/prediction/SnapshotBuffer.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

#include <enet/enet.h>

namespace {

float SquaredDistance(const ashpaw::engine::math::Vector2& lhs, const ashpaw::engine::math::Vector2& rhs) {
    const auto dx = lhs.x - rhs.x;
    const auto dy = lhs.y - rhs.y;
    return (dx * dx) + (dy * dy);
}

std::string NormalizeIdentityKey(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const auto character : value) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    return normalized;
}

struct PersistedPlayer {
    std::uint32_t entityId {1001};
    std::string displayName;
    float x {160.0F};
    float y {160.0F};
};

class ScopedProtocolServer {
public:
    ScopedProtocolServer(enet_uint16 preferredPort, std::string reservedName)
        : reservedName_(std::move(reservedName)) {
        initialized_ = enet_initialize() == 0;
        if (!initialized_) {
            return;
        }

        for (enet_uint16 candidatePort = preferredPort; candidatePort < preferredPort + 32; ++candidatePort) {
            ENetAddress address {};
            address.host = ENET_HOST_ANY;
            address.port = candidatePort;
            host_ = enet_host_create(&address, 8, 2, 0, 0);
            if (host_ != nullptr) {
                port_ = candidatePort;
                break;
            }
        }

        running_ = host_ != nullptr;
        if (running_) {
            thread_ = std::thread([this]() { Run(); });
        }
    }

    ~ScopedProtocolServer() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        if (host_ != nullptr) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
        if (initialized_) {
            enet_deinitialize();
        }
    }

    [[nodiscard]] bool Running() const {
        return running_ && host_ != nullptr;
    }

    [[nodiscard]] enet_uint16 Port() const {
        return port_;
    }

private:
    struct ConnectedPlayer {
        std::uint32_t sessionId {0};
        PersistedPlayer persisted {};
        std::string identityKey;
    };

    static void SendPacket(ENetPeer* peer, std::uint8_t channelId, const std::vector<std::uint8_t>& payload) {
        auto* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, channelId, packet);
    }

    void BroadcastIdentity(const ConnectedPlayer& player, ENetPeer* peer) {
        SendPacket(peer, 0, ashpaw::engine::net::BuildIdentityUpdatePacket({
            .entityId = player.persisted.entityId,
            .displayName = player.persisted.displayName
        }));
    }

    void BroadcastObjectState(ENetPeer* peer) {
        SendPacket(peer, 0, ashpaw::engine::net::BuildObjectStateUpdatePacket({
            .targetId = "welcome_stone",
            .isOpen = false,
            .occupantEntityId = 0
        }));
    }

    void Run() {
        ENetEvent event {};
        while (running_) {
            if (enet_host_service(host_, &event, 10) <= 0) {
                continue;
            }

            switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                const auto payload = std::string_view(
                    reinterpret_cast<const char*>(event.packet->data),
                    static_cast<std::size_t>(event.packet->dataLength)
                );
                if (!playersByPeer_.contains(event.peer)) {
                    const auto hello = ashpaw::engine::net::ParseClientHelloPacket(payload);
                    if (!hello.has_value()) {
                        SendPacket(event.peer, 0, ashpaw::engine::net::BuildJoinRejectedPacket({
                            .reasonCode = ashpaw::engine::net::JoinRejectReasonCode::MalformedPacket,
                            .message = "invalid_client_hello"
                        }));
                    } else {
                        const auto sanitizedName = ashpaw::engine::net::SanitizeDisplayName(hello->displayName);
                        if (hello->protocolVersion != ashpaw::engine::net::kProtocolVersion) {
                            SendPacket(event.peer, 0, ashpaw::engine::net::BuildJoinRejectedPacket({
                                .reasonCode = ashpaw::engine::net::JoinRejectReasonCode::InvalidProtocol,
                                .message = "invalid_protocol"
                            }));
                        } else if (sanitizedName == reservedName_) {
                            SendPacket(event.peer, 0, ashpaw::engine::net::BuildJoinRejectedPacket({
                                .reasonCode = ashpaw::engine::net::JoinRejectReasonCode::MalformedPacket,
                                .message = "name_taken"
                            }));
                        } else {
                            const auto identityKey = NormalizeIdentityKey(sanitizedName);
                            auto persisted = persistedByIdentity_.contains(identityKey)
                                ? persistedByIdentity_.at(identityKey)
                                : PersistedPlayer {
                                      .entityId = nextEntityId_++,
                                      .displayName = sanitizedName,
                                      .x = 160.0F,
                                      .y = 160.0F
                                  };
                            persisted.displayName = sanitizedName;

                            auto player = ConnectedPlayer {
                                .sessionId = nextSessionId_++,
                                .persisted = persisted,
                                .identityKey = identityKey
                            };
                            playersByPeer_[event.peer] = player;

                            SendPacket(event.peer, 0, ashpaw::engine::net::BuildServerHelloPacket({
                                .protocolVersion = ashpaw::engine::net::kProtocolVersion,
                                .tickRate = 20
                            }));
                            SendPacket(event.peer, 0, ashpaw::engine::net::BuildJoinAcceptedPacket({
                                .sessionId = player.sessionId,
                                .entityId = player.persisted.entityId,
                                .spawnX = player.persisted.x,
                                .spawnY = player.persisted.y
                            }));

                            for (const auto& [peer, existingPlayer] : playersByPeer_) {
                                static_cast<void>(peer);
                                SendPacket(event.peer, 0, ashpaw::engine::net::BuildPlayerSpawnPacket({
                                    .entityId = existingPlayer.persisted.entityId,
                                    .x = existingPlayer.persisted.x,
                                    .y = existingPlayer.persisted.y
                                }));
                                BroadcastIdentity(existingPlayer, event.peer);
                            }
                            BroadcastObjectState(event.peer);
                        }
                    }
                } else if (const auto movement = ashpaw::engine::net::ParseMovementInputPacket(payload); movement.has_value()) {
                    auto& player = playersByPeer_.at(event.peer);
                    player.persisted.x += movement->x * 8.0F;
                    player.persisted.y += movement->y * 8.0F;
                    SendPacket(event.peer, 0, ashpaw::engine::net::BuildTransformSnapshotPacket({
                        {
                            .entityId = player.persisted.entityId,
                            .x = player.persisted.x,
                            .y = player.persisted.y
                        }
                    }));
                } else if (const auto request = ashpaw::engine::net::ParseInteractionRequestPacket(payload); request.has_value()) {
                    const auto& player = playersByPeer_.at(event.peer).persisted;
                    const auto playerCenter = ashpaw::engine::math::Vector2 {
                        player.x + 18.0F,
                        player.y + 18.0F
                    };
                    const auto result = request->targetId == "welcome_stone" &&
                            SquaredDistance(playerCenter, {240.0F, 180.0F}) <= (120.0F * 120.0F)
                        ? ashpaw::engine::net::InteractionResult {
                              .status = ashpaw::engine::net::InteractionStatus::Success,
                              .targetId = "welcome_stone",
                              .message = "The carved stone reads: 'Stay kind, stay curious, and share the meadow.'"
                          }
                        : ashpaw::engine::net::InteractionResult {
                              .status = ashpaw::engine::net::InteractionStatus::OutOfRange,
                              .targetId = request->targetId,
                              .message = "Move closer before trying that interaction again."
                          };
                    SendPacket(event.peer, 0, ashpaw::engine::net::BuildInteractionResultPacket(result));
                } else if (const auto chat = ashpaw::engine::net::ParseChatSendPacket(payload); chat.has_value()) {
                    const auto& player = playersByPeer_.at(event.peer).persisted;
                    SendPacket(event.peer, 0, ashpaw::engine::net::BuildChatBroadcastPacket({
                        .entityId = player.entityId,
                        .displayName = player.displayName,
                        .message = *chat
                    }));
                }
                enet_host_flush(host_);
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_CONNECT:
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                if (playersByPeer_.contains(event.peer)) {
                    persistedByIdentity_[playersByPeer_.at(event.peer).identityKey] = playersByPeer_.at(event.peer).persisted;
                    playersByPeer_.erase(event.peer);
                }
                break;
            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
    }

    ENetHost* host_ {nullptr};
    std::thread thread_ {};
    bool running_ {false};
    bool initialized_ {false};
    enet_uint16 port_ {0};
    std::string reservedName_;
    std::uint32_t nextSessionId_ {9001};
    std::uint32_t nextEntityId_ {1001};
    std::unordered_map<ENetPeer*, ConnectedPlayer> playersByPeer_;
    std::unordered_map<std::string, PersistedPlayer> persistedByIdentity_;
};

}  // namespace

TEST_CASE("config loads expected fields", "[config]") {
    const auto tempPath = std::filesystem::temp_directory_path() / "ashpaw_test_config.json";
    std::ofstream stream(tempPath);
    stream << R"({
        "window_width": 800,
        "window_height": 600,
        "master_volume_percent": 72,
        "asset_root": "test_assets",
        "server_host": "example.test",
        "server_port": 4567,
        "player_name": "Scout",
        "auto_connect": false,
        "connect_timeout_ms": 1400,
        "handshake_timeout_ms": 2100,
        "show_debug_overlay": false,
        "show_onboarding_hints": false,
        "cached_authoritative_name": "Scout",
        "cached_last_map": "starter_meadow"
    })";
    stream.close();

    const auto config = ashpaw::engine::config::Config::LoadFromFile(tempPath);
    REQUIRE(config.windowWidth == 800);
    REQUIRE(config.windowHeight == 600);
    REQUIRE(config.masterVolumePercent == 72);
    REQUIRE(config.assetRoot == "test_assets");
    REQUIRE(config.serverHost == "example.test");
    REQUIRE(config.serverPort == 4567);
    REQUIRE(config.playerName == "Scout");
    REQUIRE(config.autoConnect == false);
    REQUIRE(config.connectTimeoutMs == 1400);
    REQUIRE(config.handshakeTimeoutMs == 2100);
    REQUIRE(config.showDebugOverlay == false);
    REQUIRE(config.showOnboardingHints == false);
    REQUIRE(config.cachedAuthoritativeName == "Scout");
    REQUIRE(config.cachedLastMap == "starter_meadow");
}

TEST_CASE("config saves and reloads persistence-facing fields", "[config]") {
    const auto tempPath = std::filesystem::temp_directory_path() / "ashpaw_saved_config.json";
    const auto savedConfig = ashpaw::engine::config::AppConfig {
        .windowWidth = 1440,
        .windowHeight = 900,
        .fullscreen = true,
        .vsync = false,
        .masterVolumePercent = 65,
        .assetRoot = "assets",
        .mapPath = "maps/test_map.json",
        .serverHost = "127.0.0.1",
        .serverPort = 8888,
        .playerName = "Saved Prowler",
        .autoConnect = false,
        .connectTimeoutMs = 1200,
        .handshakeTimeoutMs = 2200,
        .logLevel = "debug",
        .showDebugOverlay = false,
        .showOnboardingHints = false,
        .cachedAuthoritativeName = "Saved_Prowler",
        .cachedLastMap = "fern_hollow"
    };
    ashpaw::engine::config::Config::SaveToFile(tempPath, savedConfig);

    const auto loadedConfig = ashpaw::engine::config::Config::LoadFromFile(tempPath);
    REQUIRE(loadedConfig.windowWidth == 1440);
    REQUIRE(loadedConfig.windowHeight == 900);
    REQUIRE(loadedConfig.fullscreen);
    REQUIRE_FALSE(loadedConfig.vsync);
    REQUIRE(loadedConfig.masterVolumePercent == 65);
    REQUIRE(loadedConfig.serverPort == 8888);
    REQUIRE(loadedConfig.playerName == "Saved Prowler");
    REQUIRE_FALSE(loadedConfig.autoConnect);
    REQUIRE_FALSE(loadedConfig.showOnboardingHints);
    REQUIRE(loadedConfig.cachedAuthoritativeName == "Saved_Prowler");
    REQUIRE(loadedConfig.cachedLastMap == "fern_hollow");
}

TEST_CASE("binary protocol codec builds and parses current packets", "[net]") {
    const auto sanitized = ashpaw::engine::net::SanitizeDisplayName("  Prowler!!  ");
    REQUIRE(sanitized == "Prowler");

    const auto helloPayload = ashpaw::engine::net::BuildClientHelloPacket("Prowler");
    REQUIRE(helloPayload.has_value());
    const auto hello = ashpaw::engine::net::ParseClientHelloPacket({
        reinterpret_cast<const char*>(helloPayload->data()),
        helloPayload->size()
    });
    REQUIRE(hello.has_value());
    REQUIRE(hello->protocolVersion == ashpaw::engine::net::kProtocolVersion);
    REQUIRE(hello->displayName == "Prowler");

    const auto movementPayload = ashpaw::engine::net::BuildMovementInputPacket({.x = 1.0F, .y = -1.0F});
    REQUIRE(movementPayload.has_value());
    const auto movement = ashpaw::engine::net::ParseMovementInputPacket({
        reinterpret_cast<const char*>(movementPayload->data()),
        movementPayload->size()
    });
    REQUIRE(movement.has_value());
    REQUIRE(movement->x == Catch::Approx(1.0F));
    REQUIRE(movement->y == Catch::Approx(-1.0F));

    const auto interactionPayload = ashpaw::engine::net::BuildInteractionRequestPacket({.targetId = "welcome_stone"});
    REQUIRE(interactionPayload.has_value());
    const auto interaction = ashpaw::engine::net::ParseInteractionRequestPacket({
        reinterpret_cast<const char*>(interactionPayload->data()),
        interactionPayload->size()
    });
    REQUIRE(interaction.has_value());
    REQUIRE(interaction->targetId == "welcome_stone");

    const auto chatPayload = ashpaw::engine::net::BuildChatSendPacket("Hello meadow");
    REQUIRE(chatPayload.has_value());
    const auto chat = ashpaw::engine::net::ParseChatSendPacket({
        reinterpret_cast<const char*>(chatPayload->data()),
        chatPayload->size()
    });
    REQUIRE(chat.has_value());
    REQUIRE(*chat == "Hello meadow");
    REQUIRE_FALSE(ashpaw::engine::net::BuildChatSendPacket(std::string(121, 'a')).has_value());

    const auto serverHelloPayload = ashpaw::engine::net::BuildServerHelloPacket({
        .protocolVersion = 1,
        .tickRate = 20
    });
    const auto serverHello = ashpaw::engine::net::ParsePacket({
        reinterpret_cast<const char*>(serverHelloPayload.data()),
        serverHelloPayload.size()
    });
    REQUIRE(serverHello.kind == ashpaw::engine::net::PacketKind::ServerHello);
    REQUIRE(serverHello.serverHello->tickRate == 20);

    const auto joinAcceptedPayload = ashpaw::engine::net::BuildJoinAcceptedPacket({
        .sessionId = 42,
        .entityId = 1001,
        .spawnX = 12.0F,
        .spawnY = 34.0F
    });
    const auto joinAccepted = ashpaw::engine::net::ParsePacket({
        reinterpret_cast<const char*>(joinAcceptedPayload.data()),
        joinAcceptedPayload.size()
    });
    REQUIRE(joinAccepted.kind == ashpaw::engine::net::PacketKind::JoinAccepted);
    REQUIRE(joinAccepted.joinAccepted->sessionId == 42);
    REQUIRE(joinAccepted.joinAccepted->entityId == 1001);

    const auto snapshotPayload = ashpaw::engine::net::BuildTransformSnapshotPacket({
        {.entityId = 1001, .x = 160.0F, .y = 168.0F},
        {.entityId = 1002, .x = 172.0F, .y = 180.0F}
    });
    const auto snapshot = ashpaw::engine::net::ParsePacket({
        reinterpret_cast<const char*>(snapshotPayload.data()),
        snapshotPayload.size()
    });
    REQUIRE(snapshot.kind == ashpaw::engine::net::PacketKind::TransformSnapshot);
    REQUIRE(snapshot.snapshotEntities.size() == 2);
    REQUIRE(snapshot.snapshotEntities.front().entityId == 1001);

    const auto objectPayload = ashpaw::engine::net::BuildObjectStateUpdatePacket({
        .targetId = "welcome_stone",
        .isOpen = false,
        .occupantEntityId = 0
    });
    const auto objectPacket = ashpaw::engine::net::ParsePacket({
        reinterpret_cast<const char*>(objectPayload.data()),
        objectPayload.size()
    });
    REQUIRE(objectPacket.kind == ashpaw::engine::net::PacketKind::ObjectStateUpdate);
    REQUIRE(objectPacket.objectStateUpdate->occupantEntityId == 0);

    const auto identityPayload = ashpaw::engine::net::BuildIdentityUpdatePacket({
        .entityId = 1001,
        .displayName = "Prowler"
    });
    const auto identity = ashpaw::engine::net::ParsePacket({
        reinterpret_cast<const char*>(identityPayload.data()),
        identityPayload.size()
    });
    REQUIRE(identity.kind == ashpaw::engine::net::PacketKind::IdentityUpdate);
    REQUIRE(identity.identityUpdate->displayName == "Prowler");

    const auto invalid = ashpaw::engine::net::ParsePacket(std::string_view("\xFF", 1));
    REQUIRE(invalid.kind == ashpaw::engine::net::PacketKind::Invalid);
}

TEST_CASE("input mapping respects UI capture", "[input]") {
    ashpaw::engine::input::InputSystem input;

    SDL_Event event {};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_w;
    input.HandleEvent(event);
    event.key.keysym.sym = SDLK_e;
    input.HandleEvent(event);
    event.key.keysym.sym = SDLK_RETURN;
    input.HandleEvent(event);
    event.key.keysym.sym = SDLK_ESCAPE;
    input.HandleEvent(event);

    const auto freeSnapshot = input.Snapshot(false);
    REQUIRE(freeSnapshot.moveUp);
    REQUIRE(freeSnapshot.interactPressed);
    REQUIRE(freeSnapshot.openChatPressed);
    REQUIRE(freeSnapshot.dismissUiPressed);
    REQUIRE(freeSnapshot.quitRequested);

    const auto capturedSnapshot = input.Snapshot(true);
    REQUIRE_FALSE(capturedSnapshot.moveUp);
    REQUIRE_FALSE(capturedSnapshot.interactPressed);
    REQUIRE(capturedSnapshot.openChatPressed);
    REQUIRE(capturedSnapshot.dismissUiPressed);
    REQUIRE_FALSE(capturedSnapshot.quitRequested);
}

TEST_CASE("network client completes documented handshake against test server", "[net]") {
    ScopedProtocolServer server(7777, "taken");
    if (!server.Running()) {
        SKIP("Local ENet server could not bind in this environment");
    }

    ashpaw::engine::net::NetworkClient client;
    REQUIRE(client.Initialize());
    REQUIRE(client.Connect({
        .host = "127.0.0.1",
        .port = server.Port(),
        .playerName = "Prowler",
        .connectTimeoutMs = 1000,
        .handshakeTimeoutMs = 1000
    }));

    for (int attempt = 0; attempt < 50 && !client.SessionActive(); ++attempt) {
        client.Tick(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(client.SessionActive());
    REQUIRE(client.State() == ashpaw::engine::net::ConnectionState::Active);
    const auto status = client.Status();
    REQUIRE(status.serverTickRate == 20);
    REQUIRE(status.controlledEntityId > 0);

    const auto sessionInit = client.ConsumeSessionInit();
    REQUIRE(sessionInit.has_value());
    REQUIRE(sessionInit->entityId == status.controlledEntityId);
    REQUIRE(sessionInit->spawnX == Catch::Approx(160.0F));

    bool sawSpawn = false;
    bool sawIdentity = false;
    bool sawObjectState = false;
    for (int attempt = 0; attempt < 20 && !(sawSpawn && sawIdentity && sawObjectState); ++attempt) {
        client.Tick(10);
        for (const auto& message : client.ConsumeServerMessages()) {
            if (message.kind == ashpaw::engine::net::ServerMessageKind::Spawn && message.entity.has_value()) {
                sawSpawn = true;
                REQUIRE(message.entity->entityId == status.controlledEntityId);
            }
            if (message.kind == ashpaw::engine::net::ServerMessageKind::IdentityUpdate && message.identityUpdate.has_value()) {
                sawIdentity = true;
                REQUIRE(message.identityUpdate->displayName == "Prowler");
            }
            if (message.kind == ashpaw::engine::net::ServerMessageKind::ObjectStateUpdate && message.objectStateUpdate.has_value()) {
                sawObjectState = true;
                REQUIRE(message.objectStateUpdate->targetId == "welcome_stone");
                REQUIRE(message.objectStateUpdate->occupantEntityId == 0);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(sawSpawn);
    REQUIRE(sawIdentity);
    REQUIRE(sawObjectState);

    client.SendMovementIntent({.x = 1.0F, .y = 0.0F});
    bool receivedSnapshot = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        client.Tick(10);
        const auto updates = client.ConsumeServerMessages();
        if (!updates.empty() && updates.front().kind == ashpaw::engine::net::ServerMessageKind::Snapshot) {
            REQUIRE(updates.front().snapshotEntities.size() == 1);
            REQUIRE(updates.front().snapshotEntities.front().x == Catch::Approx(168.0F));
            receivedSnapshot = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(receivedSnapshot);

    client.SendInteractionRequest({.targetId = "welcome_stone"});
    bool receivedInteractionResult = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        client.Tick(10);
        const auto updates = client.ConsumeServerMessages();
        if (!updates.empty() && updates.front().kind == ashpaw::engine::net::ServerMessageKind::InteractionResult) {
            REQUIRE(updates.front().interactionResult.has_value());
            REQUIRE(updates.front().interactionResult->status == ashpaw::engine::net::InteractionStatus::Success);
            receivedInteractionResult = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(receivedInteractionResult);

    client.SendChatMessage("Hello meadow");
    bool receivedChat = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        client.Tick(10);
        const auto updates = client.ConsumeServerMessages();
        if (!updates.empty() && updates.front().kind == ashpaw::engine::net::ServerMessageKind::ChatBroadcast) {
            REQUIRE(updates.front().chatBroadcast.has_value());
            REQUIRE(updates.front().chatBroadcast->displayName == "Prowler");
            REQUIRE(updates.front().chatBroadcast->message == "Hello meadow");
            receivedChat = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(receivedChat);

    client.Shutdown();
}

TEST_CASE("network client reports rejected handshake from test server", "[net]") {
    ScopedProtocolServer server(7778, "taken");
    if (!server.Running()) {
        SKIP("Local ENet server could not bind in this environment");
    }

    ashpaw::engine::net::NetworkClient client;
    REQUIRE(client.Initialize());
    REQUIRE(client.Connect({
        .host = "127.0.0.1",
        .port = server.Port(),
        .playerName = "taken",
        .connectTimeoutMs = 1000,
        .handshakeTimeoutMs = 1000
    }));

    for (int attempt = 0; attempt < 50 && client.State() != ashpaw::engine::net::ConnectionState::Disconnected; ++attempt) {
        client.Tick(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE_FALSE(client.SessionActive());
    REQUIRE(client.State() == ashpaw::engine::net::ConnectionState::Disconnected);
    REQUIRE(client.Status().lastError == "name_taken");

    client.Shutdown();
}

TEST_CASE("network client reconnects with persisted authoritative identity", "[net]") {
    ScopedProtocolServer server(7779, "taken");
    if (!server.Running()) {
        SKIP("Local ENet server could not bind in this environment");
    }

    ashpaw::engine::net::NetworkClient firstClient;
    REQUIRE(firstClient.Initialize());
    REQUIRE(firstClient.Connect({
        .host = "127.0.0.1",
        .port = server.Port(),
        .playerName = "  Prowler!!  ",
        .connectTimeoutMs = 1000,
        .handshakeTimeoutMs = 1000
    }));

    for (int attempt = 0; attempt < 50 && !firstClient.SessionActive(); ++attempt) {
        firstClient.Tick(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(firstClient.SessionActive());
    auto firstSession = firstClient.ConsumeSessionInit();
    REQUIRE(firstSession.has_value());
    REQUIRE(firstSession->spawnX == Catch::Approx(160.0F));

    firstClient.SendMovementIntent({.x = 1.0F, .y = 0.0F});
    for (int attempt = 0; attempt < 50; ++attempt) {
        firstClient.Tick(10);
        const auto updates = firstClient.ConsumeServerMessages();
        if (!updates.empty() && updates.front().kind == ashpaw::engine::net::ServerMessageKind::Snapshot) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    firstClient.Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ashpaw::engine::net::NetworkClient secondClient;
    REQUIRE(secondClient.Initialize());
    REQUIRE(secondClient.Connect({
        .host = "127.0.0.1",
        .port = server.Port(),
        .playerName = "Prowler",
        .connectTimeoutMs = 1000,
        .handshakeTimeoutMs = 1000
    }));

    for (int attempt = 0; attempt < 50 && !secondClient.SessionActive(); ++attempt) {
        secondClient.Tick(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(secondClient.SessionActive());
    const auto secondSession = secondClient.ConsumeSessionInit();
    REQUIRE(secondSession.has_value());
    REQUIRE(secondSession->spawnX == Catch::Approx(168.0F));

    secondClient.Shutdown();
}

TEST_CASE("camera converts world coordinates to screen coordinates", "[camera]") {
    ashpaw::engine::camera::Camera2D camera;
    camera.SetViewport(400.0F, 300.0F);
    camera.SetWorldBounds(1000.0F, 1000.0F);
    camera.Follow({500.0F, 500.0F});

    const auto screen = camera.WorldToScreen({500.0F, 500.0F});
    REQUIRE(screen.x == Catch::Approx(200.0F));
    REQUIRE(screen.y == Catch::Approx(150.0F));
}

TEST_CASE("snapshot buffer interpolates between samples", "[prediction]") {
    ashpaw::engine::prediction::SnapshotBuffer buffer;
    buffer.Push({.timestampSeconds = 0.0, .position = {0.0F, 0.0F}});
    buffer.Push({.timestampSeconds = 1.0, .position = {10.0F, 20.0F}});

    const auto sample = buffer.Sample(0.5);
    REQUIRE(sample.has_value());
    REQUIRE(sample->x == Catch::Approx(5.0F));
    REQUIRE(sample->y == Catch::Approx(10.0F));
}

TEST_CASE("client world tracks authoritative entities identities and interactables", "[world]") {
    ashpaw::engine::world::ClientWorld world;
    world.SetLocalPlayerId(7);
    world.SetSceneInfo({
        .mapName = "meadow",
        .worldSize = {500.0F, 500.0F}
    });
    world.UpsertEntity({
        .id = 7,
        .position = {10.0F, 20.0F},
        .size = {32.0F, 32.0F},
        .color = {1.0F, 1.0F, 1.0F, 1.0F},
        .displayName = "tester",
        .authoritative = true,
        .renderLayer = ashpaw::engine::world::RenderLayer::Actors,
        .labelOffset = {0.0F, -18.0F}
    });
    world.UpsertEntity({
        .id = 8,
        .position = {9.0F, 5.0F},
        .size = {20.0F, 10.0F},
        .color = {1.0F, 0.0F, 0.0F, 1.0F},
        .displayName = "ground",
        .authoritative = true,
        .renderLayer = ashpaw::engine::world::RenderLayer::Grounded,
        .labelOffset = {0.0F, -18.0F}
    });
    world.UpsertIdentity({
        .entityId = 7,
        .displayName = "Scout"
    });
    world.UpsertInteractable({
        .targetId = "welcome_stone",
        .isOpen = false,
        .occupantEntityId = 0
    });

    REQUIRE(world.FindEntity(7).has_value());
    REQUIRE(world.FindIdentity(7).has_value());
    REQUIRE(world.FindIdentity(7)->displayName == "Scout");
    REQUIRE(world.FindInteractable("welcome_stone").has_value());
    REQUIRE(world.FindInteractable("welcome_stone")->occupantEntityId == 0);
    REQUIRE(world.LocalPlayerId() == 7);
    REQUIRE(world.SceneInfo().mapName == "meadow");

    const auto sorted = world.SortedEntities();
    REQUIRE(sorted.size() == 2);
    REQUIRE(sorted.front().id == 8);
    REQUIRE(sorted.back().id == 7);
}

TEST_CASE("map loader parses visual and collision data", "[assets]") {
    ashpaw::engine::assets::AssetManager assets;
    assets.SetAssetRoot("../assets");
    const auto map = assets.LoadMap("maps/test_map.json");

    REQUIRE(map.name == "starter_meadow");
    REQUIRE(map.layers.size() == 3);
    REQUIRE(map.layers.front().drawOrder == ashpaw::engine::assets::LayerDrawOrder::Background);
    REQUIRE(map.layers[1].drawOrder == ashpaw::engine::assets::LayerDrawOrder::Midground);
    REQUIRE(map.layers.back().drawOrder == ashpaw::engine::assets::LayerDrawOrder::Foreground);
    REQUIRE_FALSE(map.blockers.empty());
    REQUIRE(map.markers.size() == 3);
    REQUIRE(map.markers.front().label == "Welcome Stone");
    REQUIRE(map.worldSize.x == Catch::Approx(1600.0F));
}
