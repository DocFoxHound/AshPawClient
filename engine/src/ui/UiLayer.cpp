#include "ashpaw/engine/ui/UiLayer.hpp"

#include <cinttypes>

#include <imgui.h>
#include <backends/imgui_impl_opengl2.h>
#include <backends/imgui_impl_sdl2.h>

namespace ashpaw::engine::ui {

bool UiLayer::Initialize(SDL_Window* window, void* glContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui::GetIO().ConfigInputTextEnterKeepActive = false;
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL2_Init();
    initialized_ = true;
    return true;
}

void UiLayer::Shutdown() {
    if (!initialized_) {
        return;
    }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void UiLayer::ProcessEvent(const SDL_Event& event) {
    if (initialized_) {
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
}

void UiLayer::BeginFrame() {
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UiLayer::RenderDebugOverlay(const DebugOverlayState& state) const {
    if (!state.visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.75F);
    constexpr auto windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Debug Overlay", nullptr, windowFlags)) {
        ImGui::Text("FPS: %.1f", state.fps);
        ImGui::Text("Player: (%.1f, %.1f)", state.playerPosition.x, state.playerPosition.y);
        ImGui::Text("Local entity: %" PRIu64, state.localPlayerId);
        ImGui::Text("Collision: %s", state.colliding ? "yes" : "no");
        ImGui::Text("Entities: %d", state.entityCount);
        ImGui::Text("Network: %s", state.networkState);
        ImGui::Text("Map: %s", state.currentMap);
        ImGui::Text("Ping: %.0f ms", state.pingMs);
        ImGui::Text("Packets: sent %" PRIu64 " / recv %" PRIu64, state.packetsSent, state.packetsReceived);
        ImGui::Text("Snapshot rate: %.1f/s", state.snapshotRate);
        ImGui::Text("Interpolation delay: %.0f ms", state.interpolationDelayMs);
        ImGui::Separator();
        ImGui::TextUnformatted("WASD move | Enter chat | E interact | F1 debug | ESC close/quit");
    }
    ImGui::End();
}

void UiLayer::RenderConnectionPanel(const ConnectionPanelState& state) const {
    if (!state.visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.80F);
    ImGui::SetNextWindowPos(ImVec2(16.0F, 16.0F), ImGuiCond_Always);
    constexpr auto windowFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Session Status", nullptr, windowFlags)) {
        if (state.loading) {
            ImGui::TextColored(ImVec4(0.58F, 0.78F, 0.96F, 1.0F), "Connecting...");
            ImGui::Separator();
        } else if (state.lastError[0] != '\0') {
            ImGui::TextColored(ImVec4(0.92F, 0.40F, 0.36F, 1.0F), "Connection issue");
            ImGui::Separator();
        }
        ImGui::Text("State: %s", state.connectionState);
        ImGui::Text("Player: %s", state.playerName);
        ImGui::Text("Target: %s:%d", state.targetHost, state.targetPort);
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.detailMessage);
        if (state.lastError[0] != '\0') {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.92F, 0.40F, 0.36F, 1.0F), "Last error: %s", state.lastError);
        }
        ImGui::Spacing();
        if (state.sessionActive) {
            ImGui::TextUnformatted("Session active. Client world is live.");
        } else if (state.loading) {
            ImGui::TextUnformatted("Waiting for connect + handshake success.");
        } else {
            ImGui::TextUnformatted("Session inactive. Check settings, then connect when ready.");
        }
        if (state.canRetry) {
            ImGui::TextUnformatted("Press F5 to retry connection.");
        }
    }
    ImGui::End();
}

IdentityPanelResult UiLayer::RenderIdentityPanel(IdentityPanelState& state) const {
    IdentityPanelResult result;
    if (!state.visible || state.requestedNameBuffer == nullptr) {
        return result;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.90F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 356.0F, viewport->WorkPos.y + 16.0F),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(340.0F, 296.0F), ImGuiCond_Always);
    constexpr auto windowFlags = ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Options & Identity", nullptr, windowFlags)) {
        ImGui::Text("Requested name");
        const auto editedName = ImGui::InputText("##RequestedName", state.requestedNameBuffer->data(), state.requestedNameBuffer->size());
        result.valuesChanged = result.valuesChanged || editedName;

        ImGui::Separator();
        ImGui::Text("Authoritative name: %s", state.authoritativeName[0] == '\0' ? "(waiting for server)" : state.authoritativeName);
        ImGui::Text("Last known map: %s", state.cachedMapName[0] == '\0' ? "(none yet)" : state.cachedMapName);
        ImGui::Text("Server: %s:%d", state.host, state.port);

        ImGui::Separator();
        ImGui::TextUnformatted("Startup video");
        result.valuesChanged = ImGui::InputInt("Window width", &state.windowWidth) || result.valuesChanged;
        result.valuesChanged = ImGui::InputInt("Window height", &state.windowHeight) || result.valuesChanged;
        result.valuesChanged = ImGui::Checkbox("Auto-connect on launch", &state.autoConnect) || result.valuesChanged;
        result.valuesChanged = ImGui::Checkbox("Show debug overlay", &state.showDebugOverlay) || result.valuesChanged;
        result.valuesChanged = ImGui::Checkbox("Show onboarding hints", &state.showOnboardingHints) || result.valuesChanged;
        result.valuesChanged = ImGui::Checkbox("VSync on startup", &state.vsync) || result.valuesChanged;
        result.valuesChanged = ImGui::Checkbox("Fullscreen on startup", &state.fullscreen) || result.valuesChanged;
        result.valuesChanged = ImGui::SliderInt("Master volume", &state.masterVolumePercent, 0, 100) || result.valuesChanged;

        ImGui::Spacing();
        if (ImGui::Button("Save Settings")) {
            result.saveRequested = true;
        }
        ImGui::SameLine();
        if (!state.sessionActive) {
            if (ImGui::Button("Connect")) {
                result.connectRequested = true;
            }
        } else if (ImGui::Button("Disconnect")) {
            result.disconnectRequested = true;
        }

        ImGui::Spacing();
        ImGui::TextWrapped("Requested name is a local preference. Resolution, fullscreen, and VSync changes apply on the next launch.");
        ImGui::TextWrapped("The server still decides the authoritative identity on join.");
    }
    ImGui::End();
    return result;
}

DeveloperPanelResult UiLayer::RenderDeveloperPanel(DeveloperPanelState& state) const {
    DeveloperPanelResult result {
        .showCollisionDebug = state.showCollisionDebug,
        .showInteractionRangeDebug = state.showInteractionRangeDebug,
        .showCameraBoundsDebug = state.showCameraBoundsDebug,
        .showPacketLog = state.showPacketLog
    };
    if (!state.visible) {
        return result;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.90F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 356.0F, viewport->WorkPos.y + 272.0F),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(340.0F, 320.0F), ImGuiCond_Always);
    constexpr auto windowFlags = ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Developer Panel", nullptr, windowFlags)) {
        ImGui::Text("Map: %s", state.currentMap);
        ImGui::Text("Local entity: %" PRIu64, state.localPlayerId);
        ImGui::Text("Spawn: (%.1f, %.1f)", state.spawnX, state.spawnY);
        ImGui::Text("Ping: %.0f ms", state.pingMs);
        ImGui::Text("Packets: %" PRIu64 " sent / %" PRIu64 " recv", state.packetsSent, state.packetsReceived);
        ImGui::Separator();
        ImGui::Checkbox("Collision debug", &result.showCollisionDebug);
        ImGui::Checkbox("Interaction range", &result.showInteractionRangeDebug);
        ImGui::Checkbox("Camera bounds", &result.showCameraBoundsDebug);
        ImGui::Checkbox("Packet log", &result.showPacketLog);
        ImGui::Separator();
        ImGui::TextWrapped("Assets: %s", state.assetStatus);
        if (ImGui::Button("Reload Map")) {
            result.reloadMapRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(state.sessionActive ? "Reconnect" : "Connect")) {
            result.reconnectRequested = true;
        }

        if (result.showPacketLog) {
            ImGui::Spacing();
            if (ImGui::BeginChild("PacketLog", ImVec2(0.0F, 110.0F), true)) {
                for (const auto& line : state.packetLog) {
                    ImGui::TextWrapped("%s", line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0F) {
                    ImGui::SetScrollHereY(1.0F);
                }
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
    return result;
}

HelpPanelResult UiLayer::RenderHelpPanel(const HelpPanelState& state) const {
    HelpPanelResult result;
    if (!state.visible) {
        return result;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.92F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 16.0F, viewport->WorkPos.y + 110.0F),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(320.0F, 190.0F), ImGuiCond_Always);
    constexpr auto windowFlags = ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Getting Started", nullptr, windowFlags)) {
        ImGui::Text("Welcome%s%s",
            state.authoritativeName[0] != '\0' ? ", " : "",
            state.authoritativeName[0] != '\0' ? state.authoritativeName : "");
        if (state.currentMap[0] != '\0') {
            ImGui::Text("Map: %s", state.currentMap);
        }
        ImGui::Separator();
        ImGui::BulletText("Move with WASD");
        ImGui::BulletText("Press Enter to open chat");
        ImGui::BulletText("Press E near a marker to interact");
        ImGui::BulletText("Use F1 for the developer overlay");
        ImGui::Spacing();
        if (state.sessionActive) {
            ImGui::TextWrapped("You are connected and ready to roam. Explore the meadow and try the welcome markers.");
        } else {
            ImGui::TextWrapped("Start by reviewing your options on the right, then connect when you are ready.");
        }
        if (ImGui::Button("Got it")) {
            result.dismissed = true;
        }
    }
    ImGui::End();
    return result;
}

void UiLayer::RenderInteractionPrompt(const InteractionPromptState& state) const {
    if (!state.visible) {
        return;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.86F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 24.0F, viewport->WorkPos.y + viewport->WorkSize.y - 72.0F),
        ImGuiCond_Always
    );
    constexpr auto windowFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Interaction Prompt", nullptr, windowFlags)) {
        ImGui::Text("Near: %s", state.targetLabel);
        ImGui::Separator();
        ImGui::TextUnformatted(state.helperText);
    }
    ImGui::End();
}

void UiLayer::RenderInteractionResult(const InteractionResultState& state) const {
    if (!state.visible) {
        return;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.82F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 16.0F, viewport->WorkPos.y + viewport->WorkSize.y - 156.0F),
        ImGuiCond_Always
    );
    constexpr auto windowFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Interaction Result", nullptr, windowFlags)) {
        const auto color = state.approved ? ImVec4(0.49F, 0.78F, 0.55F, 1.0F) : ImVec4(0.90F, 0.43F, 0.39F, 1.0F);
        ImGui::TextColored(color, "%s", state.title);
        if (state.detail[0] != '\0') {
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.detail);
        }
    }
    ImGui::End();
}

void UiLayer::RenderChatFocusHint(const ChatFocusState& state) const {
    if (!state.visible) {
        return;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.86F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 448.0F, viewport->WorkPos.y + viewport->WorkSize.y - 60.0F),
        ImGuiCond_Always
    );
    constexpr auto windowFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Chat Focus", nullptr, windowFlags)) {
        ImGui::TextColored(ImVec4(0.85F, 0.77F, 0.39F, 1.0F), "Chat focus is active");
        ImGui::Separator();
        ImGui::TextUnformatted("Enter sends | ESC closes chat");
    }
    ImGui::End();
}

ChatPanelResult UiLayer::RenderChatPanel(ChatPanelState& state) const {
    ChatPanelResult result;
    if (!state.visible || state.inputBuffer == nullptr) {
        return result;
    }

    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowBgAlpha(0.88F);
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 16.0F, viewport->WorkPos.y + viewport->WorkSize.y - 320.0F),
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize(ImVec2(420.0F, 220.0F), ImGuiCond_Always);
    constexpr auto windowFlags = ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Chat", nullptr, windowFlags)) {
        if (!state.open) {
            ImGui::TextUnformatted("Press Enter to open chat.");
        }

        if (ImGui::BeginChild("ChatLog", ImVec2(0.0F, state.open ? -36.0F : 0.0F), true)) {
            const auto startIndex = state.entries.size() > state.maxMessageCount
                ? state.entries.size() - state.maxMessageCount
                : 0U;
            for (std::size_t index = startIndex; index < state.entries.size(); ++index) {
                const auto& entry = state.entries[index];
                const auto speakerColor = entry.selfAuthored
                    ? ImVec4(0.85F, 0.77F, 0.39F, 1.0F)
                    : ImVec4(0.57F, 0.74F, 0.92F, 1.0F);
                ImGui::TextColored(speakerColor, "%s", entry.speaker);
                ImGui::SameLine();
                ImGui::TextWrapped("%s", entry.body);
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0F) {
                ImGui::SetScrollHereY(1.0F);
            }
        }
        ImGui::EndChild();

        if (state.open) {
            if (state.focusInput) {
                ImGui::SetKeyboardFocusHere();
            }
            result.inputFocused = true;
            constexpr auto inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;
            result.submitted = ImGui::InputText("##ChatInput", state.inputBuffer->data(), state.inputBuffer->size(), inputFlags);
            ImGui::SameLine();
            ImGui::TextUnformatted("Enter to send");
        }
    }
    ImGui::End();
    return result;
}

void UiLayer::RenderEntityLabels(
    const std::vector<world::EntityPresentation>& entities,
    const camera::Camera2D& camera
) const {
    auto* drawList = ImGui::GetForegroundDrawList();
    for (const auto& entity : entities) {
        if (entity.displayName.empty()) {
            continue;
        }

        const auto screenPosition = camera.WorldToScreen({
            entity.position.x + (entity.size.x * 0.5F) + entity.labelOffset.x,
            entity.position.y + entity.labelOffset.y
        });
        const auto textSize = ImGui::CalcTextSize(entity.displayName.c_str());
        const ImVec2 textPosition(screenPosition.x - (textSize.x * 0.5F), screenPosition.y);
        const ImVec2 backgroundMin(textPosition.x - 6.0F, textPosition.y - 3.0F);
        const ImVec2 backgroundMax(textPosition.x + textSize.x + 6.0F, textPosition.y + textSize.y + 3.0F);

        drawList->AddRectFilled(backgroundMin, backgroundMax, IM_COL32(14, 18, 20, 180), 5.0F);
        drawList->AddText(textPosition, IM_COL32(235, 238, 240, 255), entity.displayName.c_str());
    }
}

void UiLayer::EndFrame() const {
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

bool UiLayer::WantsKeyboardCapture() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

}  // namespace ashpaw::engine::ui
