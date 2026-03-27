#include "ashpaw/engine/ui/UiLayer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_opengl2.h>
#include <backends/imgui_impl_sdl2.h>

namespace ashpaw::engine::ui {

bool UiLayer::Initialize(SDL_Window* window, void* glContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
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
        ImGui::Text("Collision: %s", state.colliding ? "yes" : "no");
        ImGui::Text("Entities: %d", state.entityCount);
        ImGui::Text("Network: %s", state.networkState);
        ImGui::Text("Map: %s", state.currentMap);
        ImGui::Separator();
        ImGui::TextUnformatted("WASD move | F1 debug | ESC quit");
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
        ImGui::TextUnformatted(state.sessionActive
            ? "Session active. Client world is live."
            : "Session inactive. Waiting for connect + handshake success.");
        if (state.canRetry) {
            ImGui::TextUnformatted("Press F5 to retry connection.");
        }
    }
    ImGui::End();
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
