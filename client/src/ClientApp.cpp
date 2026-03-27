#include "ashpaw/client/ClientApp.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "ashpaw/engine/math/Vector2.hpp"
#include "ashpaw/engine/util/Log.hpp"

namespace ashpaw::client {

namespace {
constexpr engine::world::EntityId kLocalPlayerId = 1;
constexpr float kMoveSpeed = 220.0F;
constexpr engine::math::Vector2 kPlayerSize {36.0F, 36.0F};

const char* ToString(engine::net::ConnectionState state) {
    switch (state) {
    case engine::net::ConnectionState::Disconnected:
        return "disconnected";
    case engine::net::ConnectionState::Connecting:
        return "connecting";
    case engine::net::ConnectionState::Handshaking:
        return "handshaking";
    case engine::net::ConnectionState::Active:
        return "active";
    case engine::net::ConnectionState::Disconnecting:
        return "disconnecting";
    }
    return "unknown";
}

engine::net::ConnectionConfig ToConnectionConfig(const engine::config::AppConfig& config) {
    return {
        .host = config.serverHost,
        .port = static_cast<std::uint16_t>(config.serverPort),
        .playerName = config.playerName,
        .connectTimeoutMs = static_cast<std::uint32_t>(config.connectTimeoutMs),
        .handshakeTimeoutMs = static_cast<std::uint32_t>(config.handshakeTimeoutMs)
    };
}

engine::math::Color EntityColor(const engine::net::ReplicatedEntityState& entity) {
    if (entity.localControlled) {
        return {0.82F, 0.71F, 0.31F, 1.0F};
    }
    if (entity.kind == engine::net::EntityKind::Npc) {
        return {0.81F, 0.48F, 0.36F, 1.0F};
    }
    return {0.39F, 0.67F, 0.92F, 1.0F};
}
}

bool ClientApp::Initialize(const std::filesystem::path& configPath) {
    config_ = engine::config::Config::LoadFromFile(configPath);
    engine::util::InitializeLogger(config_.logLevel);
    debugOverlayVisible_ = config_.showDebugOverlay;

    platform_.Initialize({
        .title = "AshPaw Client",
        .width = config_.windowWidth,
        .height = config_.windowHeight,
        .fullscreen = config_.fullscreen,
        .vsync = config_.vsync
    });

    renderer_.Initialize(platform_.Window(), config_.vsync);
    ui_.Initialize(platform_.Window(), SDL_GL_GetCurrentContext());
    assetManager_.SetAssetRoot(config_.assetRoot);
    if (!network_.Initialize()) {
        throw std::runtime_error("Failed to initialize network client");
    }

    LoadScene();
    if (config_.autoConnect) {
        BeginConnection();
    }
    running_ = true;
    engine::util::Logger()->info("Client initialized");
    return true;
}

int ClientApp::Run() {
    auto previousTime = std::chrono::steady_clock::now();

    while (running_) {
        input_.BeginFrame();

        SDL_Event event {};
        while (platform_.PollEvent(event)) {
            ui_.ProcessEvent(event);
            input_.HandleEvent(event);
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const auto deltaSeconds = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        Update(deltaSeconds);
        Render(deltaSeconds);
    }

    return 0;
}

void ClientApp::Shutdown() {
    if (!running_ && platform_.Window() == nullptr) {
        return;
    }
    ui_.Shutdown();
    network_.Shutdown();
    renderer_.Shutdown();
    platform_.Shutdown();
    running_ = false;
}

void ClientApp::BeginConnection() {
    if (connectAttempted_) {
        return;
    }
    connectAttempted_ = true;
    const auto connectionConfig = ToConnectionConfig(config_);
    if (network_.Connect(connectionConfig)) {
        engine::util::Logger()->info("Connecting to {}:{} as {}", config_.serverHost, config_.serverPort, config_.playerName);
    } else {
        const auto status = network_.Status();
        engine::util::Logger()->warn("Connection attempt failed: {}", status.lastError.empty() ? status.detailMessage : status.lastError);
    }
    SyncSessionState();
}

void ClientApp::RetryConnection() {
    connectAttempted_ = true;
    if (network_.Reconnect()) {
        engine::util::Logger()->info("Retrying connection to {}:{} as {}", config_.serverHost, config_.serverPort, config_.playerName);
    } else {
        const auto status = network_.Status();
        engine::util::Logger()->warn("Reconnect attempt failed: {}", status.lastError.empty() ? status.detailMessage : status.lastError);
    }
    SyncSessionState();
}

void ClientApp::LoadScene() {
    currentMap_ = assetManager_.LoadMap(config_.mapPath);
    camera_.SetViewport(static_cast<float>(config_.windowWidth), static_cast<float>(config_.windowHeight));
    camera_.SetWorldBounds(currentMap_.worldSize.x, currentMap_.worldSize.y);
    world_.SetSceneInfo({
        .mapName = currentMap_.name,
        .worldSize = currentMap_.worldSize
    });

    world_.SetLocalPlayerId(kLocalPlayerId);
    world_.UpsertEntity({
        .id = kLocalPlayerId,
        .position = currentMap_.spawnPoint,
        .size = kPlayerSize,
        .color = {0.82F, 0.71F, 0.31F, 0.45F},
        .displayName = "Waiting for session",
        .authoritative = false,
        .renderLayer = engine::world::RenderLayer::Actors,
        .labelOffset = {0.0F, -20.0F}
    });
}

void ClientApp::SyncSessionState() {
    const auto state = network_.State();
    const auto wasActive = sessionActive_;
    sessionActive_ = network_.SessionActive();
    sessionStateLabel_ = ToString(state);

    if (!wasActive && sessionActive_) {
        if (const auto sessionInit = network_.ConsumeSessionInit(); sessionInit.has_value()) {
            world_.ClearEntities();

            engine::world::EntityPresentation playerEntity {
                .id = sessionInit->playerEntityId,
                .position = {sessionInit->spawnX, sessionInit->spawnY},
                .size = kPlayerSize,
                .color = {0.82F, 0.71F, 0.31F, 1.0F},
                .displayName = sessionInit->playerName,
                .authoritative = false,
                .renderLayer = engine::world::RenderLayer::Actors,
                .labelOffset = {0.0F, -20.0F}
            };
            world_.SetLocalPlayerId(sessionInit->playerEntityId);
            world_.UpsertEntity(playerEntity);

            auto sceneInfo = world_.SceneInfo();
            if (!sessionInit->mapName.empty()) {
                sceneInfo.mapName = sessionInit->mapName;
            }
            world_.SetSceneInfo(sceneInfo);
        }
        engine::util::Logger()->info("Session activated after handshake");
    }
    if (wasActive && !sessionActive_) {
        const auto status = network_.Status();
        engine::util::Logger()->warn("Session ended: {}", status.lastError.empty() ? status.detailMessage : status.lastError);
    }
}

void ClientApp::ApplyServerMessages() {
    const auto messages = network_.ConsumeServerMessages();
    for (const auto& message : messages) {
        switch (message.kind) {
        case engine::net::ServerMessageKind::Spawn:
        case engine::net::ServerMessageKind::Snapshot: {
            if (!message.entity.has_value()) {
                break;
            }
            const auto& entity = *message.entity;
            world_.UpsertEntity({
                .id = entity.entityId,
                .position = {entity.x, entity.y},
                .size = {entity.width, entity.height},
                .color = EntityColor(entity),
                .displayName = entity.displayName,
                .authoritative = true,
                .renderLayer = engine::world::RenderLayer::Actors,
                .labelOffset = {0.0F, -20.0F}
            });
            if (entity.localControlled) {
                world_.SetLocalPlayerId(entity.entityId);
            }
            break;
        }
        case engine::net::ServerMessageKind::Despawn:
            world_.RemoveEntity(message.entityId);
            break;
        case engine::net::ServerMessageKind::Invalid:
            break;
        }
    }
}

void ClientApp::Update(float deltaSeconds) {
    static_cast<void>(deltaSeconds);
    network_.Tick();
    SyncSessionState();
    ApplyServerMessages();

    const auto uiCapturesKeyboard = ui_.WantsKeyboardCapture();
    const auto snapshot = input_.Snapshot(uiCapturesKeyboard);

    if (snapshot.quitRequested) {
        running_ = false;
        return;
    }
    if (snapshot.toggleDebugPressed) {
        debugOverlayVisible_ = !debugOverlayVisible_;
    }
    if (snapshot.reconnectPressed && !sessionActive_) {
        RetryConnection();
    }

    auto localPlayer = world_.FindEntity(world_.LocalPlayerId());
    if (!localPlayer.has_value()) {
        return;
    }

    if (!sessionActive_) {
        camera_.Follow({localPlayer->position.x + (kPlayerSize.x * 0.5F), localPlayer->position.y + (kPlayerSize.y * 0.5F)});
        return;
    }

    engine::math::Vector2 movement {};
    if (snapshot.moveUp) {
        movement.y -= 1.0F;
    }
    if (snapshot.moveDown) {
        movement.y += 1.0F;
    }
    if (snapshot.moveLeft) {
        movement.x -= 1.0F;
    }
    if (snapshot.moveRight) {
        movement.x += 1.0F;
    }

    if (movement.x != 0.0F || movement.y != 0.0F) {
        const auto magnitude = std::sqrt((movement.x * movement.x) + (movement.y * movement.y));
        movement.x /= magnitude;
        movement.y /= magnitude;
    }

    network_.SendMovementIntent({.x = movement.x, .y = movement.y});

    camera_.Follow({localPlayer->position.x + (kPlayerSize.x * 0.5F), localPlayer->position.y + (kPlayerSize.y * 0.5F)});
}

void ClientApp::Render(float deltaSeconds) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(platform_.Window(), &width, &height);

    renderer_.BeginFrame(width, height);
    ui_.BeginFrame();
    const auto sortedEntities = world_.SortedEntities();
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Background);
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Midground);
    renderer_.RenderEntities(sortedEntities, camera_);
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Foreground);
    ui_.RenderEntityLabels(sortedEntities, camera_);

    const auto localPlayer = world_.FindEntity(world_.LocalPlayerId());
    const auto fps = deltaSeconds > 0.0F ? 1.0F / deltaSeconds : 0.0F;
    const auto sceneInfo = world_.SceneInfo();
    const auto networkStatus = network_.Status();
    ui_.RenderConnectionPanel({
        .visible = !sessionActive_,
        .sessionActive = sessionActive_,
        .connectionState = sessionStateLabel_.c_str(),
        .detailMessage = networkStatus.detailMessage.c_str(),
        .lastError = networkStatus.lastError.c_str(),
        .playerName = networkStatus.playerName.c_str(),
        .targetHost = networkStatus.targetHost.c_str(),
        .targetPort = networkStatus.targetPort,
        .canRetry = !sessionActive_
    });
    ui_.RenderDebugOverlay({
        .visible = debugOverlayVisible_,
        .fps = fps,
        .playerPosition = localPlayer.has_value() ? localPlayer->position : engine::math::Vector2 {},
        .colliding = colliding_,
        .entityCount = static_cast<int>(world_.EntityCount()),
        .networkState = ToString(network_.State()),
        .currentMap = sceneInfo.mapName.c_str()
    });
    ui_.EndFrame();
    renderer_.EndFrame(platform_.Window());
}

bool ClientApp::CollidesAt(const engine::math::Vector2& position) const {
    const engine::math::Rect playerRect {position.x, position.y, kPlayerSize.x, kPlayerSize.y};
    for (const auto& blocker : currentMap_.blockers) {
        if (engine::math::Intersects(playerRect, blocker)) {
            return true;
        }
    }
    return false;
}

}  // namespace ashpaw::client
