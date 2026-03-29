#include "ashpaw/engine/platform/PlatformContext.hpp"

#include <stdexcept>

#include "ashpaw/engine/util/Log.hpp"

namespace ashpaw::engine::platform {

PlatformContext::~PlatformContext() {
    Shutdown();
}

bool PlatformContext::Initialize(const PlatformConfig& config) {
    util::Logger()->info(
        "Initializing platform: title='{}' size={}x{} fullscreen={} vsync={}",
        config.title,
        config.width,
        config.height,
        config.fullscreen,
        config.vsync
    );
    util::LogSdlEnvironment();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        util::LogSdlError("SDL_Init failed");
        throw std::runtime_error(SDL_GetError());
    }
    util::Logger()->info(
        "SDL initialized with video driver='{}' audio driver='{}'",
        SDL_GetCurrentVideoDriver() != nullptr ? SDL_GetCurrentVideoDriver() : "(null)",
        SDL_GetCurrentAudioDriver() != nullptr ? SDL_GetCurrentAudioDriver() : "(null)"
    );

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    util::Logger()->info("Requested OpenGL context version 2.1");

    const auto fullscreenFlag = config.fullscreen ? static_cast<Uint32>(SDL_WINDOW_FULLSCREEN_DESKTOP) : 0U;
    const auto flags = static_cast<Uint32>(SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | fullscreenFlag);
    window_ = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.width,
        config.height,
        flags
    );
    if (window_ == nullptr) {
        util::LogSdlError("SDL_CreateWindow failed");
        throw std::runtime_error(SDL_GetError());
    }
    util::Logger()->info("Created SDL window successfully");

    glContext_ = SDL_GL_CreateContext(window_);
    if (glContext_ == nullptr) {
        util::LogSdlError("SDL_GL_CreateContext failed");
        throw std::runtime_error(SDL_GetError());
    }
    util::Logger()->info("Created OpenGL context successfully");

    if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
        util::LogSdlError("SDL_GL_MakeCurrent failed");
        throw std::runtime_error(SDL_GetError());
    }
    util::Logger()->info("Made OpenGL context current");

    int actualMajor = 0;
    int actualMinor = 0;
    int actualProfile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &actualMajor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &actualMinor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &actualProfile);
    util::Logger()->info(
        "Actual OpenGL context attributes: major={} minor={} profile_mask={}",
        actualMajor,
        actualMinor,
        actualProfile
    );

    const auto interval = config.vsync ? 1 : 0;
    if (SDL_GL_SetSwapInterval(interval) != 0) {
        util::LogSdlError("SDL_GL_SetSwapInterval failed");
    } else {
        util::Logger()->info("Swap interval set to {}", interval);
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
    util::Logger()->info("Drawable size is {}x{}", drawableWidth, drawableHeight);
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
