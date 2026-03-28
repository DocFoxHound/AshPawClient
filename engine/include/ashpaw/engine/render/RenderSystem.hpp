#pragma once

#include <vector>

#include <SDL.h>

#include "ashpaw/engine/assets/AssetManager.hpp"
#include "ashpaw/engine/camera/Camera2D.hpp"
#include "ashpaw/engine/world/ClientWorld.hpp"

namespace ashpaw::engine::render {

class RenderSystem {
public:
    bool Initialize(SDL_Window* window, bool vsync);
    void Shutdown();
    void BeginFrame(int viewportWidth, int viewportHeight);
    void RenderMapLayers(
        const assets::MapData& map,
        const camera::Camera2D& camera,
        assets::LayerDrawOrder drawOrder
    ) const;
    void RenderCollisionDebug(const std::vector<math::Rect>& blockers, const camera::Camera2D& camera) const;
    void RenderInteractionRangeDebug(const math::Vector2& center, float range, const camera::Camera2D& camera) const;
    void RenderCameraBoundsDebug(const math::Vector2& worldSize, const camera::Camera2D& camera) const;
    void RenderMarkers(const std::vector<assets::MapMarker>& markers, const camera::Camera2D& camera) const;
    void RenderEntities(const std::vector<world::EntityPresentation>& entities, const camera::Camera2D& camera) const;
    void EndFrame(SDL_Window* window) const;

private:
    static void DrawWorldRect(const math::Rect& rect, const math::Color& color, const camera::Camera2D& camera);
    static void DrawWorldOutline(const math::Rect& rect, const math::Color& color, const camera::Camera2D& camera);
};

}  // namespace ashpaw::engine::render
