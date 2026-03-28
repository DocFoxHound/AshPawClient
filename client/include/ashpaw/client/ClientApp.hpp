#pragma once

#include <array>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ashpaw/engine/assets/AssetManager.hpp"
#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/config/Config.hpp"
#include "ashpaw/engine/input/InputSystem.hpp"
#include "ashpaw/engine/net/NetworkClient.hpp"
#include "ashpaw/engine/platform/PlatformContext.hpp"
#include "ashpaw/engine/prediction/SnapshotBuffer.hpp"
#include "ashpaw/engine/render/RenderSystem.hpp"
#include "ashpaw/engine/ui/UiLayer.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

namespace ashpaw::client {

[[nodiscard]] inline bool IsWithinInteractionRange(const engine::math::Vector2& source, const engine::math::Vector2& target) {
    constexpr float interactionRange = 80.0F;
    const auto dx = source.x - target.x;
    const auto dy = source.y - target.y;
    return (dx * dx) + (dy * dy) <= (interactionRange * interactionRange);
}

struct ClientChatEntry {
    std::uint64_t speakerEntityId {0};
    std::string speaker;
    std::string body;
    bool selfAuthored {false};
};

struct ActiveInteractableTarget {
    std::string targetId;
    std::string label;
};

class ClientApp {
public:
    bool Initialize(const std::filesystem::path& configPath);
    int Run();
    void Shutdown();

private:
    void ApplyServerMessages();
    void AppendChatMessage(const ClientChatEntry& message);
    void BeginConnection();
    [[nodiscard]] double CurrentTimeSeconds() const;
    [[nodiscard]] std::optional<ActiveInteractableTarget> FindInteractionTarget() const;
    [[nodiscard]] std::string LabelForTarget(std::string_view targetId) const;
    [[nodiscard]] bool IsMarkerAuthoritativeInteractable(const engine::assets::MapMarker& marker) const;
    void PersistConfig();
    void ReloadCurrentMap();
    void RetryConnection();
    void LoadScene();
    void SyncSessionState();
    void UpdateInteractionState();
    void UpdateInterpolatedEntities();
    void Update(float deltaSeconds);
    void Render(float deltaSeconds);
    [[nodiscard]] bool CollidesAt(const ashpaw::engine::math::Vector2& position) const;

    std::filesystem::path configPath_ {};
    engine::config::AppConfig config_ {};
    engine::platform::PlatformContext platform_ {};
    engine::render::RenderSystem renderer_ {};
    engine::assets::AssetManager assetManager_ {};
    engine::assets::MapData currentMap_ {};
    engine::input::InputSystem input_ {};
    engine::camera::Camera2D camera_ {};
    engine::world::ClientWorld world_ {};
    engine::net::NetworkClient network_ {};
    engine::ui::UiLayer ui_ {};

    bool running_ {false};
    bool debugOverlayVisible_ {true};
    bool colliding_ {false};
    bool sessionActive_ {false};
    bool connectAttempted_ {false};
    bool showCollisionDebug_ {false};
    bool showInteractionRangeDebug_ {false};
    bool showCameraBoundsDebug_ {false};
    bool showPacketLog_ {false};
    bool helpDismissedThisSession_ {false};
    bool pendingConnectRequest_ {false};
    bool pendingDisconnectRequest_ {false};
    bool pendingReloadMapRequest_ {false};
    bool pendingSaveSettingsRequest_ {false};
    std::string sessionStateLabel_ {"disconnected"};
    std::string assetStatusMessage_ {"Loaded initial map"};
    bool chatOpen_ {false};
    bool chatFocusRequested_ {false};
    std::array<char, 256> chatInputBuffer_ {};
    std::array<char, 64> requestedNameBuffer_ {};
    std::vector<ClientChatEntry> chatMessages_ {};
    std::optional<ActiveInteractableTarget> activeInteractionMarker_ {};
    bool interactionResultApproved_ {false};
    std::string interactionResultTitle_ {};
    std::string interactionResultDetail_ {};
    double interactionResultVisibleUntil_ {0.0};
    std::unordered_map<engine::world::EntityId, engine::prediction::SnapshotBuffer> remoteSnapshotBuffers_ {};
    std::deque<double> snapshotReceiptTimes_ {};
};

}  // namespace ashpaw::client
