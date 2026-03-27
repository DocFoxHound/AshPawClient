#pragma once

#include <SDL.h>

#include <vector>

#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/math/Vector2.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

namespace ashpaw::engine::ui {

struct DebugOverlayState {
    bool visible {true};
    float fps {0.0F};
    math::Vector2 playerPosition {};
    bool colliding {false};
    int entityCount {0};
    const char* networkState {"disconnected"};
    const char* currentMap {"unknown"};
};

struct ConnectionPanelState {
    bool visible {true};
    bool sessionActive {false};
    const char* connectionState {"disconnected"};
    const char* detailMessage {"Idle"};
    const char* lastError {""};
    const char* playerName {"Local Prowler"};
    const char* targetHost {"127.0.0.1"};
    int targetPort {7777};
    bool canRetry {true};
};

class UiLayer {
public:
    bool Initialize(SDL_Window* window, void* glContext);
    void Shutdown();
    void ProcessEvent(const SDL_Event& event);
    void BeginFrame();
    void RenderDebugOverlay(const DebugOverlayState& state) const;
    void RenderConnectionPanel(const ConnectionPanelState& state) const;
    void RenderEntityLabels(
        const std::vector<world::EntityPresentation>& entities,
        const camera::Camera2D& camera
    ) const;
    void EndFrame() const;
    [[nodiscard]] bool WantsKeyboardCapture() const;

private:
    bool initialized_ {false};
};

}  // namespace ashpaw::engine::ui
