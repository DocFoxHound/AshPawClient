#pragma once

#include <filesystem>
#include <string>

#include "ashpaw/engine/assets/AssetManager.hpp"
#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/config/Config.hpp"
#include "ashpaw/engine/input/InputSystem.hpp"
#include "ashpaw/engine/net/NetworkClient.hpp"
#include "ashpaw/engine/platform/PlatformContext.hpp"
#include "ashpaw/engine/render/RenderSystem.hpp"
#include "ashpaw/engine/ui/UiLayer.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

namespace ashpaw::client {

class ClientApp {
public:
    bool Initialize(const std::filesystem::path& configPath);
    int Run();
    void Shutdown();

private:
    void ApplyServerMessages();
    void BeginConnection();
    void RetryConnection();
    void LoadScene();
    void SyncSessionState();
    void Update(float deltaSeconds);
    void Render(float deltaSeconds);
    [[nodiscard]] bool CollidesAt(const ashpaw::engine::math::Vector2& position) const;

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
    std::string sessionStateLabel_ {"disconnected"};
};

}  // namespace ashpaw::client
