#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/math/Vector2.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

namespace ashpaw::engine::ui {

struct DebugOverlayState {
    bool visible {true};
    float fps {0.0F};
    math::Vector2 playerPosition {};
    std::uint64_t localPlayerId {0};
    bool colliding {false};
    int entityCount {0};
    const char* networkState {"disconnected"};
    const char* currentMap {"unknown"};
    float pingMs {0.0F};
    std::uint64_t packetsSent {0};
    std::uint64_t packetsReceived {0};
    float snapshotRate {0.0F};
    float interpolationDelayMs {0.0F};
};

struct ConnectionPanelState {
    bool visible {true};
    bool sessionActive {false};
    bool loading {false};
    const char* connectionState {"disconnected"};
    const char* detailMessage {"Idle"};
    const char* lastError {""};
    const char* playerName {"Local Prowler"};
    const char* targetHost {"127.0.0.1"};
    int targetPort {7777};
    bool canRetry {true};
};

struct InteractionPromptState {
    bool visible {false};
    const char* targetLabel {""};
    const char* helperText {"Press E to interact"};
};

struct InteractionResultState {
    bool visible {false};
    bool approved {false};
    const char* title {""};
    const char* detail {""};
};

struct ChatEntryView {
    const char* speaker {""};
    const char* body {""};
    bool selfAuthored {false};
};

struct ChatPanelState {
    bool visible {true};
    bool open {false};
    bool focusInput {false};
    std::size_t maxMessageCount {0};
    std::vector<ChatEntryView> entries;
    std::array<char, 256>* inputBuffer {nullptr};
};

struct ChatPanelResult {
    bool submitted {false};
    bool inputFocused {false};
};

struct IdentityPanelState {
    bool visible {true};
    bool sessionActive {false};
    const char* requestedName {""};
    const char* authoritativeName {""};
    const char* cachedMapName {""};
    const char* host {""};
    int port {7777};
    bool autoConnect {true};
    bool showDebugOverlay {true};
    bool showOnboardingHints {true};
    bool vsync {true};
    bool fullscreen {false};
    int windowWidth {1280};
    int windowHeight {720};
    int masterVolumePercent {100};
    std::array<char, 64>* requestedNameBuffer {nullptr};
};

struct IdentityPanelResult {
    bool saveRequested {false};
    bool connectRequested {false};
    bool disconnectRequested {false};
    bool valuesChanged {false};
};

struct DeveloperPanelState {
    bool visible {true};
    bool sessionActive {false};
    bool showCollisionDebug {false};
    bool showInteractionRangeDebug {false};
    bool showCameraBoundsDebug {false};
    bool showPacketLog {false};
    const char* currentMap {"unknown"};
    const char* assetStatus {"Idle"};
    std::uint64_t localPlayerId {0};
    float spawnX {0.0F};
    float spawnY {0.0F};
    float pingMs {0.0F};
    std::uint64_t packetsSent {0};
    std::uint64_t packetsReceived {0};
    std::vector<std::string> packetLog;
};

struct DeveloperPanelResult {
    bool reconnectRequested {false};
    bool reloadMapRequested {false};
    bool showCollisionDebug {false};
    bool showInteractionRangeDebug {false};
    bool showCameraBoundsDebug {false};
    bool showPacketLog {false};
};

struct HelpPanelState {
    bool visible {false};
    bool sessionActive {false};
    const char* authoritativeName {""};
    const char* currentMap {""};
};

struct HelpPanelResult {
    bool dismissed {false};
};

struct ChatFocusState {
    bool visible {false};
};

class UiLayer {
public:
    bool Initialize(SDL_Window* window, void* glContext);
    void Shutdown();
    void ProcessEvent(const SDL_Event& event);
    void BeginFrame();
    void RenderDebugOverlay(const DebugOverlayState& state) const;
    void RenderConnectionPanel(const ConnectionPanelState& state) const;
    [[nodiscard]] IdentityPanelResult RenderIdentityPanel(IdentityPanelState& state) const;
    [[nodiscard]] DeveloperPanelResult RenderDeveloperPanel(DeveloperPanelState& state) const;
    [[nodiscard]] HelpPanelResult RenderHelpPanel(const HelpPanelState& state) const;
    void RenderInteractionPrompt(const InteractionPromptState& state) const;
    void RenderInteractionResult(const InteractionResultState& state) const;
    void RenderChatFocusHint(const ChatFocusState& state) const;
    [[nodiscard]] ChatPanelResult RenderChatPanel(ChatPanelState& state) const;
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
