#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

#include <enet/enet.h>

#include "ashpaw/engine/net/HandshakeProtocol.hpp"
#include "ashpaw/engine/net/TemporaryProtocol.hpp"
#include "ashpaw/engine/util/Log.hpp"

namespace {

struct ServerConfig {
    std::string host {"0.0.0.0"};
    enet_uint16 port {7777};
    std::string reservedName {"taken"};
};

struct ConnectedPlayer {
    ashpaw::engine::net::ReplicatedEntityState entity;
};

ServerConfig ParseArgs(int argc, char** argv) {
    ServerConfig config;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--host" && index + 1 < argc) {
            config.host = argv[++index];
        } else if (argument == "--port" && index + 1 < argc) {
            config.port = static_cast<enet_uint16>(std::strtoul(argv[++index], nullptr, 10));
        } else if (argument == "--reserved-name" && index + 1 < argc) {
            config.reservedName = argv[++index];
        } else if (argument == "--help") {
            std::cout << "Usage: ashpaw_handshake_server [--host 0.0.0.0] [--port 7777] [--reserved-name taken]\n";
            std::exit(0);
        }
    }

    return config;
}

void SendPacket(ENetPeer* peer, const std::string& payload) {
    auto* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

void SendHandshakeResponse(ENetPeer* peer, const ashpaw::engine::net::HandshakeResponse& response) {
    SendPacket(peer, ashpaw::engine::net::BuildTemporaryHandshakeResponse(response));
}

}  // namespace

int main(int argc, char** argv) {
    const auto config = ParseArgs(argc, argv);
    ashpaw::engine::util::InitializeLogger("info");

    if (enet_initialize() != 0) {
        std::cerr << "Failed to initialize ENet\n";
        return 1;
    }

    ENetAddress address {};
    address.host = ENET_HOST_ANY;
    address.port = config.port;
    if (config.host != "0.0.0.0" && enet_address_set_host_ip(&address, config.host.c_str()) != 0) {
        std::cerr << "Failed to bind handshake server host: " << config.host << '\n';
        enet_deinitialize();
        return 1;
    }

    ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr) {
        std::cerr << "Failed to create ENet server host\n";
        enet_deinitialize();
        return 1;
    }

    ashpaw::engine::util::Logger()->info(
        "Handshake server listening on {}:{} (reserved name: {})",
        config.host,
        config.port,
        config.reservedName
    );

    std::unordered_map<ENetPeer*, ConnectedPlayer> connectedPlayers;
    std::uint64_t nextEntityId = 1001;

    ENetEvent event {};
    while (enet_host_service(server, &event, 50) >= 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            ashpaw::engine::util::Logger()->info("Client connected from {}:{}", event.peer->address.host, event.peer->address.port);
            break;
        case ENET_EVENT_TYPE_RECEIVE: {
            const auto payload = std::string_view(
                reinterpret_cast<const char*>(event.packet->data),
                static_cast<std::size_t>(event.packet->dataLength)
            );

            if (!connectedPlayers.contains(event.peer)) {
                const auto request = ashpaw::engine::net::ParseTemporaryJoinRequest(payload);
                if (request.decision == ashpaw::engine::net::JoinRequestDecision::Accepted) {
                    if (request.playerName == config.reservedName) {
                        ashpaw::engine::util::Logger()->warn("Rejected player '{}': reserved name", request.playerName);
                        SendHandshakeResponse(event.peer, {
                            .decision = ashpaw::engine::net::HandshakeDecision::Rejected,
                            .detail = "name_taken",
                            .sessionInit = std::nullopt
                        });
                    } else {
                        ConnectedPlayer player {
                            .entity = {
                                .entityId = nextEntityId++,
                                .displayName = request.playerName,
                                .x = 160.0F + static_cast<float>(connectedPlayers.size() * 48U),
                                .y = 160.0F,
                                .width = 36.0F,
                                .height = 36.0F,
                                .localControlled = true,
                                .kind = ashpaw::engine::net::EntityKind::Player
                            }
                        };

                        connectedPlayers[event.peer] = player;
                        ashpaw::engine::util::Logger()->info("Accepted player '{}'", request.playerName);
                        SendHandshakeResponse(event.peer, {
                            .decision = ashpaw::engine::net::HandshakeDecision::Accepted,
                            .detail = "Join accepted",
                            .sessionInit = ashpaw::engine::net::SessionInitData {
                                .playerEntityId = player.entity.entityId,
                                .playerName = player.entity.displayName,
                                .mapName = "starter_meadow",
                                .spawnX = player.entity.x,
                                .spawnY = player.entity.y
                            }
                        });

                        for (const auto& [peer, existingPlayer] : connectedPlayers) {
                            auto entityForNewPeer = existingPlayer.entity;
                            entityForNewPeer.localControlled = peer == event.peer;
                            SendPacket(event.peer, ashpaw::engine::net::BuildSpawnMessage(entityForNewPeer));
                        }

                        auto newPlayerForOthers = player.entity;
                        newPlayerForOthers.localControlled = false;
                        for (const auto& [peer, existingPlayer] : connectedPlayers) {
                            static_cast<void>(existingPlayer);
                            if (peer == event.peer) {
                                continue;
                            }
                            SendPacket(peer, ashpaw::engine::net::BuildSpawnMessage(newPlayerForOthers));
                        }
                    }
                } else {
                    ashpaw::engine::util::Logger()->warn("Rejected invalid join packet: {}", request.detail);
                    SendHandshakeResponse(event.peer, {
                        .decision = ashpaw::engine::net::HandshakeDecision::Rejected,
                        .detail = "invalid_join_request",
                        .sessionInit = std::nullopt
                    });
                }
            } else {
                const auto movementIntent = ashpaw::engine::net::ParseMovementIntentMessage(payload);
                if (movementIntent.has_value()) {
                    auto& player = connectedPlayers.at(event.peer).entity;
                    constexpr float serverMoveStep = 8.0F;
                    player.x += movementIntent->x * serverMoveStep;
                    player.y += movementIntent->y * serverMoveStep;
                    if (player.x < 0.0F) {
                        player.x = 0.0F;
                    }
                    if (player.y < 0.0F) {
                        player.y = 0.0F;
                    }

                    for (const auto& [peer, existingPlayer] : connectedPlayers) {
                        static_cast<void>(existingPlayer);
                        auto entityForPeer = player;
                        entityForPeer.localControlled = peer == event.peer;
                        SendPacket(peer, ashpaw::engine::net::BuildSnapshotMessage(entityForPeer));
                    }
                }
            }

            enet_host_flush(server);
            enet_packet_destroy(event.packet);
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT:
            if (connectedPlayers.contains(event.peer)) {
                const auto entityId = connectedPlayers.at(event.peer).entity.entityId;
                connectedPlayers.erase(event.peer);
                for (const auto& [peer, player] : connectedPlayers) {
                    static_cast<void>(player);
                    SendPacket(peer, ashpaw::engine::net::BuildDespawnMessage(entityId));
                }
                enet_host_flush(server);
            }
            ashpaw::engine::util::Logger()->info("Client disconnected");
            break;
        case ENET_EVENT_TYPE_NONE:
            break;
        }
    }

    enet_host_destroy(server);
    enet_deinitialize();
    return 0;
}
