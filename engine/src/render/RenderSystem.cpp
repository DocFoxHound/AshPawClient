#include "ashpaw/engine/render/RenderSystem.hpp"

#include <SDL_opengl.h>

namespace ashpaw::engine::render {

bool RenderSystem::Initialize(SDL_Window* window, bool vsync) {
    static_cast<void>(window);
    const auto interval = vsync ? 1 : 0;
    SDL_GL_SetSwapInterval(interval);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    return true;
}

void RenderSystem::Shutdown() {}

void RenderSystem::BeginFrame(int viewportWidth, int viewportHeight) {
    glViewport(0, 0, viewportWidth, viewportHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(viewportWidth), static_cast<double>(viewportHeight), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.07F, 0.08F, 0.09F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderSystem::RenderMapLayers(
    const assets::MapData& map,
    const camera::Camera2D& camera,
    assets::LayerDrawOrder drawOrder
) const {
    for (const auto& layer : map.layers) {
        if (layer.drawOrder != drawOrder) {
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

void RenderSystem::RenderEntities(const std::vector<world::EntityPresentation>& entities, const camera::Camera2D& camera) const {
    for (const auto& entity : entities) {
        DrawWorldRect(
            {entity.position.x, entity.position.y, entity.size.x, entity.size.y},
            entity.color,
            camera
        );
    }
}

void RenderSystem::RenderMarkers(const std::vector<assets::MapMarker>& markers, const camera::Camera2D& camera) const {
    for (const auto& marker : markers) {
        DrawWorldRect(
            {marker.position.x - 10.0F, marker.position.y - 10.0F, 20.0F, 20.0F},
            marker.color,
            camera
        );
    }
}

void RenderSystem::EndFrame(SDL_Window* window) const {
    SDL_GL_SwapWindow(window);
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
