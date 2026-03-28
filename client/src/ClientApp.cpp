#include "ashpaw/client/ClientApp.hpp"

#include <algorithm>
#include <array>
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
constexpr engine::math::Vector2 kPlayerSize {36.0F, 36.0F};
constexpr double kInterpolationDelaySeconds = 0.10;
constexpr float kInteractionRange = 96.0F;
constexpr double kInteractionResultDurationSeconds = 4.0;
constexpr std::size_t kMaxChatMessages = 48;

const char* ToString(engine::net::ConnectionState state) {
    switch (state) {
    case engine::net::ConnectionState::Disconnected:
        return "disconnected";
    case engine::net::ConnectionState::Connecting:
        return "connecting";
    case engine::net::ConnectionState::WaitingForServerHello:
        return "waiting_for_server_hello";
    case engine::net::ConnectionState::WaitingForJoinAccepted:
        return "waiting_for_join_accepted";
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

engine::math::Color EntityColor(bool localControlled) {
    if (localControlled) {
        return {0.82F, 0.71F, 0.31F, 1.0F};
    }
    return {0.39F, 0.67F, 0.92F, 1.0F};
}

float SquaredDistance(const engine::math::Vector2& lhs, const engine::math::Vector2& rhs) {
    const auto dx = lhs.x - rhs.x;
    const auto dy = lhs.y - rhs.y;
    return (dx * dx) + (dy * dy);
}

std::string TrimCopy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

void CopyStringToBuffer(std::string_view value, std::array<char, 64>& buffer) {
    buffer.fill('\0');
    const auto length = std::min(value.size(), buffer.size() - 1U);
    std::copy_n(value.data(), static_cast<std::ptrdiff_t>(length), buffer.data());
}

const char* InteractionStatusTitle(engine::net::InteractionStatus status) {
    switch (status) {
    case engine::net::InteractionStatus::Success:
        return "Interaction";
    case engine::net::InteractionStatus::NotFound:
        return "Not found";
    case engine::net::InteractionStatus::OutOfRange:
        return "Out of range";
    case engine::net::InteractionStatus::Blocked:
        return "Blocked";
    case engine::net::InteractionStatus::InvalidTarget:
        return "Invalid target";
    }
    return "Interaction";
}
}  // namespace

bool ClientApp::Initialize(const std::filesystem::path& configPath) {
    configPath_ = configPath;
    config_ = engine::config::Config::LoadFromFile(configPath);
    engine::util::InitializeLogger(config_.logLevel);
    debugOverlayVisible_ = config_.showDebugOverlay;
    CopyStringToBuffer(config_.playerName, requestedNameBuffer_);

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
    PersistConfig();
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

void ClientApp::PersistConfig() {
    config_.showDebugOverlay = debugOverlayVisible_;
    config_.windowWidth = std::max(config_.windowWidth, 800);
    config_.windowHeight = std::max(config_.windowHeight, 600);
    config_.masterVolumePercent = std::clamp(config_.masterVolumePercent, 0, 100);
    config_.playerName = engine::net::SanitizeDisplayName(requestedNameBuffer_.data());
    CopyStringToBuffer(config_.playerName, requestedNameBuffer_);
    if (!configPath_.empty()) {
        try {
            engine::config::Config::SaveToFile(configPath_, config_);
        } catch (const std::exception& exception) {
            engine::util::Logger()->warn("Failed to persist config: {}", exception.what());
        }
    }
}

void ClientApp::ReloadCurrentMap() {
    try {
        currentMap_ = assetManager_.LoadMap(config_.mapPath);
        camera_.SetWorldBounds(currentMap_.worldSize.x, currentMap_.worldSize.y);
        auto sceneInfo = world_.SceneInfo();
        sceneInfo.mapName = currentMap_.name;
        sceneInfo.worldSize = currentMap_.worldSize;
        world_.SetSceneInfo(sceneInfo);
        config_.cachedLastMap = currentMap_.name;
        assetStatusMessage_ = "Reloaded " + config_.mapPath;
        engine::util::Logger()->info("Reloaded map from {}", config_.mapPath);
    } catch (const std::exception& exception) {
        assetStatusMessage_ = std::string("Reload failed: ") + exception.what();
        engine::util::Logger()->error("Map reload failed: {}", exception.what());
    }
}

double ClientApp::CurrentTimeSeconds() const {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
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
    world_.ClearAuthorityState();
    remoteSnapshotBuffers_.clear();
    snapshotReceiptTimes_.clear();
    pendingConnectRequest_ = false;
    pendingDisconnectRequest_ = false;
    pendingReloadMapRequest_ = false;
    pendingSaveSettingsRequest_ = false;
    helpDismissedThisSession_ = false;
    chatOpen_ = false;
    chatFocusRequested_ = false;
    chatInputBuffer_.fill('\0');
    chatMessages_.clear();
    activeInteractionMarker_.reset();
    interactionResultTitle_.clear();
    interactionResultDetail_.clear();
    interactionResultVisibleUntil_ = 0.0;
    assetStatusMessage_ = "Loaded " + config_.mapPath;
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

void ClientApp::AppendChatMessage(const ClientChatEntry& message) {
    chatMessages_.push_back(message);
    if (chatMessages_.size() > kMaxChatMessages) {
        chatMessages_.erase(
            chatMessages_.begin(),
            chatMessages_.begin() + static_cast<std::ptrdiff_t>(chatMessages_.size() - kMaxChatMessages)
        );
    }
}

void ClientApp::SyncSessionState() {
    const auto state = network_.State();
    const auto wasActive = sessionActive_;
    sessionActive_ = network_.SessionActive();
    sessionStateLabel_ = ToString(state);

    if (!wasActive && sessionActive_) {
        if (const auto sessionInit = network_.ConsumeSessionInit(); sessionInit.has_value()) {
            world_.ClearEntities();
            world_.ClearAuthorityState();
            remoteSnapshotBuffers_.clear();

            world_.SetLocalPlayerId(sessionInit->entityId);
            world_.UpsertEntity({
                .id = sessionInit->entityId,
                .position = {sessionInit->spawnX, sessionInit->spawnY},
                .size = kPlayerSize,
                .color = EntityColor(true),
                .displayName = config_.cachedAuthoritativeName.empty() ? "Joining..." : config_.cachedAuthoritativeName,
                .authoritative = true,
                .renderLayer = engine::world::RenderLayer::Actors,
                .labelOffset = {0.0F, -20.0F}
            });
            config_.cachedLastMap = world_.SceneInfo().mapName;
        }
        activeInteractionMarker_.reset();
        chatOpen_ = false;
        chatFocusRequested_ = false;
        chatInputBuffer_.fill('\0');
        chatMessages_.clear();
        interactionResultTitle_.clear();
        interactionResultDetail_.clear();
        interactionResultVisibleUntil_ = 0.0;
        AppendChatMessage({
            .speakerEntityId = 0,
            .speaker = "System",
            .body = "Connected. Press Enter to chat with nearby players.",
            .selfAuthored = false
        });
        PersistConfig();
        engine::util::Logger()->info("Session activated after handshake");
    }
    if (wasActive && !sessionActive_) {
        const auto status = network_.Status();
        engine::util::Logger()->warn("Session ended: {}", status.lastError.empty() ? status.detailMessage : status.lastError);
    }
}

std::string ClientApp::LabelForTarget(std::string_view targetId) const {
    const auto marker = std::find_if(currentMap_.markers.begin(), currentMap_.markers.end(), [&](const auto& candidate) {
        return candidate.id == targetId;
    });
    if (marker != currentMap_.markers.end() && !marker->label.empty()) {
        return marker->label;
    }
    return std::string(targetId);
}

bool ClientApp::IsMarkerAuthoritativeInteractable(const engine::assets::MapMarker& marker) const {
    if (world_.FindInteractable(marker.id).has_value()) {
        return true;
    }
    return marker.type == "sign" || marker.type == "signpost";
}

std::optional<ActiveInteractableTarget> ClientApp::FindInteractionTarget() const {
    const auto localPlayer = world_.FindEntity(world_.LocalPlayerId());
    if (!localPlayer.has_value()) {
        return std::nullopt;
    }

    const auto playerCenter = engine::math::Vector2 {
        localPlayer->position.x + (localPlayer->size.x * 0.5F),
        localPlayer->position.y + (localPlayer->size.y * 0.5F)
    };

    std::optional<ActiveInteractableTarget> nearestMarker;
    auto nearestDistanceSquared = kInteractionRange * kInteractionRange;
    for (const auto& marker : currentMap_.markers) {
        if (!IsMarkerAuthoritativeInteractable(marker)) {
            continue;
        }
        const auto distanceSquared = SquaredDistance(playerCenter, marker.position);
        if (distanceSquared > nearestDistanceSquared) {
            continue;
        }
        nearestDistanceSquared = distanceSquared;
        nearestMarker = ActiveInteractableTarget {
            .targetId = marker.id,
            .label = marker.label
        };
    }
    return nearestMarker;
}

void ClientApp::UpdateInteractionState() {
    if (!sessionActive_) {
        activeInteractionMarker_.reset();
        return;
    }
    activeInteractionMarker_ = FindInteractionTarget();
}

void ClientApp::ApplyServerMessages() {
    const auto messages = network_.ConsumeServerMessages();
    const auto nowSeconds = CurrentTimeSeconds();
    for (const auto& message : messages) {
        switch (message.kind) {
        case engine::net::ServerMessageKind::Spawn:
            if (!message.entity.has_value()) {
                break;
            }
            {
                const auto& entity = *message.entity;
                const auto isLocalEntity = entity.entityId == world_.LocalPlayerId();
                const auto identity = world_.FindIdentity(entity.entityId);
                world_.UpsertEntity({
                    .id = entity.entityId,
                    .position = {entity.x, entity.y},
                    .size = kPlayerSize,
                    .color = EntityColor(isLocalEntity),
                    .displayName = identity.has_value() ? identity->displayName : "",
                    .authoritative = true,
                    .renderLayer = engine::world::RenderLayer::Actors,
                    .labelOffset = {0.0F, -20.0F}
                });
                if (!isLocalEntity) {
                    remoteSnapshotBuffers_[entity.entityId].Push({
                        .timestampSeconds = nowSeconds,
                        .position = {entity.x, entity.y}
                    });
                }
            }
            break;
        case engine::net::ServerMessageKind::Snapshot:
            for (const auto& entity : message.snapshotEntities) {
                const auto isLocalEntity = entity.entityId == world_.LocalPlayerId();
                auto worldEntity = world_.FindEntity(entity.entityId).value_or(engine::world::EntityPresentation {
                    .id = entity.entityId,
                    .position = {entity.x, entity.y},
                    .size = kPlayerSize,
                    .color = EntityColor(isLocalEntity),
                    .displayName = world_.FindIdentity(entity.entityId).has_value()
                        ? world_.FindIdentity(entity.entityId)->displayName
                        : "",
                    .authoritative = true,
                    .renderLayer = engine::world::RenderLayer::Actors,
                    .labelOffset = {0.0F, -20.0F}
                });
                worldEntity.color = EntityColor(isLocalEntity);
                worldEntity.authoritative = true;
                if (isLocalEntity) {
                    worldEntity.position = {entity.x, entity.y};
                    world_.UpsertEntity(worldEntity);
                } else {
                    world_.UpsertEntity(worldEntity);
                    remoteSnapshotBuffers_[entity.entityId].Push({
                        .timestampSeconds = nowSeconds,
                        .position = {entity.x, entity.y}
                    });
                    snapshotReceiptTimes_.push_back(nowSeconds);
                }
            }
            break;
        case engine::net::ServerMessageKind::Despawn:
            world_.RemoveEntity(message.entityId);
            world_.RemoveIdentity(message.entityId);
            remoteSnapshotBuffers_.erase(message.entityId);
            break;
        case engine::net::ServerMessageKind::InteractionResult:
            if (!message.interactionResult.has_value()) {
                break;
            }
            interactionResultApproved_ = message.interactionResult->status == engine::net::InteractionStatus::Success;
            interactionResultTitle_ = LabelForTarget(message.interactionResult->targetId);
            if (interactionResultTitle_ == message.interactionResult->targetId || interactionResultTitle_.empty()) {
                interactionResultTitle_ = InteractionStatusTitle(message.interactionResult->status);
            }
            interactionResultDetail_ = message.interactionResult->message;
            interactionResultVisibleUntil_ = CurrentTimeSeconds() + kInteractionResultDurationSeconds;
            break;
        case engine::net::ServerMessageKind::ObjectStateUpdate:
            if (!message.objectStateUpdate.has_value()) {
                break;
            }
            world_.UpsertInteractable({
                .targetId = message.objectStateUpdate->targetId,
                .isOpen = message.objectStateUpdate->isOpen,
                .occupantEntityId = message.objectStateUpdate->occupantEntityId
            });
            break;
        case engine::net::ServerMessageKind::ChatBroadcast:
            if (!message.chatBroadcast.has_value()) {
                break;
            }
            world_.UpsertIdentity({
                .entityId = message.chatBroadcast->entityId,
                .displayName = message.chatBroadcast->displayName
            });
            if (auto worldEntity = world_.FindEntity(message.chatBroadcast->entityId); worldEntity.has_value()) {
                worldEntity->displayName = message.chatBroadcast->displayName;
                world_.UpsertEntity(*worldEntity);
            }
            AppendChatMessage({
                .speakerEntityId = message.chatBroadcast->entityId,
                .speaker = message.chatBroadcast->displayName,
                .body = message.chatBroadcast->message,
                .selfAuthored = message.chatBroadcast->entityId == world_.LocalPlayerId()
            });
            break;
        case engine::net::ServerMessageKind::IdentityUpdate:
            if (!message.identityUpdate.has_value()) {
                break;
            }
            world_.UpsertIdentity({
                .entityId = message.identityUpdate->entityId,
                .displayName = message.identityUpdate->displayName
            });
            if (auto worldEntity = world_.FindEntity(message.identityUpdate->entityId); worldEntity.has_value()) {
                worldEntity->displayName = message.identityUpdate->displayName;
                world_.UpsertEntity(*worldEntity);
            }
            if (message.identityUpdate->entityId == world_.LocalPlayerId()) {
                config_.cachedAuthoritativeName = message.identityUpdate->displayName;
            }
            break;
        case engine::net::ServerMessageKind::Invalid:
            break;
        }
    }

    while (!snapshotReceiptTimes_.empty() && nowSeconds - snapshotReceiptTimes_.front() > 1.0) {
        snapshotReceiptTimes_.pop_front();
    }
}

void ClientApp::UpdateInterpolatedEntities() {
    const auto sampleTime = CurrentTimeSeconds() - kInterpolationDelaySeconds;
    for (auto& [entityId, buffer] : remoteSnapshotBuffers_) {
        const auto sampledPosition = buffer.Sample(sampleTime);
        if (!sampledPosition.has_value()) {
            continue;
        }
        auto entity = world_.FindEntity(entityId);
        if (!entity.has_value()) {
            continue;
        }
        entity->position = *sampledPosition;
        world_.UpsertEntity(*entity);
    }
}

void ClientApp::Update(float deltaSeconds) {
    static_cast<void>(deltaSeconds);
    network_.Tick();
    SyncSessionState();
    ApplyServerMessages();
    UpdateInterpolatedEntities();
    UpdateInteractionState();

    const auto uiCapturesKeyboard = ui_.WantsKeyboardCapture();
    const auto snapshot = input_.Snapshot(uiCapturesKeyboard);

    if (snapshot.quitRequested) {
        running_ = false;
        return;
    }
    if (snapshot.toggleDebugPressed) {
        debugOverlayVisible_ = !debugOverlayVisible_;
    }
    if (snapshot.dismissUiPressed && chatOpen_) {
        chatOpen_ = false;
        chatFocusRequested_ = false;
        chatInputBuffer_.fill('\0');
    }
    if (snapshot.openChatPressed && sessionActive_ && !chatOpen_) {
        chatOpen_ = true;
        chatFocusRequested_ = true;
    }
    if (snapshot.reconnectPressed && !sessionActive_) {
        RetryConnection();
    }
    if (pendingSaveSettingsRequest_) {
        pendingSaveSettingsRequest_ = false;
        PersistConfig();
    }
    if (pendingDisconnectRequest_) {
        pendingDisconnectRequest_ = false;
        network_.Disconnect();
        sessionActive_ = false;
    }
    if (pendingReloadMapRequest_) {
        pendingReloadMapRequest_ = false;
        ReloadCurrentMap();
    }
    if (pendingConnectRequest_) {
        pendingConnectRequest_ = false;
        config_.playerName = engine::net::SanitizeDisplayName(requestedNameBuffer_.data());
        CopyStringToBuffer(config_.playerName, requestedNameBuffer_);
        PersistConfig();
        if (connectAttempted_) {
            RetryConnection();
        } else {
            BeginConnection();
        }
    }

    auto localPlayer = world_.FindEntity(world_.LocalPlayerId());
    if (!localPlayer.has_value()) {
        return;
    }

    if (!sessionActive_) {
        camera_.Follow({localPlayer->position.x + (kPlayerSize.x * 0.5F), localPlayer->position.y + (kPlayerSize.y * 0.5F)});
        return;
    }

    if (snapshot.interactPressed && activeInteractionMarker_.has_value()) {
        network_.SendInteractionRequest({
            .targetId = activeInteractionMarker_->targetId
        });
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
    const auto localPlayer = world_.FindEntity(world_.LocalPlayerId());
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Background);
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Midground);
    renderer_.RenderMarkers(currentMap_.markers, camera_);
    renderer_.RenderEntities(sortedEntities, camera_);
    renderer_.RenderMapLayers(currentMap_, camera_, engine::assets::LayerDrawOrder::Foreground);
    if (showCollisionDebug_) {
        renderer_.RenderCollisionDebug(currentMap_.blockers, camera_);
    }
    if (showCameraBoundsDebug_) {
        renderer_.RenderCameraBoundsDebug(currentMap_.worldSize, camera_);
    }
    if (showInteractionRangeDebug_ && localPlayer.has_value()) {
        renderer_.RenderInteractionRangeDebug(
            {
                localPlayer->position.x + (localPlayer->size.x * 0.5F),
                localPlayer->position.y + (localPlayer->size.y * 0.5F)
            },
            kInteractionRange,
            camera_
        );
    }
    ui_.RenderEntityLabels(sortedEntities, camera_);

    const auto fps = deltaSeconds > 0.0F ? 1.0F / deltaSeconds : 0.0F;
    const auto sceneInfo = world_.SceneInfo();
    const auto networkStatus = network_.Status();
    ui_.RenderConnectionPanel({
        .visible = !sessionActive_,
        .sessionActive = sessionActive_,
        .loading = network_.State() == engine::net::ConnectionState::Connecting ||
                   network_.State() == engine::net::ConnectionState::WaitingForServerHello ||
                   network_.State() == engine::net::ConnectionState::WaitingForJoinAccepted,
        .connectionState = sessionStateLabel_.c_str(),
        .detailMessage = networkStatus.detailMessage.c_str(),
        .lastError = networkStatus.lastError.c_str(),
        .playerName = networkStatus.playerName.c_str(),
        .targetHost = networkStatus.targetHost.c_str(),
        .targetPort = networkStatus.targetPort,
        .canRetry = !sessionActive_
    });
    auto identityPanelState = engine::ui::IdentityPanelState {
        .visible = true,
        .sessionActive = sessionActive_,
        .requestedName = config_.playerName.c_str(),
        .authoritativeName = config_.cachedAuthoritativeName.c_str(),
        .cachedMapName = config_.cachedLastMap.c_str(),
        .host = config_.serverHost.c_str(),
        .port = config_.serverPort,
        .autoConnect = config_.autoConnect,
        .showDebugOverlay = debugOverlayVisible_,
        .showOnboardingHints = config_.showOnboardingHints,
        .vsync = config_.vsync,
        .fullscreen = config_.fullscreen,
        .windowWidth = config_.windowWidth,
        .windowHeight = config_.windowHeight,
        .masterVolumePercent = config_.masterVolumePercent,
        .requestedNameBuffer = &requestedNameBuffer_
    };
    const auto identityPanelResult = ui_.RenderIdentityPanel(identityPanelState);
    config_.autoConnect = identityPanelState.autoConnect;
    debugOverlayVisible_ = identityPanelState.showDebugOverlay;
    config_.showDebugOverlay = identityPanelState.showDebugOverlay;
    config_.showOnboardingHints = identityPanelState.showOnboardingHints;
    config_.vsync = identityPanelState.vsync;
    config_.fullscreen = identityPanelState.fullscreen;
    config_.windowWidth = std::max(identityPanelState.windowWidth, 800);
    config_.windowHeight = std::max(identityPanelState.windowHeight, 600);
    config_.masterVolumePercent = identityPanelState.masterVolumePercent;
    if (identityPanelResult.valuesChanged) {
        config_.playerName = engine::net::SanitizeDisplayName(requestedNameBuffer_.data());
    }
    pendingSaveSettingsRequest_ = pendingSaveSettingsRequest_ || identityPanelResult.saveRequested;
    pendingConnectRequest_ = pendingConnectRequest_ || identityPanelResult.connectRequested;
    pendingDisconnectRequest_ = pendingDisconnectRequest_ || identityPanelResult.disconnectRequested;
    auto developerPanelState = engine::ui::DeveloperPanelState {
        .visible = debugOverlayVisible_,
        .sessionActive = sessionActive_,
        .showCollisionDebug = showCollisionDebug_,
        .showInteractionRangeDebug = showInteractionRangeDebug_,
        .showCameraBoundsDebug = showCameraBoundsDebug_,
        .showPacketLog = showPacketLog_,
        .currentMap = sceneInfo.mapName.c_str(),
        .assetStatus = assetStatusMessage_.c_str(),
        .localPlayerId = world_.LocalPlayerId(),
        .spawnX = localPlayer.has_value() ? localPlayer->position.x : 0.0F,
        .spawnY = localPlayer.has_value() ? localPlayer->position.y : 0.0F,
        .pingMs = static_cast<float>(networkStatus.pingMs),
        .packetsSent = networkStatus.packetsSent,
        .packetsReceived = networkStatus.packetsReceived,
        .packetLog = network_.PacketLog()
    };
    const auto developerPanelResult = ui_.RenderDeveloperPanel(developerPanelState);
    showCollisionDebug_ = developerPanelResult.showCollisionDebug;
    showInteractionRangeDebug_ = developerPanelResult.showInteractionRangeDebug;
    showCameraBoundsDebug_ = developerPanelResult.showCameraBoundsDebug;
    showPacketLog_ = developerPanelResult.showPacketLog;
    pendingConnectRequest_ = pendingConnectRequest_ || developerPanelResult.reconnectRequested;
    pendingReloadMapRequest_ = pendingReloadMapRequest_ || developerPanelResult.reloadMapRequested;
    const auto helpPanelResult = ui_.RenderHelpPanel({
        .visible = config_.showOnboardingHints && !helpDismissedThisSession_,
        .sessionActive = sessionActive_,
        .authoritativeName = config_.cachedAuthoritativeName.c_str(),
        .currentMap = sceneInfo.mapName.c_str()
    });
    if (helpPanelResult.dismissed) {
        helpDismissedThisSession_ = true;
    }
    ui_.RenderInteractionPrompt({
        .visible = sessionActive_ && activeInteractionMarker_.has_value(),
        .targetLabel = activeInteractionMarker_.has_value() ? activeInteractionMarker_->label.c_str() : "",
        .helperText = "Press E to interact"
    });
    ui_.RenderInteractionResult({
        .visible = CurrentTimeSeconds() < interactionResultVisibleUntil_,
        .approved = interactionResultApproved_,
        .title = interactionResultTitle_.c_str(),
        .detail = interactionResultDetail_.c_str()
    });
    std::vector<engine::ui::ChatEntryView> chatEntries;
    chatEntries.reserve(chatMessages_.size());
    for (const auto& message : chatMessages_) {
        chatEntries.push_back({
            .speaker = message.speaker.c_str(),
            .body = message.body.c_str(),
            .selfAuthored = message.selfAuthored
        });
    }
    auto chatState = engine::ui::ChatPanelState {
        .visible = true,
        .open = chatOpen_,
        .focusInput = chatFocusRequested_,
        .maxMessageCount = 10,
        .entries = std::move(chatEntries),
        .inputBuffer = &chatInputBuffer_
    };
    const auto chatResult = ui_.RenderChatPanel(chatState);
    chatFocusRequested_ = false;
    ui_.RenderChatFocusHint({
        .visible = chatOpen_
    });
    if (chatResult.submitted) {
        const auto trimmedMessage = TrimCopy(chatInputBuffer_.data());
        if (!trimmedMessage.empty() && trimmedMessage.size() <= engine::net::kMaxChatMessageLength) {
            network_.SendChatMessage(trimmedMessage);
        }
        chatOpen_ = false;
        chatInputBuffer_.fill('\0');
    }
    ui_.RenderDebugOverlay({
        .visible = debugOverlayVisible_,
        .fps = fps,
        .playerPosition = localPlayer.has_value() ? localPlayer->position : engine::math::Vector2 {},
        .localPlayerId = world_.LocalPlayerId(),
        .colliding = colliding_,
        .entityCount = static_cast<int>(world_.EntityCount()),
        .networkState = ToString(network_.State()),
        .currentMap = sceneInfo.mapName.c_str(),
        .pingMs = static_cast<float>(networkStatus.pingMs),
        .packetsSent = networkStatus.packetsSent,
        .packetsReceived = networkStatus.packetsReceived,
        .snapshotRate = static_cast<float>(snapshotReceiptTimes_.size()),
        .interpolationDelayMs = static_cast<float>(kInterpolationDelaySeconds * 1000.0)
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
