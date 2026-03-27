#include "ashpaw/engine/platform/PlatformContext.hpp"

#include <stdexcept>

namespace ashpaw::engine::platform {

PlatformContext::~PlatformContext() {
    Shutdown();
}

bool PlatformContext::Initialize(const PlatformConfig& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    const auto flags = static_cast<Uint32>(SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
        (config.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0U));
    window_ = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.width,
        config.height,
        flags
    );
    if (window_ == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        throw std::runtime_error(SDL_GetError());
    }

    if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
        throw std::runtime_error(SDL_GetError());
    }

    const auto interval = config.vsync ? 1 : 0;
    SDL_GL_SetSwapInterval(interval);
    return true;
}

void PlatformContext::Shutdown() {
    if (glContext_ != nullptr) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool PlatformContext::PollEvent(SDL_Event& event) const {
    return SDL_PollEvent(&event) != 0;
}

SDL_Window* PlatformContext::Window() const {
    return window_;
}

}  // namespace ashpaw::engine::platform
