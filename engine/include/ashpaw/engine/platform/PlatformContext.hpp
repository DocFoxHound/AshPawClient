#pragma once

#include <string>

#include <SDL.h>

namespace ashpaw::engine::platform {

struct PlatformConfig {
    std::string title {"AshPaw Client"};
    int width {1280};
    int height {720};
    bool fullscreen {false};
    bool vsync {true};
};

class PlatformContext {
public:
    PlatformContext() = default;
    ~PlatformContext();

    bool Initialize(const PlatformConfig& config);
    void Shutdown();

    [[nodiscard]] bool PollEvent(SDL_Event& event) const;
    [[nodiscard]] SDL_Window* Window() const;

private:
    SDL_Window* window_ {nullptr};
    SDL_GLContext glContext_ {nullptr};
};

}  // namespace ashpaw::engine::platform
