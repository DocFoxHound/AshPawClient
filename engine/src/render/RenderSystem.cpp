#include "ashpaw/engine/render/RenderSystem.hpp"

#include <string_view>

#include <SDL_opengl.h>

#include "ashpaw/engine/util/Log.hpp"

namespace ashpaw::engine::render {

namespace {

const char* GlErrorToString(GLenum error) {
    switch (error) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "GL_UNKNOWN_ERROR";
    }
}

}

bool RenderSystem::Initialize(SDL_Window* window, bool vsync) {
    static_cast<void>(window);
    const auto interval = vsync ? 1 : 0;
    SDL_GL_SetSwapInterval(interval);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    util::Logger()->info("OpenGL vendor: {}", vendor != nullptr ? vendor : "(null)");
    util::Logger()->info("OpenGL renderer: {}", renderer != nullptr ? renderer : "(null)");
    util::Logger()->info("OpenGL version: {}", version != nullptr ? version : "(null)");
    util::Logger()->info("GLSL version: {}", glslVersion != nullptr ? glslVersion : "(null)");
    LogGlError("RenderSystem::Initialize");
    return true;
}

void RenderSystem::Shutdown() {}

void RenderSystem::BeginFrame(int viewportWidth, int viewportHeight) {
    if (!firstFrameLogged_) {
        util::Logger()->info("Beginning first frame at {}x{}", viewportWidth, viewportHeight);
        firstFrameLogged_ = true;
    }
    glViewport(0, 0, viewportWidth, viewportHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(viewportWidth), static_cast<double>(viewportHeight), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.07F, 0.08F, 0.09F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    LogGlError("RenderSystem::BeginFrame");
}

void RenderSystem::RenderMapLayers(
    const assets::MapData& map,
    const camera::Camera2D& camera,
    assets::LayerDrawOrder drawOrder,
    std::int32_t activeZ
) const {
    for (const auto& layer : map.layers) {
        if (layer.drawOrder != drawOrder || layer.z != activeZ) {
            continue;
        }
        for (const auto& visual : layer.visuals) {
            DrawWorldRect(visual.bounds, visual.color, camera);
        }
    }

}

void RenderSystem::RenderCollisionDebug(const std::vector<math::Rect>& blockers, const camera::Camera2D& camera) const {
    for (const auto& blocker : blockers) {
        DrawWorldRect(blocker, {0.58F, 0.42F, 0.28F, 0.32F}, camera);
        DrawWorldOutline(blocker, {0.88F, 0.69F, 0.41F, 0.95F}, camera);
    }
}

void RenderSystem::RenderInteractionRangeDebug(const math::Vector2& center, float range, const camera::Camera2D& camera) const {
    DrawWorldOutline(
        {center.x - range, center.y - range, range * 2.0F, range * 2.0F},
        {0.48F, 0.82F, 0.61F, 0.95F},
        camera
    );
}

void RenderSystem::RenderCameraBoundsDebug(const math::Vector2& worldSize, const camera::Camera2D& camera) const {
    DrawWorldOutline(
        {0.0F, 0.0F, worldSize.x, worldSize.y},
        {0.54F, 0.78F, 0.96F, 0.95F},
        camera
    );
}

void RenderSystem::RenderEntities(const std::vector<world::EntityPresentation>& entities, const camera::Camera2D& camera, std::int32_t activeZ) const {
    for (const auto& entity : entities) {
        if (entity.z != activeZ) {
            continue;
        }
        DrawWorldRect(
            {entity.position.x, entity.position.y, entity.size.x, entity.size.y},
            entity.color,
            camera
        );
    }
}

void RenderSystem::RenderMarkers(const std::vector<assets::MapMarker>& markers, const camera::Camera2D& camera, std::int32_t activeZ) const {
    for (const auto& marker : markers) {
        if (marker.z != activeZ) {
            continue;
        }
        DrawWorldRect(
            {marker.position.x - 10.0F, marker.position.y - 10.0F, 20.0F, 20.0F},
            marker.color,
            camera
        );
    }
}

void RenderSystem::EndFrame(SDL_Window* window) const {
    LogGlError("RenderSystem::EndFrame before swap");
    SDL_GL_SwapWindow(window);
}

void RenderSystem::LogGlError(std::string_view stage) const {
    for (auto error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
        if (loggedGlErrorCount_ < 12) {
            util::Logger()->error("{} failed with {} ({})", stage, GlErrorToString(error), static_cast<unsigned int>(error));
        } else if (loggedGlErrorCount_ == 12) {
            util::Logger()->error("Additional OpenGL errors suppressed");
        }
        ++loggedGlErrorCount_;
    }
}

void RenderSystem::DrawWorldRect(const math::Rect& rect, const math::Color& color, const camera::Camera2D& camera) {
    const auto topLeft = camera.WorldToScreen({rect.x, rect.y});
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(topLeft.x, topLeft.y);
    glVertex2f(topLeft.x + rect.w, topLeft.y);
    glVertex2f(topLeft.x + rect.w, topLeft.y + rect.h);
    glVertex2f(topLeft.x, topLeft.y + rect.h);
    glEnd();
}

void RenderSystem::DrawWorldOutline(const math::Rect& rect, const math::Color& color, const camera::Camera2D& camera) {
    const auto topLeft = camera.WorldToScreen({rect.x, rect.y});
    glColor4f(color.r, color.g, color.b, color.a);
    glLineWidth(2.0F);
    glBegin(GL_LINE_LOOP);
    glVertex2f(topLeft.x, topLeft.y);
    glVertex2f(topLeft.x + rect.w, topLeft.y);
    glVertex2f(topLeft.x + rect.w, topLeft.y + rect.h);
    glVertex2f(topLeft.x, topLeft.y + rect.h);
    glEnd();
}

}  // namespace ashpaw::engine::render
