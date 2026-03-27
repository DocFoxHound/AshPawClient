#include <fstream>
#include <chrono>
#include <thread>
#include <unordered_map>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ashpaw/engine/assets/AssetManager.hpp"
#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/config/Config.hpp"
#include "ashpaw/engine/input/InputSystem.hpp"
#include "ashpaw/engine/net/HandshakeProtocol.hpp"
#include "ashpaw/engine/net/NetworkClient.hpp"
#include "ashpaw/engine/net/TemporaryProtocol.hpp"
#include "ashpaw/engine/prediction/SnapshotBuffer.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

#include <enet/enet.h>

namespace {

class ScopedEnetServer {
public:
    ScopedEnetServer(enet_uint16 preferredPort, std::string reservedName)
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

    ~ScopedEnetServer() {
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
                if (!playerByPeer_.contains(event.peer)) {
                    const auto request = ashpaw::engine::net::ParseTemporaryJoinRequest(payload);
                    ashpaw::engine::net::HandshakeResponse response;
                    if (request.decision == ashpaw::engine::net::JoinRequestDecision::Accepted &&
                        request.playerName != reservedName_) {
                        playerByPeer_[event.peer] = {
                            .entityId = 1001,
                            .displayName = request.playerName,
                            .x = 160.0F,
                            .y = 160.0F,
                            .width = 36.0F,
                            .height = 36.0F,
                            .localControlled = true,
                            .kind = ashpaw::engine::net::EntityKind::Player
                        };
                        response = {
                            .decision = ashpaw::engine::net::HandshakeDecision::Accepted,
                            .detail = "Join accepted",
                            .sessionInit = ashpaw::engine::net::SessionInitData {
                                .playerEntityId = playerByPeer_[event.peer].entityId,
                                .playerName = playerByPeer_[event.peer].displayName,
                                .mapName = "starter_meadow",
                                .spawnX = playerByPeer_[event.peer].x,
                                .spawnY = playerByPeer_[event.peer].y
                            }
                        };
                    } else {
                        response = {
                            .decision = ashpaw::engine::net::HandshakeDecision::Rejected,
                            .detail = request.playerName == reservedName_ ? "name_taken" : "invalid_join_request",
                            .sessionInit = std::nullopt
                        };
                    }
                    const auto reply = ashpaw::engine::net::BuildTemporaryHandshakeResponse(response);
                    auto* packet = enet_packet_create(reply.data(), reply.size(), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                    if (response.decision == ashpaw::engine::net::HandshakeDecision::Accepted) {
                        auto entity = playerByPeer_.at(event.peer);
                        entity.localControlled = true;
                        const auto spawnPayload = ashpaw::engine::net::BuildSpawnMessage(entity);
                        auto* spawnPacket = enet_packet_create(spawnPayload.data(), spawnPayload.size(), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, spawnPacket);
                    }
                } else if (const auto movement = ashpaw::engine::net::ParseMovementIntentMessage(payload); movement.has_value()) {
                    auto& entity = playerByPeer_.at(event.peer);
                    entity.x += movement->x * 8.0F;
                    entity.y += movement->y * 8.0F;
                    entity.localControlled = true;
                    const auto snapshotPayload = ashpaw::engine::net::BuildSnapshotMessage(entity);
                    auto* snapshotPacket = enet_packet_create(snapshotPayload.data(), snapshotPayload.size(), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, snapshotPacket);
                }
                enet_host_flush(host_);
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_CONNECT:
            case ENET_EVENT_TYPE_DISCONNECT:
            case ENET_EVENT_TYPE_NONE:
                break;
            }
        }
    }

    ENetHost* host_ {nullptr};
    std::string reservedName_;
    std::thread thread_ {};
    bool running_ {false};
    bool initialized_ {false};
    enet_uint16 port_ {0};
    std::unordered_map<ENetPeer*, ashpaw::engine::net::ReplicatedEntityState> playerByPeer_;
};

}  // namespace

TEST_CASE("config loads expected fields", "[config]") {
    const auto tempPath = std::filesystem::temp_directory_path() / "ashpaw_test_config.json";
    std::ofstream stream(tempPath);
    stream << R"({
        "window_width": 800,
        "window_height": 600,
        "asset_root": "test_assets",
        "server_host": "example.test",
        "server_port": 4567,
        "player_name": "Scout",
        "auto_connect": false,
        "connect_timeout_ms": 1400,
        "handshake_timeout_ms": 2100,
        "show_debug_overlay": false
    })";
    stream.close();

    const auto config = ashpaw::engine::config::Config::LoadFromFile(tempPath);
    REQUIRE(config.windowWidth == 800);
    REQUIRE(config.windowHeight == 600);
    REQUIRE(config.assetRoot == "test_assets");
    REQUIRE(config.serverHost == "example.test");
    REQUIRE(config.serverPort == 4567);
    REQUIRE(config.playerName == "Scout");
    REQUIRE(config.autoConnect == false);
    REQUIRE(config.connectTimeoutMs == 1400);
    REQUIRE(config.handshakeTimeoutMs == 2100);
    REQUIRE(config.showDebugOverlay == false);
}

TEST_CASE("temporary handshake protocol builds and parses responses", "[net]") {
    const auto request = ashpaw::engine::net::BuildTemporaryJoinRequest({
        .playerName = "Prowler"
    });
    REQUIRE(request == "join:Prowler");

    const auto parsedJoin = ashpaw::engine::net::ParseTemporaryJoinRequest(request);
    REQUIRE(parsedJoin.decision == ashpaw::engine::net::JoinRequestDecision::Accepted);
    REQUIRE(parsedJoin.playerName == "Prowler");

    const auto invalidJoin = ashpaw::engine::net::ParseTemporaryJoinRequest("ping");
    REQUIRE(invalidJoin.decision == ashpaw::engine::net::JoinRequestDecision::Invalid);

    const auto acceptedPayload = ashpaw::engine::net::BuildTemporaryHandshakeResponse({
        .decision = ashpaw::engine::net::HandshakeDecision::Accepted,
        .detail = "ignored",
        .sessionInit = ashpaw::engine::net::SessionInitData {
            .playerEntityId = 42,
            .playerName = "Prowler",
            .mapName = "starter_meadow",
            .spawnX = 12.0F,
            .spawnY = 34.0F
        }
    });
    REQUIRE(acceptedPayload.starts_with("join_accepted:"));

    const auto rejectedPayload = ashpaw::engine::net::BuildTemporaryHandshakeResponse({
        .decision = ashpaw::engine::net::HandshakeDecision::Rejected,
        .detail = "name_taken",
        .sessionInit = std::nullopt
    });
    REQUIRE(rejectedPayload == "join_rejected:name_taken");

    const auto accepted = ashpaw::engine::net::ParseTemporaryHandshakeResponse("join_accepted");
    REQUIRE(accepted.decision == ashpaw::engine::net::HandshakeDecision::Accepted);
    REQUIRE_FALSE(accepted.sessionInit.has_value());

    const auto acceptedWithSession = ashpaw::engine::net::ParseTemporaryHandshakeResponse(acceptedPayload);
    REQUIRE(acceptedWithSession.decision == ashpaw::engine::net::HandshakeDecision::Accepted);
    REQUIRE(acceptedWithSession.sessionInit.has_value());
    REQUIRE(acceptedWithSession.sessionInit->playerEntityId == 42);
    REQUIRE(acceptedWithSession.sessionInit->playerName == "Prowler");
    REQUIRE(acceptedWithSession.sessionInit->mapName == "starter_meadow");
    REQUIRE(acceptedWithSession.sessionInit->spawnX == Catch::Approx(12.0F));
    REQUIRE(acceptedWithSession.sessionInit->spawnY == Catch::Approx(34.0F));

    const auto rejected = ashpaw::engine::net::ParseTemporaryHandshakeResponse("join_rejected:name_taken");
    REQUIRE(rejected.decision == ashpaw::engine::net::HandshakeDecision::Rejected);
    REQUIRE(rejected.detail == "name_taken");

    const auto invalid = ashpaw::engine::net::ParseTemporaryHandshakeResponse("mystery_payload");
    REQUIRE(invalid.decision == ashpaw::engine::net::HandshakeDecision::Invalid);

    const auto movementPayload = ashpaw::engine::net::BuildMovementIntentMessage({.x = 1.0F, .y = -1.0F});
    const auto movement = ashpaw::engine::net::ParseMovementIntentMessage(movementPayload);
    REQUIRE(movement.has_value());
    REQUIRE(movement->x == Catch::Approx(1.0F));
    REQUIRE(movement->y == Catch::Approx(-1.0F));

    const ashpaw::engine::net::ReplicatedEntityState entity {
        .entityId = 1001,
        .displayName = "Prowler",
        .x = 160.0F,
        .y = 168.0F,
        .width = 36.0F,
        .height = 36.0F,
        .localControlled = true,
        .kind = ashpaw::engine::net::EntityKind::Player
    };
    const auto spawn = ashpaw::engine::net::ParseServerMessage(ashpaw::engine::net::BuildSpawnMessage(entity));
    REQUIRE(spawn.kind == ashpaw::engine::net::ServerMessageKind::Spawn);
    REQUIRE(spawn.entity.has_value());
    REQUIRE(spawn.entity->entityId == 1001);

    const auto snapshot = ashpaw::engine::net::ParseServerMessage(ashpaw::engine::net::BuildSnapshotMessage(entity));
    REQUIRE(snapshot.kind == ashpaw::engine::net::ServerMessageKind::Snapshot);
    REQUIRE(snapshot.entity.has_value());
    REQUIRE(snapshot.entity->localControlled == true);

    const auto despawn = ashpaw::engine::net::ParseServerMessage(ashpaw::engine::net::BuildDespawnMessage(1001));
    REQUIRE(despawn.kind == ashpaw::engine::net::ServerMessageKind::Despawn);
    REQUIRE(despawn.entityId == 1001);
}

TEST_CASE("input mapping respects UI capture", "[input]") {
    ashpaw::engine::input::InputSystem input;

    SDL_Event event {};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_w;
    input.HandleEvent(event);

    const auto freeSnapshot = input.Snapshot(false);
    REQUIRE(freeSnapshot.moveUp);

    const auto capturedSnapshot = input.Snapshot(true);
    REQUIRE_FALSE(capturedSnapshot.moveUp);
}

TEST_CASE("network client completes local handshake against test server", "[net]") {
    ScopedEnetServer server(7777, "taken");
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
    const auto sessionInit = client.ConsumeSessionInit();
    REQUIRE(sessionInit.has_value());
    REQUIRE(sessionInit->playerEntityId == 1001);
    REQUIRE(sessionInit->playerName == "Prowler");
    REQUIRE(sessionInit->mapName == "starter_meadow");
    REQUIRE(sessionInit->spawnX == Catch::Approx(160.0F));
    REQUIRE(sessionInit->spawnY == Catch::Approx(160.0F));

    const auto initialMessages = client.ConsumeServerMessages();
    REQUIRE(initialMessages.size() == 1);
    REQUIRE(initialMessages.front().kind == ashpaw::engine::net::ServerMessageKind::Spawn);
    REQUIRE(initialMessages.front().entity.has_value());
    REQUIRE(initialMessages.front().entity->entityId == 1001);

    client.SendMovementIntent({.x = 1.0F, .y = 0.0F});
    bool receivedSnapshot = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        client.Tick(10);
        const auto updates = client.ConsumeServerMessages();
        if (!updates.empty()) {
            REQUIRE(updates.front().kind == ashpaw::engine::net::ServerMessageKind::Snapshot);
            REQUIRE(updates.front().entity.has_value());
            REQUIRE(updates.front().entity->x == Catch::Approx(168.0F));
            receivedSnapshot = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(receivedSnapshot);

    client.Shutdown();
}

TEST_CASE("network client reports rejected handshake from test server", "[net]") {
    ScopedEnetServer server(7778, "taken");
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

TEST_CASE("client world tracks entities by authoritative id", "[world]") {
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
        .authoritative = false,
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

    const auto entity = world.FindEntity(7);
    REQUIRE(entity.has_value());
    REQUIRE(entity->displayName == "tester");
    REQUIRE(world.LocalPlayerId() == 7);
    REQUIRE(world.SceneInfo().mapName == "meadow");
    REQUIRE(world.EntityCount() == 2);

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
    REQUIRE(map.markers.size() == 2);
    REQUIRE(map.markers.front().label == "Guide Post");
    REQUIRE(map.worldSize.x == Catch::Approx(1600.0F));
}
